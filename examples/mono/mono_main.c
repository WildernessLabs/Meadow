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

  int ret;
  const int mono_argc = 4;
  char *mono_argv[] = {"mono", "--trace", "--interp", "/tmp/app.exe"};

  setenv("MONO_LOG_LEVEL", "debug", 1);
  mono_set_assemblies_path("/meadow0");

  mono_dl_register_library("nuttx", meadow_os_mappings);
  ret = mono_main_driver (mono_argc, mono_argv);

  return ret;
}
