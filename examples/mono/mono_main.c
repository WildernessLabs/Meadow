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

typedef struct {
  const char *name;
  void *addr;
} MonoDlMapping;

#include "mappings-meadow.h"
#include "mappings-system-native.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void hcom_mono_ctrl_clear_mono_is_running_flag(void);

/****************************************************************************
 * mono_main
 ****************************************************************************/

extern int mono_main (int argc, char* argv[]);
extern void mono_dl_register_library(char *name, MonoDlMapping *mappings);

extern void symtab_initialize(void);

bool mono_should_run = true;

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int mono_main(int argc, char *argv[])
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

  usleep(300 * 1000);

  int ret;
  const char app_path[] = MONO_MEADOW_EXECUTABLE_APP_EXE;
  const char *mono_argv[] = {"mono", "--interp", app_path};
  const int mono_argc = sizeof(mono_argv) / sizeof(mono_argv[0]);

  setenv("MONO_LOG_LEVEL", "debug", 1);
  setenv("MONO_GC_PARAMS", "max-heap-size=8m,nursery-size=512k,soft-heap-limit=4m,major=marksweep", 1);

#ifdef CONFIG_MTD_PARTITION
  mono_set_assemblies_path("/meadow0");
#else
  mono_set_assemblies_path("/meadow");
#endif

  mono_dl_register_library("System.Native", system_native_mappings);
  mono_dl_register_library("nuttx", meadow_mappings);

  // Joao, this call may need to be somewhere within mono.
  // I put it here as a place holder, but it seems to do the job.
  // apparently some of the above calls hang up nuttx.
  //
  // Notify hcom that everything is running correctly
  hcom_mono_ctrl_clear_mono_is_running_flag();
  
  ret = mono_main_driver (mono_argc, mono_argv);

  return ret;
}
