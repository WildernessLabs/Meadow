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

#include <meadow/hcom_bbreg_defn.h>

#include <meadow/meadow_os.h>
#include <meadow/meadow_os_battery_backed_domain.h>

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
 * Local defintions.
 ****************************************************************************/

#define MONO_CRASH_FILE CRASH_DIR "/" "mono_error.txt"
#define MONO_CRASH_FILE_SIZE 65536

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Local methods.
 ****************************************************************************/

/****************************************************************************
 * Name: induce_reset
 *
 * Description:
 *  Registered Mono error handler.  This will be registered with Mono in
 *  mono_main.
 * 
 *  The handler will eventually force the board to reset after the error
 *  message has been written to BKPSRAM and a file.
 * 
 *  Note that any issues recording the error message will result in the
 *  board being reset anyway.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void induce_reset(void)
{
  //
  //  First we record that the run-time has errored and that we are attempting Phase 1
  //  error recording.  This involves getting the full error message and writing as
  //  much as possible to BKPSRAM (limited to 4096 bytes maximum).
  //
  uint32_t fault_status;
  fault_status = (FAULT_LOGGING_RT_COMPONENT_ERRORED | FAULT_LOGGING_RT_PHASE1_STARTED);
  meadow_os_bbd_register_set_value(HCOM_NX_MEADOW_RESET_SOURCE_INFO_BBR_NUM, fault_status);
  //
  //  Now we actually start Phase 1.
  //
  const char *assertion_msg = monoeg_get_assertion_message();
  if (assertion_msg == NULL)
  {
    assertion_msg = "No Mono error message available";
  }

  meadow_os_bbd_strdup_to_sram(assertion_msg);
  fault_status |= FAULT_LOGGING_RT_PHASE1_COMPLETED | FAULT_LOGGING_RT_PHASE2_STARTED;
  meadow_os_bbd_register_set_value(HCOM_NX_MEADOW_RESET_SOURCE_INFO_BBR_NUM, fault_status);
  //
  //  Phase 1 marked as complete and Phase 2 marked as started. Start generating a file
  //  containing the full error message.  This may be longer than 4096 bytes hence writing
  //  to a file.
  //
  mkdir(CRASH_DIR, 0777);
  FILE *crash_file = fopen(MONO_CRASH_FILE, "w");
  if (crash_file)
  {
    if (assertion_msg)
    {
      //
      //  Assume Phase 2 will complete successfully.
      //
      fault_status |= FAULT_LOGGING_RT_PHASE2_COMPLETED;
      //
      int chars_left = strnlen(assertion_msg, MONO_CRASH_FILE_SIZE);
      char *p = (char *) assertion_msg;
      const char *end = assertion_msg + chars_left;
      while (p != end)
      {
        int write_count = fwrite(p, sizeof(char), chars_left, crash_file);
        if (write_count < 1) 
        {
          //
          //  Abandon on any error and record Phase 2 as possibly incomplete.
          //
          p = (char *) end;
          fault_status &= ~FAULT_LOGGING_RT_PHASE2_COMPLETED;
        }
        else
        {
          p += write_count;
          chars_left -= write_count;
        }
      }
    }
    fflush(crash_file);
    fclose(crash_file);
    meadow_os_bbd_register_set_value(HCOM_NX_MEADOW_RESET_SOURCE_INFO_BBR_NUM, fault_status);
  }
  else
  {
    fault_status |= FAULT_LOGGING_RT_FILE_ERROR;
    meadow_os_bbd_register_set_value(HCOM_NX_MEADOW_RESET_SOURCE_INFO_BBR_NUM, fault_status);
  }
  //
  //  Try to use syslog as well in case something is listening to the serial port.
  //
  syslog(LOG_ERR, "Mono error message: %s\n", assertion_msg);

  // TODO: If the runtime is asking for an abort, it is unstable, and any further execution
  // from any Mono thread is suspect, so waiting before resetting is a slight invitation for catastrophe.
  // However, this allows for HCOM and the user to catch a glimpse of the abort reason.
  // This should be removed when the Mono abort reason is saved across resets.
  fprintf(stderr, "Unrecoverable .NET Runtime error. Meadow will restart in 5 seconds\n");
  fflush (stderr);
  sleep(5);

  meadow_os_reset_board(0);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * mono_main
 ****************************************************************************/

extern int mono_main (int argc, char* argv[]);
extern void mono_dl_register_library(char *name, MonoDlMapping *mappings);
extern void monoeg_assertion_disable_global (void * abort_func);

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

  // When mono runtime aborts, crash the device
  monoeg_assertion_disable_global (induce_reset);

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
