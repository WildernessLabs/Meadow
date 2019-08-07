/****************************************************************************
 * examples/mono/mono_main.c
 *
 *   Copyright (C) 2018-2019 Wilderness Labs. All rights reserved.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <syscall.h>

#include "nuttx-functions.h"


/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * hello_main
 ****************************************************************************/

extern int mono_main (int argc, char* argv[]);
extern void mono_dl_register_library(char *name, MonoDlMapping *mappings);

extern void symtab_initialize(void);

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int mono_main(int argc, char *argv[])
#endif
{
  usleep(300 * 1000);
  symtab_initialize();

  const char app_path[] = "/meadow0/app.exe";

  if (access(app_path, F_OK) == -1) {
    syslog(LOG_ERR, "Mono managed app was not found in %s\nSkipping Mono...",
      app_path);
    return 0;
  }

  int ret;
  const int mono_argc = 4;
  const char *mono_argv[] = {"mono", "--trace", "--interp", app_path};

  setenv("MONO_LOG_LEVEL", "debug", 1);
  mono_set_assemblies_path("/meadow0");

  mono_dl_register_library("nuttx", meadow_os_mappings);
  ret = mono_main_driver (mono_argc, mono_argv);

  return ret;
}
