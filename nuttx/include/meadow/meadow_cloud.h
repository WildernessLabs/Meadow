#include <nuttx/config.h>

#include <sys/types.h>

#if defined(CONFIG_MEADOW_CLOUD)

int meadow_cloud_provision(FAR const char *, int, FAR const char *, int, FAR void*);
int meadow_cloud_retrieve_private_key(FAR const char **private_key_buf_ptr, int *len);
int meadow_cloud_release_private_key(FAR const char **private_key_buf_ptr);

#endif