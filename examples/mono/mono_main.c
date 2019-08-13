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
#include "../../../nuttx/configs/stm32f777zit6-meadow/src/hcom/hcom_common.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t _startupAction;

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
  // When nuttx launches the user defined entry point (CONFIG_USER_ENTRYPOINT) 
  // there's 1 argument and argv[0] = "init" which is the name of the task.
  if(argc == 1 && strcmp(argv[0], "init") == 0)
  {
    // Normal NuttX startup of mono. Is there some special action requested?
    if(_startupAction == HCOM_MONO_ACTION_ENABLE_DISABLE_KEY)
    {
      return 0;
    }
  }
  else
  {
    // Not being launch from NuttX. Maybe there's a work request to
    // be acted upon on the next MCU reset.
    if(argc == (int)HCOM_MONO_MAIN_ACCESS_KEY)
    {
      // Save the action value till NuttX launches mono the next time.
      _startupAction = atoi(argv[0]);   // This is a number
    }
    return 0;
  }

  usleep(300 * 1000);
  symtab_initialize();

  const char app_path[] = "/meadow0/App.exe";

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
