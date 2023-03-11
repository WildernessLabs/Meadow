#include <nuttx/config.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "espcp/espcp_file_system.h"

#include <meadow/hcom_shared_common.h>

#define MEADOW_CLOUD_PRIVATE_KEY_FILE "meadow_cloud_private.pem"
#define MEADOW_CLOUD_PUBLIC_KEY_FILE "meadow_cloud_public.pem"

#if defined(CONFIG_MEADOW_CLOUD)

int meadow_cloud_provision(FAR const char *private_key_buf, int private_key_len, FAR const char *public_key_buf, int public_key_len, FAR void* unused)
{
    if (espcp_file_system_write_file(MEADOW_CLOUD_PRIVATE_KEY_FILE, private_key_buf, private_key_len) < 0)
        return -1;

    if (espcp_file_system_write_file(MEADOW_CLOUD_PUBLIC_KEY_FILE, public_key_buf, public_key_len) < 0)
        return -2;
    return 0;
}
#endif
