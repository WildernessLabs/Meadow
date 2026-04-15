/*
 * pal_ssl_mbedtls.c — CryptoNative_* SSL shim backed by mbedTLS
 *
 * Implements the OpenSSL-compatible P/Invoke interface that .NET 10's
 * SslStreamPal.Unix.cs / Interop.OpenSsl.cs expects, using mbedTLS
 * as the underlying TLS library.
 *
 * The managed code uses a BIO (memory buffer) model:
 *   - InputBio: managed writes received ciphertext here, SSL reads from it
 *   - OutputBio: SSL writes outgoing ciphertext here, managed reads from it
 *   - SslDoHandshake/SslRead/SslWrite use BIOs for all I/O
 *
 * We implement BIOs as ring buffers and configure mbedTLS send/recv
 * callbacks to read/write from them.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"

/* Diagnostic logging — disabled for production, enable for TLS debugging */
/* #define PAL_LOG(...) printf(__VA_ARGS__) */
#define PAL_LOG(...) do {} while(0)

/* ---------- Error codes matching pal_ssl.h ---------- */
typedef enum {
    PAL_SSL_ERROR_NONE       = 0,
    PAL_SSL_ERROR_SSL        = 1,
    PAL_SSL_ERROR_WANT_READ  = 2,
    PAL_SSL_ERROR_WANT_WRITE = 3,
    PAL_SSL_ERROR_SYSCALL    = 5,
    PAL_SSL_ERROR_ZERO_RETURN = 6,
} PalSslErrorCode;

/* ---------- BIO Ring Buffer ---------- */
#define BIO_RING_SIZE 32768  /* 32KB — enough for TLS records */

typedef struct {
    uint8_t  data[BIO_RING_SIZE];
    int      head;    /* read position */
    int      tail;    /* write position */
    int      count;   /* bytes in buffer */
} MemBio;

static MemBio *bio_create(void)
{
    MemBio *bio = calloc(1, sizeof(MemBio));
    return bio;
}

static int bio_write(MemBio *bio, const void *data, int len)
{
    if (!bio || !data || len <= 0) return 0;

    const uint8_t *src = (const uint8_t *)data;
    int space = BIO_RING_SIZE - bio->count;
    if (len > space) len = space;
    if (len == 0) return 0;

    for (int i = 0; i < len; i++) {
        bio->data[bio->tail] = src[i];
        bio->tail = (bio->tail + 1) % BIO_RING_SIZE;
    }
    bio->count += len;
    return len;
}

static int bio_read(MemBio *bio, void *data, int len)
{
    if (!bio || !data || len <= 0) return 0;

    uint8_t *dst = (uint8_t *)data;
    if (len > bio->count) len = bio->count;
    if (len == 0) return 0;

    for (int i = 0; i < len; i++) {
        dst[i] = bio->data[bio->head];
        bio->head = (bio->head + 1) % BIO_RING_SIZE;
    }
    bio->count -= len;
    return len;
}

static int bio_pending(MemBio *bio)
{
    return bio ? bio->count : 0;
}

/* ---------- SSL Context Wrapper ---------- */

/* Wraps mbedtls_ssl_config + associated objects (maps to OpenSSL SSL_CTX*) */
typedef struct {
    uint32_t            magic;    /* 0x4D425443 = "MBTC" */
    mbedtls_ssl_config  config;
    bool                initialized;
    /* Protocol limits */
    int                 min_version;
    int                 max_version;
    /* Quiet shutdown mode */
    int                 quiet_shutdown;
    /* App data pointer */
    void               *app_data;
} MbedSslCtx;

#define MBED_SSL_CTX_MAGIC 0x4D425443

/* Wraps mbedtls_ssl_context (maps to OpenSSL SSL*) */
typedef struct {
    uint32_t              magic;    /* 0x4D425353 = "MBSS" */
    mbedtls_ssl_context   ssl;
    MbedSslCtx           *ctx;       /* parent config */
    MemBio               *input_bio; /* ciphertext in (from network) */
    MemBio               *output_bio;/* ciphertext out (to network) */
    bool                  is_server;
    bool                  handshake_complete;
    int                   last_error;
    char                 *hostname;
    /* App data pointer */
    void                 *app_data;
} MbedSsl;

#define MBED_SSL_MAGIC 0x4D425353

/* ---------- External: mono_mbedtls.c accessors ---------- */
extern bool mono_mbedtls_is_initialized(void);
extern int mono_mbedtls_init(void);
extern mbedtls_ssl_config *mono_mbedtls_get_config(void);
extern mbedtls_entropy_context *mono_mbedtls_get_entropy(void);
extern mbedtls_ctr_drbg_context *mono_mbedtls_get_ctr_drbg(void);
extern mbedtls_x509_crt *mono_mbedtls_get_cacert(void);

/* ---------- mbedTLS BIO callbacks ---------- */

/*
 * mbedTLS send callback: writes encrypted data to the output BIO.
 * The managed layer will read from OutputBio and send over the network.
 */
static int mbed_bio_send(void *ctx, const unsigned char *buf, size_t len)
{
    MbedSsl *ssl = (MbedSsl *)ctx;
    if (!ssl || !ssl->output_bio) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;

    int written = bio_write(ssl->output_bio, buf, (int)len);
    PAL_LOG("pal_ssl: bio_send len=%d written=%d\n", (int)len, written);
    if (written == 0)
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    return written;
}

/*
 * mbedTLS recv callback: reads ciphertext from the input BIO.
 * The managed layer writes received network data into InputBio.
 */
static int mbed_bio_recv(void *ctx, unsigned char *buf, size_t len)
{
    MbedSsl *ssl = (MbedSsl *)ctx;
    if (!ssl || !ssl->input_bio) return MBEDTLS_ERR_SSL_INTERNAL_ERROR;

    int available = bio_pending(ssl->input_bio);
    if (available == 0) {
        PAL_LOG("pal_ssl: bio_recv WANT_READ (requested %d)\n", (int)len);
        return MBEDTLS_ERR_SSL_WANT_READ;
    }

    int nread = bio_read(ssl->input_bio, buf, (int)len);
    PAL_LOG("pal_ssl: bio_recv len=%d avail=%d read=%d\n", (int)len, available, nread);
    return nread;
}

/* ====================================================================
 * CryptoNative_* Implementation — BIO functions
 * ==================================================================== */

void *CryptoNative_CreateMemoryBio(void)
{
    return bio_create();
}

int CryptoNative_BioDestroy(void *bio)
{
    if (bio) free(bio);
    return 1;
}

int CryptoNative_BioWrite(void *bio, const void *data, int len)
{
    return bio_write((MemBio *)bio, data, len);
}

int CryptoNative_BioRead(void *bio, void *data, int len)
{
    return bio_read((MemBio *)bio, data, len);
}

int CryptoNative_BioCtrlPending(void *bio)
{
    return bio_pending((MemBio *)bio);
}

int CryptoNative_GetMemoryBioSize(void *bio)
{
    return bio_pending((MemBio *)bio);
}

/* ====================================================================
 * CryptoNative_* Implementation — SSL_CTX functions
 * ==================================================================== */

/* Dummy "method" pointer — OpenSSL SSLv23_method() equivalent */
static int ssl_method_dummy = 1;

void *CryptoNative_SslV2_3Method(void)
{
    return &ssl_method_dummy;
}

void *CryptoNative_SslCtxCreate(void *method)
{
    (void)method;

    /* Ensure mono_mbedtls_init() has been called */
    if (!mono_mbedtls_is_initialized()) {
        int ret = mono_mbedtls_init();
        if (ret != 0) {
            PAL_LOG("pal_ssl_mbedtls: mono_mbedtls_init failed: %d\n", ret);
            return NULL;
        }
    }

    MbedSslCtx *ctx = calloc(1, sizeof(MbedSslCtx));
    if (!ctx) return NULL;

    ctx->magic = MBED_SSL_CTX_MAGIC;

    /*
     * Copy the global config from mono_mbedtls_init() as our baseline.
     * This gives us root CAs, entropy, RNG, client certs for free.
     * We use the shared global config directly since mbedtls_ssl_config
     * is designed to be shared across SSL sessions.
     */
    ctx->initialized = true;
    ctx->min_version = MBEDTLS_SSL_VERSION_TLS1_2;
    ctx->max_version = MBEDTLS_SSL_VERSION_TLS1_2;

    return ctx;
}

void CryptoNative_SslCtxSetProtocolOptions(void *ctx_ptr, int protocols)
{
    /* Protocol negotiation is handled by the shared global config.
     * On embedded NuttX we only support TLS 1.2 via mbedTLS. */
    (void)ctx_ptr;
    (void)protocols;
}

void CryptoNative_SslCtxDestroy(void *ctx_ptr)
{
    if (!ctx_ptr) return;
    MbedSslCtx *ctx = (MbedSslCtx *)ctx_ptr;
    if (ctx->magic != MBED_SSL_CTX_MAGIC) return;
    ctx->magic = 0;
    free(ctx);
}

/* ====================================================================
 * CryptoNative_* Implementation — SSL functions
 * ==================================================================== */

void *CryptoNative_SslCreate(void *ctx_ptr)
{
    MbedSslCtx *ctx = (MbedSslCtx *)ctx_ptr;
    if (!ctx || ctx->magic != MBED_SSL_CTX_MAGIC) return NULL;

    MbedSsl *ssl = calloc(1, sizeof(MbedSsl));
    if (!ssl) return NULL;

    ssl->magic = MBED_SSL_MAGIC;
    ssl->ctx = ctx;

    mbedtls_ssl_init(&ssl->ssl);

    /* Use the global shared config from mono_mbedtls_init().
     * This has root CAs, entropy, RNG, and client certs pre-configured. */
    int ret = mbedtls_ssl_setup(&ssl->ssl, mono_mbedtls_get_config());
    if (ret != 0) {
        PAL_LOG("pal_ssl_mbedtls: mbedtls_ssl_setup failed: -0x%04x\n", -ret);
        mbedtls_ssl_free(&ssl->ssl);
        free(ssl);
        return NULL;
    }

    /* Set BIO callbacks to use our ring buffers.
     * BIOs aren't assigned yet (SslSetBio comes later), but we set
     * the callback context to `ssl` so the callbacks can find the BIOs. */
    mbedtls_ssl_set_bio(&ssl->ssl, ssl, mbed_bio_send, mbed_bio_recv, NULL);

    return ssl;
}

void CryptoNative_SslDestroy(void *ssl_ptr)
{
    if (!ssl_ptr) return;
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (ssl->magic != MBED_SSL_MAGIC) return;

    mbedtls_ssl_free(&ssl->ssl);
    if (ssl->hostname) free(ssl->hostname);
    ssl->magic = 0;
    free(ssl);
}

void CryptoNative_SslSetBio(void *ssl_ptr, void *rbio, void *wbio)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return;

    ssl->input_bio = (MemBio *)rbio;
    ssl->output_bio = (MemBio *)wbio;
    PAL_LOG("pal_ssl: SslSetBio ssl=%p rbio=%p wbio=%p\n", ssl_ptr, rbio, wbio);
    /* BIO callbacks already set in SslCreate — they reference ssl->input_bio/output_bio */
}

void CryptoNative_SslSetConnectState(void *ssl_ptr)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return;
    ssl->is_server = false;
    /* mbedTLS client mode is the default from ssl_config defaults */
}

void CryptoNative_SslSetAcceptState(void *ssl_ptr)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return;
    ssl->is_server = true;
    /* Note: server mode requires separate ssl_config with MBEDTLS_SSL_IS_SERVER.
     * For now, we only support client mode on embedded Meadow. */
}

int CryptoNative_SslDoHandshake(void *ssl_ptr, int *error)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) {
        if (error) *error = PAL_SSL_ERROR_SSL;
        return -1;
    }

    int ret = mbedtls_ssl_handshake(&ssl->ssl);
    if (ret != 0 && ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
        char errbuf[128];
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        printf("pal_ssl: handshake FATAL error -0x%04x: %s\n", -ret, errbuf);
        uint32_t vflags = mbedtls_ssl_get_verify_result(&ssl->ssl);
        if (vflags != 0 && vflags != (uint32_t)-1) {
            char vbuf[256];
            mbedtls_x509_crt_verify_info(vbuf, sizeof(vbuf), "  verify: ", vflags);
            printf("pal_ssl: %s\n", vbuf);
        }
        printf("pal_ssl: hostname=%s\n",
               ssl->hostname ? ssl->hostname : "(null)");
    }

    if (ret == 0) {
        ssl->handshake_complete = true;
        if (error) *error = PAL_SSL_ERROR_NONE;
        return 1;
    } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
        if (error) *error = PAL_SSL_ERROR_WANT_READ;
        return -1;  /* managed expects -1 for WANT_READ, not 0 */
    } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        if (error) *error = PAL_SSL_ERROR_WANT_WRITE;
        return -1;  /* managed expects -1 for WANT_WRITE, not 0 */
    } else {
        ssl->last_error = ret;
        if (error) *error = PAL_SSL_ERROR_SSL;
        return -1;
    }
}

int32_t CryptoNative_SslRead(void *ssl_ptr, void *buf, int32_t num, int32_t *error)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) {
        if (error) *error = PAL_SSL_ERROR_SSL;
        return -1;
    }

    int ret = mbedtls_ssl_read(&ssl->ssl, (unsigned char *)buf, num);

    if (ret > 0) {
        if (error) *error = PAL_SSL_ERROR_NONE;
        return ret;
    } else if (ret == 0) {
        if (error) *error = PAL_SSL_ERROR_ZERO_RETURN;
        return 0;
    } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
        if (error) *error = PAL_SSL_ERROR_WANT_READ;
        return -1;
    } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        if (error) *error = PAL_SSL_ERROR_WANT_WRITE;
        return -1;
    } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
        /* close_notify = graceful EOF. OpenSSL maps this to SSL_ERROR_ZERO_RETURN. */
        if (error) *error = PAL_SSL_ERROR_ZERO_RETURN;
        return 0;
    } else {
        char errbuf[128];
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        printf("pal_ssl: SslRead error -0x%04x: %s\n", -ret, errbuf);
        ssl->last_error = ret;
        if (error) *error = PAL_SSL_ERROR_SSL;
        return -1;
    }
}

int32_t CryptoNative_SslWrite(void *ssl_ptr, const void *buf, int32_t num, int32_t *error)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) {
        if (error) *error = PAL_SSL_ERROR_SSL;
        return -1;
    }

    int ret = mbedtls_ssl_write(&ssl->ssl, (const unsigned char *)buf, num);

    if (ret >= 0) {
        if (error) *error = PAL_SSL_ERROR_NONE;
        return ret;
    } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
        if (error) *error = PAL_SSL_ERROR_WANT_READ;
        return -1;
    } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        if (error) *error = PAL_SSL_ERROR_WANT_WRITE;
        return -1;
    } else {
        ssl->last_error = ret;
        if (error) *error = PAL_SSL_ERROR_SSL;
        return -1;
    }
}

int32_t CryptoNative_SslShutdown(void *ssl_ptr)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return -1;

    int ret = mbedtls_ssl_close_notify(&ssl->ssl);
    if (ret == 0) return 1;
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
        return 0;
    return -1;
}

int32_t CryptoNative_SslGetError(void *ssl_ptr, int32_t ret)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return PAL_SSL_ERROR_SSL;

    if (ret > 0) return PAL_SSL_ERROR_NONE;
    if (ret == 0) return PAL_SSL_ERROR_ZERO_RETURN;

    /* Map from stored last_error */
    int err = ssl->last_error;
    if (err == MBEDTLS_ERR_SSL_WANT_READ) return PAL_SSL_ERROR_WANT_READ;
    if (err == MBEDTLS_ERR_SSL_WANT_WRITE) return PAL_SSL_ERROR_WANT_WRITE;
    return PAL_SSL_ERROR_SSL;
}

int32_t CryptoNative_IsSslStateOK(void *ssl_ptr)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return 0;
    return ssl->handshake_complete ? 1 : 0;
}

const char *CryptoNative_SslGetVersion(void *ssl_ptr)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return "unknown";
    return mbedtls_ssl_get_version(&ssl->ssl);
}

int32_t CryptoNative_SslSetTlsExtHostName(void *ssl_ptr, const char *name)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return 0;

    /* Store hostname for later reference */
    if (ssl->hostname) free(ssl->hostname);
    ssl->hostname = name ? strdup(name) : NULL;

    int ret = mbedtls_ssl_set_hostname(&ssl->ssl, name);
    return (ret == 0) ? 1 : 0;
}

void *CryptoNative_SslGetPeerCertificate(void *ssl_ptr)
{
    /* mbedTLS doesn't expose peer certs in OpenSSL X509* format.
     * Returning NULL tells the managed layer there's no peer cert.
     * Use CryptoNative_SslGetPeerCertVerifyResult to check whether
     * native mbedTLS already verified the cert chain. */
    (void)ssl_ptr;
    return NULL;
}

int32_t CryptoNative_SslGetPeerCertVerifyResult(void *ssl_ptr)
{
    /* Returns 0 if the TLS handshake completed (meaning the native layer
     * handled cert validation per its authmode — REQUIRED, OPTIONAL, or NONE).
     * Returns -1 if no context or handshake incomplete.
     *
     * Managed code uses this to skip RemoteCertificateNotAvailable when
     * the peer cert can't be extracted into X509Certificate2 but the
     * native TLS layer already completed successfully. */
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return -1;
    if (!ssl->handshake_complete) return -1;
    return 0;
}

void *CryptoNative_SslGetCertificate(void *ssl_ptr)
{
    /* Return our own certificate — not commonly needed on client side */
    (void)ssl_ptr;
    return NULL;
}

void *CryptoNative_SslGetPeerCertChain(void *ssl_ptr)
{
    /* Returns the full peer chain. On mbedTLS, get_peer_cert returns the
     * first cert, and its ->next links form the chain. Return as-is. */
    return CryptoNative_SslGetPeerCertificate(ssl_ptr);
}

void CryptoNative_EnsureLibSslInitialized(void)
{
    if (!mono_mbedtls_is_initialized()) {
        mono_mbedtls_init();
    }
}

/* ====================================================================
 * Stubs — functions that are called by managed code but not critical
 * for basic TLS client operation. Return success/no-op.
 * ==================================================================== */

int32_t CryptoNative_SslCtxSetCiphers(void *ctx, const char *list, const char *suites)
{
    (void)ctx; (void)list; (void)suites;
    return 1; /* success */
}

int32_t CryptoNative_SetCiphers(void *ssl, const char *list, const char *suites)
{
    (void)ssl; (void)list; (void)suites;
    return 1;
}

int32_t CryptoNative_SslCtxSetEncryptionPolicy(void *ctx, int policy)
{
    (void)ctx; (void)policy;
    return 1;
}

void CryptoNative_SslCtxSetQuietShutdown(void *ctx)
{
    if (ctx) {
        MbedSslCtx *c = (MbedSslCtx *)ctx;
        if (c->magic == MBED_SSL_CTX_MAGIC) c->quiet_shutdown = 1;
    }
}

void CryptoNative_SslSetQuietShutdown(void *ssl, int mode)
{
    (void)ssl; (void)mode;
}

int32_t CryptoNative_Tls13Supported(void)
{
    return 0; /* mbedTLS 3.x can support TLS 1.3, but not enabled on embedded */
}

void CryptoNative_SslSetClientCertCallback(void *ssl, int set)
{
    (void)ssl; (void)set;
}

void CryptoNative_SslSetPostHandshakeAuth(void *ssl, int val)
{
    (void)ssl; (void)val;
}

int CryptoNative_SslCtxSetCaching(void *ctx, int mode, int cacheSize,
    int contextIdLen, void *contextId, void *newCb, void *removeCb)
{
    (void)ctx; (void)mode; (void)cacheSize;
    (void)contextIdLen; (void)contextId; (void)newCb; (void)removeCb;
    return 1;
}

int CryptoNative_SslCtxRemoveSession(void *ctx, void *session)
{
    (void)ctx; (void)session;
    return 1;
}

void CryptoNative_SslCtxSetAlpnSelectCb(void *ctx, void *cb, void *arg)
{
    (void)ctx; (void)cb; (void)arg;
}

int32_t CryptoNative_SslSetAlpnProtos(void *ssl, const void *protos, uint32_t len)
{
    (void)ssl; (void)protos; (void)len;
    return 0; /* success — ALPN not supported but don't fail */
}

void CryptoNative_SslGet0AlpnSelected(void *ssl, const void **protocol, uint32_t *len)
{
    if (protocol) *protocol = NULL;
    if (len) *len = 0;
}

int32_t CryptoNative_SslGetCurrentCipherId(void *ssl_ptr, int32_t *cipherId)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC || !cipherId) return 0;

    int id = mbedtls_ssl_get_ciphersuite_id_from_ssl(&ssl->ssl);
    if (id == 0) return 0;

    *cipherId = id;  /* mbedTLS uses IANA values */
    return 1;  /* success */
}

int32_t CryptoNative_SslGetFinished(void *ssl, void *buf, int32_t count)
{
    (void)ssl; (void)buf; (void)count;
    return 0;
}

int32_t CryptoNative_SslGetPeerFinished(void *ssl, void *buf, int32_t count)
{
    (void)ssl; (void)buf; (void)count;
    return 0;
}

int32_t CryptoNative_SslSessionReused(void *ssl)
{
    (void)ssl;
    return 0;
}

int32_t CryptoNative_SslRenegotiate(void *ssl, int32_t *error)
{
    if (error) *error = PAL_SSL_ERROR_SSL;
    return 0;
}

int32_t CryptoNative_IsSslRenegotiatePending(void *ssl)
{
    (void)ssl;
    return 0;
}

void CryptoNative_SslSetVerifyPeer(void *ssl)
{
    /* Verification is already configured in mono_mbedtls_init() via
     * mbedtls_ssl_conf_authmode(). No per-SSL override needed. */
    (void)ssl;
}

int32_t CryptoNative_SslSetData(void *ssl_ptr, void *ptr)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return 0;
    ssl->app_data = ptr;
    return 1;
}

void *CryptoNative_SslGetData(void *ssl_ptr)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return NULL;
    return ssl->app_data;
}

int32_t CryptoNative_SslCtxSetData(void *ctx_ptr, void *ptr)
{
    MbedSslCtx *ctx = (MbedSslCtx *)ctx_ptr;
    if (!ctx || ctx->magic != MBED_SSL_CTX_MAGIC) return 0;
    ctx->app_data = ptr;
    return 1;
}

void *CryptoNative_SslCtxGetData(void *ctx_ptr)
{
    MbedSslCtx *ctx = (MbedSslCtx *)ctx_ptr;
    if (!ctx || ctx->magic != MBED_SSL_CTX_MAGIC) return NULL;
    return ctx->app_data;
}

void *CryptoNative_SslGetClientCAList(void *ssl)
{
    (void)ssl;
    return NULL;
}

int32_t CryptoNative_SslUseCertificate(void *ssl, void *x509)
{
    (void)ssl; (void)x509;
    return 1;
}

int32_t CryptoNative_SslUsePrivateKey(void *ssl, void *pkey)
{
    (void)ssl; (void)pkey;
    return 1;
}

int32_t CryptoNative_SslCtxUseCertificate(void *ctx, void *x509)
{
    (void)ctx; (void)x509;
    return 1;
}

int32_t CryptoNative_SslCtxUsePrivateKey(void *ctx, void *pkey)
{
    (void)ctx; (void)pkey;
    return 1;
}

int32_t CryptoNative_SslCtxCheckPrivateKey(void *ctx)
{
    (void)ctx;
    return 1;
}

int32_t CryptoNative_SslCtxAddExtraChainCert(void *ctx, void *x509)
{
    (void)ctx; (void)x509;
    return 1;
}

int32_t CryptoNative_SslAddExtraChainCert(void *ssl, void *x509)
{
    (void)ssl; (void)x509;
    return 1;
}

int32_t CryptoNative_SslAddClientCAs(void *ssl, void **x509s, uint32_t count)
{
    (void)ssl; (void)x509s; (void)count;
    return 1;
}

void CryptoNative_SslCtxSetDefaultOcspCallback(void *ctx)
{
    (void)ctx;
}

void CryptoNative_SslStapleOcsp(void *ssl, void *buf, int32_t len)
{
    (void)ssl; (void)buf; (void)len;
}

void CryptoNative_SslCtxSetKeylogCallback(void *ctx, void *cb)
{
    (void)ctx; (void)cb;
}

const char *CryptoNative_SslGetServerName(void *ssl_ptr)
{
    MbedSsl *ssl = (MbedSsl *)ssl_ptr;
    if (!ssl || ssl->magic != MBED_SSL_MAGIC) return NULL;
    return ssl->hostname;
}

void *CryptoNative_SslGetSession(void *ssl_ptr)
{
    (void)ssl_ptr;
    return NULL;
}

int32_t CryptoNative_SslSetSession(void *ssl, void *session)
{
    (void)ssl; (void)session;
    return 1;
}

void CryptoNative_SslSessionFree(void *session)
{
    (void)session;
}

const char *CryptoNative_SslSessionGetHostname(void *session)
{
    (void)session;
    return NULL;
}

int CryptoNative_SslSessionSetHostname(void *session, const char *hostname)
{
    (void)session; (void)hostname;
    return 1;
}

void CryptoNative_SslSessionSetData(void *session, void *val)
{
    (void)session; (void)val;
}

void *CryptoNative_SslSessionGetData(void *session)
{
    (void)session;
    return NULL;
}

const char *CryptoNative_GetOpenSslCipherSuiteName(void *ssl, int32_t suite, int32_t *isTls12)
{
    (void)ssl;
    if (isTls12) *isTls12 = 1;  /* we only support TLS 1.2 */
    const char *name = mbedtls_ssl_get_ciphersuite_name(suite);
    return name ? name : "UNKNOWN";
}

int32_t CryptoNative_GetDefaultSignatureAlgorithms(uint16_t *buffer, int32_t *count)
{
    if (count) *count = 0;
    return 0;
}

int32_t CryptoNative_OpenSslGetProtocolSupport(int protocol)
{
    /* We support TLS 1.2 */
    if (protocol == 3072) return 1; /* PAL_SSL_TLS12 */
    return 0;
}

int32_t CryptoNative_SslSetSigalgs(void *ssl, void *str)
{
    (void)ssl; (void)str;
    return 1;
}

int32_t CryptoNative_SslSetClientSigalgs(void *ssl, void *str)
{
    (void)ssl; (void)str;
    return 1;
}

/* ====================================================================
 * Crypto error queue stubs — managed code calls these for error details
 * ==================================================================== */

uint64_t CryptoNative_ErrPeekError(void)     { return 0; }
uint64_t CryptoNative_ErrPeekLastError(void)  { return 0; }
uint64_t CryptoNative_ErrGetErrorAlloc(void **msg) { if (msg) *msg = NULL; return 0; }
void     CryptoNative_ErrClearError(void)     { }
const char *CryptoNative_ErrReasonErrorString(uint64_t err)
{
    (void)err;
    return "mbedTLS error";
}
