/****************************************************************************
 * examples/mono/mono_main.c
 *
 *   Copyright (C) 2018-2020 Wilderness Labs. All rights reserved.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/net/net.h>

#include <arch/board/boardctl.h>
#include <sys/boardctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <syscall.h>
#include <dirent.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include "../../../mono/config.h"

#include <meadow/hcom_shared_common.h>
#include "../hcom/hcom_common.h"

#include "ota.h"

typedef struct {
  const char *name;
  void *addr;
} MonoDlMapping;

#include "mappings-meadow.h"
#include "mappings-system-native.h"
#include "mappings-mbedtls.h"
#if defined (CONFIG_EXAMPLES_MEADOW_SQLITE)
#include "mappings-sqlite.h"
#endif
/****************************************************************************
 * External methods
 ****************************************************************************/
extern int mono_main_driver(int, char **);
extern void mono_set_assemblies_path(const char *);

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * mono_main
 ****************************************************************************/

extern int mono_main (int argc, char* argv[]);
extern void mono_dl_register_library(char *name, MonoDlMapping *mappings);

extern void symtab_initialize(void);

bool mono_should_run = true;

#ifdef CONFIG_BUILD_KERNEL
int main(int hcom_argc, FAR char *hcom_argv[])
#else
int mono_main(int hcom_argc, char *hcom_argv[])
#endif
{
  // Normal mono startup follows
  symtab_initialize();

  if (hcom_via_nx_copy_mono_runtime_to_ram() < 0)
  {
    syslog(LOG_ERR, "Mono runtime is not present or is invalid.\n");
    return -1;
  }
  syslog(LOG_INFO, "Mono runtime copied into RAM.\n");

  int ret;
  char app_path[] = MONO_MEADOW_EXECUTABLE_APP_EXE;
#ifdef CONFIG_BUILD_KERNEL
  char *mono_argv[] = {"mono", app_path};
#else
  char *mono_argv[] = {"mono", app_path};
#endif

  //
  //  Modify this code to turn JIT or AOT on.  To turn interp off simply reduce hcom_argc by 1.
  //  For JIT / AOT then modify hcom_mono_ctrl_extract_mono_options in hcom_mono_control.c
  //  to add any required options.
  //
  if ((hcom_argc > 0) && (hcom_argv !=  NULL))
  {
    if (strcmp(hcom_argv[hcom_argc - 1], MONO_OPTION_JIT) == 0)
    {
      hcom_argc--;
    }
    else
    {
      if (strcmp(hcom_argv[hcom_argc -1], MONO_OPTION_AOT) == 0)
      {
        hcom_argc--;
        // Do AOT stuff here.
      }
    }
  }
  
  //
  //  Now we need to put all of the arguments together for Mono.
  //
  int mono_argc = sizeof(mono_argv) / sizeof(mono_argv[0]);

  // Combine the above hardcoded command line arguments with those provided by hcom
  int finalArgc = hcom_argc + mono_argc;
  char **finalArgv = (char **) malloc(finalArgc * sizeof(char *));
  DEBUGASSERT(finalArgv != NULL);
  int i, j;

  // It appears that app_path needs to be last. So, copy all the hard code mono args
  // except for app_path, then hcom args and last app_path.
  for(i = 0; i < mono_argc - 1; i++)
    finalArgv[i] = mono_argv[i];
  for(j = 0; i < finalArgc - 1; i++, j++)
    finalArgv[i] = hcom_argv[j];
  finalArgv[i] = mono_argv[mono_argc - 1];

  // for(int check = 0; check < finalArgc; check++)
  // {
  //   syslog(2, "finalArgv[%d] is '%s'\n", check, finalArgv[check]);
  // }

  setenv("MONO_LOG_LEVEL", "warning", 1);
  setenv("MONO_GC_PARAMS", "max-heap-size=16m,nursery-size=512k,soft-heap-limit=4m,major=marksweep", 1);
  setenv("MONO_GC_DEBUG", "max-valloc-size=24M", 1);
  setenv("MONO_TRACE_LISTENER", "Console.Out", 1);

#ifdef CONFIG_MTD_PARTITION
  mono_set_assemblies_path("/meadow0");
#else
  mono_set_assemblies_path("/meadow");
#endif

  mono_dl_register_library("System.Native", system_native_mappings);
  mono_dl_register_library("nuttx", meadow_mappings);
  mono_dl_register_library("mbedtls", mbedtls_mappings);
#if defined (CONFIG_EXAMPLES_MEADOW_SQLITE)
  mono_dl_register_library("sqlite3", sqlite_mappings);
#endif

  // Note: This call may need to be somewhere within mono. However, it seems to work
  // well here. So far, one of the above calls hang up this thread before reaching
  // this point.
  //
  // Notify hcom that everything will run correctly. An error within this call
  // will prevent mono from starting.
  ret = hcom_mono_ctrl_mono_appears_to_be_running();
  if (ret < 0)
    return ret;

  // ret = mono_main_driver(mono_argc, mono_argv);
  ret = mono_main_driver(finalArgc, finalArgv);

  //
  //  If we sort out the application exit then we need to think about tidying
  //  up the memory allocations.
  //
  return ret;
}
