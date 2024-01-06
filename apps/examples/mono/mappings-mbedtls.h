extern intptr_t mono_mbedtls_connect (intptr_t mono_fd, intptr_t readbuf, intptr_t writebuf);
extern int mono_mbedtls_read (intptr_t * ctx, int length);
extern int mono_mbedtls_write (intptr_t * ctx, int length);
extern void mono_mbedtls_close (intptr_t * ctx);
extern int mono_mbedtls_init ();
extern int mono_mbedtls_set_server_cert_authmode (int authmode);
extern int mono_mbedtls_handshake (intptr_t * ctx);

static MonoDlMapping mbedtls_mappings[] = {
    {"mono_mbedtls_connect", mono_mbedtls_connect},
    {"mono_mbedtls_read", mono_mbedtls_read},
    {"mono_mbedtls_write", mono_mbedtls_write},
    {"mono_mbedtls_close", mono_mbedtls_close},
    {"mono_mbedtls_init", mono_mbedtls_init},
    {"mono_mbedtls_set_server_cert_authmode", mono_mbedtls_set_server_cert_authmode},
    {"mono_mbedtls_handshake", mono_mbedtls_handshake},
    {NULL, NULL}};
