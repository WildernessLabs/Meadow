/****************************************************************************
 * examples/mono/mono_main.c
 *
 *   Copyright (C) 2018-2026 Wilderness Labs. All rights reserved.
 *
 *   .NET 10 monovm hosting API integration.
 *   Replaces legacy Mono 6.9 entry point with monovm_initialize /
 *   monovm_execute_assembly / monovm_shutdown.
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

#include <meadow/hcom_shared_common.h>
#include "../hcom/hcom_common.h"

#include <meadow/hcom_bbreg_defn.h>

#include <meadow/meadow_os.h>
#include <meadow/meadow_os_battery_backed_domain.h>

#include "ota.h"

/****************************************************************************
 * P/Invoke mapping tables
 *
 * These are the same mapping tables from the legacy mono_main.c.
 * They are now consumed by the pinvoke_override callback instead of
 * mono_dl_register_library().
 ****************************************************************************/

typedef struct {
  const char *name;
  void *addr;
} MonoDlMapping;

#include "mappings-meadow.h"
/* TODO Track 05+: System.Native PAL needs to be built for .NET 10/NuttX.
 * The legacy mappings-system-native.h references SystemNative_* functions
 * from the old corefx PAL library which is not yet ported.
 * For now, the pinvoke_override callback returns NULL for "System.Native"
 * and managed code that calls System.Native will fail at runtime.
 */
/* #include "mappings-system-native.h" */
/* TODO Track 05+: mbedtls mappings need updating for .NET 10 */
/* #include "mappings-mbedtls.h" */
#if defined (CONFIG_EXAMPLES_MEADOW_SQLITE)
/* #include "mappings-sqlite.h" */
#endif

/****************************************************************************
 * .NET 10 monovm hosting API declarations
 ****************************************************************************/

extern int monovm_initialize(int propertyCount, const char **propertyKeys,
                             const char **propertyValues);
extern int monovm_execute_assembly(int argc, const char **argv,
                                   const char *managedAssemblyPath,
                                   unsigned int *exitCode);
extern int monovm_shutdown(int *latchedExitCode);

/****************************************************************************
 * External methods
 ****************************************************************************/

extern void symtab_initialize(void);

/****************************************************************************
 * Local definitions
 ****************************************************************************/

#define MONO_CRASH_FILE CRASH_DIR "/" "mono_error.txt"
#define MONO_CRASH_FILE_SIZE 65536

/* TPA list separator — colon on non-Windows */
#define TPA_SEPARATOR ":"

/* Max properties we pass to monovm_initialize */
#define MAX_PROPERTIES 5

/****************************************************************************
 * Private Data
 ****************************************************************************/

bool mono_should_run = true;

/****************************************************************************
 * Name: meadow_pinvoke_override
 *
 * Description:
 *   P/Invoke override callback for the .NET 10 monovm hosting API.
 *   Replaces the legacy mono_dl_register_library() mechanism.
 *
 *   When managed code does a DllImport("libname"), the runtime calls this
 *   function to resolve native symbols. We walk the appropriate mapping
 *   table based on the library name.
 *
 * Input Parameters:
 *   libraryName   - The native library name from DllImport
 *   entrypointName - The native function name to resolve
 *
 * Returned Value:
 *   Function pointer if found, NULL otherwise (runtime continues default search)
 *
 ****************************************************************************/

static void *meadow_pinvoke_override(const char *libraryName,
                                     const char *entrypointName)
{
  MonoDlMapping *mappings = NULL;

  if (strcmp(libraryName, "System.Native") == 0 ||
      strcmp(libraryName, "libSystem.Native") == 0)
    {
      /* TODO Track 05+: System.Native PAL not yet ported to .NET 10/NuttX */
      syslog(LOG_WARNING, "P/Invoke: System.Native not yet available\n");
      return NULL;
    }
  else if (strcmp(libraryName, "nuttx") == 0 ||
           strcmp(libraryName, "libnuttx") == 0)
    {
      mappings = meadow_mappings;
    }
  else if (strcmp(libraryName, "mbedtls") == 0 ||
           strcmp(libraryName, "libmbedtls") == 0)
    {
      /* TODO Track 05+: mbedtls mappings not yet ported */
      return NULL;
    }
#if defined (CONFIG_EXAMPLES_MEADOW_SQLITE)
  else if (strcmp(libraryName, "sqlite3") == 0 ||
           strcmp(libraryName, "libsqlite3") == 0)
    {
      /* TODO Track 05+: sqlite mappings not yet ported */
      return NULL;
    }
#endif
  else
    {
      return NULL;
    }

  for (MonoDlMapping *m = mappings; m->name != NULL; m++)
    {
      if (strcmp(m->name, entrypointName) == 0)
        {
          return m->addr;
        }
    }

  syslog(LOG_WARNING, "P/Invoke: '%s!%s' not found in mapping table\n",
         libraryName, entrypointName);
  return NULL;
}

/****************************************************************************
 * Name: build_tpa_list
 *
 * Description:
 *   Build the Trusted Platform Assemblies (TPA) list by enumerating all
 *   .dll files in the given base directory. Returns a colon-separated
 *   string of full paths suitable for the TRUSTED_PLATFORM_ASSEMBLIES
 *   property.
 *
 * Input Parameters:
 *   base_path - Directory to scan (e.g., "/meadow0")
 *
 * Returned Value:
 *   Heap-allocated TPA string, or NULL on failure. Caller must free().
 *
 ****************************************************************************/

static char *build_tpa_list(const char *base_path)
{
  DIR *dir;
  struct dirent *entry;
  size_t total_len = 0;
  size_t base_len = strlen(base_path);
  int count = 0;

  dir = opendir(base_path);
  if (dir == NULL)
    {
      syslog(LOG_ERR, "TPA: Cannot open directory '%s': %d\n",
             base_path, errno);
      return NULL;
    }

  /* First pass: calculate total string length needed */

  while ((entry = readdir(dir)) != NULL)
    {
      size_t nlen = strlen(entry->d_name);
      if (nlen > 4 && strcmp(entry->d_name + nlen - 4, ".dll") == 0)
        {
          /* base_path + "/" + filename + separator */
          total_len += base_len + 1 + nlen + 1;
          count++;
        }
    }

  if (count == 0)
    {
      closedir(dir);
      syslog(LOG_WARNING, "TPA: No .dll files found in '%s'\n", base_path);
      return NULL;
    }

  char *tpa = (char *)malloc(total_len + 1);
  if (tpa == NULL)
    {
      closedir(dir);
      syslog(LOG_ERR, "TPA: Cannot allocate %zu bytes\n", total_len + 1);
      return NULL;
    }

  tpa[0] = '\0';

  /* Second pass: build the string */

  rewinddir(dir);
  int first = 1;
  while ((entry = readdir(dir)) != NULL)
    {
      size_t nlen = strlen(entry->d_name);
      if (nlen > 4 && strcmp(entry->d_name + nlen - 4, ".dll") == 0)
        {
          if (!first)
            {
              strcat(tpa, TPA_SEPARATOR);
            }

          strcat(tpa, base_path);
          strcat(tpa, "/");
          strcat(tpa, entry->d_name);
          first = 0;
        }
    }

  closedir(dir);
  syslog(LOG_INFO, "TPA: Found %d assemblies in '%s'\n", count, base_path);
  return tpa;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mono_main
 *
 * Description:
 *   .NET 10 runtime entry point. Called by hcom_mono_ctrl_start_mono_main()
 *   via task_create(). Replaces the legacy Mono 6.9 mono_main that called
 *   mono_main_driver().
 *
 *   Initialization sequence:
 *     1. Copy mono runtime binary to SDRAM (if needed)
 *     2. Build TPA list from deployed assemblies
 *     3. Set up monovm properties (TPA, APP_PATHS, PINVOKE_OVERRIDE)
 *     4. Call monovm_initialize()
 *     5. Notify HCOM that mono is running
 *     6. Call monovm_execute_assembly() if app assembly exists
 *     7. Call monovm_shutdown()
 *
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int hcom_argc, FAR char *hcom_argv[])
#else
int meadow_mono_main(int hcom_argc, char *hcom_argv[])
#endif
{
  int ret;

  syslog(LOG_NOTICE, "mono_main: .NET 10 monovm hosting API startup\n");

  /* Initialize the symbol table */

  symtab_initialize();

  /* Copy the mono runtime binary to SDRAM */

  if (hcom_via_nx_copy_mono_runtime_to_ram() < 0)
    {
      syslog(LOG_ERR, "Mono runtime is not present or is invalid.\n");
      return -1;
    }

  syslog(LOG_INFO, "Mono runtime copied into RAM.\n");

  /* Zero .mono_bss in SDRAM — Mono runtime static globals.
   * This section is not covered by the regular .bss zeroing (which only
   * handles internal SRAM).  Without this, stale pointers surviving a
   * software reset cause heap corruption when monovm_initialize calls
   * g_strfreev on dangling pointers from the previous boot.
   */

  {
    extern uint32_t _s_mono_bss;
    extern uint32_t _e_mono_bss;
    uint32_t *mdest = &_s_mono_bss;
    uint32_t *mend  = &_e_mono_bss;

    while (mdest < mend)
      {
        *mdest++ = 0;
      }

    syslog(LOG_INFO, "Cleared .mono_bss: %u bytes\n",
           (unsigned)((uint8_t *)mend - (uint8_t *)&_s_mono_bss));
  }

  /* Set environment variables for the runtime */

  setenv("MONO_LOG_LEVEL", "warning", 1);
  setenv("MONO_ENV_OPTIONS", "--interpreter", 1);
  setenv("TMPDIR", "/meadow0/Temp", 1);

  /* Build the Trusted Platform Assemblies list */

  char *tpa_list = build_tpa_list(MONO_MEADOW_EXECUTABLE_PARTITION_NAME);
  if (tpa_list == NULL)
    {
      syslog(LOG_ERR, "Failed to build TPA list — no assemblies deployed?\n");
      return -1;
    }

  /* Convert the P/Invoke override function pointer to a string.
   * monovm_initialize parses it back via strtoull().
   */

  char pinvoke_override_str[32];
  snprintf(pinvoke_override_str, sizeof(pinvoke_override_str),
           "0x%lx", (unsigned long)(uintptr_t)meadow_pinvoke_override);

  /* Set up properties for monovm_initialize */

  const char *property_keys[] = {
    "TRUSTED_PLATFORM_ASSEMBLIES",
    "APP_PATHS",
    "NATIVE_DLL_SEARCH_DIRECTORIES",
    "PINVOKE_OVERRIDE",
  };

  const char *property_values[] = {
    tpa_list,
    MONO_MEADOW_EXECUTABLE_PARTITION_NAME,
    MONO_MEADOW_EXECUTABLE_PARTITION_NAME,
    pinvoke_override_str,
  };

  int property_count = sizeof(property_keys) / sizeof(property_keys[0]);

  syslog(LOG_INFO, "Calling monovm_initialize with %d properties...\n",
         property_count);
  syslog(LOG_INFO, "  TPA: %s\n", tpa_list);
  syslog(LOG_INFO, "  APP_PATHS: %s\n", MONO_MEADOW_EXECUTABLE_PARTITION_NAME);
  syslog(LOG_INFO, "  PINVOKE_OVERRIDE: %s\n", pinvoke_override_str);

  /* Initialize the .NET 10 monovm runtime */

  ret = monovm_initialize(property_count, property_keys, property_values);
  if (ret != 0)
    {
      syslog(LOG_ERR, "monovm_initialize failed: 0x%08x\n", ret);
      free(tpa_list);
      return -1;
    }

  syslog(LOG_NOTICE, "monovm_initialize succeeded\n");

  /* Notify HCOM that mono appears to be running.
   * This sets up stdout/stderr redirection, clears the lockup BBR bit,
   * and reconfigures the blue LED.
   */

  ret = hcom_mono_ctrl_mono_appears_to_be_running();
  if (ret < 0)
    {
      syslog(LOG_ERR, "hcom_mono_ctrl_mono_appears_to_be_running failed: %d\n",
             ret);
      free(tpa_list);
      return ret;
    }

  /* Change to the app directory */

  chdir(MONO_MEADOW_EXECUTABLE_PARTITION_NAME);

  /* Check if the app assembly exists before trying to execute it */

  char *app_path = MONO_MEADOW_EXECUTABLE_APP_EXE;
  int app_fd = open(app_path, O_RDONLY);
  if (app_fd < 0)
    {
      syslog(LOG_WARNING, "App assembly '%s' not found — "
             "runtime initialized but no app to execute\n", app_path);
      /* Runtime initialized successfully, just no app to run.
       * Shut down cleanly.
       */
    }
  else
    {
      close(app_fd);
      syslog(LOG_NOTICE, "Executing assembly: %s\n", app_path);

      unsigned int exit_code = 0;
      ret = monovm_execute_assembly(0, NULL, app_path, &exit_code);
      if (ret != 0)
        {
          syslog(LOG_ERR, "monovm_execute_assembly failed: 0x%08x\n", ret);
        }
      else
        {
          syslog(LOG_NOTICE, "Assembly execution completed, exit code: %u\n",
                 exit_code);
        }
    }

  /* Shutdown the runtime */

  int latched_exit_code = 0;
  monovm_shutdown(&latched_exit_code);
  syslog(LOG_NOTICE, "monovm_shutdown complete, latched exit code: %d\n",
         latched_exit_code);

  free(tpa_list);
  return 0;
}
