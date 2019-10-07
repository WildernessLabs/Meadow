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
#include "../../../nuttx/configs/stm32f777zit6-meadow/src/hcom/hcom_mono_main.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t _startupAction;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static bool _shutting_down = false;
static int _pipe_fd = -1;

//==================================================================
static int RedirectStdout(void)
{
  int ret;
  int errcode = 0;

  if(!_shutting_down && _pipe_fd < 0)
  {
    set_errno(0);
    do
    {
      // Note: normally open blocks if no reader has opened the read end,
      // that's why O_NONBLOCK is used
      _pipe_fd = open(HCOM_MONO_MAIN_STDOUT_PIPE, O_WRONLY|O_NONBLOCK);
      if(_pipe_fd > 0)
        break;

      errcode = errno;
      if(errcode != ENOENT)   // ENOENT = Error No Entity -> No such file or directory
      {
        syslog(LOG_ERR, "%s() Error: open() of %s failed with errno=%d\n",
          __func__, HCOM_MONO_MAIN_STDOUT_PIPE, errcode);
        _pipe_fd = -1;
        return 1;
      }
      
      usleep(500 * 1000);
    } while (errcode == ENOENT);

    ret = dup2(_pipe_fd, STDOUT_FILENO);
    if (ret != 0)
    {
      syslog(LOG_ERR, "redirect_writer: dup2 failed: %d\n", errno);
      return 2;
    }

    /* Close the original file descriptor */
    ret = close(_pipe_fd);
    _pipe_fd = -1;    
    if (ret != 0)
    {
      syslog(LOG_ERR, "redirect_reader: failed to close fdout=%d\n", _pipe_fd);
      return 3;
    }
  }    
  return OK;
}

/****************************************************************************
 * mono_main
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
  // argc == 1 and argv[0] = "init" this is the name NuttX always gives the
  // task it internally launches.
  if(argc != 1 || strcmp(argv[0], "init") != 0)
  {
    // This is NOT THE NORMAL NUTTX STARTUP via 'init' task
    // Maybe there's a work request to be acted upon for the next MCU reset.
    if(argc == (int)HCOM_MONO_MAIN_ACCESS_KEY)
    {
      // Save the numberic value until NuttX re-starts mono. It will then be tested
      // and if a match is found take some special action.
      _startupAction = atoi(argv[0]);
      return OK;
    }
#ifdef CONFIG_SYSTEM_NSH
    // Note: there's always 1 argument, it's the name of the task. For the one
    // defined by CONFIG_USER_ENTRYPOINT it's "init". So using a different task
    // name (e.g. "nshTask") this entry point to be reused to launch NuttShell.
    if(argc == 1 && strcmp(argv[0], "nshTask") == 0)
    {
      nsh_main(argc, argv);
      return OK;
    }
#endif

    return OK;    // No special work identified so exit
  }

  // NuttX is attempting to start mono.
  // Check if it should be started
  if(_startupAction == HCOM_MONO_MAIN_ACTION_ENABLE_KEY)
    return OK;    // Disable mono by returning the thread that was to run it

  RedirectStdout();

  usleep(300 * 1000);

  // Normal mono startup follows
  symtab_initialize();

#ifdef CONFIG_MTD_PARTITION
  const char app_path[] = "/meadow0/App.exe";
#else
  const char app_path[] = "/meadow/App.exe";
#endif
  if (access(app_path, F_OK) == -1) {
    syslog(LOG_ERR, "Mono managed app was not found in %s\nSkipping Mono...",
      app_path);
    return 0;
  }

  int ret;
  const int mono_argc = 4;
  const char *mono_argv[] = {"mono", "--trace", "--interp", app_path};

  setenv("MONO_LOG_LEVEL", "debug", 1);
  
#ifdef CONFIG_MTD_PARTITION
  mono_set_assemblies_path("/meadow0");
#else
  mono_set_assemblies_path("/meadow");
#endif

  mono_dl_register_library("nuttx", meadow_os_mappings);
  ret = mono_main_driver (mono_argc, mono_argv);

  return ret;
}
