/*
 * mappings-crypto-native.h — P/Invoke mapping table for
 * System.Security.Cryptography.Native.OpenSsl backed by mbedTLS
 *
 * These functions are called by .NET 10's SslStreamPal.Unix.cs via
 * Interop.OpenSsl.cs and Interop.Ssl.cs P/Invoke declarations.
 */

/* === BIO functions === */
extern void *CryptoNative_CreateMemoryBio(void);
extern int   CryptoNative_BioDestroy(void *bio);
extern int   CryptoNative_BioWrite(void *bio, const void *data, int len);
extern int   CryptoNative_BioRead(void *bio, void *data, int len);
extern int   CryptoNative_BioCtrlPending(void *bio);
extern int   CryptoNative_GetMemoryBioSize(void *bio);

/* === SSL_CTX functions === */
extern void *CryptoNative_SslV2_3Method(void);
extern void *CryptoNative_SslCtxCreate(void *method);
extern void  CryptoNative_SslCtxSetProtocolOptions(void *ctx, int protocols);
extern void  CryptoNative_SslCtxDestroy(void *ctx);
extern int32_t CryptoNative_SslCtxSetCiphers(void *ctx, const char *list, const char *suites);
extern int32_t CryptoNative_SetCiphers(void *ssl, const char *list, const char *suites);
extern int32_t CryptoNative_SslCtxSetEncryptionPolicy(void *ctx, int policy);
extern void  CryptoNative_SslCtxSetQuietShutdown(void *ctx);
extern int32_t CryptoNative_SslCtxUseCertificate(void *ctx, void *x509);
extern int32_t CryptoNative_SslCtxUsePrivateKey(void *ctx, void *pkey);
extern int32_t CryptoNative_SslCtxCheckPrivateKey(void *ctx);
extern int32_t CryptoNative_SslCtxAddExtraChainCert(void *ctx, void *x509);
extern void  CryptoNative_SslCtxSetAlpnSelectCb(void *ctx, void *cb, void *arg);
extern int   CryptoNative_SslCtxSetCaching(void *ctx, int mode, int cacheSize, int contextIdLen, void *contextId, void *newCb, void *removeCb);
extern int   CryptoNative_SslCtxRemoveSession(void *ctx, void *session);
extern void  CryptoNative_SslCtxSetDefaultOcspCallback(void *ctx);
extern void  CryptoNative_SslCtxSetKeylogCallback(void *ctx, void *cb);
extern int32_t CryptoNative_SslCtxSetData(void *ctx, void *ptr);
extern void *CryptoNative_SslCtxGetData(void *ctx);

/* === SSL functions === */
extern void *CryptoNative_SslCreate(void *ctx);
extern void  CryptoNative_SslDestroy(void *ssl);
extern void  CryptoNative_SslSetBio(void *ssl, void *rbio, void *wbio);
extern void  CryptoNative_SslSetConnectState(void *ssl);
extern void  CryptoNative_SslSetAcceptState(void *ssl);
extern int32_t CryptoNative_SslDoHandshake(void *ssl, int32_t *error);
extern int32_t CryptoNative_SslRead(void *ssl, void *buf, int32_t num, int32_t *error);
extern int32_t CryptoNative_SslWrite(void *ssl, const void *buf, int32_t num, int32_t *error);
extern int32_t CryptoNative_SslShutdown(void *ssl);
extern int32_t CryptoNative_SslGetError(void *ssl, int32_t ret);
extern int32_t CryptoNative_IsSslStateOK(void *ssl);
extern const char *CryptoNative_SslGetVersion(void *ssl);
extern int32_t CryptoNative_SslSetTlsExtHostName(void *ssl, const char *name);
extern void *CryptoNative_SslGetPeerCertificate(void *ssl);
extern int32_t CryptoNative_SslGetPeerCertVerifyResult(void *ssl);
extern void *CryptoNative_SslGetCertificate(void *ssl);
extern void *CryptoNative_SslGetPeerCertChain(void *ssl);
extern void  CryptoNative_SslSetQuietShutdown(void *ssl, int mode);
extern int32_t CryptoNative_Tls13Supported(void);
extern void  CryptoNative_SslSetClientCertCallback(void *ssl, int set);
extern void  CryptoNative_SslSetPostHandshakeAuth(void *ssl, int val);
extern int32_t CryptoNative_SslSetAlpnProtos(void *ssl, const void *protos, uint32_t len);
extern void  CryptoNative_SslGet0AlpnSelected(void *ssl, const void **protocol, uint32_t *len);
extern int32_t CryptoNative_SslGetCurrentCipherId(void *ssl, int32_t *cipherId);
extern int32_t CryptoNative_SslGetFinished(void *ssl, void *buf, int32_t count);
extern int32_t CryptoNative_SslGetPeerFinished(void *ssl, void *buf, int32_t count);
extern int32_t CryptoNative_SslSessionReused(void *ssl);
extern int32_t CryptoNative_SslRenegotiate(void *ssl, int32_t *error);
extern int32_t CryptoNative_IsSslRenegotiatePending(void *ssl);
extern void  CryptoNative_SslSetVerifyPeer(void *ssl);
extern int32_t CryptoNative_SslSetData(void *ssl, void *ptr);
extern void *CryptoNative_SslGetData(void *ssl);
extern void *CryptoNative_SslGetClientCAList(void *ssl);
extern int32_t CryptoNative_SslUseCertificate(void *ssl, void *x509);
extern int32_t CryptoNative_SslUsePrivateKey(void *ssl, void *pkey);
extern int32_t CryptoNative_SslAddExtraChainCert(void *ssl, void *x509);
extern int32_t CryptoNative_SslAddClientCAs(void *ssl, void **x509s, uint32_t count);
extern void  CryptoNative_SslStapleOcsp(void *ssl, void *buf, int32_t len);
extern const char *CryptoNative_SslGetServerName(void *ssl);
extern int32_t CryptoNative_SslSetSigalgs(void *ssl, void *str);
extern int32_t CryptoNative_SslSetClientSigalgs(void *ssl, void *str);

/* === Session functions === */
extern void *CryptoNative_SslGetSession(void *ssl);
extern int32_t CryptoNative_SslSetSession(void *ssl, void *session);
extern void  CryptoNative_SslSessionFree(void *session);
extern const char *CryptoNative_SslSessionGetHostname(void *session);
extern int   CryptoNative_SslSessionSetHostname(void *session, const char *hostname);
extern void  CryptoNative_SslSessionSetData(void *session, void *val);
extern void *CryptoNative_SslSessionGetData(void *session);

/* === Init / Info === */
extern void  CryptoNative_EnsureLibSslInitialized(void);
static int   CryptoNative_EnsureOpenSslInitialized(void) { CryptoNative_EnsureLibSslInitialized(); return 0; }
extern const char *CryptoNative_GetOpenSslCipherSuiteName(void *ssl, int32_t suite, int32_t *isTls12);
extern int32_t CryptoNative_GetDefaultSignatureAlgorithms(uint16_t *buffer, int32_t *count);
extern int32_t CryptoNative_OpenSslGetProtocolSupport(int protocol);

/* === Error queue === */
extern uint64_t CryptoNative_ErrPeekError(void);
extern uint64_t CryptoNative_ErrPeekLastError(void);
extern uint64_t CryptoNative_ErrGetErrorAlloc(void **msg);
extern uint64_t CryptoNative_ErrGetExceptionError(int32_t *isAllocFailure);
extern void     CryptoNative_ErrClearError(void);
extern const char *CryptoNative_ErrReasonErrorString(uint64_t err);

/* ---------- Mapping table: CryptoNative ---------- */

static const MonoDlMapping crypto_native_mappings[] = {
    /* BIO */
    {"CryptoNative_CreateMemoryBio",         CryptoNative_CreateMemoryBio},
    {"CryptoNative_BioDestroy",              CryptoNative_BioDestroy},
    {"CryptoNative_BioWrite",                CryptoNative_BioWrite},
    {"CryptoNative_BioRead",                 CryptoNative_BioRead},
    {"CryptoNative_BioCtrlPending",          CryptoNative_BioCtrlPending},
    {"CryptoNative_GetMemoryBioSize",        CryptoNative_GetMemoryBioSize},
    /* SSL_CTX */
    {"CryptoNative_SslV2_3Method",           CryptoNative_SslV2_3Method},
    {"CryptoNative_SslCtxCreate",            CryptoNative_SslCtxCreate},
    {"CryptoNative_SslCtxSetProtocolOptions", CryptoNative_SslCtxSetProtocolOptions},
    {"CryptoNative_SslCtxDestroy",           CryptoNative_SslCtxDestroy},
    {"CryptoNative_SslCtxSetCiphers",        CryptoNative_SslCtxSetCiphers},
    {"CryptoNative_SetCiphers",              CryptoNative_SetCiphers},
    {"CryptoNative_SslCtxSetEncryptionPolicy", CryptoNative_SslCtxSetEncryptionPolicy},
    {"CryptoNative_SslCtxSetQuietShutdown",  CryptoNative_SslCtxSetQuietShutdown},
    {"CryptoNative_SslCtxUseCertificate",    CryptoNative_SslCtxUseCertificate},
    {"CryptoNative_SslCtxUsePrivateKey",     CryptoNative_SslCtxUsePrivateKey},
    {"CryptoNative_SslCtxCheckPrivateKey",   CryptoNative_SslCtxCheckPrivateKey},
    {"CryptoNative_SslCtxAddExtraChainCert", CryptoNative_SslCtxAddExtraChainCert},
    {"CryptoNative_SslCtxSetAlpnSelectCb",   CryptoNative_SslCtxSetAlpnSelectCb},
    {"CryptoNative_SslCtxSetCaching",        CryptoNative_SslCtxSetCaching},
    {"CryptoNative_SslCtxRemoveSession",     CryptoNative_SslCtxRemoveSession},
    {"CryptoNative_SslCtxSetDefaultOcspCallback", CryptoNative_SslCtxSetDefaultOcspCallback},
    {"CryptoNative_SslCtxSetKeylogCallback", CryptoNative_SslCtxSetKeylogCallback},
    {"CryptoNative_SslCtxSetData",           CryptoNative_SslCtxSetData},
    {"CryptoNative_SslCtxGetData",           CryptoNative_SslCtxGetData},
    /* SSL */
    {"CryptoNative_SslCreate",               CryptoNative_SslCreate},
    {"CryptoNative_SslDestroy",              CryptoNative_SslDestroy},
    {"CryptoNative_SslSetBio",               CryptoNative_SslSetBio},
    {"CryptoNative_SslSetConnectState",      CryptoNative_SslSetConnectState},
    {"CryptoNative_SslSetAcceptState",       CryptoNative_SslSetAcceptState},
    {"CryptoNative_SslDoHandshake",          CryptoNative_SslDoHandshake},
    {"CryptoNative_SslRead",                 CryptoNative_SslRead},
    {"CryptoNative_SslWrite",                CryptoNative_SslWrite},
    {"CryptoNative_SslShutdown",             CryptoNative_SslShutdown},
    {"CryptoNative_SslGetError",             CryptoNative_SslGetError},
    {"CryptoNative_IsSslStateOK",            CryptoNative_IsSslStateOK},
    {"CryptoNative_SslGetVersion",           CryptoNative_SslGetVersion},
    {"CryptoNative_SslSetTlsExtHostName",    CryptoNative_SslSetTlsExtHostName},
    {"CryptoNative_SslGetPeerCertificate",   CryptoNative_SslGetPeerCertificate},
    {"CryptoNative_SslGetPeerCertVerifyResult", CryptoNative_SslGetPeerCertVerifyResult},
    {"CryptoNative_SslGetCertificate",       CryptoNative_SslGetCertificate},
    {"CryptoNative_SslGetPeerCertChain",     CryptoNative_SslGetPeerCertChain},
    {"CryptoNative_SslSetQuietShutdown",     CryptoNative_SslSetQuietShutdown},
    {"CryptoNative_Tls13Supported",          CryptoNative_Tls13Supported},
    {"CryptoNative_SslSetClientCertCallback", CryptoNative_SslSetClientCertCallback},
    {"CryptoNative_SslSetPostHandshakeAuth", CryptoNative_SslSetPostHandshakeAuth},
    {"CryptoNative_SslSetAlpnProtos",        CryptoNative_SslSetAlpnProtos},
    {"CryptoNative_SslGet0AlpnSelected",     CryptoNative_SslGet0AlpnSelected},
    {"CryptoNative_SslGetCurrentCipherId",   CryptoNative_SslGetCurrentCipherId},
    {"CryptoNative_SslGetFinished",          CryptoNative_SslGetFinished},
    {"CryptoNative_SslGetPeerFinished",      CryptoNative_SslGetPeerFinished},
    {"CryptoNative_SslSessionReused",        CryptoNative_SslSessionReused},
    {"CryptoNative_SslRenegotiate",          CryptoNative_SslRenegotiate},
    {"CryptoNative_IsSslRenegotiatePending", CryptoNative_IsSslRenegotiatePending},
    {"CryptoNative_SslSetVerifyPeer",        CryptoNative_SslSetVerifyPeer},
    {"CryptoNative_SslSetData",              CryptoNative_SslSetData},
    {"CryptoNative_SslGetData",              CryptoNative_SslGetData},
    {"CryptoNative_SslGetClientCAList",      CryptoNative_SslGetClientCAList},
    {"CryptoNative_SslUseCertificate",       CryptoNative_SslUseCertificate},
    {"CryptoNative_SslUsePrivateKey",        CryptoNative_SslUsePrivateKey},
    {"CryptoNative_SslAddExtraChainCert",    CryptoNative_SslAddExtraChainCert},
    {"CryptoNative_SslAddClientCAs",         CryptoNative_SslAddClientCAs},
    {"CryptoNative_SslStapleOcsp",           CryptoNative_SslStapleOcsp},
    {"CryptoNative_SslGetServerName",        CryptoNative_SslGetServerName},
    {"CryptoNative_SslSetSigalgs",           CryptoNative_SslSetSigalgs},
    {"CryptoNative_SslSetClientSigalgs",     CryptoNative_SslSetClientSigalgs},
    /* Session */
    {"CryptoNative_SslGetSession",           CryptoNative_SslGetSession},
    {"CryptoNative_SslSetSession",           CryptoNative_SslSetSession},
    {"CryptoNative_SslSessionFree",          CryptoNative_SslSessionFree},
    {"CryptoNative_SslSessionGetHostname",   CryptoNative_SslSessionGetHostname},
    {"CryptoNative_SslSessionSetHostname",   CryptoNative_SslSessionSetHostname},
    {"CryptoNative_SslSessionSetData",       CryptoNative_SslSessionSetData},
    {"CryptoNative_SslSessionGetData",       CryptoNative_SslSessionGetData},
    /* Init/Info */
    {"CryptoNative_EnsureLibSslInitialized", CryptoNative_EnsureLibSslInitialized},
    {"CryptoNative_EnsureOpenSslInitialized", CryptoNative_EnsureOpenSslInitialized},
    {"CryptoNative_GetOpenSslCipherSuiteName", CryptoNative_GetOpenSslCipherSuiteName},
    {"CryptoNative_GetDefaultSignatureAlgorithms", CryptoNative_GetDefaultSignatureAlgorithms},
    {"CryptoNative_OpenSslGetProtocolSupport", CryptoNative_OpenSslGetProtocolSupport},
    /* Error queue */
    {"CryptoNative_ErrPeekError",            CryptoNative_ErrPeekError},
    {"CryptoNative_ErrPeekLastError",        CryptoNative_ErrPeekLastError},
    {"CryptoNative_ErrGetErrorAlloc",        CryptoNative_ErrGetErrorAlloc},
    {"CryptoNative_ErrGetExceptionError",    CryptoNative_ErrGetExceptionError},
    {"CryptoNative_ErrClearError",           CryptoNative_ErrClearError},
    {"CryptoNative_ErrReasonErrorString",    CryptoNative_ErrReasonErrorString},
    {NULL, NULL}
};

