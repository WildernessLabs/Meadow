/*
 * mono_mbedtls.c — MbedTLS provider for Meadow/.NET 10
 *
 * Ported from legacy Mono 6.9 fork: mono/mono/mbedtls/mono-mbedtls.c
 * Stripped of eglib/glib and legacy Mono (MonoFDHandle) dependencies.
 * Core mbedTLS logic preserved unchanged.
 *
 * Provides two layers:
 *   1. mono_mbedtls_*() — direct mbedTLS session API (legacy Meadow interface)
 *   2. CryptoNative_Ssl*() — OpenSSL-compatible shim for .NET 10 SslStream PAL
 *      (implemented in pal_ssl_mbedtls.c)
 */

#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"

#if defined(__NuttX__)
#include <nuttx/config.h>
#include "meadow/meadow_client_cert.h"
#endif

#define INVALID_SERVER_CERT_VALIDATION_MODE  1
#define MBEDTLS_HAS_ALREADY_STARTED          2

typedef struct {
    intptr_t read_buf;
    intptr_t write_buf;
    mbedtls_ssl_context *mbedtls_ctx;
    mbedtls_net_context *mbedtls_fd;
} MonoMbedTlsContext;

static bool mono_mbedtls_initialized = false;

/* Client certificate credentials */
static unsigned char *client_cert_retrieved;
static unsigned char *private_key_retrieved;
static unsigned char *private_key_pass_retrieved;
static int client_cert_retrieved_len;
static int private_key_retrieved_len;
static int private_key_pass_retrieved_len;
static mbedtls_pk_context *pkey = NULL;
static mbedtls_x509_crt *clicert = NULL;
static int server_cert_authmode = MBEDTLS_SSL_VERIFY_REQUIRED;

/* Forward declarations */
int mono_mbedtls_init(void);
intptr_t mono_mbedtls_connect(intptr_t mono_fd, intptr_t readbuf, intptr_t writebuf, char *hostname);
int mono_mbedtls_read(MonoMbedTlsContext *ctx, int length);
int mono_mbedtls_write(MonoMbedTlsContext *ctx, int length);
void mono_mbedtls_close(MonoMbedTlsContext *ctx);
int mono_mbedtls_handshake(MonoMbedTlsContext *ctx);
int mono_mbedtls_set_server_cert_authmode(int authmode);

static void my_debug(void *ctx, int level, const char *file, int line, const char *str)
{
    ((void)level);
    fprintf((FILE *)ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *)ctx);
}

/* Global TLS configuration — shared across all connections */
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_ssl_config conf;
mbedtls_x509_crt cacert;

#include "root-ca-der.h"

#define DEV_URANDOM_THRESHOLD       32
#define DEV_RANDOM_THRESHOLD        32
#define FAILED                      -1

#define NO_DEBUG            0
#define DEBUG_ERROR         1
#define DEBUG_STATE_CHANGE  2
#define DEBUG_INFO          3
#define DEBUG_VERBOSE       4

#define DEBUG_THRESHOLD     DEBUG_ERROR

#if DEBUG_THRESHOLD > NO_DEBUG
    #define MBEDTLS_PRINTF(...) do { printf(__VA_ARGS__); } while (0)
#else
    #define MBEDTLS_PRINTF(...)
#endif

/* Entropy source using /dev/random (NuttX) */
static int dev_random_entropy_poll(void *data, unsigned char *output,
                             size_t len, size_t *olen)
{
    FILE *file;
    size_t ret, left = len;
    unsigned char *p = output;
    ((void)data);

    *olen = 0;

    file = fopen("/dev/random", "rb");
    if (file == NULL)
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;

    while (left > 0) {
        ret = fread(p, 1, left, file);
        if (ret == 0 && ferror(file)) {
            fclose(file);
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }

        p += ret;
        left -= ret;
        if (left > 0)
            usleep(10000);  /* 10ms — yield CPU while waiting for more entropy */
    }
    fclose(file);
    *olen = len;

    return 0;
}

/*
 * One-time TLS initialization: entropy, RNG, root CAs, client certs, config.
 * Called once at startup. Thread-unsafe (designed for single-init embedded use).
 */
/* Serializes init across callers: the shim's lazy path and the eager
 * boot-time warm-up thread (mono_main) may race here.
 */
static pthread_mutex_t g_init_mutex = PTHREAD_MUTEX_INITIALIZER;

int mono_mbedtls_init(void)
{
    int ret;

    pthread_mutex_lock(&g_init_mutex);
    if (mono_mbedtls_initialized) {
        pthread_mutex_unlock(&g_init_mutex);
        return 0;
    }

    mbedtls_ssl_config_init(&conf);
    mbedtls_debug_set_threshold(DEBUG_THRESHOLD);

    /* RNG must be usable BEFORE mbedtls_pk_parse_key below: key parsing
     * passes the DRBG for RSA blinding, and seeding it afterwards (as this
     * code previously did) hands an unseeded DRBG to the parser.
     */
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    if ((ret = mbedtls_entropy_add_source(&entropy, dev_random_entropy_poll,
                                          NULL, DEV_RANDOM_THRESHOLD,
                                          MBEDTLS_ENTROPY_SOURCE_STRONG)) != 0) {
        MBEDTLS_PRINTF(" failed\n  ! adding /dev/random entropy returned -0x%04x\n",
                       (unsigned int)-ret);
        goto error;
    }

    {
        const char *pers = "meadow_sslserver";
        if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                        (const unsigned char *)pers, strlen(pers))) != 0) {
            MBEDTLS_PRINTF(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret);
            goto error;
        }
    }

    /* Retrieve client certificate credentials from NuttX storage */
#if defined(__NuttX__)
    meadow_client_cert_retrieve_credentials(
        (const char **)&client_cert_retrieved, &client_cert_retrieved_len,
        (const char **)&private_key_retrieved, &private_key_retrieved_len,
        (const char **)&private_key_pass_retrieved, &private_key_pass_retrieved_len);
#endif

    /* Load client private key */
    if (private_key_retrieved_len > 1) {
        pkey = malloc(sizeof(mbedtls_pk_context));
        mbedtls_pk_init(pkey);

        /* Handle empty private key passphrase file case */
        if (private_key_pass_retrieved_len == 1) {
            private_key_pass_retrieved = NULL;
        }

        if ((ret = mbedtls_pk_parse_key(pkey, private_key_retrieved,
                private_key_retrieved_len, private_key_pass_retrieved,
                private_key_pass_retrieved_len - 1,
                mbedtls_ctr_drbg_random, &ctr_drbg)) != 0) {
            MBEDTLS_PRINTF(" failed to parse private key %d\n\n", ret);
            goto error;
        }
    }

    /* Load client certificate */
    if (client_cert_retrieved_len > 1) {
        clicert = malloc(sizeof(mbedtls_x509_crt));
        mbedtls_x509_crt_init(clicert);

        if ((ret = mbedtls_x509_crt_parse(clicert, client_cert_retrieved,
                client_cert_retrieved_len)) != 0) {
            MBEDTLS_PRINTF(" failed to parse client certificate %d\n\n", ret);
            goto error;
        }
    }

#if defined(__NuttX__)
    meadow_client_cert_release_credentials(
        (char **const)&client_cert_retrieved,
        (char **const)&private_key_retrieved,
        (char **const)&private_key_pass_retrieved);
#endif

    if ((ret = mbedtls_ssl_config_defaults(&conf,
            MBEDTLS_SSL_IS_CLIENT,
            MBEDTLS_SSL_TRANSPORT_STREAM,
            MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        MBEDTLS_PRINTF(" failed\n ! mbedtls_ssl_config_defaults returned %d\n\n", ret);
        goto error;
    }

    mbedtls_ssl_conf_authmode(&conf, server_cert_authmode);

    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    // mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);  // DISABLED — see HCOM blocking note below
    /* mbedTLS debug callback (my_debug above) uses fprintf+fflush which can block on
       a full HCOM stdout buffer mid-handshake, causing intermittent TLS hangs. Leave
       disabled unless actively debugging — and even then use a non-blocking path. */
    mbedtls_x509_crt_init(&cacert);

    /* Load embedded root CA certificates (Mozilla trust store, DER format) */
    for (size_t i = 0; i < ROOT_CA_DER_COUNT; ++i) {
        ret = mbedtls_x509_crt_parse_der_nocopy(&cacert, ROOT_CA_DER_LIST[i], ROOT_CA_DER_LEN[i]);
        if (ret != 0) {
            MBEDTLS_PRINTF("mono-mbedtls: failed parsing DER root CA %u, ret=%d\n",
                          (unsigned)i, ret);
            goto error;
        }
    }
    mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);

    mono_mbedtls_initialized = true;
    pthread_mutex_unlock(&g_init_mutex);
    printf("mono_mbedtls_init: OK (root CAs loaded, RNG seeded)\n");
    return 0;

error:
    printf("mono_mbedtls_init: FAILED ret=-0x%04x\n", (unsigned)-ret);
    if (pkey) {
        mbedtls_pk_free(pkey);
        free(pkey);
        pkey = NULL;
    }
    if (clicert) {
        mbedtls_x509_crt_free(clicert);
        free(clicert);
        clicert = NULL;
    }
#if defined(__NuttX__)
    meadow_client_cert_release_credentials(
        (char **const)&client_cert_retrieved,
        (char **const)&private_key_retrieved,
        (char **const)&private_key_pass_retrieved);
#endif
    pthread_mutex_unlock(&g_init_mutex);
    return ret;
}

/*
 * Create a TLS connection context for the given socket FD.
 * Returns opaque MonoMbedTlsContext* or NULL on failure.
 */
intptr_t mono_mbedtls_connect(intptr_t mono_fd, intptr_t readbuf, intptr_t writebuf, char *hostname)
{
    mbedtls_net_context *server_fd = NULL;
    mbedtls_ssl_context *ssl = NULL;

    server_fd = malloc(sizeof(mbedtls_net_context));
    if (server_fd == NULL) {
        MBEDTLS_PRINTF("Failed to allocate socket descriptor\n");
        goto error;
    }

    /* Wrapper of the socket descriptor */
    mbedtls_net_init(server_fd);
    server_fd->fd = mono_fd;

    /* Set socket to non-blocking mode */
    int flags = fcntl(server_fd->fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(server_fd->fd, F_SETFL, flags | O_NONBLOCK);
    }

    ssl = malloc(sizeof(mbedtls_ssl_context));
    if (ssl == NULL) {
        MBEDTLS_PRINTF("Failed to allocate ssl context\n");
        goto error;
    }

    /* TLS context which represents the session */
    mbedtls_ssl_init(ssl);

    int ret;

    /* Assign the TLS config to the TLS context */
    if ((ret = mbedtls_ssl_setup(ssl, &conf)) != 0) {
        MBEDTLS_PRINTF("mbedtls_ssl_setup returned -0x%x\n", -ret);
        goto error;
    }

    /* Set hostname for verification */
    if ((ret = mbedtls_ssl_set_hostname(ssl, hostname)) != 0) {
        MBEDTLS_PRINTF("mbedtls_ssl_set_hostname returned -0x%x\n", -ret);
        goto error;
    }

    if (clicert != NULL && pkey != NULL) {
        /* Configure SSL context with client certificate and private key */
        if ((ret = mbedtls_ssl_conf_own_cert(&conf, clicert, pkey)) != 0) {
            MBEDTLS_PRINTF(" failed to configure client certificate and private key %d\n\n", ret);
            goto error;
        }
    }

    /* Link the socket wrapper to the TLS session structure */
    mbedtls_ssl_set_bio(ssl, server_fd, mbedtls_net_send, NULL, mbedtls_net_recv_timeout);

    MonoMbedTlsContext *new_ctx = malloc(sizeof(MonoMbedTlsContext));
    if (new_ctx == NULL) {
        MBEDTLS_PRINTF("MonoMbedTlsContext failed to be allocated\n");
        goto error;
    }

    new_ctx->read_buf = readbuf;
    new_ctx->write_buf = writebuf;
    new_ctx->mbedtls_ctx = ssl;
    new_ctx->mbedtls_fd = server_fd;
    return (intptr_t)new_ctx;

error:
    MBEDTLS_PRINTF(" Failed to Connect\n");

    if (ssl) {
        mbedtls_ssl_free(ssl);
        free(ssl);
    }
    if (server_fd) {
        free(server_fd);
    }
    return (intptr_t)NULL;
}

int mono_mbedtls_handshake(MonoMbedTlsContext *ctx)
{
    int ret = FAILED;

    if (ctx->mbedtls_ctx != NULL) {
        /* Perform the TLS handshake */
        while ((ret = mbedtls_ssl_handshake(ctx->mbedtls_ctx)) != 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
                ret != MBEDTLS_ERR_SSL_WANT_WRITE &&
                ret != MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
                return ret;
            }
        }
    }
    return ret;
}

int mono_mbedtls_read(MonoMbedTlsContext *ctx, int length)
{
    int ret = FAILED;

    if (ctx->mbedtls_ctx != NULL && ctx->read_buf != 0) {
        char *buffer = (char *)ctx->read_buf;
        ret = mbedtls_ssl_read(ctx->mbedtls_ctx, (unsigned char *)buffer, length);
    }
    return ret;
}

int mono_mbedtls_write(MonoMbedTlsContext *ctx, int length)
{
    int written = 0;
    int frags = 0;
    int ret;

    do {
        while ((ret = mbedtls_ssl_write(ctx->mbedtls_ctx,
                (const unsigned char *)(ctx->write_buf + written),
                length - written)) < 0) {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
                ret != MBEDTLS_ERR_SSL_WANT_WRITE)
                goto exit;
        }
        frags++;
        written += ret;
    } while (written < length);

exit:
    return ret;
}

void mono_mbedtls_close(MonoMbedTlsContext *ctx)
{
    int ret = FAILED;

    if (ctx != NULL) {
        if ((ret = mbedtls_ssl_close_notify(ctx->mbedtls_ctx) != 0)) {
            MBEDTLS_PRINTF("mbedtls_ssl_close_notify returned: -0x%x\n", -ret);
        }

        if (ctx->mbedtls_ctx) {
            mbedtls_ssl_free(ctx->mbedtls_ctx);
            free(ctx->mbedtls_ctx);
        }

        if (ctx->mbedtls_fd) {
            free(ctx->mbedtls_fd);
        }
        free(ctx);
    }
}

int mono_mbedtls_set_server_cert_authmode(int authmode)
{
    /* The server certificate validation mode cannot be changed after TLS initialization */
    if (mono_mbedtls_initialized == true) {
        server_cert_authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
        return -MBEDTLS_HAS_ALREADY_STARTED;
    }

    if (authmode == MBEDTLS_SSL_VERIFY_REQUIRED ||
        authmode == MBEDTLS_SSL_VERIFY_OPTIONAL ||
        authmode == MBEDTLS_SSL_VERIFY_NONE) {
        server_cert_authmode = authmode;
    } else {
        server_cert_authmode = MBEDTLS_SSL_VERIFY_REQUIRED;
        return -INVALID_SERVER_CERT_VALIDATION_MODE;
    }

    return server_cert_authmode;
}

/* === Accessors for the CryptoNative shim layer (pal_ssl_mbedtls.c) === */

mbedtls_ssl_config *mono_mbedtls_get_config(void)
{
    return &conf;
}

mbedtls_entropy_context *mono_mbedtls_get_entropy(void)
{
    return &entropy;
}

mbedtls_ctr_drbg_context *mono_mbedtls_get_ctr_drbg(void)
{
    return &ctr_drbg;
}

mbedtls_x509_crt *mono_mbedtls_get_cacert(void)
{
    return &cacert;
}

bool mono_mbedtls_is_initialized(void)
{
    return mono_mbedtls_initialized;
}
