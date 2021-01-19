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

#include "../../../mono/config.h"

#include <meadow/hcom_shared_common.h>
#include "../hcom/hcom_common.h"

typedef struct {
  const char *name;
  void *addr;
} MonoDlMapping;

#include "mappings-meadow.h"
#include "mappings-system-native.h"
#include "mappings-mbedtls.h"

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

  // Enable QSPI memory mapping mode.
  boardctl(BIOC_ENTER_MEMMAP, 0);

  // Check if Meadow.OS runtime is flashed at external flash.
  #define STM32_FMCBANK4_BASE  0x90000000     /* 0x90000000-0x9fffffff: FMC bank 4 */
  uint32_t signature = *((uint32_t*)STM32_FMCBANK4_BASE);
  if (signature != 0xDDCCBBAA)
  {
    syslog(LOG_ERR, "Mono runtime was not found flashed in external flash.\n");
    return 0;
  }

  // Copy the Meadow.OS runtime to SDRAM for execution.
  memcpy(CONFIG_HEAP2_BASE, STM32_FMCBANK4_BASE, 0x200000);

  boardctl(BIOC_EXIT_MEMMAP, 0);

  // Is this still needed?
  usleep(300 * 1000);

  int ret;
  char app_path[] = MONO_MEADOW_EXECUTABLE_APP_EXE;
  char *mono_argv[] = {"mono", "--interp", app_path};
  int mono_argc = sizeof(mono_argv) / sizeof(mono_argv[0]);

  // Combine the above hardcoded command line arguments with those provided by hcom
  char *finalArgv[8];
  int finalArgc = hcom_argc + mono_argc;
  DEBUGASSERT(finalArgc <= 8);
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

  setenv("MONO_LOG_LEVEL", "debug", 1);
  setenv("MONO_GC_PARAMS", "max-heap-size=8m,nursery-size=512k,soft-heap-limit=4m,major=marksweep", 1);

#ifdef CONFIG_MTD_PARTITION
  mono_set_assemblies_path("/meadow0");
#else
  mono_set_assemblies_path("/meadow");
#endif

  mono_dl_register_library("System.Native", system_native_mappings);
  mono_dl_register_library("nuttx", meadow_mappings);
  mono_dl_register_library("mbedtls", mbedtls_mappings);

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
  return ret;
}
