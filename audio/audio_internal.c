#include "audio/audio_internal.h"

#include <assert.h>

typedef struct audio_filter_list_t
{
    struct audio_filter_list_t *next;
    audio_filter_t *filter;
} audio_filter_list_t;

audio_filter_list_t *filter_list;

audio_filter_t *get_filter_by_id( enum AudioFilter id )
{
    assert( filter_list );
    int i = 0;
    audio_filter_list_t *f = filter_list;
    while( i++ < id )
    {
        f = f->next;
        if( !f )
        {
            fprintf( stderr, "audio [error]: Invalid filter ID requested: %d\n", id );
            return NULL;
        }
    }
    return f->filter;
}

static void append_to_filter_list( audio_filter_t *f )
{
    if( !filter_list )
    {
        filter_list = calloc( 1, sizeof( audio_filter_list_t ) );
        filter_list->filter = f;
    }
    else
    {
        audio_filter_list_t *last_filter = filter_list;
        while( last_filter->next )
            last_filter = last_filter->next;
        last_filter->next = calloc( 1, sizeof( audio_filter_list_t ) );
        last_filter->next->filter = f;
    }
}

/* NOTE: this function must be synchronized with the AudioFilter enum (audio.h) */
void register_all( void )
{
#define REGISTER(typename) { extern audio_filter_t typename; append_to_filter_list( &typename ); }
    REGISTER( audio_lavf_source );
#undef REGISTER
}

static void free_filter_list( audio_filter_list_t *f )
{
    if( !f )
        return;
    if( f->next )
        free_filter_list( f->next );
    free( f );
}

void unregister_all( void )
{
    free_filter_list( filter_list );
    filter_list = NULL;
}

#include <stdlib.h>
#include <string.h>
#include <assert.h>

char **split_string( char *string, char *sep, unsigned limit )
{
    if( !string )
        return NULL;
    int sep_count = 0;
    char *tmp = string;
    while( ( tmp = ( tmp = strstr( tmp, sep ) ) ? tmp + strlen( sep ) : 0 ) )
        ++sep_count;
    if( sep_count == 0 )
    {
        if( string[0] == '\0' )
            return calloc( 1, sizeof( char** ) );
        char **ret = calloc( 2, sizeof( char** ) );
        ret[0] = strdup( string );
        return ret;
    }

    char **split = calloc( ( limit > 0 ? limit : sep_count ) + 2, sizeof(char**) );
    int i = 0;
    char *str = strdup( string );
    assert( str );
    char *esc = NULL;
    char *tok = str, *nexttok = str;
    do
    {
        nexttok = strstr( nexttok, sep );
        if( nexttok )
            *nexttok++ = '\0';
        if( ( limit > 0 && i >= limit ) ||
            ( i > 0 && ( ( esc = strrchr( split[i-1], '\\' ) ) ? esc[1] == '\0' : 0 ) ) ) // Allow escaping
        {
            int j = i-1;
            if( esc )
                esc[0] = '\0';
            split[j] = realloc( split[j], strlen( split[j] ) + strlen( sep ) + strlen( tok ) + 1 );
            assert( split[j] );
            strcat( split[j], sep );
            strcat( split[j], tok );
            esc = NULL;
        }
        else
            assert( ( split[i++] = strdup( tok ) ) );
        tok = nexttok;
    } while ( tok );
    free( str );
    assert( !split[i] );

    return split;
}

void free_string_array( char **array )
{
    for( int i = 0; array[i] != NULL; i++ )
        free( array[i] );
    free( array );
}

char **split_options( const char *opt_str, char *options[] )
{
    char *opt_str_dup = strdup( opt_str );
    char **split = split_string( opt_str_dup, ":", 0 );
    free( opt_str_dup );
    int split_count = 0;
    while( split[split_count] != NULL )
        ++split_count;

    int options_count = 0;
    while( options[options_count] != NULL )
        ++options_count;

    char **opts = calloc( split_count * 2 + 2, sizeof( char ** ) );
    int opt = 0;
    int found_named = 0;
    int i, invalid = 0;
    for( i = 0; split[i] != NULL; i++, invalid = 0 )
    {
        char **arg = split_string( split[i], "=", 2 );
        if( arg == NULL )
        {
            if( found_named )
                invalid = 1;
            else if( i > options_count || options[i] == NULL )
                fprintf( stderr, "options [warning]: Too many options given\n" );
            else
            {
                opts[opt++] = strdup( options[i] );
                opts[opt++] = strdup( "" );
            }
        }
        else if( arg[0] == NULL || arg[1] == NULL )
        {
            if( found_named )
                invalid = 1;
            else if( i > options_count || options[i] == NULL )
                fprintf( stderr, "options [warning]: Too many options given\n" );
            else
            {
                opts[opt++] = strdup( options[i] );
                if( arg[0] )
                    opts[opt++] = strdup( arg[0] );
                else
                    opts[opt++] = strdup( "" );
            }
        }
        else
        {
            found_named = 1;
            int j = 0;
            while( options[j] != NULL && !strcmp( arg[0], options[j] ) )
                ++j;
            if( options[j] == NULL )
                fprintf( stderr, "options [warning]: Invalid option '%s'\n", arg[0] );
            else
            {
                opts[opt++] = strdup( arg[0] );
                opts[opt++] = strdup( arg[1] );
            }
        }
        if( invalid )
            fprintf( stderr, "options [warning]: Ordered option given after named, ignoring\n" );
        free_string_array( arg );
    }
    free_string_array( split );
    return opts;
}

char *get_option( const char *name, char **split_options )
{
    int last_i = -1;
    for( int i = 0; split_options[i] != NULL; i += 2 )
        if( !strcmp( split_options[i], name ) )
            last_i = i;
    if( last_i >= 0 )
        return split_options[last_i+1][0] ? split_options[last_i+1] : NULL;
    return NULL;
}
