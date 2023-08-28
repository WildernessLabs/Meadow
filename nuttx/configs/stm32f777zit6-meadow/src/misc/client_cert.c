#include <nuttx/config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "espcp/espcp_file_system.h"
#include <meadow/client_cert.h>

static int client_cert_buf_size = 0;
static int private_key_buf_size = 0;
static int private_key_pass_buf_size = 0;

int client_cert_initialize() {

    // Client certificate
    syslog(LOG_INFO, "Loading client certificate.\n");

    FILE *client_cert_file = fopen(CLIENT_CERT_FILE_PATH, "r");
    if (client_cert_file == NULL) {
        syslog(LOG_INFO, "Failed to open client_cert.pem.\n");
        return 1;
    }

    fseek(client_cert_file, 0, SEEK_END);
    long client_cert_size = ftell(client_cert_file);
    rewind(client_cert_file);

    char *client_cert = (char *)malloc(client_cert_size + 1);
    if (client_cert == NULL) {
        syslog(LOG_INFO, "Memory allocation failed for client_cert.\n");
        fclose(client_cert_file);
        return 1;
    }

    size_t client_cert_len = fread(client_cert, 1, client_cert_size, client_cert_file);
    client_cert[client_cert_len] = '\0';
    fclose(client_cert_file);
    syslog(LOG_INFO, "Client certificate loaded.\n");
    syslog(LOG_INFO, "client_cert_len: %d\n", client_cert_len);

    // Client private key
    syslog(LOG_INFO, "Loading client certificate private key.\n");

    FILE *private_key_file = fopen(CLIENT_CERT_PRIVATE_KEY_FILE_PATH, "r");
    if (private_key_file == NULL) {
        syslog(LOG_INFO, "Failed to open private_key.pem.\n");
        free(client_cert);
        return 1;
    }

    fseek(private_key_file, 0, SEEK_END);
    long private_key_size = ftell(private_key_file);
    rewind(private_key_file);

    char *private_key = (char *)malloc(private_key_size + 1);
    if (private_key == NULL) {
        syslog(LOG_INFO, "Memory allocation failed for private_key.\n");
        fclose(private_key_file);
        free(client_cert); // TODO: double check
        return 1;
    }

    size_t private_key_len = fread(private_key, 1, private_key_size, private_key_file);
    private_key[private_key_len] = '\0';
    fclose(private_key_file);
    syslog(LOG_INFO, "Private key loaded.\n");
    syslog(LOG_INFO, "private_key_len: %d\n", private_key_len);

    // Client private key pass
    syslog(LOG_INFO, "Loading private key pass.\n");

    FILE *private_key_pass_file = fopen(CLIENT_CERT_PRIVATE_KEY_PASS_FILE_PATH, "r");
    if (private_key_pass_file == NULL) {
        syslog(LOG_INFO, "Failed to open private_key_pass.txt.\n");
        free(client_cert); // TODO: double check
        return 1;
    }

    fseek(private_key_pass_file, 0, SEEK_END);
    long private_key_pass_size = ftell(private_key_pass_file);
    rewind(private_key_pass_file);

    char *private_key_pass = (char *)malloc(private_key_pass_size + 1);
    if (private_key_pass == NULL) {
        syslog(LOG_INFO, "Memory allocation failed for private_key.\n");
        fclose(private_key_pass_file);
        free(client_cert);
        return 1;
    }

    size_t private_key_pass_len = fread(private_key_pass, 1, private_key_pass_size, private_key_pass_file);
    private_key_pass[private_key_pass_len] = '\0';
    fclose(private_key_pass_file);
    syslog(LOG_INFO, "Private key pass loaded.\n");
    syslog(LOG_INFO, "private_key_pass_len: %d\n", private_key_pass_len);
    syslog(LOG_INFO, "Private key pass: %s\n", private_key_pass);

    // Storing credentials
    syslog(LOG_INFO, "Calling client_cert_store_credentials.\n");

    int client_cert_store_ret = client_cert_store_credentials(
        (const char *)client_cert, client_cert_len + 1,
        (const char *)private_key, private_key_len + 1,
        (const char *)private_key_pass, private_key_pass_len + 1, 
        NULL
    );
    syslog(LOG_INFO, "client_cert_store_credentials ret: %d\n", client_cert_store_ret);

    free(client_cert);
    free(private_key);
    free(private_key_pass);

    return 0;
}

int client_cert_store_credentials(FAR const char *client_cert_buf, int client_cert_len, FAR const char *private_key_buf, int private_key_len, FAR const char *private_key_pass_buf, int private_key_pass_len, FAR void *unused)
{
    if (espcp_file_system_write_file(CLIENT_CERT_FILE, client_cert_buf, client_cert_len) < 0)
        return -1;

    if (espcp_file_system_write_file(CLIENT_CERT_PRIVATE_KEY_FILE, private_key_buf, private_key_len) < 0)
        return -2;

    if (espcp_file_system_write_file(CLIENT_CERT_PRIVATE_KEY_PASS_FILE, private_key_pass_buf, private_key_pass_len) < 0)
        return -2;
    return 0;
}

int client_cert_retrieve_certificate(FAR const char **client_cert_buf_ptr, int *len)
{
    int16_t length;
    const char *buf = espcp_file_system_read_file(CLIENT_CERT_FILE, &length);
    *client_cert_buf_ptr = buf;
    if (buf == NULL)
        return -1;

    client_cert_buf_size = length;
    *len = length;
    return 0;
}

int client_cert_retrieve_private_key(FAR const char **private_key_buf_ptr, int *len)
{
    int16_t length;
    const char *buf = espcp_file_system_read_file(CLIENT_CERT_PRIVATE_KEY_FILE, &length);
    *private_key_buf_ptr = buf;
    if (buf == NULL)
        return -1;

    private_key_buf_size = length;
    *len = length;
    return 0;
}

int client_cert_retrieve_private_key_pass(FAR const char **private_key_pass_buf_ptr, int *len)
{
    int16_t length;
    const char *buf = espcp_file_system_read_file(CLIENT_CERT_PRIVATE_KEY_PASS_FILE, &length);
    *private_key_pass_buf_ptr = buf;
    if (buf == NULL)
        return -1;

    private_key_pass_buf_size = length;
    *len = length;
    return 0;
}

int client_cert_release_credentials(FAR const char **client_cert_buf_ptr, FAR const char **private_key_buf_ptr, FAR const char **private_key_pass_buf_ptr)
{
    if (client_cert_buf_size == 0)
        up_assert(__FILE__, __LINE__); // release without retrieve

    memset(*client_cert_buf_ptr, client_cert_buf_size, 0);
    free(*client_cert_buf_ptr);
    *client_cert_buf_ptr = NULL;

    if (private_key_buf_size == 0)
        up_assert(__FILE__, __LINE__); // release without retrieve

    memset(*private_key_buf_ptr, private_key_buf_size, 0);
    free(*private_key_buf_ptr);
    *private_key_buf_ptr = NULL;

    if (private_key_pass_buf_size == 0)
        up_assert(__FILE__, __LINE__); // release without retrieve

    memset(*private_key_pass_buf_ptr, private_key_pass_buf_size, 0);
    free(*private_key_pass_buf_ptr);
    *private_key_pass_buf_ptr = NULL;

    return 0;
}
