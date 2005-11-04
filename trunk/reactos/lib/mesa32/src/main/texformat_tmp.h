/* $XFree86$ */
/*
 * Mesa 3-D graphics library
 * Version:  6.2
 *
 * Copyright (C) 1999-2004  Brian Paul   All Rights Reserved.
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
 * \file texformat_tmp.h
 * Texel fetch functions template.
 * 
 * This template file is used by texformat.c to generate texel fetch functions
 * for 1-D, 2-D and 3-D texture images. 
 *
 * It should be expanded by defining \p DIM as the number texture dimensions
 * (1, 2 or 3).  According to the value of \p DIM a series of macros is defined
 * for the texel lookup in the gl_texture_image::Data.
 * 
 * \sa texformat.c and FetchTexel.
 * 
 * \author Gareth Hughes
 * \author Brian Paul
 */


#if DIM == 1

#define CHAN_SRC( t, i, j, k, sz )					\
	((GLchan *)(t)->Data + (i) * (sz))
#define UBYTE_SRC( t, i, j, k, sz )					\
	((GLubyte *)(t)->Data + (i) * (sz))
#define USHORT_SRC( t, i, j, k )					\
	((GLushort *)(t)->Data + (i))
#define UINT_SRC( t, i, j, k )						\
	((GLuint *)(t)->Data + (i))
#define FLOAT_SRC( t, i, j, k, sz )					\
	((GLfloat *)(t)->Data + (i) * (sz))
#define HALF_SRC( t, i, j, k, sz )					\
	((GLhalfARB *)(t)->Data + (i) * (sz))

#define FETCH(x) fetch_texel_1d_##x

#elif DIM == 2

#define CHAN_SRC( t, i, j, k, sz )					\
	((GLchan *)(t)->Data + ((t)->RowStride * (j) + (i)) * (sz))
#define UBYTE_SRC( t, i, j, k, sz )					\
	((GLubyte *)(t)->Data + ((t)->RowStride * (j) + (i)) * (sz))
#define USHORT_SRC( t, i, j, k )					\
	((GLushort *)(t)->Data + ((t)->RowStride * (j) + (i)))
#define UINT_SRC( t, i, j, k )						\
	((GLuint *)(t)->Data + ((t)->RowStride * (j) + (i)))
#define FLOAT_SRC( t, i, j, k, sz )					\
	((GLfloat *)(t)->Data + ((t)->RowStride * (j) + (i)) * (sz))
#define HALF_SRC( t, i, j, k, sz )					\
	((GLhalfARB *)(t)->Data + ((t)->RowStride * (j) + (i)) * (sz))

#define FETCH(x) fetch_texel_2d_##x

#elif DIM == 3

#define CHAN_SRC( t, i, j, k, sz )					\
	(GLchan *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				(t)->RowStride + (i)) * (sz)
#define UBYTE_SRC( t, i, j, k, sz )					\
	((GLubyte *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				 (t)->RowStride + (i)) * (sz))
#define USHORT_SRC( t, i, j, k )					\
	((GLushort *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				  (t)->RowStride + (i)))
#define UINT_SRC( t, i, j, k )						\
	((GLuint *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				  (t)->RowStride + (i)))
#define FLOAT_SRC( t, i, j, k, sz )					\
	((GLfloat *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				  (t)->RowStride + (i)) * (sz))
#define HALF_SRC( t, i, j, k, sz )					\
	((GLhalfARB *)(t)->Data + (((t)->Height * (k) + (j)) *		\
				  (t)->RowStride + (i)) * (sz))

#define FETCH(x) fetch_texel_3d_##x

#else
#error	illegal number of texture dimensions
#endif


/* Fetch texel from 1D, 2D or 3D RGBA texture, returning 4 GLchans */
static void FETCH(rgba)( const struct gl_texture_image *texImage,
			 GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 4 );
   COPY_CHAN4( texel, src );
}

/* Fetch texel from 1D, 2D or 3D RGBA texture, returning 4 GLfloats */
static void FETCH(f_rgba)( const struct gl_texture_image *texImage,
                           GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 4 );
   texel[RCOMP] = CHAN_TO_FLOAT(src[0]);
   texel[GCOMP] = CHAN_TO_FLOAT(src[1]);
   texel[BCOMP] = CHAN_TO_FLOAT(src[2]);
   texel[ACOMP] = CHAN_TO_FLOAT(src[3]);
}


/* Fetch texel from 1D, 2D or 3D RGB texture, returning 4 GLchans */
static void FETCH(rgb)( const struct gl_texture_image *texImage,
			GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 3 );
   texel[RCOMP] = src[0];
   texel[GCOMP] = src[1];
   texel[BCOMP] = src[2];
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D RGB texture, returning 4 GLfloats */
static void FETCH(f_rgb)( const struct gl_texture_image *texImage,
                          GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 3 );
   texel[RCOMP] = CHAN_TO_FLOAT(src[0]);
   texel[GCOMP] = CHAN_TO_FLOAT(src[1]);
   texel[BCOMP] = CHAN_TO_FLOAT(src[2]);
   texel[ACOMP] = CHAN_MAXF;
}

/* Fetch texel from 1D, 2D or 3D ALPHA texture, returning 4 GLchans */
static void FETCH(alpha)( const struct gl_texture_image *texImage,
			  GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = 0;
   texel[ACOMP] = src[0];
}

/* Fetch texel from 1D, 2D or 3D ALPHA texture, returning 4 GLfloats */
static void FETCH(f_alpha)( const struct gl_texture_image *texImage,
                            GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = 0.0;
   texel[ACOMP] = CHAN_TO_FLOAT(src[0]);
}

/* Fetch texel from 1D, 2D or 3D LUMIN texture, returning 4 GLchans */
static void FETCH(luminance)( const struct gl_texture_image *texImage,
			      GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = src[0];
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D LUMIN texture, returning 4 GLchans */
static void FETCH(f_luminance)( const struct gl_texture_image *texImage,
                                GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = CHAN_TO_FLOAT(src[0]);
   texel[ACOMP] = CHAN_MAXF;
}

/* Fetch texel from 1D, 2D or 3D L_A texture, returning 4 GLchans */
static void FETCH(luminance_alpha)( const struct gl_texture_image *texImage,
				    GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 2 );
   texel[RCOMP] = src[0];
   texel[GCOMP] = src[0];
   texel[BCOMP] = src[0];
   texel[ACOMP] = src[1];
}

/* Fetch texel from 1D, 2D or 3D L_A texture, returning 4 GLfloats */
static void FETCH(f_luminance_alpha)( const struct gl_texture_image *texImage,
                                  GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 2 );
   texel[RCOMP] = 
   texel[GCOMP] = 
   texel[BCOMP] = CHAN_TO_FLOAT(src[0]);
   texel[ACOMP] = CHAN_TO_FLOAT(src[1]);
}


/* Fetch texel from 1D, 2D or 3D INT. texture, returning 4 GLchans */
static void FETCH(intensity)( const struct gl_texture_image *texImage,
			      GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] = src[0];
   texel[GCOMP] = src[0];
   texel[BCOMP] = src[0];
   texel[ACOMP] = src[0];
}

/* Fetch texel from 1D, 2D or 3D INT. texture, returning 4 GLfloats */
static void FETCH(f_intensity)( const struct gl_texture_image *texImage,
                                GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLchan *src = CHAN_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] = 
   texel[GCOMP] = 
   texel[BCOMP] = 
   texel[ACOMP] = CHAN_TO_FLOAT(src[0]);
}


/* Fetch depth texel from 1D, 2D or 3D float32 DEPTH texture,
 * returning 1 GLfloat.
 * Note: no GLchan version of this function.
 */
static void FETCH(f_depth_component_f32)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 1 );
   texel[0] = src[0];
}


/* Fetch depth texel from 1D, 2D or 3D float32 DEPTH texture,
 * returning 1 GLfloat.
 * Note: no GLchan version of this function.
 */
static void FETCH(f_depth_component16)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   texel[0] = src[0] * (1.0F / 65535.0F);
}


/* Fetch texel from 1D, 2D or 3D RGBA_FLOAT32 texture,
 * returning 4 GLchans.
 */
static void FETCH(rgba_f32)( const struct gl_texture_image *texImage,
                               GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 4 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], src[0]);
   UNCLAMPED_FLOAT_TO_CHAN(texel[GCOMP], src[1]);
   UNCLAMPED_FLOAT_TO_CHAN(texel[BCOMP], src[2]);
   UNCLAMPED_FLOAT_TO_CHAN(texel[ACOMP], src[3]);
}

/* Fetch texel from 1D, 2D or 3D RGBA_FLOAT32 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_rgba_f32)( const struct gl_texture_image *texImage,
                               GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 4 );
   texel[RCOMP] = src[0];
   texel[GCOMP] = src[1];
   texel[BCOMP] = src[2];
   texel[ACOMP] = src[3];
}

/* Fetch texel from 1D, 2D or 3D RGBA_FLOAT16 texture,
 * returning 4 GLchans.
 */
static void FETCH(rgba_f16)( const struct gl_texture_image *texImage,
                               GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 4 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], _mesa_half_to_float(src[0]));
   UNCLAMPED_FLOAT_TO_CHAN(texel[GCOMP], _mesa_half_to_float(src[1]));
   UNCLAMPED_FLOAT_TO_CHAN(texel[BCOMP], _mesa_half_to_float(src[2]));
   UNCLAMPED_FLOAT_TO_CHAN(texel[ACOMP], _mesa_half_to_float(src[3]));
}

/* Fetch texel from 1D, 2D or 3D RGBA_FLOAT16 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_rgba_f16)( const struct gl_texture_image *texImage,
                               GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 4 );
   texel[RCOMP] = _mesa_half_to_float(src[0]);
   texel[GCOMP] = _mesa_half_to_float(src[1]);
   texel[BCOMP] = _mesa_half_to_float(src[2]);
   texel[ACOMP] = _mesa_half_to_float(src[3]);
}

/* Fetch texel from 1D, 2D or 3D RGB_FLOAT32 texture,
 * returning 4 GLchans.
 */
static void FETCH(rgb_f32)( const struct gl_texture_image *texImage,
                            GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 3 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], src[0]);
   UNCLAMPED_FLOAT_TO_CHAN(texel[GCOMP], src[1]);
   UNCLAMPED_FLOAT_TO_CHAN(texel[BCOMP], src[2]);
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D RGB_FLOAT32 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_rgb_f32)( const struct gl_texture_image *texImage,
                              GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 3 );
   texel[RCOMP] = src[0];
   texel[GCOMP] = src[1];
   texel[BCOMP] = src[2];
   texel[ACOMP] = CHAN_MAXF;
}

/* Fetch texel from 1D, 2D or 3D RGBA_FLOAT16 texture,
 * returning 4 GLchans.
 */
static void FETCH(rgb_f16)( const struct gl_texture_image *texImage,
                            GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 3 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], _mesa_half_to_float(src[0]));
   UNCLAMPED_FLOAT_TO_CHAN(texel[GCOMP], _mesa_half_to_float(src[1]));
   UNCLAMPED_FLOAT_TO_CHAN(texel[BCOMP], _mesa_half_to_float(src[2]));
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D RGB_FLOAT16 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_rgb_f16)( const struct gl_texture_image *texImage,
                              GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 3 );
   texel[RCOMP] = _mesa_half_to_float(src[0]);
   texel[GCOMP] = _mesa_half_to_float(src[1]);
   texel[BCOMP] = _mesa_half_to_float(src[2]);
   texel[ACOMP] = CHAN_MAXF;
}

/* Fetch texel from 1D, 2D or 3D ALPHA_FLOAT32 texture,
 * returning 4 GLchans.
 */
static void FETCH(alpha_f32)( const struct gl_texture_image *texImage,
                              GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = 0;
   UNCLAMPED_FLOAT_TO_CHAN(texel[ACOMP], src[0]);
}

/* Fetch texel from 1D, 2D or 3D ALPHA_FLOAT32 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_alpha_f32)( const struct gl_texture_image *texImage,
                              GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = 0.0F;
   texel[ACOMP] = src[0];
}

/* Fetch texel from 1D, 2D or 3D ALPHA_FLOAT16 texture,
 * returning 4 GLchans.
 */
static void FETCH(alpha_f16)( const struct gl_texture_image *texImage,
                              GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = 0;
   UNCLAMPED_FLOAT_TO_CHAN(texel[ACOMP], _mesa_half_to_float(src[0]));
}

/* Fetch texel from 1D, 2D or 3D ALPHA_FLOAT16 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_alpha_f16)( const struct gl_texture_image *texImage,
                              GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = 0.0F;
   texel[ACOMP] = _mesa_half_to_float(src[0]);
}

/* Fetch texel from 1D, 2D or 3D LUMINANCE_FLOAT32 texture,
 * returning 4 GLchans.
 */
static void FETCH(luminance_f32)( const struct gl_texture_image *texImage,
                                  GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 1 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], src[0]);
   texel[GCOMP] =
   texel[BCOMP] = texel[RCOMP];
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D LUMINANCE_FLOAT32 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_luminance_f32)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = src[0];
   texel[ACOMP] = CHAN_MAXF;
}

/* Fetch texel from 1D, 2D or 3D LUMINANCE_FLOAT16 texture,
 * returning 4 GLchans.
 */
static void FETCH(luminance_f16)( const struct gl_texture_image *texImage,
                                  GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 1 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], _mesa_half_to_float(src[0]));
   texel[GCOMP] =
   texel[BCOMP] = texel[RCOMP];
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D LUMINANCE_FLOAT16 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_luminance_f16)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = _mesa_half_to_float(src[0]);
   texel[ACOMP] = CHAN_MAXF;
}

/* Fetch texel from 1D, 2D or 3D LUMINANCE_ALPHA_FLOAT32 texture,
 * returning 4 GLchans.
 */
static void FETCH(luminance_alpha_f32)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 2 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], src[0]);
   texel[GCOMP] =
   texel[BCOMP] = texel[RCOMP];
   UNCLAMPED_FLOAT_TO_CHAN(texel[ACOMP], src[1]);
}

/* Fetch texel from 1D, 2D or 3D LUMINANCE_ALPHA_FLOAT32 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_luminance_alpha_f32)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 2 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = src[0];
   texel[ACOMP] = src[1];
}

/* Fetch texel from 1D, 2D or 3D LUMINANCE_ALPHA_FLOAT16 texture,
 * returning 4 GLfloats.
 */
static void FETCH(luminance_alpha_f16)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 2 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], _mesa_half_to_float(src[0]));
   texel[GCOMP] =
   texel[BCOMP] = texel[RCOMP];
   UNCLAMPED_FLOAT_TO_CHAN(texel[ACOMP], _mesa_half_to_float(src[1]));
}

/* Fetch texel from 1D, 2D or 3D LUMINANCE_ALPHA_FLOAT16 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_luminance_alpha_f16)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 2 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = _mesa_half_to_float(src[0]);
   texel[ACOMP] = _mesa_half_to_float(src[1]);
}

/* Fetch texel from 1D, 2D or 3D INTENSITY_FLOAT32 texture,
 * returning 4 GLchans.
 */
static void FETCH(intensity_f32)( const struct gl_texture_image *texImage,
                                  GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 1 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], src[0]);
   texel[GCOMP] =
   texel[BCOMP] =
   texel[ACOMP] = texel[RCOMP];
}

/* Fetch texel from 1D, 2D or 3D INTENSITY_FLOAT32 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_intensity_f32)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLfloat *src = FLOAT_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] =
   texel[ACOMP] = src[0];
}

/* Fetch texel from 1D, 2D or 3D INTENSITY_FLOAT16 texture,
 * returning 4 GLchans.
 */
static void FETCH(intensity_f16)( const struct gl_texture_image *texImage,
                                  GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 1 );
   UNCLAMPED_FLOAT_TO_CHAN(texel[RCOMP], _mesa_half_to_float(src[0]));
   texel[GCOMP] =
   texel[BCOMP] =
   texel[ACOMP] = texel[RCOMP];
}

/* Fetch texel from 1D, 2D or 3D INTENSITY_FLOAT16 texture,
 * returning 4 GLfloats.
 */
static void FETCH(f_intensity_f16)( const struct gl_texture_image *texImage,
                                    GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLhalfARB *src = HALF_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] =
   texel[ACOMP] = _mesa_half_to_float(src[0]);
}



/*
 * Begin Hardware formats
 */

/* Fetch texel from 1D, 2D or 3D rgba8888 texture, return 4 GLchans */
static void FETCH(rgba8888)( const struct gl_texture_image *texImage,
			     GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLuint s = *UINT_SRC( texImage, i, j, k );
   texel[RCOMP] = UBYTE_TO_CHAN( (s >> 24)        );
   texel[GCOMP] = UBYTE_TO_CHAN( (s >> 16) & 0xff );
   texel[BCOMP] = UBYTE_TO_CHAN( (s >>  8) & 0xff );
   texel[ACOMP] = UBYTE_TO_CHAN( (s      ) & 0xff );
}

/* Fetch texel from 1D, 2D or 3D rgba8888 texture, return 4 GLfloats */
static void FETCH(f_rgba8888)( const struct gl_texture_image *texImage,
                               GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLuint s = *UINT_SRC( texImage, i, j, k );
   texel[RCOMP] = UBYTE_TO_FLOAT( (s >> 24)        );
   texel[GCOMP] = UBYTE_TO_FLOAT( (s >> 16) & 0xff );
   texel[BCOMP] = UBYTE_TO_FLOAT( (s >>  8) & 0xff );
   texel[ACOMP] = UBYTE_TO_FLOAT( (s      ) & 0xff );
}


/* Fetch texel from 1D, 2D or 3D abgr8888 texture, return 4 GLchans */
static void FETCH(rgba8888_rev)( const struct gl_texture_image *texImage,
                                 GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLuint s = *UINT_SRC( texImage, i, j, k );
   texel[RCOMP] = UBYTE_TO_CHAN( (s      ) & 0xff );
   texel[GCOMP] = UBYTE_TO_CHAN( (s >>  8) & 0xff );
   texel[BCOMP] = UBYTE_TO_CHAN( (s >> 16) & 0xff );
   texel[ACOMP] = UBYTE_TO_CHAN( (s >> 24)        );
}

/* Fetch texel from 1D, 2D or 3D abgr8888 texture, return 4 GLfloats */
static void FETCH(f_rgba8888_rev)( const struct gl_texture_image *texImage,
                                   GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLuint s = *UINT_SRC( texImage, i, j, k );
   texel[RCOMP] = UBYTE_TO_FLOAT( (s      ) & 0xff );
   texel[GCOMP] = UBYTE_TO_FLOAT( (s >>  8) & 0xff );
   texel[BCOMP] = UBYTE_TO_FLOAT( (s >> 16) & 0xff );
   texel[ACOMP] = UBYTE_TO_FLOAT( (s >> 24)        );
}


/* Fetch texel from 1D, 2D or 3D argb8888 texture, return 4 GLchans */
static void FETCH(argb8888)( const struct gl_texture_image *texImage,
			     GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLuint s = *UINT_SRC( texImage, i, j, k );
   texel[RCOMP] = UBYTE_TO_CHAN( (s >> 16) & 0xff );
   texel[GCOMP] = UBYTE_TO_CHAN( (s >>  8) & 0xff );
   texel[BCOMP] = UBYTE_TO_CHAN( (s      ) & 0xff );
   texel[ACOMP] = UBYTE_TO_CHAN( (s >> 24)        );
}

/* Fetch texel from 1D, 2D or 3D argb8888 texture, return 4 GLfloats */
static void FETCH(f_argb8888)( const struct gl_texture_image *texImage,
                               GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLuint s = *UINT_SRC( texImage, i, j, k );
   texel[RCOMP] = UBYTE_TO_FLOAT( (s >> 16) & 0xff );
   texel[GCOMP] = UBYTE_TO_FLOAT( (s >>  8) & 0xff );
   texel[BCOMP] = UBYTE_TO_FLOAT( (s      ) & 0xff );
   texel[ACOMP] = UBYTE_TO_FLOAT( (s >> 24)        );
}


/* Fetch texel from 1D, 2D or 3D argb8888_rev texture, return 4 GLchans */
static void FETCH(argb8888_rev)( const struct gl_texture_image *texImage,
                                 GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLuint s = *UINT_SRC( texImage, i, j, k );
   texel[RCOMP] = UBYTE_TO_CHAN( (s >>  8) & 0xff );
   texel[GCOMP] = UBYTE_TO_CHAN( (s >> 16) & 0xff );
   texel[BCOMP] = UBYTE_TO_CHAN( (s >> 24)        );
   texel[ACOMP] = UBYTE_TO_CHAN( (s      ) & 0xff );
}


/* Fetch texel from 1D, 2D or 3D argb8888_rev texture, return 4 GLfloats */
static void FETCH(f_argb8888_rev)( const struct gl_texture_image *texImage,
                                   GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLuint s = *UINT_SRC( texImage, i, j, k );
   texel[RCOMP] = UBYTE_TO_FLOAT( (s >>  8) & 0xff );
   texel[GCOMP] = UBYTE_TO_FLOAT( (s >> 16) & 0xff );
   texel[BCOMP] = UBYTE_TO_FLOAT( (s >> 24)        );
   texel[ACOMP] = UBYTE_TO_FLOAT( (s      ) & 0xff );
}


/* Fetch texel from 1D, 2D or 3D rgb888 texture, return 4 GLchans */
static void FETCH(rgb888)( const struct gl_texture_image *texImage,
			   GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 3 );
   texel[RCOMP] = UBYTE_TO_CHAN( src[2] );
   texel[GCOMP] = UBYTE_TO_CHAN( src[1] );
   texel[BCOMP] = UBYTE_TO_CHAN( src[0] );
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D rgb888 texture, return 4 GLfloats */
static void FETCH(f_rgb888)( const struct gl_texture_image *texImage,
                             GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 3 );
   texel[RCOMP] = UBYTE_TO_FLOAT( src[2] );
   texel[GCOMP] = UBYTE_TO_FLOAT( src[1] );
   texel[BCOMP] = UBYTE_TO_FLOAT( src[0] );
   texel[ACOMP] = CHAN_MAXF;
}


/* Fetch texel from 1D, 2D or 3D bgr888 texture, return 4 GLchans */
static void FETCH(bgr888)( const struct gl_texture_image *texImage,
			   GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 3 );
   texel[RCOMP] = UBYTE_TO_CHAN( src[0] );
   texel[GCOMP] = UBYTE_TO_CHAN( src[1] );
   texel[BCOMP] = UBYTE_TO_CHAN( src[2] );
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D bgr888 texture, return 4 GLfloats */
static void FETCH(f_bgr888)( const struct gl_texture_image *texImage,
                             GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 3 );
   texel[RCOMP] = UBYTE_TO_FLOAT( src[0] );
   texel[GCOMP] = UBYTE_TO_FLOAT( src[1] );
   texel[BCOMP] = UBYTE_TO_FLOAT( src[2] );
   texel[ACOMP] = CHAN_MAXF;
}


/* Fetch texel from 1D, 2D or 3D rgb565 texture, return 4 GLchans */
static void FETCH(rgb565)( const struct gl_texture_image *texImage,
			   GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = *src;
   texel[RCOMP] = UBYTE_TO_CHAN( ((s >> 8) & 0xf8) * 255 / 0xf8 );
   texel[GCOMP] = UBYTE_TO_CHAN( ((s >> 3) & 0xfc) * 255 / 0xfc );
   texel[BCOMP] = UBYTE_TO_CHAN( ((s << 3) & 0xf8) * 255 / 0xf8 );
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D rgb565 texture, return 4 GLfloats */
static void FETCH(f_rgb565)( const struct gl_texture_image *texImage,
                             GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = *src;
   texel[RCOMP] = ((s >> 8) & 0xf8) * (1.0F / 248.0F);
   texel[GCOMP] = ((s >> 3) & 0xfc) * (1.0F / 252.0F);
   texel[BCOMP] = ((s << 3) & 0xf8) * (1.0F / 248.0F);
   texel[ACOMP] = CHAN_MAXF;
}


/* Fetch texel from 1D, 2D or 3D rgb565_rev texture, return 4 GLchans */
static void FETCH(rgb565_rev)( const struct gl_texture_image *texImage,
                               GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = (*src >> 8) | (*src << 8); /* byte swap */
   texel[RCOMP] = UBYTE_TO_CHAN( ((s >> 8) & 0xf8) * 255 / 0xf8 );
   texel[GCOMP] = UBYTE_TO_CHAN( ((s >> 3) & 0xfc) * 255 / 0xfc );
   texel[BCOMP] = UBYTE_TO_CHAN( ((s << 3) & 0xf8) * 255 / 0xf8 );
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D rgb565_rev texture, return 4 GLfloats */
static void FETCH(f_rgb565_rev)( const struct gl_texture_image *texImage,
                                 GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = (*src >> 8) | (*src << 8); /* byte swap */
   texel[RCOMP] = ((s >> 8) & 0xf8) * (1.0F / 248.0F);
   texel[GCOMP] = ((s >> 3) & 0xfc) * (1.0F / 252.0F);
   texel[BCOMP] = ((s << 3) & 0xf8) * (1.0F / 248.0F);
   texel[ACOMP] = CHAN_MAXF;
}


/* Fetch texel from 1D, 2D or 3D argb444 texture, return 4 GLchans */
static void FETCH(argb4444)( const struct gl_texture_image *texImage,
			     GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = *src;
   texel[RCOMP] = UBYTE_TO_CHAN( ((s >>  8) & 0xf) * 255 / 0xf );
   texel[GCOMP] = UBYTE_TO_CHAN( ((s >>  4) & 0xf) * 255 / 0xf );
   texel[BCOMP] = UBYTE_TO_CHAN( ((s      ) & 0xf) * 255 / 0xf );
   texel[ACOMP] = UBYTE_TO_CHAN( ((s >> 12) & 0xf) * 255 / 0xf );
}

/* Fetch texel from 1D, 2D or 3D argb4444 texture, return 4 GLfloats */
static void FETCH(f_argb4444)( const struct gl_texture_image *texImage,
			     GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = *src;
   texel[RCOMP] = ((s >>  8) & 0xf) * (1.0F / 15.0F);
   texel[GCOMP] = ((s >>  4) & 0xf) * (1.0F / 15.0F);
   texel[BCOMP] = ((s      ) & 0xf) * (1.0F / 15.0F);
   texel[ACOMP] = ((s >> 12) & 0xf) * (1.0F / 15.0F);
}


/* Fetch texel from 1D, 2D or 3D argb4444_rev texture, return 4 GLchans */
static void FETCH(argb4444_rev)( const struct gl_texture_image *texImage,
                                 GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort s = *USHORT_SRC( texImage, i, j, k );
   texel[RCOMP] = UBYTE_TO_CHAN( ((s      ) & 0xf) * 255 / 0xf );
   texel[GCOMP] = UBYTE_TO_CHAN( ((s >> 12) & 0xf) * 255 / 0xf );
   texel[BCOMP] = UBYTE_TO_CHAN( ((s >>  8) & 0xf) * 255 / 0xf );
   texel[ACOMP] = UBYTE_TO_CHAN( ((s >>  4) & 0xf) * 255 / 0xf );
}

/* Fetch texel from 1D, 2D or 3D argb4444_rev texture, return 4 GLfloats */
static void FETCH(f_argb4444_rev)( const struct gl_texture_image *texImage,
                                   GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort s = *USHORT_SRC( texImage, i, j, k );
   texel[RCOMP] = ((s      ) & 0xf) * (1.0F / 15.0F);
   texel[GCOMP] = ((s >> 12) & 0xf) * (1.0F / 15.0F);
   texel[BCOMP] = ((s >>  8) & 0xf) * (1.0F / 15.0F);
   texel[ACOMP] = ((s >>  4) & 0xf) * (1.0F / 15.0F);
}


/* Fetch texel from 1D, 2D or 3D argb1555 texture, return 4 GLchans */
static void FETCH(argb1555)( const struct gl_texture_image *texImage,
			     GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = *src;
   texel[RCOMP] = UBYTE_TO_CHAN( ((s >> 10) & 0x1f) * 255 / 0x1f );
   texel[GCOMP] = UBYTE_TO_CHAN( ((s >>  5) & 0x1f) * 255 / 0x1f );
   texel[BCOMP] = UBYTE_TO_CHAN( ((s      ) & 0x1f) * 255 / 0x1f );
   texel[ACOMP] = UBYTE_TO_CHAN( ((s >> 15) & 0x01) * 255 );
}

/* Fetch texel from 1D, 2D or 3D argb1555 texture, return 4 GLfloats */
static void FETCH(f_argb1555)( const struct gl_texture_image *texImage,
                               GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = *src;
   texel[RCOMP] = ((s >> 10) & 0x1f) * (1.0F / 31.0F);
   texel[GCOMP] = ((s >>  5) & 0x1f) * (1.0F / 31.0F);
   texel[BCOMP] = ((s      ) & 0x1f) * (1.0F / 31.0F);
   texel[ACOMP] = ((s >> 15) & 0x01);
}


/* Fetch texel from 1D, 2D or 3D argb1555_rev texture, return 4 GLchans */
static void FETCH(argb1555_rev)( const struct gl_texture_image *texImage,
                                 GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = (*src << 8) | (*src >> 8); /* byteswap */
   texel[RCOMP] = UBYTE_TO_CHAN( ((s >> 10) & 0x1f) * 255 / 0x1f );
   texel[GCOMP] = UBYTE_TO_CHAN( ((s >>  5) & 0x1f) * 255 / 0x1f );
   texel[BCOMP] = UBYTE_TO_CHAN( ((s      ) & 0x1f) * 255 / 0x1f );
   texel[ACOMP] = UBYTE_TO_CHAN( ((s >> 15) & 0x01) * 255 );
}

/* Fetch texel from 1D, 2D or 3D argb1555_rev texture, return 4 GLfloats */
static void FETCH(f_argb1555_rev)( const struct gl_texture_image *texImage,
                                   GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort *src = USHORT_SRC( texImage, i, j, k );
   const GLushort s = (*src << 8) | (*src >> 8); /* byteswap */
   texel[RCOMP] = ((s >> 10) & 0x1f) * (1.0F / 31.0F);
   texel[GCOMP] = ((s >>  5) & 0x1f) * (1.0F / 31.0F);
   texel[BCOMP] = ((s      ) & 0x1f) * (1.0F / 31.0F);
   texel[ACOMP] = ((s >> 15) & 0x01);
}


/* Fetch texel from 1D, 2D or 3D al88 texture, return 4 GLchans */
static void FETCH(al88)( const struct gl_texture_image *texImage,
			 GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort s = *USHORT_SRC( texImage, i, j, k );
   texel[RCOMP] = 
   texel[GCOMP] = 
   texel[BCOMP] = UBYTE_TO_CHAN( s & 0xff );
   texel[ACOMP] = UBYTE_TO_CHAN( s >> 8 );
}

/* Fetch texel from 1D, 2D or 3D al88 texture, return 4 GLfloats */
static void FETCH(f_al88)( const struct gl_texture_image *texImage,
                           GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort s = *USHORT_SRC( texImage, i, j, k );
   texel[RCOMP] = 
   texel[GCOMP] = 
   texel[BCOMP] = UBYTE_TO_FLOAT( s & 0xff );
   texel[ACOMP] = UBYTE_TO_FLOAT( s >> 8 );
}


/* Fetch texel from 1D, 2D or 3D al88_rev texture, return 4 GLchans */
static void FETCH(al88_rev)( const struct gl_texture_image *texImage,
                             GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort s = *USHORT_SRC( texImage, i, j, k );
   texel[RCOMP] = 
   texel[GCOMP] = 
   texel[BCOMP] = UBYTE_TO_CHAN( s >> 8 );
   texel[ACOMP] = UBYTE_TO_CHAN( s & 0xff );
}

/* Fetch texel from 1D, 2D or 3D al88_rev texture, return 4 GLfloats */
static void FETCH(f_al88_rev)( const struct gl_texture_image *texImage,
                               GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort s = *USHORT_SRC( texImage, i, j, k );
   texel[RCOMP] = 
   texel[GCOMP] = 
   texel[BCOMP] = UBYTE_TO_FLOAT( s >> 8 );
   texel[ACOMP] = UBYTE_TO_FLOAT( s & 0xff );
}


/* Fetch texel from 1D, 2D or 3D rgb332 texture, return 4 GLchans */
static void FETCH(rgb332)( const struct gl_texture_image *texImage,
			   GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 1 );
   const GLubyte s = *src;
   texel[RCOMP] = UBYTE_TO_CHAN( ((s     ) & 0xe0) * 255 / 0xe0 );
   texel[GCOMP] = UBYTE_TO_CHAN( ((s << 3) & 0xe0) * 255 / 0xe0 );
   texel[BCOMP] = UBYTE_TO_CHAN( ((s << 6) & 0xc0) * 255 / 0xc0 );
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D rgb332 texture, return 4 GLfloats */
static void FETCH(f_rgb332)( const struct gl_texture_image *texImage,
                             GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 1 );
   const GLubyte s = *src;
   texel[RCOMP] = ((s     ) & 0xe0) * (1.0F / 224.0F);
   texel[GCOMP] = ((s << 3) & 0xe0) * (1.0F / 224.0F);
   texel[BCOMP] = ((s << 6) & 0xc0) * (1.0F / 192.0F);
   texel[ACOMP] = CHAN_MAXF;
}


/* Fetch texel from 1D, 2D or 3D a8 texture, return 4 GLchans */
static void FETCH(a8)( const struct gl_texture_image *texImage,
		       GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = 0;
   texel[ACOMP] = UBYTE_TO_CHAN( src[0] );
}

/* Fetch texel from 1D, 2D or 3D a8 texture, return 4 GLfloats */
static void FETCH(f_a8)( const struct gl_texture_image *texImage,
                         GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] = 
   texel[GCOMP] = 
   texel[BCOMP] = 0.0;
   texel[ACOMP] = UBYTE_TO_FLOAT( src[0] );
}


/* Fetch texel from 1D, 2D or 3D l8 texture, return 4 GLchans */
static void FETCH(l8)( const struct gl_texture_image *texImage,
		       GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] = UBYTE_TO_CHAN( src[0] );
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D l8 texture, return 4 GLfloats */
static void FETCH(f_l8)( const struct gl_texture_image *texImage,
                         GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] = 
   texel[GCOMP] = 
   texel[BCOMP] = UBYTE_TO_FLOAT( src[0] );
   texel[ACOMP] = CHAN_MAXF;
}


/* Fetch texel from 1D, 2D or 3D i8 texture, return 4 GLchans */
static void FETCH(i8)( const struct gl_texture_image *texImage,
		       GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] =
   texel[GCOMP] =
   texel[BCOMP] =
   texel[ACOMP] = UBYTE_TO_CHAN( src[0] );
}

/* Fetch texel from 1D, 2D or 3D i8 texture, return 4 GLfloats */
static void FETCH(f_i8)( const struct gl_texture_image *texImage,
                         GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 1 );
   texel[RCOMP] = 
   texel[GCOMP] = 
   texel[BCOMP] = 
   texel[ACOMP] = UBYTE_TO_FLOAT( src[0] );
}


/* Fetch CI texel from 1D, 2D or 3D ci8 texture, lookup the index in a
 * color table, and return 4 GLchans.
 */
static void FETCH(ci8)( const struct gl_texture_image *texImage,
			GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLubyte *src = UBYTE_SRC( texImage, i, j, k, 1 );
   const struct gl_color_table *palette;
   const GLchan *table;
   GLuint index;
   GET_CURRENT_CONTEXT(ctx);

   if (ctx->Texture.SharedPalette) {
      palette = &ctx->Texture.Palette;
   }
   else {
      palette = &texImage->TexObject->Palette;
   }
   if (palette->Size == 0)
      return; /* undefined results */
   ASSERT(palette->Type != GL_FLOAT);
   table = (const GLchan *) palette->Table;

   /* Mask the index against size of palette to avoid going out of bounds */
   index = (*src) & (palette->Size - 1);

   switch (palette->Format) {
      case GL_ALPHA:
         texel[RCOMP] =
         texel[GCOMP] =
         texel[BCOMP] = 0;
         texel[ACOMP] = table[index];
         return;
      case GL_LUMINANCE:
         texel[RCOMP] =
         texel[GCOMP] =
         texel[BCOMP] = table[index];
         texel[ACOMP] = CHAN_MAX;
         break;
      case GL_INTENSITY:
         texel[RCOMP] =
         texel[GCOMP] =
         texel[BCOMP] =
         texel[ACOMP] = table[index];
         return;
      case GL_LUMINANCE_ALPHA:
         texel[RCOMP] =
         texel[GCOMP] =
         texel[BCOMP] = table[index * 2 + 0];
         texel[ACOMP] = table[index * 2 + 1];
         return;
      case GL_RGB:
         texel[RCOMP] = table[index * 3 + 0];
         texel[GCOMP] = table[index * 3 + 1];
         texel[BCOMP] = table[index * 3 + 2];
         texel[ACOMP] = CHAN_MAX;
         return;
      case GL_RGBA:
         texel[RCOMP] = table[index * 4 + 0];
         texel[GCOMP] = table[index * 4 + 1];
         texel[BCOMP] = table[index * 4 + 2];
         texel[ACOMP] = table[index * 4 + 3];
         return;
      default:
         _mesa_problem(ctx, "Bad palette format in palette_sample");
   }
}


/* Fetch CI texel from 1D, 2D or 3D ci8 texture, lookup the index in a
 * color table, and return 4 GLfloats.
 */
static void FETCH(f_ci8)( const struct gl_texture_image *texImage,
                          GLint i, GLint j, GLint k, GLfloat *texel )
{
   GLchan rgba[4];
   /* Sample as GLchan */
   FETCH(ci8)(texImage, i, j, k, rgba);
   /* and return as floats */
   texel[RCOMP] = CHAN_TO_FLOAT(rgba[RCOMP]);
   texel[GCOMP] = CHAN_TO_FLOAT(rgba[GCOMP]);
   texel[BCOMP] = CHAN_TO_FLOAT(rgba[BCOMP]);
   texel[ACOMP] = CHAN_TO_FLOAT(rgba[ACOMP]);
}


/* Fetch texel from 1D, 2D or 3D ycbcr texture, return 4 GLchans */
/* We convert YCbCr to RGB here */
/* XXX this may break if GLchan != GLubyte */
static void FETCH(ycbcr)( const struct gl_texture_image *texImage,
                          GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort *src0 = USHORT_SRC( texImage, (i & ~1), j, k ); /* even */
   const GLushort *src1 = src0 + 1;                               /* odd */
   const GLubyte y0 = (*src0 >> 8) & 0xff;  /* luminance */
   const GLubyte cb = *src0 & 0xff;         /* chroma U */
   const GLubyte y1 = (*src1 >> 8) & 0xff;  /* luminance */
   const GLubyte cr = *src1 & 0xff;         /* chroma V */
   GLint r, g, b;
   if (i & 1) {
      /* odd pixel: use y1,cr,cb */
      r = (GLint) (1.164 * (y1-16) + 1.596 * (cr-128));
      g = (GLint) (1.164 * (y1-16) - 0.813 * (cr-128) - 0.391 * (cb-128));
      b = (GLint) (1.164 * (y1-16) + 2.018 * (cb-128));
   }
   else {
      /* even pixel: use y0,cr,cb */
      r = (GLint) (1.164 * (y0-16) + 1.596 * (cr-128));
      g = (GLint) (1.164 * (y0-16) - 0.813 * (cr-128) - 0.391 * (cb-128));
      b = (GLint) (1.164 * (y0-16) + 2.018 * (cb-128));
   }
   texel[RCOMP] = CLAMP(r, 0, CHAN_MAX);
   texel[GCOMP] = CLAMP(g, 0, CHAN_MAX);
   texel[BCOMP] = CLAMP(b, 0, CHAN_MAX);
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D ycbcr texture, return 4 GLfloats */
/* We convert YCbCr to RGB here */
static void FETCH(f_ycbcr)( const struct gl_texture_image *texImage,
                            GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort *src0 = USHORT_SRC( texImage, (i & ~1), j, k ); /* even */
   const GLushort *src1 = src0 + 1;                               /* odd */
   const GLubyte y0 = (*src0 >> 8) & 0xff;  /* luminance */
   const GLubyte cb = *src0 & 0xff;         /* chroma U */
   const GLubyte y1 = (*src1 >> 8) & 0xff;  /* luminance */
   const GLubyte cr = *src1 & 0xff;         /* chroma V */
   GLfloat r, g, b;
   if (i & 1) {
      /* odd pixel: use y1,cr,cb */
      r = (1.164 * (y1-16) + 1.596 * (cr-128));
      g = (1.164 * (y1-16) - 0.813 * (cr-128) - 0.391 * (cb-128));
      b = (1.164 * (y1-16) + 2.018 * (cb-128));
   }
   else {
      /* even pixel: use y0,cr,cb */
      r = (1.164 * (y0-16) + 1.596 * (cr-128));
      g = (1.164 * (y0-16) - 0.813 * (cr-128) - 0.391 * (cb-128));
      b = (1.164 * (y0-16) + 2.018 * (cb-128));
   }
   /* XXX remove / 255 here by tweaking arithmetic above */
   r /= 255.0;
   g /= 255.0;
   b /= 255.0;
   /* XXX should we really clamp??? */
   texel[RCOMP] = CLAMP(r, 0.0, 1.0);
   texel[GCOMP] = CLAMP(g, 0.0, 1.0);
   texel[BCOMP] = CLAMP(b, 0.0, 1.0);
   texel[ACOMP] = CHAN_MAXF;
}


/* Fetch texel from 1D, 2D or 3D ycbcr_rev texture, return 4 GLchans */
/* We convert YCbCr to RGB here */
/* XXX this may break if GLchan != GLubyte */
static void FETCH(ycbcr_rev)( const struct gl_texture_image *texImage,
                              GLint i, GLint j, GLint k, GLchan *texel )
{
   const GLushort *src0 = USHORT_SRC( texImage, (i & ~1), j, k ); /* even */
   const GLushort *src1 = src0 + 1;                               /* odd */
   const GLubyte y0 = *src0 & 0xff;         /* luminance */
   const GLubyte cr = (*src0 >> 8) & 0xff;  /* chroma V */
   const GLubyte y1 = *src1 & 0xff;         /* luminance */
   const GLubyte cb = (*src1 >> 8) & 0xff;  /* chroma U */
   GLint r, g, b;
   if (i & 1) {
      /* odd pixel: use y1,cr,cb */
      r = (GLint) (1.164 * (y1-16) + 1.596 * (cr-128));
      g = (GLint) (1.164 * (y1-16) - 0.813 * (cr-128) - 0.391 * (cb-128));
      b = (GLint) (1.164 * (y1-16) + 2.018 * (cb-128));
   }
   else {
      /* even pixel: use y0,cr,cb */
      r = (GLint) (1.164 * (y0-16) + 1.596 * (cr-128));
      g = (GLint) (1.164 * (y0-16) - 0.813 * (cr-128) - 0.391 * (cb-128));
      b = (GLint) (1.164 * (y0-16) + 2.018 * (cb-128));
   }
   texel[RCOMP] = CLAMP(r, 0, CHAN_MAX);
   texel[GCOMP] = CLAMP(g, 0, CHAN_MAX);
   texel[BCOMP] = CLAMP(b, 0, CHAN_MAX);
   texel[ACOMP] = CHAN_MAX;
}

/* Fetch texel from 1D, 2D or 3D ycbcr_rev texture, return 4 GLfloats */
/* We convert YCbCr to RGB here */
static void FETCH(f_ycbcr_rev)( const struct gl_texture_image *texImage,
                                GLint i, GLint j, GLint k, GLfloat *texel )
{
   const GLushort *src0 = USHORT_SRC( texImage, (i & ~1), j, k ); /* even */
   const GLushort *src1 = src0 + 1;                               /* odd */
   const GLubyte y0 = *src0 & 0xff;         /* luminance */
   const GLubyte cr = (*src0 >> 8) & 0xff;  /* chroma V */
   const GLubyte y1 = *src1 & 0xff;         /* luminance */
   const GLubyte cb = (*src1 >> 8) & 0xff;  /* chroma U */
   GLfloat r, g, b;
   if (i & 1) {
      /* odd pixel: use y1,cr,cb */
      r = (1.164 * (y1-16) + 1.596 * (cr-128));
      g = (1.164 * (y1-16) - 0.813 * (cr-128) - 0.391 * (cb-128));
      b = (1.164 * (y1-16) + 2.018 * (cb-128));
   }
   else {
      /* even pixel: use y0,cr,cb */
      r = (1.164 * (y0-16) + 1.596 * (cr-128));
      g = (1.164 * (y0-16) - 0.813 * (cr-128) - 0.391 * (cb-128));
      b = (1.164 * (y0-16) + 2.018 * (cb-128));
   }
   /* XXX remove / 255 here by tweaking arithmetic above */
   r /= 255.0;
   g /= 255.0;
   b /= 255.0;
   /* XXX should we really clamp??? */
   texel[RCOMP] = CLAMP(r, 0.0, 1.0);
   texel[GCOMP] = CLAMP(g, 0.0, 1.0);
   texel[BCOMP] = CLAMP(b, 0.0, 1.0);
   texel[ACOMP] = CHAN_MAXF;
}



#undef CHAN_SRC
#undef UBYTE_SRC
#undef USHORT_SRC
#undef UINT_SRC
#undef FLOAT_SRC
#undef HALF_SRC
#undef FETCH
#undef DIM
