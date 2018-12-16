#include <nuttx/config.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <syscall.h>
#include <syslog.h>

#include "nuttx-functions.h"

int shim_open_void(char *pathname, int flags) {
  return open(pathname, flags);
}
