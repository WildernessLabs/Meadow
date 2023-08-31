#include <nuttx/config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "espcp/espcp_file_system.h"
#include <meadow/meadow_client_cert.h>

static int client_cert_buf_size = 0;
static int private_key_buf_size = 0;
static int private_key_pass_buf_size = 0;

/****************************************************************************
 * Name: meadow_client_cert_check_if_credential_files_exist
 *
 * Description:
 *  Check if there are the client credential files in the STM storage,
 * i.e. the client certificate, the client private key, and the client 
 * private key passphrase (optional).
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  True, if there are the client certificate and client private key,
 * otherwise, False.
 *
 * Assumptions/Limitations:
 *  The private key passphrase is optional.
 *
 ****************************************************************************/
bool meadow_client_cert_check_if_credential_files_exist()
{
    FILE *client_cert_file = fopen(CLIENT_CERT_FILE_PATH, "r");
    if (client_cert_file)
    {
        syslog(LOG_INFO, "Client certificate file found\n\n");
        fclose(client_cert_file);
    }

    FILE *private_key_file = fopen(CLIENT_CERT_PRIVATE_KEY_FILE_PATH, "r");
    if (private_key_file)
    {
        syslog(LOG_INFO, "Private key file found\n\n");
        fclose(private_key_file);
    }

    FILE *private_key_pass_file = fopen(CLIENT_CERT_PRIVATE_KEY_PASS_FILE_PATH, "r");
    if (private_key_pass_file)
    {
        syslog(LOG_INFO, "Private key passphrase file found\n\n");
        fclose(private_key_pass_file);
    }

    // The private key passsphrase is optional
    return client_cert_file && private_key_file;
}

/****************************************************************************
 * Name: meadow_client_cert_initialize
 *
 * Description:
 *  This function is responsible for loading the client certificate, client
 * private key, and the private key passphrase (if provided). It stores 
 * these values into ESP32, adding an extra security layer for the credentials.
 * After processing, the client credentials files are deleted.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Zero, if the function succeeds, otherwise returns a specific NuttX 
 * error code corresponding to the encountered issue.
 *
 * Assumptions/Limitations:
 *  It assumes that the meadow_client_cert_check_if_credential_files_exist function was
 * previously called to ensure that the necessary files for the client
 * certificate auth method exists in the STM storage.
 *
 ****************************************************************************/
int meadow_client_cert_initialize() {

    // Loading client certificate
    syslog(LOG_INFO, "Loading client certificate.\n");

    FILE *client_cert_file = fopen(CLIENT_CERT_FILE_PATH, "r");
    if (client_cert_file == NULL)
    {
        syslog(LOG_INFO, "Failed to open client certificate file.\n");
        return -ENOENT;
    }

    fseek(client_cert_file, 0, SEEK_END);
    long client_cert_size = ftell(client_cert_file);
    rewind(client_cert_file);

    char *client_cert = (char *)malloc(client_cert_size + 1);
    if (client_cert == NULL)
    {
        syslog(LOG_INFO, "Memory allocation failed for client_cert.\n");
        fclose(client_cert_file);
        return -ENOMEM;
    }

    size_t client_cert_len = fread(client_cert, 1, client_cert_size, client_cert_file);
    client_cert[client_cert_len] = '\0';
    fclose(client_cert_file);
    syslog(LOG_INFO, "Client certificate length: %d\nContent: %s", client_cert_len, client_cert);

    // Loading client private key
    syslog(LOG_INFO, "Loading client private key.\n");

    FILE *private_key_file = fopen(CLIENT_CERT_PRIVATE_KEY_FILE_PATH, "r");
    if (private_key_file == NULL)
    {
        syslog(LOG_ERR, "Failed to open the private key file.\n");
        free(client_cert);
        return -ENOENT;
    }

    fseek(private_key_file, 0, SEEK_END);
    long private_key_size = ftell(private_key_file);
    rewind(private_key_file);

    char *private_key = (char *)malloc(private_key_size + 1);
    if (private_key == NULL)
    {
        syslog(LOG_ERR, "Memory allocation failed for private_key.\n");
        fclose(private_key_file);
        free(client_cert);
        return -ENOMEM;
    }

    size_t private_key_len = fread(private_key, 1, private_key_size, private_key_file);
    private_key[private_key_len] = '\0';
    fclose(private_key_file);
    syslog(LOG_INFO, "Private key length: %d\nContent: %s", private_key_len, private_key);

    // Loading client private key passphrase
    syslog(LOG_INFO, "Loading private key passphrase.\n");

    char *private_key_pass = NULL;
    size_t private_key_pass_len = 0;
    FILE *private_key_pass_file = fopen(CLIENT_CERT_PRIVATE_KEY_PASS_FILE_PATH, "r");
    if (private_key_pass_file == NULL)
    {
        syslog(LOG_WARNING, "Failed to open the private key passphrase file. The private key is assumed to be decrypted.\n");
    }
    else
    {
        fseek(private_key_pass_file, 0, SEEK_END);
        long private_key_pass_size = ftell(private_key_pass_file);
        rewind(private_key_pass_file);

        private_key_pass = (char *)malloc(private_key_pass_size + 1);
        if (private_key_pass == NULL)
        {
            syslog(LOG_ERR, "Memory allocation failed for private_key_pass.\n");
            fclose(private_key_pass_file);
            free(client_cert);
            free(private_key);
            return -ENOMEM;
        }

        private_key_pass_len = fread(private_key_pass, 1, private_key_pass_size, private_key_pass_file);
        private_key_pass[private_key_pass_len] = '\0';
        fclose(private_key_pass_file);
        syslog(LOG_INFO, "Private key passphrase length: %d\nContent: %s", private_key_pass_len, private_key_pass);
    }

    // Storing credentials
    int ret = meadow_client_cert_store_credentials(
        (const char *)client_cert, client_cert_len + 1,
        (const char *)private_key, private_key_len + 1,
        (const char *)private_key_pass, private_key_pass_len + 1, 
        NULL
    );
    if (ret < 0)
    {
        syslog(LOG_ERR, "Failed to store client credentials.\n");
        return ret;
    }

    free(client_cert);
    free(private_key);
    free(private_key_pass);

    // Delete the files from STM storage
    remove(CLIENT_CERT_FILE_PATH);
    remove(CLIENT_CERT_PRIVATE_KEY_FILE_PATH);
    remove(CLIENT_CERT_PRIVATE_KEY_PASS_FILE_PATH);
    
    return 0;
}

int meadow_client_cert_store_credentials(FAR const char *client_cert_buf, int client_cert_len, FAR const char *private_key_buf, int private_key_len, FAR const char *private_key_pass_buf, int private_key_pass_len, FAR void *unused)
{
    if (espcp_file_system_write_file(CLIENT_CERT_FILE, client_cert_buf, client_cert_len) < 0)
        return -1;

    if (espcp_file_system_write_file(CLIENT_CERT_PRIVATE_KEY_FILE, private_key_buf, private_key_len) < 0)
        return -2;

    if (espcp_file_system_write_file(CLIENT_CERT_PRIVATE_KEY_PASS_FILE, private_key_pass_buf, private_key_pass_len) < 0)
        return -3;
    return 0;
}

int meadow_client_cert_retrieve_certificate(FAR const char **client_cert_buf_ptr, int *len)
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

int meadow_client_cert_retrieve_private_key(FAR const char **private_key_buf_ptr, int *len)
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

int meadow_client_cert_retrieve_private_key_pass(FAR const char **private_key_pass_buf_ptr, int *len)
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

int meadow_client_cert_release_credentials(FAR const char **client_cert_buf_ptr, FAR const char **private_key_buf_ptr, FAR const char **private_key_pass_buf_ptr)
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
