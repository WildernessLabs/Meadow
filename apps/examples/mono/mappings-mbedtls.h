extern intptr_t mono_mbedtls_connect (intptr_t mono_fd, intptr_t readbuf, intptr_t writebuf);
extern int mono_mbedtls_read (intptr_t * ctx, int length);
extern int mono_mbedtls_write (intptr_t * ctx, int length);
extern void mono_mbedtls_close (intptr_t * ctx);

static MonoDlMapping mbedtls_mappings[] = {
    {"mono_mbedtls_connect", mono_mbedtls_connect},
    {"mono_mbedtls_read", mono_mbedtls_read},
    {"mono_mbedtls_write", mono_mbedtls_write},
    {"mono_mbedtls_close", mono_mbedtls_close},
    {NULL, NULL}};
