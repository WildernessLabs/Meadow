/****************************************************************************
 * examples/mono/ota.c
 *
 *   Copyright (C) Wilderness Labs. All rights reserved.
 *   Over-The-Air Update Application
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include <meadow/hcom_shared_common.h>
#include "../hcom/hcom_common.h"
#include "../hcom/misc/hcom_config_manager.h"

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
  closedir(dir);
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
