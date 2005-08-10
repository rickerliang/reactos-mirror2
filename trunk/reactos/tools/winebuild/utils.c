/*
 * Small utility functions for winebuild
 *
 * Copyright 2000 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#if !defined(WIN32)
#undef strdup
#endif

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build.h"

void *xmalloc (size_t size)
{
    void *res;

    res = malloc (size ? size : 1);
    if (res == NULL)
    {
        fprintf (stderr, "Virtual memory exhausted.\n");
        exit (1);
    }
    return res;
}

void *xrealloc (void *ptr, size_t size)
{
    void *res = realloc (ptr, size);
    if (size && res == NULL)
    {
        fprintf (stderr, "Virtual memory exhausted.\n");
        exit (1);
    }
    return res;
}

char *xstrdup( const char *str )
{
    char *res = strdup( str );
    if (!res)
    {
        fprintf (stderr, "Virtual memory exhausted.\n");
        exit (1);
    }
    return res;
}

char *strupper(char *s)
{
    char *p;
    for (p = s; *p; p++) *p = toupper(*p);
    return s;
}

int strendswith(const char* str, const char* end)
{
    int l = strlen(str);
    int m = strlen(end);
    return l >= m && strcmp(str + l - m, end) == 0;
}

void fatal_error( const char *msg, ... )
{
    va_list valist;
    va_start( valist, msg );
    if (input_file_name)
    {
        fprintf( stderr, "%s:", input_file_name );
        if (current_line)
            fprintf( stderr, "%d:", current_line );
        fputc( ' ', stderr );
    }
    else fprintf( stderr, "winebuild: " );
    vfprintf( stderr, msg, valist );
    va_end( valist );
    exit(1);
}

void fatal_perror( const char *msg, ... )
{
    va_list valist;
    va_start( valist, msg );
    if (input_file_name)
    {
        fprintf( stderr, "%s:", input_file_name );
        if (current_line)
            fprintf( stderr, "%d:", current_line );
        fputc( ' ', stderr );
    }
    vfprintf( stderr, msg, valist );
    perror( " " );
    va_end( valist );
    exit(1);
}

void error( const char *msg, ... )
{
    va_list valist;
    va_start( valist, msg );
    if (input_file_name)
    {
        fprintf( stderr, "%s:", input_file_name );
        if (current_line)
            fprintf( stderr, "%d:", current_line );
        fputc( ' ', stderr );
    }
    vfprintf( stderr, msg, valist );
    va_end( valist );
    nb_errors++;
}

void warning( const char *msg, ... )
{
    va_list valist;

    if (!display_warnings) return;
    va_start( valist, msg );
    if (input_file_name)
    {
        fprintf( stderr, "%s:", input_file_name );
        if (current_line)
            fprintf( stderr, "%d:", current_line );
        fputc( ' ', stderr );
    }
    fprintf( stderr, "warning: " );
    vfprintf( stderr, msg, valist );
    va_end( valist );
}

/* output a standard header for generated files */
void output_standard_file_header( FILE *outfile )
{
    if (spec_file_name)
        fprintf( outfile, "/* File generated automatically from %s; do not edit! */\n",
                 spec_file_name );
    else
        fprintf( outfile, "/* File generated automatically; do not edit! */\n" );
    fprintf( outfile,
             "/* This file can be copied, modified and distributed without restriction. */\n\n" );
}

/* dump a byte stream into the assembly code */
void dump_bytes( FILE *outfile, const unsigned char *data, int len,
                 const char *label, int constant )
{
    int i;

    fprintf( outfile, "\nstatic %sunsigned char %s[%d] = {",
             constant ? "const " : "", label, len );
    for (i = 0; i < len; i++)
    {
        if (!(i & 7)) fprintf( outfile, "\n  " );
        fprintf( outfile, "0x%02x", *data++ );
        if (i < len - 1) fprintf( outfile, "," );
    }
    fprintf( outfile, "\n};\n" );
}


/*******************************************************************
 *         open_input_file
 *
 * Open a file in the given srcdir and set the input_file_name global variable.
 */
FILE *open_input_file( const char *srcdir, const char *name )
{
    char *fullname;
    FILE *file = fopen( name, "r" );

    if (!file && srcdir)
    {
        fullname = xmalloc( strlen(srcdir) + strlen(name) + 2 );
        strcpy( fullname, srcdir );
        strcat( fullname, "/" );
        strcat( fullname, name );
        file = fopen( fullname, "r" );
    }
    else fullname = xstrdup( name );

    if (!file) fatal_error( "Cannot open file '%s'\n", fullname );
    input_file_name = fullname;
    current_line = 1;
    return file;
}


/*******************************************************************
 *         close_input_file
 *
 * Close the current input file (must have been opened with open_input_file).
 */
void close_input_file( FILE *file )
{
    fclose( file );
    free( input_file_name );
    input_file_name = NULL;
    current_line = 0;
}


/*******************************************************************
 *         remove_stdcall_decoration
 *
 * Remove a possible @xx suffix from a function name.
 * Return the numerical value of the suffix, or -1 if none.
 */
int remove_stdcall_decoration( char *name )
{
    char *p, *end = strrchr( name, '@' );
    if (!end || !end[1] || end == name) return -1;
    /* make sure all the rest is digits */
    for (p = end + 1; *p; p++) if (!isdigit(*p)) return -1;
    *end = 0;
    return atoi( end + 1 );
}


/*******************************************************************
 *         alloc_dll_spec
 *
 * Create a new dll spec file descriptor
 */
DLLSPEC *alloc_dll_spec(void)
{
    DLLSPEC *spec;

    spec = xmalloc( sizeof(*spec) );
    spec->file_name          = NULL;
    spec->dll_name           = NULL;
    spec->owner_name         = NULL;
    spec->init_func          = NULL;
    spec->type               = SPEC_WIN32;
    spec->base               = MAX_ORDINALS;
    spec->limit              = 0;
    spec->stack_size         = 0;
    spec->heap_size          = 0;
    spec->nb_entry_points    = 0;
    spec->alloc_entry_points = 0;
    spec->nb_names           = 0;
    spec->nb_resources       = 0;
    spec->characteristics    = 0;
    spec->subsystem          = 0;
    spec->subsystem_major    = 4;
    spec->subsystem_minor    = 0;
    spec->entry_points       = NULL;
    spec->names              = NULL;
    spec->ordinals           = NULL;
    spec->resources          = NULL;
    return spec;
}


/*******************************************************************
 *         free_dll_spec
 *
 * Free dll spec file descriptor
 */
void free_dll_spec( DLLSPEC *spec )
{
    int i;

    for (i = 0; i < spec->nb_entry_points; i++)
    {
        ORDDEF *odp = &spec->entry_points[i];
        free( odp->name );
        free( odp->export_name );
        free( odp->link_name );
    }
    free( spec->file_name );
    free( spec->dll_name );
    free( spec->owner_name );
    free( spec->init_func );
    free( spec->entry_points );
    free( spec->names );
    free( spec->ordinals );
    free( spec->resources );
    free( spec );
}


/*******************************************************************
 *         make_c_identifier
 *
 * Map a string to a valid C identifier.
 */
const char *make_c_identifier( const char *str )
{
    static char buffer[256];
    char *p;

    for (p = buffer; *str && p < buffer+sizeof(buffer)-1; p++, str++)
    {
        if (isalnum(*str)) *p = *str;
        else *p = '_';
    }
    *p = 0;
    return buffer;
}


/*****************************************************************
 *  Function:    get_alignment
 *
 *  Description:
 *    According to the info page for gas, the .align directive behaves
 * differently on different systems.  On some architectures, the
 * argument of a .align directive is the number of bytes to pad to, so
 * to align on an 8-byte boundary you'd say
 *     .align 8
 * On other systems, the argument is "the number of low-order zero bits
 * that the location counter must have after advancement."  So to
 * align on an 8-byte boundary you'd say
 *     .align 3
 *
 * The reason gas is written this way is that it's trying to mimick
 * native assemblers for the various architectures it runs on.  gas
 * provides other directives that work consistantly across
 * architectures, but of course we want to work on all arches with or
 * without gas.  Hence this function.
 *
 *
 *  Parameters:
 *    align  --  the number of bytes to align to. Must be a power of 2.
 */
unsigned int get_alignment(unsigned int align)
{
    unsigned int n;

    assert( !(align & (align - 1)) );

    switch(target_cpu)
    {
    case CPU_x86:
    case CPU_SPARC:
        if (target_platform != PLATFORM_APPLE) return align;
        /* fall through */
    case CPU_POWERPC:
    case CPU_ALPHA:
        n = 0;
        while ((1 << n) != align) n++;
        return n;
    }
    /* unreached */
    assert(0);
    return 0;
}

/* return the page size for the target CPU */
unsigned int get_page_size(void)
{
    switch(target_cpu)
    {
    case CPU_x86:     return 4096;
    case CPU_POWERPC: return 4096;
    case CPU_SPARC:   return 8192;
    case CPU_ALPHA:   return 8192;
    }
    /* unreached */
    assert(0);
    return 0;
}

/* return the assembly name for a C symbol */
const char *asm_name( const char *sym )
{
    static char buffer[256];

    switch (target_platform)
    {
    case PLATFORM_APPLE:
    case PLATFORM_WINDOWS:
        buffer[0] = '_';
        strcpy( buffer + 1, sym );
        return buffer;
    default:
        return sym;
    }
}

/* return an assembly function declaration for a C function name */
const char *func_declaration( const char *func )
{
    static char buffer[256];

    switch (target_platform)
    {
    case PLATFORM_APPLE:
        return "";
    case PLATFORM_WINDOWS:
        sprintf( buffer, ".def _%s; .scl 2; .type 32; .endef", func );
        break;
    case PLATFORM_SVR4:
        sprintf( buffer, ".type %s,2", func );
        break;
    default:
        sprintf( buffer, ".type %s,@function", func );
        break;
    }
    return buffer;
}

/* return a size declaration for an assembly function */
const char *func_size( const char *func )
{
    static char buffer[256];

    switch (target_platform)
    {
    case PLATFORM_APPLE:
    case PLATFORM_WINDOWS:
        return "";
    default:
        sprintf( buffer, ".size %s, .-%s", func, func );
        return buffer;
    }
}

const char *get_asm_string_keyword(void)
{
    switch (target_platform)
    {
    case PLATFORM_APPLE:
    case PLATFORM_SVR4:
        return ".asciz";
    default:
        return ".string";
    }
}

const char *get_asm_short_keyword(void)
{
    switch (target_platform)
    {
    case PLATFORM_SVR4: return ".half";
    default:            return ".short";
    }
}
