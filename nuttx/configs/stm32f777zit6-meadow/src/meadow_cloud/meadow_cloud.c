#include <nuttx/config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "espcp/espcp_file_system.h"

#include <meadow/hcom_shared_common.h>

#define MEADOW_CLOUD_PRIVATE_KEY_FILE "meadow_cloud_private.pem"
#define MEADOW_CLOUD_PUBLIC_KEY_FILE "meadow_cloud_public.pem"

#if defined(CONFIG_MEADOW_CLOUD)

static int private_key_buf_size = 0;

int meadow_cloud_provision(FAR const char *private_key_buf, int private_key_len, FAR const char *public_key_buf, int public_key_len, FAR void *unused)
{
    if (espcp_file_system_write_file(MEADOW_CLOUD_PRIVATE_KEY_FILE, (uint8_t *) private_key_buf, private_key_len) < 0)
        return -1;

    if (espcp_file_system_write_file(MEADOW_CLOUD_PUBLIC_KEY_FILE, (uint8_t *) public_key_buf, public_key_len) < 0)
        return -2;
    return 0;
}

int meadow_cloud_retrieve_private_key(FAR const char **private_key_buf_ptr, int *len)
{
    int16_t length;
    const char *buf = (const char *) espcp_file_system_read_file(MEADOW_CLOUD_PRIVATE_KEY_FILE, &length);
    *private_key_buf_ptr = buf;
    if (buf == NULL)
        return -1;

    private_key_buf_size = length;
    *len = length;
    return 0;
}

int meadow_cloud_release_private_key(char ** const private_key_buf_ptr)
{
    if (private_key_buf_size == 0)
        up_assert(__FILE__, __LINE__); // release without retrieve

    memset(*private_key_buf_ptr, 0, private_key_buf_size);
    free(*private_key_buf_ptr);
    *private_key_buf_ptr = NULL;
    return 0;
}
#endif
