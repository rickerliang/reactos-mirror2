/*
 * Mesa 3-D graphics library
 *
 * Copyright (C) 2004-2008  Brian Paul   All Rights Reserved.
 * Copyright (C) 2009-2010  VMware, Inc.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * \file shaderapi.c
 * \author Brian Paul
 *
 * Implementation of GLSL-related API functions.
 * The glUniform* functions are in uniforms.c
 *
 *
 * XXX things to do:
 * 1. Check that the right error code is generated for all _mesa_error() calls.
 * 2. Insert FLUSH_VERTICES calls in various places
 */


#include "main/glheader.h"
#include "main/context.h"
#include "main/dispatch.h"
#include "main/enums.h"
#include "main/hash.h"
#include "main/mfeatures.h"
#include "main/mtypes.h"
#include "main/shaderapi.h"
#include "main/shaderobj.h"
#include "main/uniforms.h"
#include "program/program.h"
#include "program/prog_parameter.h"
#include "ralloc.h"
#include <stdbool.h>
#include "../glsl/glsl_parser_extras.h"
#include "../glsl/ir_uniform.h"

/** Define this to enable shader substitution (see below) */
#define SHADER_SUBST 0


/**
 * Return mask of GLSL_x flags by examining the MESA_GLSL env var.
 */
static GLbitfield
get_shader_flags(void)
{
   GLbitfield flags = 0x0;
   const char *env = _mesa_getenv("MESA_GLSL");

   if (env) {
      if (strstr(env, "dump"))
         flags |= GLSL_DUMP;
      if (strstr(env, "log"))
         flags |= GLSL_LOG;
      if (strstr(env, "nopvert"))
         flags |= GLSL_NOP_VERT;
      if (strstr(env, "nopfrag"))
         flags |= GLSL_NOP_FRAG;
      if (strstr(env, "nopt"))
         flags |= GLSL_NO_OPT;
      else if (strstr(env, "opt"))
         flags |= GLSL_OPT;
      if (strstr(env, "uniform"))
         flags |= GLSL_UNIFORMS;
      if (strstr(env, "useprog"))
         flags |= GLSL_USE_PROG;
   }

   return flags;
}


/**
 * Initialize context's shader state.
 */
void
_mesa_init_shader_state(struct gl_context *ctx)
{
   /* Device drivers may override these to control what kind of instructions
    * are generated by the GLSL compiler.
    */
   struct gl_shader_compiler_options options;
   gl_shader_type sh;

   memset(&options, 0, sizeof(options));
   options.MaxUnrollIterations = 32;

   /* Default pragma settings */
   options.DefaultPragmas.Optimize = GL_TRUE;

   for (sh = 0; sh < MESA_SHADER_TYPES; ++sh)
      memcpy(&ctx->ShaderCompilerOptions[sh], &options, sizeof(options));

   ctx->Shader.Flags = get_shader_flags();
}


/**
 * Free the per-context shader-related state.
 */
void
_mesa_free_shader_state(struct gl_context *ctx)
{
   _mesa_reference_shader_program(ctx, &ctx->Shader.CurrentVertexProgram, NULL);
   _mesa_reference_shader_program(ctx, &ctx->Shader.CurrentFragmentProgram,
				  NULL);
   _mesa_reference_shader_program(ctx, &ctx->Shader._CurrentFragmentProgram,
				  NULL);
   _mesa_reference_shader_program(ctx, &ctx->Shader.ActiveProgram, NULL);
}


/**
 * Return the size of the given GLSL datatype, in floats (components).
 */
GLint
_mesa_sizeof_glsl_type(GLenum type)
{
   switch (type) {
   case GL_FLOAT:
   case GL_INT:
   case GL_UNSIGNED_INT:
   case GL_BOOL:
   case GL_SAMPLER_1D:
   case GL_SAMPLER_2D:
   case GL_SAMPLER_3D:
   case GL_SAMPLER_CUBE:
   case GL_SAMPLER_1D_SHADOW:
   case GL_SAMPLER_2D_SHADOW:
   case GL_SAMPLER_2D_RECT_ARB:
   case GL_SAMPLER_2D_RECT_SHADOW_ARB:
   case GL_SAMPLER_1D_ARRAY_EXT:
   case GL_SAMPLER_2D_ARRAY_EXT:
   case GL_SAMPLER_1D_ARRAY_SHADOW_EXT:
   case GL_SAMPLER_2D_ARRAY_SHADOW_EXT:
   case GL_SAMPLER_CUBE_SHADOW_EXT:
   case GL_SAMPLER_EXTERNAL_OES:
      return 1;
   case GL_FLOAT_VEC2:
   case GL_INT_VEC2:
   case GL_UNSIGNED_INT_VEC2:
   case GL_BOOL_VEC2:
      return 2;
   case GL_FLOAT_VEC3:
   case GL_INT_VEC3:
   case GL_UNSIGNED_INT_VEC3:
   case GL_BOOL_VEC3:
      return 3;
   case GL_FLOAT_VEC4:
   case GL_INT_VEC4:
   case GL_UNSIGNED_INT_VEC4:
   case GL_BOOL_VEC4:
      return 4;
   case GL_FLOAT_MAT2:
   case GL_FLOAT_MAT2x3:
   case GL_FLOAT_MAT2x4:
      return 8; /* two float[4] vectors */
   case GL_FLOAT_MAT3:
   case GL_FLOAT_MAT3x2:
   case GL_FLOAT_MAT3x4:
      return 12; /* three float[4] vectors */
   case GL_FLOAT_MAT4:
   case GL_FLOAT_MAT4x2:
   case GL_FLOAT_MAT4x3:
      return 16;  /* four float[4] vectors */
   default:
      _mesa_problem(NULL, "Invalid type in _mesa_sizeof_glsl_type()");
      return 1;
   }
}


/**
 * Copy string from <src> to <dst>, up to maxLength characters, returning
 * length of <dst> in <length>.
 * \param src  the strings source
 * \param maxLength  max chars to copy
 * \param length  returns number of chars copied
 * \param dst  the string destination
 */
void
_mesa_copy_string(GLchar *dst, GLsizei maxLength,
                  GLsizei *length, const GLchar *src)
{
   GLsizei len;
   for (len = 0; len < maxLength - 1 && src && src[len]; len++)
      dst[len] = src[len];
   if (maxLength > 0)
      dst[len] = 0;
   if (length)
      *length = len;
}



/**
 * Confirm that the a shader type is valid and supported by the implementation
 *
 * \param ctx   Current GL context
 * \param type  Shader target
 *
 */
static bool
validate_shader_target(const struct gl_context *ctx, GLenum type)
{
   switch (type) {
#if FEATURE_ARB_fragment_shader
   case GL_FRAGMENT_SHADER:
      return ctx->Extensions.ARB_fragment_shader;
#endif
#if FEATURE_ARB_vertex_shader
   case GL_VERTEX_SHADER:
      return ctx->Extensions.ARB_vertex_shader;
#endif
   default:
      return false;
   }
}

static GLboolean
is_program(struct gl_context *ctx, GLuint name)
{
   struct gl_shader_program *shProg = _mesa_lookup_shader_program(ctx, name);
   return shProg ? GL_TRUE : GL_FALSE;
}


static GLboolean
is_shader(struct gl_context *ctx, GLuint name)
{
   struct gl_shader *shader = _mesa_lookup_shader(ctx, name);
   return shader ? GL_TRUE : GL_FALSE;
}


/**
 * Attach shader to a shader program.
 */
static void
attach_shader(struct gl_context *ctx, GLuint program, GLuint shader)
{
   struct gl_shader_program *shProg;
   struct gl_shader *sh;
   GLuint i, n;

   shProg = _mesa_lookup_shader_program_err(ctx, program, "glAttachShader");
   if (!shProg)
      return;

   sh = _mesa_lookup_shader_err(ctx, shader, "glAttachShader");
   if (!sh) {
      return;
   }

   n = shProg->NumShaders;
   for (i = 0; i < n; i++) {
      if (shProg->Shaders[i] == sh) {
         /* The shader is already attched to this program.  The
          * GL_ARB_shader_objects spec says:
          *
          *     "The error INVALID_OPERATION is generated by AttachObjectARB
          *     if <obj> is already attached to <containerObj>."
          */
         _mesa_error(ctx, GL_INVALID_OPERATION, "glAttachShader");
         return;
      }
   }

   /* grow list */
   shProg->Shaders = (struct gl_shader **)
      _mesa_realloc(shProg->Shaders,
                    n * sizeof(struct gl_shader *),
                    (n + 1) * sizeof(struct gl_shader *));
   if (!shProg->Shaders) {
      _mesa_error(ctx, GL_OUT_OF_MEMORY, "glAttachShader");
      return;
   }

   /* append */
   shProg->Shaders[n] = NULL; /* since realloc() didn't zero the new space */
   _mesa_reference_shader(ctx, &shProg->Shaders[n], sh);
   shProg->NumShaders++;
}


static GLuint
create_shader(struct gl_context *ctx, GLenum type)
{
   struct gl_shader *sh;
   GLuint name;

   if (!validate_shader_target(ctx, type)) {
      _mesa_error(ctx, GL_INVALID_ENUM, "CreateShader(type)");
      return 0;
   }

   name = _mesa_HashFindFreeKeyBlock(ctx->Shared->ShaderObjects, 1);
   sh = ctx->Driver.NewShader(ctx, name, type);
   _mesa_HashInsert(ctx->Shared->ShaderObjects, name, sh);

   return name;
}


static GLuint 
create_shader_program(struct gl_context *ctx)
{
   GLuint name;
   struct gl_shader_program *shProg;

   name = _mesa_HashFindFreeKeyBlock(ctx->Shared->ShaderObjects, 1);

   shProg = ctx->Driver.NewShaderProgram(ctx, name);

   _mesa_HashInsert(ctx->Shared->ShaderObjects, name, shProg);

   assert(shProg->RefCount == 1);

   return name;
}


/**
 * Named w/ "2" to indicate OpenGL 2.x vs GL_ARB_fragment_programs's
 * DeleteProgramARB.
 */
static void
delete_shader_program(struct gl_context *ctx, GLuint name)
{
   /*
    * NOTE: deleting shaders/programs works a bit differently than
    * texture objects (and buffer objects, etc).  Shader/program
    * handles/IDs exist in the hash table until the object is really
    * deleted (refcount==0).  With texture objects, the handle/ID is
    * removed from the hash table in glDeleteTextures() while the tex
    * object itself might linger until its refcount goes to zero.
    */
   struct gl_shader_program *shProg;

   shProg = _mesa_lookup_shader_program_err(ctx, name, "glDeleteProgram");
   if (!shProg)
      return;

   if (!shProg->DeletePending) {
      shProg->DeletePending = GL_TRUE;

      /* effectively, decr shProg's refcount */
      _mesa_reference_shader_program(ctx, &shProg, NULL);
   }
}


static void
delete_shader(struct gl_context *ctx, GLuint shader)
{
   struct gl_shader *sh;

   sh = _mesa_lookup_shader_err(ctx, shader, "glDeleteShader");
   if (!sh)
      return;

   sh->DeletePending = GL_TRUE;

   /* effectively, decr sh's refcount */
   _mesa_reference_shader(ctx, &sh, NULL);
}


static void
detach_shader(struct gl_context *ctx, GLuint program, GLuint shader)
{
   struct gl_shader_program *shProg;
   GLuint n;
   GLuint i, j;

   shProg = _mesa_lookup_shader_program_err(ctx, program, "glDetachShader");
   if (!shProg)
      return;

   n = shProg->NumShaders;

   for (i = 0; i < n; i++) {
      if (shProg->Shaders[i]->Name == shader) {
         /* found it */
         struct gl_shader **newList;

         /* release */
         _mesa_reference_shader(ctx, &shProg->Shaders[i], NULL);

         /* alloc new, smaller array */
         newList = (struct gl_shader **)
            malloc((n - 1) * sizeof(struct gl_shader *));
         if (!newList) {
            _mesa_error(ctx, GL_OUT_OF_MEMORY, "glDetachShader");
            return;
         }
         for (j = 0; j < i; j++) {
            newList[j] = shProg->Shaders[j];
         }
         while (++i < n)
            newList[j++] = shProg->Shaders[i];
         free(shProg->Shaders);

         shProg->Shaders = newList;
         shProg->NumShaders = n - 1;

#ifdef DEBUG
         /* sanity check */
         {
            for (j = 0; j < shProg->NumShaders; j++) {
               assert(shProg->Shaders[j]->Type == GL_VERTEX_SHADER ||
                      shProg->Shaders[j]->Type == GL_FRAGMENT_SHADER);
               assert(shProg->Shaders[j]->RefCount > 0);
            }
         }
#endif

         return;
      }
   }

   /* not found */
   {
      GLenum err;
      if (is_shader(ctx, shader))
         err = GL_INVALID_OPERATION;
      else if (is_program(ctx, shader))
         err = GL_INVALID_OPERATION;
      else
         err = GL_INVALID_VALUE;
      _mesa_error(ctx, err, "glDetachProgram(shader)");
      return;
   }
}


/**
 * Return list of shaders attached to shader program.
 */
static void
get_attached_shaders(struct gl_context *ctx, GLuint program, GLsizei maxCount,
                     GLsizei *count, GLuint *obj)
{
   struct gl_shader_program *shProg =
      _mesa_lookup_shader_program_err(ctx, program, "glGetAttachedShaders");
   if (shProg) {
      GLuint i;
      for (i = 0; i < (GLuint) maxCount && i < shProg->NumShaders; i++) {
         obj[i] = shProg->Shaders[i]->Name;
      }
      if (count)
         *count = i;
   }
}


/**
 * glGetHandleARB() - return ID/name of currently bound shader program.
 */
static GLuint
get_handle(struct gl_context *ctx, GLenum pname)
{
   if (pname == GL_PROGRAM_OBJECT_ARB) {
      if (ctx->Shader.ActiveProgram)
         return ctx->Shader.ActiveProgram->Name;
      else
         return 0;
   }
   else {
      _mesa_error(ctx, GL_INVALID_ENUM, "glGetHandleARB");
      return 0;
   }
}


/**
 * glGetProgramiv() - get shader program state.
 * Note that this is for GLSL shader programs, not ARB vertex/fragment
 * programs (see glGetProgramivARB).
 */
static void
get_programiv(struct gl_context *ctx, GLuint program, GLenum pname, GLint *params)
{
   struct gl_shader_program *shProg
      = _mesa_lookup_shader_program(ctx, program);

   if (!shProg) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glGetProgramiv(program)");
      return;
   }

   switch (pname) {
   case GL_DELETE_STATUS:
      *params = shProg->DeletePending;
      break; 
   case GL_LINK_STATUS:
      *params = shProg->LinkStatus;
      break;
   case GL_VALIDATE_STATUS:
      *params = shProg->Validated;
      break;
   case GL_INFO_LOG_LENGTH:
      *params = shProg->InfoLog ? strlen(shProg->InfoLog) + 1 : 0;
      break;
   case GL_ATTACHED_SHADERS:
      *params = shProg->NumShaders;
      break;
   case GL_ACTIVE_ATTRIBUTES:
      *params = _mesa_count_active_attribs(shProg);
      break;
   case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
      *params = _mesa_longest_attribute_name_length(shProg);
      break;
   case GL_ACTIVE_UNIFORMS:
      *params = shProg->NumUserUniformStorage;
      break;
   case GL_ACTIVE_UNIFORM_MAX_LENGTH: {
      unsigned i;
      GLint max_len = 0;

      for (i = 0; i < shProg->NumUserUniformStorage; i++) {
	 /* Add one for the terminating NUL character.
	  */
	 const GLint len = strlen(shProg->UniformStorage[i].name) + 1;

	 if (len > max_len)
	    max_len = len;
      }

      *params = max_len;
      break;
   }
   case GL_PROGRAM_BINARY_LENGTH_OES:
      *params = 0;
      break;
   default:
      _mesa_error(ctx, GL_INVALID_ENUM, "glGetProgramiv(pname)");
      return;
   }
}


/**
 * glGetShaderiv() - get GLSL shader state
 */
static void
get_shaderiv(struct gl_context *ctx, GLuint name, GLenum pname, GLint *params)
{
   struct gl_shader *shader =
      _mesa_lookup_shader_err(ctx, name, "glGetShaderiv");

   if (!shader) {
      return;
   }

   switch (pname) {
   case GL_SHADER_TYPE:
      *params = shader->Type;
      break;
   case GL_DELETE_STATUS:
      *params = shader->DeletePending;
      break;
   case GL_COMPILE_STATUS:
      *params = shader->CompileStatus;
      break;
   case GL_INFO_LOG_LENGTH:
      *params = shader->InfoLog ? strlen(shader->InfoLog) + 1 : 0;
      break;
   case GL_SHADER_SOURCE_LENGTH:
      *params = shader->Source ? strlen((char *) shader->Source) + 1 : 0;
      break;
   default:
      _mesa_error(ctx, GL_INVALID_ENUM, "glGetShaderiv(pname)");
      return;
   }
}


static void
get_program_info_log(struct gl_context *ctx, GLuint program, GLsizei bufSize,
                     GLsizei *length, GLchar *infoLog)
{
   struct gl_shader_program *shProg
      = _mesa_lookup_shader_program(ctx, program);
   if (!shProg) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glGetProgramInfoLog(program)");
      return;
   }
   _mesa_copy_string(infoLog, bufSize, length, shProg->InfoLog);
}


static void
get_shader_info_log(struct gl_context *ctx, GLuint shader, GLsizei bufSize,
                    GLsizei *length, GLchar *infoLog)
{
   struct gl_shader *sh = _mesa_lookup_shader(ctx, shader);
   if (!sh) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glGetShaderInfoLog(shader)");
      return;
   }
   _mesa_copy_string(infoLog, bufSize, length, sh->InfoLog);
}


/**
 * Return shader source code.
 */
static void
get_shader_source(struct gl_context *ctx, GLuint shader, GLsizei maxLength,
                  GLsizei *length, GLchar *sourceOut)
{
   struct gl_shader *sh;
   sh = _mesa_lookup_shader_err(ctx, shader, "glGetShaderSource");
   if (!sh) {
      return;
   }
   _mesa_copy_string(sourceOut, maxLength, length, sh->Source);
}


/**
 * Set/replace shader source code.
 */
static void
shader_source(struct gl_context *ctx, GLuint shader, const GLchar *source)
{
   struct gl_shader *sh;

   sh = _mesa_lookup_shader_err(ctx, shader, "glShaderSource");
   if (!sh)
      return;

   /* free old shader source string and install new one */
   if (sh->Source) {
      free((void *) sh->Source);
   }
   sh->Source = source;
   sh->CompileStatus = GL_FALSE;
#ifdef DEBUG
   sh->SourceChecksum = _mesa_str_checksum(sh->Source);
#endif
}


/**
 * Compile a shader.
 */
static void
compile_shader(struct gl_context *ctx, GLuint shaderObj)
{
   struct gl_shader *sh;
   struct gl_shader_compiler_options *options;

   sh = _mesa_lookup_shader_err(ctx, shaderObj, "glCompileShader");
   if (!sh)
      return;

   options = &ctx->ShaderCompilerOptions[_mesa_shader_type_to_index(sh->Type)];

   /* set default pragma state for shader */
   sh->Pragmas = options->DefaultPragmas;

   /* this call will set the sh->CompileStatus field to indicate if
    * compilation was successful.
    */
   _mesa_glsl_compile_shader(ctx, sh);
}


/**
 * Link a program's shaders.
 */
static void
link_program(struct gl_context *ctx, GLuint program)
{
   struct gl_shader_program *shProg;

   shProg = _mesa_lookup_shader_program_err(ctx, program, "glLinkProgram");
   if (!shProg)
      return;

   FLUSH_VERTICES(ctx, _NEW_PROGRAM);

   _mesa_glsl_link_shader(ctx, shProg);

   /* debug code */
   if (0) {
      GLuint i;

      printf("Link %u shaders in program %u: %s\n",
                   shProg->NumShaders, shProg->Name,
                   shProg->LinkStatus ? "Success" : "Failed");

      for (i = 0; i < shProg->NumShaders; i++) {
         printf(" shader %u, type 0x%x\n",
                      shProg->Shaders[i]->Name,
                      shProg->Shaders[i]->Type);
      }
   }
}


/**
 * Print basic shader info (for debug).
 */
static void
print_shader_info(const struct gl_shader_program *shProg)
{
   GLuint i;

   printf("Mesa: glUseProgram(%u)\n", shProg->Name);
   for (i = 0; i < shProg->NumShaders; i++) {
      const char *s;
      switch (shProg->Shaders[i]->Type) {
      case GL_VERTEX_SHADER:
         s = "vertex";
         break;
      case GL_FRAGMENT_SHADER:
         s = "fragment";
         break;
      default:
         s = "";
      }
      printf("  %s shader %u, checksum %u\n", s, 
	     shProg->Shaders[i]->Name,
	     shProg->Shaders[i]->SourceChecksum);
   }
   if (shProg->_LinkedShaders[MESA_SHADER_VERTEX])
      printf("  vert prog %u\n",
	     shProg->_LinkedShaders[MESA_SHADER_VERTEX]->Program->Id);
   if (shProg->_LinkedShaders[MESA_SHADER_FRAGMENT])
      printf("  frag prog %u\n",
	     shProg->_LinkedShaders[MESA_SHADER_FRAGMENT]->Program->Id);
}


/**
 * Use the named shader program for subsequent glUniform calls
 */
void
_mesa_active_program(struct gl_context *ctx, struct gl_shader_program *shProg,
		     const char *caller)
{
   if ((shProg != NULL) && !shProg->LinkStatus) {
      _mesa_error(ctx, GL_INVALID_OPERATION,
		  "%s(program %u not linked)", caller, shProg->Name);
      return;
   }

   if (ctx->Shader.ActiveProgram != shProg) {
      _mesa_reference_shader_program(ctx, &ctx->Shader.ActiveProgram, shProg);
   }
}

/**
 */
static bool
use_shader_program(struct gl_context *ctx, GLenum type,
		   struct gl_shader_program *shProg)
{
   struct gl_shader_program **target;

   switch (type) {
#if FEATURE_ARB_vertex_shader
   case GL_VERTEX_SHADER:
      target = &ctx->Shader.CurrentVertexProgram;
      if ((shProg == NULL)
	  || (shProg->_LinkedShaders[MESA_SHADER_VERTEX] == NULL)) {
	 shProg = NULL;
      }
      break;
#endif
#if FEATURE_ARB_fragment_shader
   case GL_FRAGMENT_SHADER:
      target = &ctx->Shader.CurrentFragmentProgram;
      if ((shProg == NULL)
	  || (shProg->_LinkedShaders[MESA_SHADER_FRAGMENT] == NULL)) {
	 shProg = NULL;
      }
      break;
#endif
   default:
      return false;
   }

   if (*target != shProg) {
      FLUSH_VERTICES(ctx, _NEW_PROGRAM | _NEW_PROGRAM_CONSTANTS);

      /* If the shader is also bound as the current rendering shader, unbind
       * it from that binding point as well.  This ensures that the correct
       * semantics of glDeleteProgram are maintained.
       */
      switch (type) {
#if FEATURE_ARB_vertex_shader
      case GL_VERTEX_SHADER:
	 /* Empty for now. */
	 break;
#endif
#if FEATURE_ARB_fragment_shader
      case GL_FRAGMENT_SHADER:
	 if (*target == ctx->Shader._CurrentFragmentProgram) {
	    _mesa_reference_shader_program(ctx,
					   &ctx->Shader._CurrentFragmentProgram,
					   NULL);
	 }
	 break;
#endif
      }

      _mesa_reference_shader_program(ctx, target, shProg);
      return true;
   }

   return false;
}

/**
 * Use the named shader program for subsequent rendering.
 */
void
_mesa_use_program(struct gl_context *ctx, struct gl_shader_program *shProg)
{
   use_shader_program(ctx, GL_VERTEX_SHADER, shProg);
   use_shader_program(ctx, GL_FRAGMENT_SHADER, shProg);
   _mesa_active_program(ctx, shProg, "glUseProgram");

   if (ctx->Driver.UseProgram)
      ctx->Driver.UseProgram(ctx, shProg);
}

/**
 * Do validation of the given shader program.
 * \param errMsg  returns error message if validation fails.
 * \return GL_TRUE if valid, GL_FALSE if invalid (and set errMsg)
 */
static GLboolean
validate_shader_program(const struct gl_shader_program *shProg,
                        char *errMsg)
{
   if (!shProg->LinkStatus) {
      return GL_FALSE;
   }

   /* From the GL spec, a program is invalid if any of these are true:

     any two active samplers in the current program object are of
     different types, but refer to the same texture image unit,

     any active sampler in the current program object refers to a texture
     image unit where fixed-function fragment processing accesses a
     texture target that does not match the sampler type, or 

     the sum of the number of active samplers in the program and the
     number of texture image units enabled for fixed-function fragment
     processing exceeds the combined limit on the total number of texture
     image units allowed.
   */


   /*
    * Check: any two active samplers in the current program object are of
    * different types, but refer to the same texture image unit,
    */
   if (!_mesa_sampler_uniforms_are_valid(shProg, errMsg, 100))
      return GL_FALSE;

   return GL_TRUE;
}


/**
 * Called via glValidateProgram()
 */
static void
validate_program(struct gl_context *ctx, GLuint program)
{
   struct gl_shader_program *shProg;
   char errMsg[100] = "";

   shProg = _mesa_lookup_shader_program_err(ctx, program, "glValidateProgram");
   if (!shProg) {
      return;
   }

   shProg->Validated = validate_shader_program(shProg, errMsg);
   if (!shProg->Validated) {
      /* update info log */
      if (shProg->InfoLog) {
         ralloc_free(shProg->InfoLog);
      }
      shProg->InfoLog = ralloc_strdup(shProg, errMsg);
   }
}



void GLAPIENTRY
_mesa_AttachObjectARB(GLhandleARB program, GLhandleARB shader)
{
   GET_CURRENT_CONTEXT(ctx);
   attach_shader(ctx, program, shader);
}


void GLAPIENTRY
_mesa_AttachShader(GLuint program, GLuint shader)
{
   GET_CURRENT_CONTEXT(ctx);
   attach_shader(ctx, program, shader);
}


void GLAPIENTRY
_mesa_CompileShaderARB(GLhandleARB shaderObj)
{
   GET_CURRENT_CONTEXT(ctx);
   if (MESA_VERBOSE & VERBOSE_API)
      _mesa_debug(ctx, "glCompileShader %u\n", shaderObj);
   compile_shader(ctx, shaderObj);
}


GLuint GLAPIENTRY
_mesa_CreateShader(GLenum type)
{
   GET_CURRENT_CONTEXT(ctx);
   if (MESA_VERBOSE & VERBOSE_API)
      _mesa_debug(ctx, "glCreateShader %s\n", _mesa_lookup_enum_by_nr(type));
   return create_shader(ctx, type);
}


GLhandleARB GLAPIENTRY
_mesa_CreateShaderObjectARB(GLenum type)
{
   GET_CURRENT_CONTEXT(ctx);
   return create_shader(ctx, type);
}


GLuint GLAPIENTRY
_mesa_CreateProgram(void)
{
   GET_CURRENT_CONTEXT(ctx);
   if (MESA_VERBOSE & VERBOSE_API)
      _mesa_debug(ctx, "glCreateProgram\n");
   return create_shader_program(ctx);
}


GLhandleARB GLAPIENTRY
_mesa_CreateProgramObjectARB(void)
{
   GET_CURRENT_CONTEXT(ctx);
   return create_shader_program(ctx);
}


void GLAPIENTRY
_mesa_DeleteObjectARB(GLhandleARB obj)
{
   if (MESA_VERBOSE & VERBOSE_API) {
      GET_CURRENT_CONTEXT(ctx);
      _mesa_debug(ctx, "glDeleteObjectARB(%u)\n", obj);
   }

   if (obj) {
      GET_CURRENT_CONTEXT(ctx);
      FLUSH_VERTICES(ctx, 0);
      if (is_program(ctx, obj)) {
         delete_shader_program(ctx, obj);
      }
      else if (is_shader(ctx, obj)) {
         delete_shader(ctx, obj);
      }
      else {
         /* error? */
      }
   }
}


void GLAPIENTRY
_mesa_DeleteProgram(GLuint name)
{
   if (name) {
      GET_CURRENT_CONTEXT(ctx);
      FLUSH_VERTICES(ctx, 0);
      delete_shader_program(ctx, name);
   }
}


void GLAPIENTRY
_mesa_DeleteShader(GLuint name)
{
   if (name) {
      GET_CURRENT_CONTEXT(ctx);
      FLUSH_VERTICES(ctx, 0);
      delete_shader(ctx, name);
   }
}


void GLAPIENTRY
_mesa_DetachObjectARB(GLhandleARB program, GLhandleARB shader)
{
   GET_CURRENT_CONTEXT(ctx);
   detach_shader(ctx, program, shader);
}


void GLAPIENTRY
_mesa_DetachShader(GLuint program, GLuint shader)
{
   GET_CURRENT_CONTEXT(ctx);
   detach_shader(ctx, program, shader);
}


void GLAPIENTRY
_mesa_GetAttachedObjectsARB(GLhandleARB container, GLsizei maxCount,
                            GLsizei * count, GLhandleARB * obj)
{
   GET_CURRENT_CONTEXT(ctx);
   get_attached_shaders(ctx, container, maxCount, count, obj);
}


void GLAPIENTRY
_mesa_GetAttachedShaders(GLuint program, GLsizei maxCount,
                         GLsizei *count, GLuint *obj)
{
   GET_CURRENT_CONTEXT(ctx);
   get_attached_shaders(ctx, program, maxCount, count, obj);
}


void GLAPIENTRY
_mesa_GetInfoLogARB(GLhandleARB object, GLsizei maxLength, GLsizei * length,
                    GLcharARB * infoLog)
{
   GET_CURRENT_CONTEXT(ctx);
   if (is_program(ctx, object)) {
      get_program_info_log(ctx, object, maxLength, length, infoLog);
   }
   else if (is_shader(ctx, object)) {
      get_shader_info_log(ctx, object, maxLength, length, infoLog);
   }
   else {
      _mesa_error(ctx, GL_INVALID_OPERATION, "glGetInfoLogARB");
   }
}


void GLAPIENTRY
_mesa_GetObjectParameterivARB(GLhandleARB object, GLenum pname, GLint *params)
{
   GET_CURRENT_CONTEXT(ctx);
   /* Implement in terms of GetProgramiv, GetShaderiv */
   if (is_program(ctx, object)) {
      if (pname == GL_OBJECT_TYPE_ARB) {
	 *params = GL_PROGRAM_OBJECT_ARB;
      }
      else {
	 get_programiv(ctx, object, pname, params);
      }
   }
   else if (is_shader(ctx, object)) {
      if (pname == GL_OBJECT_TYPE_ARB) {
	 *params = GL_SHADER_OBJECT_ARB;
      }
      else {
	 get_shaderiv(ctx, object, pname, params);
      }
   }
   else {
      _mesa_error(ctx, GL_INVALID_VALUE, "glGetObjectParameterivARB");
   }
}


void GLAPIENTRY
_mesa_GetObjectParameterfvARB(GLhandleARB object, GLenum pname,
                              GLfloat *params)
{
   GLint iparams[1];  /* XXX is one element enough? */
   _mesa_GetObjectParameterivARB(object, pname, iparams);
   params[0] = (GLfloat) iparams[0];
}


void GLAPIENTRY
_mesa_GetProgramiv(GLuint program, GLenum pname, GLint *params)
{
   GET_CURRENT_CONTEXT(ctx);
   get_programiv(ctx, program, pname, params);
}


void GLAPIENTRY
_mesa_GetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
   GET_CURRENT_CONTEXT(ctx);
   get_shaderiv(ctx, shader, pname, params);
}


void GLAPIENTRY
_mesa_GetProgramInfoLog(GLuint program, GLsizei bufSize,
                        GLsizei *length, GLchar *infoLog)
{
   GET_CURRENT_CONTEXT(ctx);
   get_program_info_log(ctx, program, bufSize, length, infoLog);
}


void GLAPIENTRY
_mesa_GetShaderInfoLog(GLuint shader, GLsizei bufSize,
                       GLsizei *length, GLchar *infoLog)
{
   GET_CURRENT_CONTEXT(ctx);
   get_shader_info_log(ctx, shader, bufSize, length, infoLog);
}


void GLAPIENTRY
_mesa_GetShaderSourceARB(GLhandleARB shader, GLsizei maxLength,
                         GLsizei *length, GLcharARB *sourceOut)
{
   GET_CURRENT_CONTEXT(ctx);
   get_shader_source(ctx, shader, maxLength, length, sourceOut);
}


GLhandleARB GLAPIENTRY
_mesa_GetHandleARB(GLenum pname)
{
   GET_CURRENT_CONTEXT(ctx);
   return get_handle(ctx, pname);
}


GLboolean GLAPIENTRY
_mesa_IsProgram(GLuint name)
{
   GET_CURRENT_CONTEXT(ctx);
   return is_program(ctx, name);
}


GLboolean GLAPIENTRY
_mesa_IsShader(GLuint name)
{
   GET_CURRENT_CONTEXT(ctx);
   return is_shader(ctx, name);
}


void GLAPIENTRY
_mesa_LinkProgramARB(GLhandleARB programObj)
{
   GET_CURRENT_CONTEXT(ctx);
   link_program(ctx, programObj);
}



/**
 * Read shader source code from a file.
 * Useful for debugging to override an app's shader.
 */
static GLcharARB *
read_shader(const char *fname)
{
   const int max = 50*1000;
   FILE *f = fopen(fname, "r");
   GLcharARB *buffer, *shader;
   int len;

   if (!f) {
      return NULL;
   }

   buffer = (char *) malloc(max);
   len = fread(buffer, 1, max, f);
   buffer[len] = 0;

   fclose(f);

   shader = _mesa_strdup(buffer);
   free(buffer);

   return shader;
}


/**
 * Called via glShaderSource() and glShaderSourceARB() API functions.
 * Basically, concatenate the source code strings into one long string
 * and pass it to _mesa_shader_source().
 */
void GLAPIENTRY
_mesa_ShaderSourceARB(GLhandleARB shaderObj, GLsizei count,
                      const GLcharARB ** string, const GLint * length)
{
   GET_CURRENT_CONTEXT(ctx);
   GLint *offsets;
   GLsizei i, totalLength;
   GLcharARB *source;
   GLuint checksum;

   if (!shaderObj || string == NULL) {
      _mesa_error(ctx, GL_INVALID_VALUE, "glShaderSourceARB");
      return;
   }

   /*
    * This array holds offsets of where the appropriate string ends, thus the
    * last element will be set to the total length of the source code.
    */
   offsets = (GLint *) malloc(count * sizeof(GLint));
   if (offsets == NULL) {
      _mesa_error(ctx, GL_OUT_OF_MEMORY, "glShaderSourceARB");
      return;
   }

   for (i = 0; i < count; i++) {
      if (string[i] == NULL) {
         free((GLvoid *) offsets);
         _mesa_error(ctx, GL_INVALID_OPERATION,
                     "glShaderSourceARB(null string)");
         return;
      }
      if (length == NULL || length[i] < 0)
         offsets[i] = strlen(string[i]);
      else
         offsets[i] = length[i];
      /* accumulate string lengths */
      if (i > 0)
         offsets[i] += offsets[i - 1];
   }

   /* Total length of source string is sum off all strings plus two.
    * One extra byte for terminating zero, another extra byte to silence
    * valgrind warnings in the parser/grammer code.
    */
   totalLength = offsets[count - 1] + 2;
   source = (GLcharARB *) malloc(totalLength * sizeof(GLcharARB));
   if (source == NULL) {
      free((GLvoid *) offsets);
      _mesa_error(ctx, GL_OUT_OF_MEMORY, "glShaderSourceARB");
      return;
   }

   for (i = 0; i < count; i++) {
      GLint start = (i > 0) ? offsets[i - 1] : 0;
      memcpy(source + start, string[i],
             (offsets[i] - start) * sizeof(GLcharARB));
   }
   source[totalLength - 1] = '\0';
   source[totalLength - 2] = '\0';

   if (SHADER_SUBST) {
      /* Compute the shader's source code checksum then try to open a file
       * named newshader_<CHECKSUM>.  If it exists, use it in place of the
       * original shader source code.  For debugging.
       */
      char filename[100];
      GLcharARB *newSource;

      checksum = _mesa_str_checksum(source);

      _mesa_snprintf(filename, sizeof(filename), "newshader_%d", checksum);

      newSource = read_shader(filename);
      if (newSource) {
         fprintf(stderr, "Mesa: Replacing shader %u chksum=%d with %s\n",
                       shaderObj, checksum, filename);
         free(source);
         source = newSource;
      }
   }

   shader_source(ctx, shaderObj, source);

   if (SHADER_SUBST) {
      struct gl_shader *sh = _mesa_lookup_shader(ctx, shaderObj);
      if (sh)
         sh->SourceChecksum = checksum; /* save original checksum */
   }

   free(offsets);
}


void GLAPIENTRY
_mesa_UseProgramObjectARB(GLhandleARB program)
{
   GET_CURRENT_CONTEXT(ctx);
   struct gl_shader_program *shProg;

   ASSERT_OUTSIDE_BEGIN_END(ctx);

   if (program) {
      shProg = _mesa_lookup_shader_program_err(ctx, program, "glUseProgram");
      if (!shProg) {
         return;
      }
      if (!shProg->LinkStatus) {
         _mesa_error(ctx, GL_INVALID_OPERATION,
                     "glUseProgram(program %u not linked)", program);
         return;
      }

      /* debug code */
      if (ctx->Shader.Flags & GLSL_USE_PROG) {
         print_shader_info(shProg);
      }
   }
   else {
      shProg = NULL;
   }

   _mesa_use_program(ctx, shProg);
}


void GLAPIENTRY
_mesa_ValidateProgramARB(GLhandleARB program)
{
   GET_CURRENT_CONTEXT(ctx);
   validate_program(ctx, program);
}


void
_mesa_use_shader_program(struct gl_context *ctx, GLenum type,
			 struct gl_shader_program *shProg)
{
   use_shader_program(ctx, type, shProg);

   if (ctx->Driver.UseProgram)
      ctx->Driver.UseProgram(ctx, shProg);
}

void GLAPIENTRY
_mesa_UseShaderProgramEXT(GLenum type, GLuint program)
{
   GET_CURRENT_CONTEXT(ctx);
   struct gl_shader_program *shProg = NULL;

   ASSERT_OUTSIDE_BEGIN_END(ctx);

   if (!validate_shader_target(ctx, type)) {
      _mesa_error(ctx, GL_INVALID_ENUM, "glUseShaderProgramEXT(type)");
      return;
   }

   if (program) {
      shProg = _mesa_lookup_shader_program_err(ctx, program,
					       "glUseShaderProgramEXT");
      if (shProg == NULL)
	 return;

      if (!shProg->LinkStatus) {
	 _mesa_error(ctx, GL_INVALID_OPERATION,
		     "glUseShaderProgramEXT(program not linked)");
	 return;
      }
   }

   _mesa_use_shader_program(ctx, type, shProg);
}

void GLAPIENTRY
_mesa_ActiveProgramEXT(GLuint program)
{
   GET_CURRENT_CONTEXT(ctx);
   struct gl_shader_program *shProg = (program != 0)
      ? _mesa_lookup_shader_program_err(ctx, program, "glActiveProgramEXT")
      : NULL;

   _mesa_active_program(ctx, shProg, "glActiveProgramEXT");
   return;
}

GLuint GLAPIENTRY
_mesa_CreateShaderProgramEXT(GLenum type, const GLchar *string)
{
   GET_CURRENT_CONTEXT(ctx);
   const GLuint shader = create_shader(ctx, type);
   GLuint program = 0;

   if (shader) {
      shader_source(ctx, shader, _mesa_strdup(string));
      compile_shader(ctx, shader);

      program = create_shader_program(ctx);
      if (program) {
	 struct gl_shader_program *shProg;
	 struct gl_shader *sh;
	 GLint compiled = GL_FALSE;

	 shProg = _mesa_lookup_shader_program(ctx, program);
	 sh = _mesa_lookup_shader(ctx, shader);

	 get_shaderiv(ctx, shader, GL_COMPILE_STATUS, &compiled);
	 if (compiled) {
	    attach_shader(ctx, program, shader);
	    link_program(ctx, program);
	    detach_shader(ctx, program, shader);

#if 0
	    /* Possibly... */
	    if (active-user-defined-varyings-in-linked-program) {
	       append-error-to-info-log;
	       shProg->LinkStatus = GL_FALSE;
	    }
#endif
	 }

	 ralloc_strcat(&shProg->InfoLog, sh->InfoLog);
      }

      delete_shader(ctx, shader);
   }

   return program;
}

/**
 * Plug in shader-related functions into API dispatch table.
 */
void
_mesa_init_shader_dispatch(struct _glapi_table *exec)
{
#if FEATURE_GL
   /* GL_ARB_vertex/fragment_shader */
   SET_DeleteObjectARB(exec, _mesa_DeleteObjectARB);
   SET_GetHandleARB(exec, _mesa_GetHandleARB);
   SET_DetachObjectARB(exec, _mesa_DetachObjectARB);
   SET_CreateShaderObjectARB(exec, _mesa_CreateShaderObjectARB);
   SET_ShaderSourceARB(exec, _mesa_ShaderSourceARB);
   SET_CompileShaderARB(exec, _mesa_CompileShaderARB);
   SET_CreateProgramObjectARB(exec, _mesa_CreateProgramObjectARB);
   SET_AttachObjectARB(exec, _mesa_AttachObjectARB);
   SET_LinkProgramARB(exec, _mesa_LinkProgramARB);
   SET_UseProgramObjectARB(exec, _mesa_UseProgramObjectARB);
   SET_ValidateProgramARB(exec, _mesa_ValidateProgramARB);
   SET_GetObjectParameterfvARB(exec, _mesa_GetObjectParameterfvARB);
   SET_GetObjectParameterivARB(exec, _mesa_GetObjectParameterivARB);
   SET_GetInfoLogARB(exec, _mesa_GetInfoLogARB);
   SET_GetAttachedObjectsARB(exec, _mesa_GetAttachedObjectsARB);
   SET_GetShaderSourceARB(exec, _mesa_GetShaderSourceARB);

   /* OpenGL 2.0 */
   SET_AttachShader(exec, _mesa_AttachShader);
   SET_CreateProgram(exec, _mesa_CreateProgram);
   SET_CreateShader(exec, _mesa_CreateShader);
   SET_DeleteProgram(exec, _mesa_DeleteProgram);
   SET_DeleteShader(exec, _mesa_DeleteShader);
   SET_DetachShader(exec, _mesa_DetachShader);
   SET_GetAttachedShaders(exec, _mesa_GetAttachedShaders);
   SET_GetProgramiv(exec, _mesa_GetProgramiv);
   SET_GetProgramInfoLog(exec, _mesa_GetProgramInfoLog);
   SET_GetShaderiv(exec, _mesa_GetShaderiv);
   SET_GetShaderInfoLog(exec, _mesa_GetShaderInfoLog);
   SET_IsProgram(exec, _mesa_IsProgram);
   SET_IsShader(exec, _mesa_IsShader);

#if FEATURE_ARB_vertex_shader
   SET_BindAttribLocationARB(exec, _mesa_BindAttribLocationARB);
   SET_GetActiveAttribARB(exec, _mesa_GetActiveAttribARB);
   SET_GetAttribLocationARB(exec, _mesa_GetAttribLocationARB);
#endif

   SET_UseShaderProgramEXT(exec, _mesa_UseShaderProgramEXT);
   SET_ActiveProgramEXT(exec, _mesa_ActiveProgramEXT);
   SET_CreateShaderProgramEXT(exec, _mesa_CreateShaderProgramEXT);

   /* GL_EXT_gpu_shader4 / GL 3.0 */
   SET_BindFragDataLocationEXT(exec, _mesa_BindFragDataLocation);
   SET_GetFragDataLocationEXT(exec, _mesa_GetFragDataLocation);

#endif /* FEATURE_GL */
}

