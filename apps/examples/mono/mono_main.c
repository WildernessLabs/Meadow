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

#include "../../../mono/config.h"

#include <meadow/hcom_shared_common.h>
#include "../hcom/hcom_common.h"
#include "../hcom/misc/hcom_config_manager.h"

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

int update_file(const char *srcpath, const char *destpath, const char *rollbackpath)
{
  syslog(LOG_ERR, "%s -> %s\n", srcpath, destpath);
  struct stat statbuf;
  int ret;
  if (stat(destpath, &statbuf) != 0)
  {
    if (rollbackpath)
    {
      ret = update_file(destpath, rollbackpath, NULL);
      if (ret != 0)
        return ret;
    }
    else
    {
      ret = unlink(destpath);
      if (ret != 0)
        return ret;
    }
  }
  return rename(srcpath, destpath);
}

int deltree(const char *path)
{
  DIR *dir = opendir(path);
  struct dirent *entry;

  if (!dir)
    return 0;

  bool error = false;

  while ((entry = readdir(dir)) != NULL && !error)
  {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
    if (DIRENT_ISDIRECTORY(entry->d_type))
    {
      deltree(full_path);
    }
    if (DIRENT_ISFILE(entry->d_type))
    {
      unlink(full_path);
    }
  }
  closedir(path);
  rmdir(path);

  return 0;
}

int app_update(void)
{
  DIR *update_dir = opendir(UPDATE_APP_DIR);
  struct dirent *entry;

  if (!update_dir)
    return 0;

  bool error = false;
  mkdir(ROLLBACK_DIR, 0777);

  // TODO: Recursive copying
  while ((entry = readdir(update_dir)) != NULL && !error)
  {
    if (DIRENT_ISFILE(entry->d_type))
    {
      char source_path[PATH_MAX];
      char target_path[PATH_MAX];
      char rollback_path[PATH_MAX];
      snprintf(source_path, sizeof(source_path), "%s%s", UPDATE_APP_DIR, entry->d_name);
      snprintf(target_path, sizeof(target_path), "/meadow0/%s", entry->d_name);
      snprintf(rollback_path, sizeof(target_path), "%s%s", ROLLBACK_DIR, entry->d_name);
      if (update_file(source_path, target_path, rollback_path) != 0)
        error = true;
    }
  }
  closedir(update_dir);
  if (error) // Invalid update; roll back
  {
    deltree(UPDATE_APP_DIR);
    DIR *rollback_dir = opendir(ROLLBACK_DIR);

    if (!rollback_dir)
      return 0;

    // TODO: Recursive copying
    while ((entry = readdir(rollback_dir)) != NULL && !error)
    {
      if (DIRENT_ISFILE(entry->d_type))
      {
        char source_path[PATH_MAX];
        char target_path[PATH_MAX];
        snprintf(source_path, sizeof(source_path), "%s%s", ROLLBACK_DIR, entry->d_name);
        snprintf(target_path, sizeof(target_path), "/meadow0/%s", entry->d_name);
        if (update_file(source_path, target_path, NULL) != 0)
        { 
          syslog(LOG_ERR, "Error rolling back update, failed to restore %s to %s\n", source_path, target_path);
        }
      }
    }
    closedir(rollback_dir);
  }
  return 1;

}

#define OS_BINARY_SIGNATURE_EXT ".sig"

static int update_os_part1(void)
{
  return hcom_via_nx_update_OS1();
}

static int update_os_part2(void)
{
  return hcom_via_nx_update_OS2();
}

static int validate_signature(const char *path)
{
  // mbedtls_pk_verify ()
  return -1;
}

#define OS_PART1_BINARY_FILENAME HCOM_NX_FS_NUTTX_UPDATE_FILENAME
#define OS_PART2_BINARY_FILENAME HCOM_NX_FS_MONO_RUNTIME_FILENAME

int os_update(void)
{
  DIR *update_dir = opendir(UPDATE_OS_DIR);
  struct dirent *entry;

  if (!update_dir)
    return 0;

  bool part1_rollback_happening = false; // TODO: Check OTADATA for rollback

  if (part1_rollback_happening)
  {
    deltree(UPDATE_OS_DIR);
    return -1;
  }

  bool part1_update = false;
  bool part2_update = false;

  while ((entry = readdir(update_dir)) != NULL)
  {
    if (DIRENT_ISFILE(entry->d_type))
    {
      if (strncmp(entry->d_name, OS_PART1_BINARY_FILENAME, strnlen(OS_PART1_BINARY_FILENAME, PATH_MAX)))
        part1_update = true;
      if (strncmp(entry->d_name, OS_PART2_BINARY_FILENAME, strnlen(OS_PART2_BINARY_FILENAME, PATH_MAX)))
        part2_update = true;
    }
  }
  closedir(update_dir);

  if (part1_update && part2_update)
  {
    validate_signature(OS_PART1_BINARY_FILENAME);
    validate_signature(OS_PART2_BINARY_FILENAME);
    update_os_part1();
    // TODO: reset
  }

  if (!part1_update && part2_update)
  {
    // TODO: Confirm Part 1 update
    return update_os_part2();
  }

  return -2;
}

#ifdef CONFIG_BUILD_KERNEL
int main(int hcom_argc, FAR char *hcom_argv[])
#else
int mono_main(int hcom_argc, char *hcom_argv[])
#endif
{
  os_update();
  app_update();
  // Normal mono startup follows
  symtab_initialize();

  // meadow_configuration_t *config = hcom_config_get_pointer();
  // uint8_t mono_is_valid = config->mono_is_valid;
  // hcom_config_free_resources(config);
  // if (mono_is_valid == 0)
  // {
  //   syslog(LOG_ERR, "Mono runtime is not present or valid.\n");
  //   return -1;
  // }

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
  if (strcmp(hcom_argv[hcom_argc - 1], MONO_OPTION_JIT) == 0)
  {
    hcom_argc--;
  }
  else
  {
    if (strcmp(hcom_argv[hcom_argc -1], MONO_OPTION_AOT) == 0)
    {
      // Do AOT stuff here.
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
