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
 *  None
 *
 ****************************************************************************/
int meadow_client_cert_initialize() {

    // Loading client certificate
    syslog(LOG_INFO, "Checking for a client certificate in STM storage.\n");
    int ret;
    char *client_cert = NULL;
    char *private_key = NULL;
    char *private_key_pass = NULL;

    FILE *client_cert_file = fopen(CLIENT_CERT_FILE_PATH, "r");
    if (client_cert_file == NULL)
    {
        syslog(LOG_INFO, "Client certificate not found. Failed to open file '%s'.\n", CLIENT_CERT_FILE_PATH);
    }
    else
    {
        syslog(LOG_INFO, "Client certificate file found.\n");
        // Add a null-terminator character add the end of the file
        //  since it's required by mbedTLS
        fseek(client_cert_file, 0, SEEK_END);
        long client_cert_size = ftell(client_cert_file);
        rewind(client_cert_file);

        client_cert = (char *)malloc(client_cert_size + 1);
        if (client_cert == NULL)
        {
            syslog(LOG_INFO, "Memory allocation failed for client_cert.\n");
            fclose(client_cert_file);
            return -ENOMEM;
        }

        size_t client_cert_len = fread(client_cert, 1, client_cert_size, client_cert_file);
        client_cert[client_cert_len] = '\0';
        fclose(client_cert_file);

        // Storing client certificate
        syslog(LOG_INFO, "Storing client certificate length: %d\nContent: %s", client_cert_len, client_cert);
        ret = espcp_file_system_write_file(CLIENT_CERT_FILE, (const char *)client_cert, client_cert_len + 1);
        if (ret < 0)
        {
            syslog(LOG_ERR, "Failed to store client cert private key.\n");
            return ret;
        }

        // Remove file from STM storage
        remove(CLIENT_CERT_FILE_PATH);
    }

    // Loading client private key
    syslog(LOG_INFO, "Checking for a client cert private key in STM storage.\n");

    FILE *private_key_file = fopen(CLIENT_CERT_PRIVATE_KEY_FILE_PATH, "r");
    if (private_key_file == NULL)
    {
        syslog(LOG_INFO, "Client cert private key not found. Failed to open file '%s'.\n", CLIENT_CERT_PRIVATE_KEY_FILE_PATH);
    }
    else
    {
        syslog(LOG_INFO, "Client certificate private key file found.\n");
        // Add a null-terminator character add the end of the file
        //  since it's required by mbedTLS
        fseek(private_key_file, 0, SEEK_END);
        long private_key_size = ftell(private_key_file);
        rewind(private_key_file);

        private_key = (char *)malloc(private_key_size + 1);
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

        // Storing client certificate private key
        syslog(LOG_INFO, "Storing private key length: %d\nContent: %s", private_key_len, private_key);
        ret = espcp_file_system_write_file(CLIENT_CERT_PRIVATE_KEY_FILE, (const char *)private_key, private_key_len + 1);
        if (ret < 0)
        {
            syslog(LOG_ERR, "Failed to store client cert private key.\n");
            free(client_cert);
            return ret;
        }

        // Remove file from STM storage
        remove(CLIENT_CERT_PRIVATE_KEY_FILE_PATH);
    }

    // Loading client private key passphrase
    syslog(LOG_INFO, "Checking for a client cert private key passphrase in STM storage.\n");

    size_t private_key_pass_len = 0;
    FILE *private_key_pass_file = fopen(CLIENT_CERT_PRIVATE_KEY_PASS_FILE_PATH, "r");
    if (private_key_pass_file == NULL)
    {
        syslog(LOG_INFO, "Client cert private key pass not found. Failed to open file '%s'.\n", CLIENT_CERT_PRIVATE_KEY_PASS_FILE_PATH);
    }
    else
    {
        syslog(LOG_INFO, "Client certificate private key passphrase file found.\n");
        // Add a null-terminator character add the end of the file
        //  since it's required by mbedTLS
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

        // Storing client certificate private key passphrase
        syslog(LOG_INFO, "Storing private key passphrase length: %d\nContent: %s\n", private_key_pass_len, private_key_pass);
        ret = espcp_file_system_write_file(CLIENT_CERT_PRIVATE_KEY_PASS_FILE, (const char *)private_key_pass, private_key_pass_len + 1);
        if (ret < 0)
        {
            syslog(LOG_ERR, "Failed to store client cert private key passphrase.\n");
            free(client_cert);
            free(private_key);
            return ret;
        }

        // Remove file from STM storage
        remove(CLIENT_CERT_PRIVATE_KEY_PASS_FILE_PATH);
    }

    free(client_cert);
    free(private_key);
    free(private_key_pass);

    return OK;
}


int meadow_client_cert_retrieve_credentials(FAR const char **client_cert_buf_ptr, int *client_cert_len, FAR const char **private_key_buf_ptr, int *private_key_len, FAR const char **private_key_pass_buf_ptr, int *private_key_pass_len)
{
    syslog(LOG_INFO, "Retrieving client cert credentials from ESP32...\n");

    int16_t client_cert_length;
    const char *client_cert_buf = espcp_file_system_read_file(CLIENT_CERT_FILE, &client_cert_length);
    if (client_cert_buf == NULL)
    {
        syslog(LOG_ERR, "Fail to retrieve client cert");
        return -1;
    }

    syslog(LOG_INFO, "Retrieved client cert: %s len: %d\n", client_cert_buf, client_cert_length);
    *client_cert_buf_ptr = client_cert_buf;
    client_cert_buf_size = client_cert_length;
    *client_cert_len = client_cert_length;

    syslog(LOG_INFO, "Retrieving client cert private key from ESP32...\n");

    int16_t private_key_length;
    const char *private_key_buf = espcp_file_system_read_file(CLIENT_CERT_PRIVATE_KEY_FILE, &private_key_length);
    if (private_key_buf == NULL)
    {
        syslog(LOG_ERR, "Fail to retrieve client cert private key");
        return -1;
    }

    syslog(LOG_INFO, "Retrieved client cert private key: %s len: %d\n", private_key_buf, private_key_length);
    *private_key_buf_ptr = private_key_buf;
    private_key_buf_size = private_key_length;
    *private_key_len = private_key_length;

    syslog(LOG_INFO,"Retrieving client cert private key pass from ESP32...\n");

    int16_t private_key_pass_length;
    const char *private_key_pass_buf = espcp_file_system_read_file(CLIENT_CERT_PRIVATE_KEY_PASS_FILE, &private_key_pass_length);
    if (private_key_pass_buf == NULL)
    {
        syslog(LOG_ERR, "Fail to retrieve client cert private key passphrase");
        return -1;
    }

    syslog(LOG_INFO, "Retrieved client cert private key pass: %s len: %d\n", private_key_pass_buf, private_key_pass_length);
    *private_key_pass_buf_ptr = private_key_pass_buf;
    private_key_pass_buf_size = private_key_pass_length;
    *private_key_pass_len = private_key_pass_length;

    return OK;
}

int meadow_client_cert_release_credentials(FAR const char **client_cert_buf_ptr, FAR const char **private_key_buf_ptr, FAR const char **private_key_pass_buf_ptr)
{
    if (*client_cert_buf_ptr != NULL)
    {
        memset(*client_cert_buf_ptr, client_cert_buf_size, 0);
        free(*client_cert_buf_ptr);
        *client_cert_buf_ptr = NULL;
    }

    if (*private_key_buf_ptr != NULL)
    {
        memset(*private_key_buf_ptr, private_key_buf_size, 0);
        free(*private_key_buf_ptr);
        *private_key_buf_ptr = NULL;
    }

    if (*private_key_pass_buf_ptr != NULL)
    {
        memset(*private_key_pass_buf_ptr, private_key_pass_buf_size, 0);
        free(*private_key_pass_buf_ptr);
        *private_key_pass_buf_ptr = NULL;
    }

    return 0;
}