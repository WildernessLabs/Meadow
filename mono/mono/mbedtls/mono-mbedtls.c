#include <string.h>
#include "eglib/glib.h"
#include "metadata/fdhandle.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"

typedef struct {
    intptr_t read_buf;
    intptr_t write_buf;
    mbedtls_ssl_context * mbedtls_ctx;
    mbedtls_net_context * mbedtls_fd;
} MonoMbedTlsContext;


intptr_t mono_mbedtls_init (intptr_t mono_fd, intptr_t readbuf, intptr_t writebuf);
int mono_mbedtls_read (MonoMbedTlsContext * ctx, int length);
int mono_mbedtls_write (MonoMbedTlsContext * ctx, int length);
void mono_mbedtls_close (MonoMbedTlsContext * ctx);

static void my_debug( void *ctx, int level, const char *file, int line, const char *str )
{
    ((void) level);
    fprintf( (FILE *) ctx, "%s:%04d: %s", file, line, str );
    fflush( (FILE *) ctx );
}

typedef struct {
	MonoFDHandle fdhandle;
	gint domain;
	gint type;
	gint protocol;
	gint saved_error;
	gint still_readable;
} SocketHandle;

mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_config conf;

intptr_t mono_mbedtls_init (intptr_t mono_fd, intptr_t readbuf, intptr_t writebuf)
{
    mbedtls_net_context *server_fd = NULL;
    mbedtls_ssl_context *ssl = NULL;

    SocketHandle *sockethandle;
    if (!mono_fdhandle_lookup_and_ref (mono_fd, (MonoFDHandle**) &sockethandle)) {
        printf ("Socket FD not found!\n");
        return NULL;
    }

    server_fd = g_malloc (sizeof(mbedtls_net_context));
    mbedtls_net_init( server_fd );
    server_fd->fd = sockethandle->fdhandle.fd;

    ssl = g_malloc (sizeof(mbedtls_ssl_context));
    mbedtls_ssl_init( ssl );
    mbedtls_ssl_config_init( &conf );
    mbedtls_debug_set_threshold(0);
    int ret;

    if( ( ret = mbedtls_ssl_config_defaults( &conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
    {
        printf (" failed\n ! mbedtls_ssl_config_defaults returned %d\n\n", ret );
        goto error;
    }

    mbedtls_ssl_conf_authmode( &conf, MBEDTLS_SSL_VERIFY_NONE );

    //debug
    mbedtls_ctr_drbg_init( &ctr_drbg );
    mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );
    // mbedtls_ssl_conf_dbg( &conf, my_debug, stdout );
    // // mbedtls_x509_crt_init( &cacert );
    mbedtls_entropy_init( &entropy );

    const char * pers = "meadow_sslserver";
    if( ( ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
                            (const unsigned char *) pers,
                            strlen( pers ) ) ) != 0 )
    {
        printf( " failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret );
        goto error;
    }

    //SSL Connection
    ret = mbedtls_ssl_setup (ssl, &conf);
    if( ( ret = mbedtls_ssl_set_hostname( ssl, "meadow" ) ) != 0 ) {
        printf( " failed\n ! mbedtls_ssl_set_hostname returned %d\n\n", ret );
        goto error;
    }

    mbedtls_ssl_set_bio( ssl, server_fd, mbedtls_net_send, mbedtls_net_recv, NULL );

    ret = mbedtls_ssl_handshake (ssl);

    if (ret < 0)
        goto error;

    MonoMbedTlsContext *new_ctx = g_malloc (sizeof(MonoMbedTlsContext));
    new_ctx->read_buf = readbuf;
    new_ctx->write_buf = writebuf;
    new_ctx->mbedtls_ctx = ssl;
    new_ctx->mbedtls_fd = server_fd;
    return new_ctx;

error:
    mbedtls_ssl_free (ssl);
    g_free (ssl);
    mbedtls_net_free (server_fd);
    g_free (server_fd);
    return NULL;
}


int mono_mbedtls_read (MonoMbedTlsContext * ctx, int length)
{
    int ret = mbedtls_ssl_read(ctx->mbedtls_ctx, ctx->read_buf, length);
    return ret;
}

int mono_mbedtls_write (MonoMbedTlsContext * ctx, int length)
{
    int ret = mbedtls_ssl_write(ctx->mbedtls_ctx, ctx->write_buf, length);
    return ret;
}

void mono_mbedtls_close (MonoMbedTlsContext * ctx)
{
    mbedtls_ssl_free (ctx->mbedtls_ctx);
    g_free (ctx->mbedtls_ctx);
    mbedtls_net_free (ctx->mbedtls_fd);
    g_free (ctx->mbedtls_fd);
    g_free (ctx);
    return;
}