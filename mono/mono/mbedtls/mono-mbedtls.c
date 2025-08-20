#include <string.h>
#include <stdbool.h>
#include "eglib/glib.h"
#include "metadata/fdhandle.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#if defined(__NuttX__)
#include "meadow/meadow_client_cert.h"
#endif

#define INVALID_SERVER_CERT_VALIDATION_MODE  1
#define MBEDTLS_HAS_ALREADY_STARTED          2

typedef struct {
    intptr_t read_buf;
    intptr_t write_buf;
    mbedtls_ssl_context * mbedtls_ctx;
    mbedtls_net_context * mbedtls_fd;
} MonoMbedTlsContext;

static gboolean mono_mbedtls_initialized = FALSE;

// Client certificate credentials
static unsigned char *client_cert_retrieved;
static unsigned char *private_key_retrieved;
static unsigned char *private_key_pass_retrieved;
static int client_cert_retrieved_len;
static int private_key_retrieved_len;
static int private_key_pass_retrieved_len;
static mbedtls_pk_context *pkey = NULL;
static mbedtls_x509_crt *clicert = NULL;
static int server_cert_authmode = MBEDTLS_SSL_VERIFY_REQUIRED;

int mono_mbedtls_init (void);
intptr_t mono_mbedtls_connect (intptr_t mono_fd, intptr_t readbuf, intptr_t writebuf, char * hostname);
int mono_mbedtls_read (MonoMbedTlsContext * ctx, int length);
int mono_mbedtls_write (MonoMbedTlsContext * ctx, int length);
void mono_mbedtls_close (MonoMbedTlsContext * ctx);
int mono_mbedtls_handshake (MonoMbedTlsContext *ctx);
int mono_mbedtls_set_server_cert_authmode (int authmode);

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
mbedtls_x509_crt cacert;

#include "root-ca.h"

int root_ca_pems_len = sizeof(root_ca_pems);
int is_clear = 0;

#define DEV_URANDOM_THRESHOLD       32
#define DEV_RANDOM_THRESHOLD        32
#define FAILED                      -1

#define NO_DEBUG            0
#define DEBUG_ERROR         1
#define DEBUG_STATE_CHANGE  2
#define DEBUG_INFO          3
#define DEBUG_VERBOSE       4

#define DEBUG_THRESHOLD     NO_DEBUG

#if DEBUG_THRESHOLD > NO_DEBUG
    #define MBEDTLS_PRINTF(...) do { printf(__VA_ARGS__); } while (0)
#else
    #define MBEDTLS_PRINTF(...)
#endif

// copied from mbedtls/programs/pkey/gen_key.c
static int dev_random_entropy_poll( void *data, unsigned char *output,
                             size_t len, size_t *olen )
{
    FILE *file;
    size_t ret, left = len;
    unsigned char *p = output;
    ((void) data);

    *olen = 0;

    file = fopen( "/dev/random", "rb" );
    if( file == NULL )
        return( MBEDTLS_ERR_ENTROPY_SOURCE_FAILED );

    while( left > 0 )
    {
        /* /dev/random can return much less than requested. If so, try again */
        ret = fread( p, 1, left, file );
        if( ret == 0 && ferror( file ) )
        {
            fclose( file );
            return( MBEDTLS_ERR_ENTROPY_SOURCE_FAILED );
        }

        p += ret;
        left -= ret;
        sleep( 1 );
    }
    fclose( file );
    *olen = len;

    return( 0 );
}

int mono_mbedtls_init (void)
{
    mono_mbedtls_initialized = TRUE;

    int ret;
    mbedtls_ssl_config_init( &conf );
    mbedtls_debug_set_threshold(DEBUG_THRESHOLD);

    // Retrieving credentials used on client certificate TLS authentication
#if defined(__NuttX__)
    meadow_client_cert_retrieve_credentials((const char**) &client_cert_retrieved, &client_cert_retrieved_len, (const char**) &private_key_retrieved, &private_key_retrieved_len, (const char**) &private_key_pass_retrieved, &private_key_pass_retrieved_len);
#endif

    // Load client private key
    if ( private_key_retrieved_len > 1 ) {

        pkey = g_malloc (sizeof(mbedtls_pk_context));
        mbedtls_pk_init( pkey );

        // Handle empty private key passphrase file case
        if ( private_key_pass_retrieved_len == 1 ) {
            private_key_pass_retrieved = NULL;
        } 

        if ( ( ret = mbedtls_pk_parse_key( pkey, private_key_retrieved, private_key_retrieved_len, private_key_pass_retrieved, private_key_pass_retrieved_len - 1, mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 ) {
            MBEDTLS_PRINTF( " failed to parse private key %d\n\n", ret );
            goto error;
        }
    }

    // Load client certificate
    if ( client_cert_retrieved_len > 1 ) {

        clicert = g_malloc (sizeof(mbedtls_x509_crt));
        mbedtls_x509_crt_init( clicert );

        if ( ( ret = mbedtls_x509_crt_parse( clicert, client_cert_retrieved, client_cert_retrieved_len ) ) != 0 ) {
            MBEDTLS_PRINTF( " failed to parse client certificate %d\n\n", ret);
            goto error;
        }
    }

#if defined(__NuttX__)
    meadow_client_cert_release_credentials((char ** const) &client_cert_retrieved, (char ** const) &private_key_retrieved, (char ** const) &private_key_pass_retrieved);
#endif

    if( ( ret = mbedtls_ssl_config_defaults( &conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
    {
        MBEDTLS_PRINTF (" failed\n ! mbedtls_ssl_config_defaults returned %d\n\n", ret );
        goto error;
    }

    mbedtls_ssl_conf_authmode ( &conf, server_cert_authmode );

    //debug
    mbedtls_ctr_drbg_init( &ctr_drbg );
    mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );
    mbedtls_ssl_conf_dbg( &conf, my_debug, stdout );
    mbedtls_x509_crt_init( &cacert );
    mbedtls_entropy_init( &entropy );

    if ((ret = mbedtls_entropy_add_source(&entropy, dev_random_entropy_poll,
                                          NULL, DEV_RANDOM_THRESHOLD,
                                          MBEDTLS_ENTROPY_SOURCE_STRONG)) != 0)
    {
        MBEDTLS_PRINTF(" failed\n  ! adding /dev/random entropy returned -0x%04x\n", (unsigned int)-ret);
        goto error;
    }

    if ( ( ret = mbedtls_x509_crt_parse (&cacert, root_ca_pems, root_ca_pems_len) ) != 0)
    {
        MBEDTLS_PRINTF ("mono-mbedtls: failed parsing the root CA PEMs\n");
        goto error;
    }
    mbedtls_ssl_conf_ca_chain( &conf, &cacert, NULL );

    const char * pers = "meadow_sslserver";
    if( ( ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
                            (const unsigned char *) pers,
                            strlen( pers ) ) ) != 0 )
    {
        MBEDTLS_PRINTF( " failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret );
        goto error;
    }
    return 0;

    error:
        if (pkey) {
            mbedtls_pk_free (pkey);
            g_free (pkey);
        }
        if (clicert) {
            mbedtls_x509_crt_free (clicert);
            g_free (clicert);
        }
#if defined(__NuttX__)
        meadow_client_cert_release_credentials((char ** const) &client_cert_retrieved, (char ** const) &private_key_retrieved, (char ** const) &private_key_pass_retrieved);
#endif
        return ret;
}

intptr_t mono_mbedtls_connect (intptr_t mono_fd, intptr_t readbuf, intptr_t writebuf, char* hostname)
{
    mbedtls_net_context *server_fd = NULL;
    mbedtls_ssl_context *ssl = NULL;

    server_fd = g_malloc (sizeof(mbedtls_net_context));
    if (server_fd == NULL)
    {
        MBEDTLS_PRINTF ("Failed to allocate socket descriptor\n");
        goto error;
    }

    // Wrapper of the socket descriptor
    mbedtls_net_init( server_fd );
    server_fd->fd = mono_fd;

    ssl = g_malloc (sizeof(mbedtls_ssl_context));
    if (ssl == NULL)
    {
        MBEDTLS_PRINTF ("Failed to allocate ssl context\n");
        goto error;
    }

    // TLS conext which represent the session
    mbedtls_ssl_init( ssl );

    int ret;

    //Assing the TLS config to the TLS context 
    if (( ret = mbedtls_ssl_setup (ssl, &conf) ) != 0)
    {
        MBEDTLS_PRINTF( "mbedtls_ssl_setup returned -0x%x\n", -ret );
        goto error;
    }

    // Set hostname for verification
    if (( ret = mbedtls_ssl_set_hostname( ssl, hostname ) ) != 0 ) 
    {
        MBEDTLS_PRINTF( "mbedtls_ssl_set_hostname returned -0x%x\n", -ret );
        goto error;
    }

    if ( clicert != NULL && pkey != NULL ) 
    {
        // Configure SSL context with client certificate and private key
        if ( ( ret = mbedtls_ssl_conf_own_cert( &conf, clicert, pkey ) ) != 0 ) 
        {
            MBEDTLS_PRINTF( " failed to configure client certificate and private key %d\n\n", ret );
            goto error;
        } 
    }

    // Link the socket wrapper to the TLS session structure
    mbedtls_ssl_set_bio( ssl, server_fd, mbedtls_net_send, mbedtls_net_recv, NULL );

    MonoMbedTlsContext *new_ctx = g_malloc (sizeof(MonoMbedTlsContext));
    if (new_ctx == NULL)
    {
        MBEDTLS_PRINTF("MonoMbedTlsConext failed to be allocated\n");
        goto error;
    }

    new_ctx->read_buf = readbuf;
    new_ctx->write_buf = writebuf;
    new_ctx->mbedtls_ctx = ssl;
    new_ctx->mbedtls_fd = server_fd;
    return (intptr_t) new_ctx;

error:
    MBEDTLS_PRINTF(" Failed to Connected \n");

    if (ssl) {
        mbedtls_ssl_free (ssl);
        g_free (ssl);
    }
    if (server_fd) {
        g_free (server_fd);
    }
    return (intptr_t) NULL;
}

int mono_mbedtls_handshake(MonoMbedTlsContext *ctx)
{
    int ret = FAILED;

    if (ctx->mbedtls_ctx != NULL)
    {
        // Perform the TLS handshake
        ret = mbedtls_ssl_handshake(ctx->mbedtls_ctx);
    }
    return ret;
}

int mono_mbedtls_read (MonoMbedTlsContext * ctx, int length)
{
    int ret = FAILED;

    if (ctx->mbedtls_ctx != NULL && ctx->read_buf != 0)
    {
        char *buffer = (char *)ctx->read_buf;
        ret = mbedtls_ssl_read(ctx->mbedtls_ctx, (unsigned char*)buffer, length);
    }
    return ret;
}

int mono_mbedtls_write (MonoMbedTlsContext * ctx, int length)
{
    int written = 0;
    int frags = 0;
    int ret;

    do
    {
        while( ( ret = mbedtls_ssl_write(ctx->mbedtls_ctx, (const unsigned char *) (ctx->write_buf + written), length - written)) < 0 )
        {
            if( ret != MBEDTLS_ERR_SSL_WANT_READ &&
                ret != MBEDTLS_ERR_SSL_WANT_WRITE)
                goto exit;
        }
        frags++;
        written += ret;
    }
    while( written < length );
exit:
    return ret;
}

void mono_mbedtls_close (MonoMbedTlsContext * ctx)
{
    int ret = FAILED;

    if (ctx != NULL)
    {
        /* Close the connection by sending "close notify" message to the server. */
        if ((ret = mbedtls_ssl_close_notify(ctx->mbedtls_ctx) != 0))
        {
            MBEDTLS_PRINTF("mbedtls_ssl_close_notify returned: -0x%x\n", -ret);
        }

        if (ctx->mbedtls_ctx)
        {
            mbedtls_ssl_free (ctx->mbedtls_ctx);
            g_free (ctx->mbedtls_ctx);
        }

        if (ctx->mbedtls_fd)
        {
            g_free (ctx->mbedtls_fd);
        }
        g_free (ctx);
    }
    return;
}

int mono_mbedtls_set_server_cert_authmode (int authmode)
{
    // The server certificate validation mode cannot be changed after TLS initialization
    if (mono_mbedtls_initialized == TRUE)
    {
        server_cert_authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
        return -MBEDTLS_HAS_ALREADY_STARTED;
    }

    if ( authmode == MBEDTLS_SSL_VERIFY_REQUIRED ||
        authmode == MBEDTLS_SSL_VERIFY_OPTIONAL ||
        authmode == MBEDTLS_SSL_VERIFY_NONE )
    {
        server_cert_authmode = authmode;
    }
    else
    {
        server_cert_authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
        return -INVALID_SERVER_CERT_VALIDATION_MODE;
    }

    return server_cert_authmode;
}