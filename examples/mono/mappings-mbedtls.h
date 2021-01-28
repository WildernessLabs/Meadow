extern void mono_mbedtls_init (intptr_t mono_fd, intptr_t readbuf, intptr_t writebuf);

static MonoDlMapping mbedtls_mappings[] = {
    {"mono_mbedtls_init", mono_mbedtls_init},
    {NULL, NULL}};