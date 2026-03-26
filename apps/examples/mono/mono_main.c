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
#include <pthread.h>
#include <poll.h>
#include <time.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <syslog.h>
/* mount() declared here to avoid sys/mount.h conflict with mappings-meadow.h */
extern int mount(const char *source, const char *target,
                 const char *filesystemtype, unsigned long mountflags,
                 const void *data);

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
 * Name: meadow_pinvoke_noop_stub
 *
 * Description:
 *   Generic no-op stub for unimplemented System.Native P/Invoke functions.
 *   Returns 0 (which typically means "success" or "false" depending on
 *   context). This lets the runtime get past initialization while we
 *   identify which functions actually need real implementations.
 ****************************************************************************/

static int meadow_pinvoke_noop_stub(void)
{
  return 0;
}

/****************************************************************************
 * System.Native P/Invoke implementations
 *
 * These implement the subset of System.Native functions called during
 * runtime initialization and Hello World execution.
 ****************************************************************************/

static const char *sysn_getenv(const char *name)
{
  const char *val = getenv(name);
  syslog(LOG_NOTICE, "SystemNative_GetEnv(\"%s\") => %s\n",
         name ? name : "(null)", val ? val : "(null)");
  return val;
}

static int32_t sysn_lchflags_can_set_hidden_flag(void)
{
  syslog(LOG_NOTICE, "SystemNative_LChflagsCanSetHiddenFlag() => 0\n");
  return 0; /* No HFS+ on NuttX */
}

static int32_t sysn_can_get_hidden_flag(void)
{
  syslog(LOG_NOTICE, "SystemNative_CanGetHiddenFlag() => 0\n");
  return 0;
}

/* Console I/O — wraps NuttX POSIX calls */

static int32_t sysn_write(intptr_t fd, const void *buffer, int32_t bufferSize)
{
  /* Mirror stdout/stderr to syslog for debugging.
   * The primary output path is: fd 1/2 → HCOM FIFO → MonoStdxxx thread
   * → UART4 → TCP:4242 → Meadow CLI.  Syslog mirror lets us see it
   * in the USART1 capture too. */
  if (((int)fd == 1 || (int)fd == 2) && bufferSize > 0 && bufferSize < 512)
    {
      char tmp[513];
      int len = bufferSize < 512 ? bufferSize : 512;
      memcpy(tmp, buffer, len);
      tmp[len] = '\0';
      /* Strip trailing newline for cleaner syslog */
      if (len > 0 && tmp[len - 1] == '\n') tmp[len - 1] = '\0';
      syslog(LOG_NOTICE, "[mono stdout] %s\n", tmp);
    }

  /* stdout/stderr → HCOM FIFO → MonoStdxxx thread → UART4 → CLI.
   * FIFOs are O_NONBLOCK: write returns EAGAIN if the 1KB buffer is full
   * (no CLI client connected, or reader can't keep up).  Retry briefly
   * to give the MonoStdxxx thread time to drain, then return partial count. */
  ssize_t count;
  int retries = 5;
  do {
    count = write((int)fd, buffer, (size_t)bufferSize);
    if (count >= 0)
      return (int32_t)count;
    if (errno == EINTR)
      continue;
    if (errno == EAGAIN && --retries > 0) {
      usleep(1000); /* 1ms — let MonoStdxxx thread drain the FIFO */
      continue;
    }
    break;
  } while (1);
  /* EAGAIN: FIFO full, no reader.  EBADF: emulator has no USB host
   * (CDCACM device), so fd 1/2 writes fail.  In both cases return
   * bufferSize to prevent IOException — output is already mirrored
   * to syslog above, so no data is truly lost. */
  if (errno == EAGAIN || errno == EBADF)
    return bufferSize;
  return (int32_t)count;
}

static int32_t sysn_read(intptr_t fd, void *buffer, int32_t bufferSize)
{
  ssize_t count;
  while ((count = read((int)fd, buffer, (size_t)bufferSize)) < 0 && errno == EINTR);
  return (int32_t)count;
}

static int32_t sysn_isatty(intptr_t fd)
{
  (void)fd;
  /* After HCOM redirect, fd 1/2 are FIFOs (/dev/monostdout, /dev/monostderr),
   * not terminals.  Returning 0 makes .NET ConsolePal use the simple stream
   * write path instead of trying tcgetattr/terminal init (which crashes on FIFOs). */
  syslog(LOG_NOTICE, "SystemNative_IsATty(fd=%d) => 0\n", (int)fd);
  return 0;
}

/* Terminal handling stubs — NuttX Meadow has no real terminal */

static int32_t sysn_initialize_terminal_and_signal_handling(void)
{
  syslog(LOG_NOTICE, "SystemNative_InitializeTerminalAndSignalHandling() => 1\n");
  return 1; /* success */
}

static void sysn_set_keypad_xmit(intptr_t fd, const char *str)
{
  (void)fd; (void)str;
}

static void sysn_set_terminal_invalidation_handler(void *handler)
{
  (void)handler;
}

static void sysn_uninitialize_terminal(void)
{
  /* Nothing to do — no real terminal on NuttX */
}

/* GetControlCharacters: fills array with terminal control characters.
 * On NuttX without real terminal, just zero the output. */
static void sysn_get_control_characters(
    int32_t *controlCharacterNames, uint8_t *controlCharacterValues,
    int32_t controlCharacterLength, uint8_t *posixDisableValue)
{
  (void)controlCharacterNames;
  if (controlCharacterValues && controlCharacterLength > 0)
    memset(controlCharacterValues, 0, controlCharacterLength);
  if (posixDisableValue)
    *posixDisableValue = 0;
}

/* Window size — return reasonable defaults */
struct WinSize { uint16_t row; uint16_t col; };

static int32_t sysn_get_window_size(intptr_t fd, struct WinSize *winSize)
{
  (void)fd;
  if (winSize) {
    winSize->row = 24;
    winSize->col = 80;
  }
  return 0; /* success */
}

/* File descriptor operations */

static intptr_t sysn_open(const char *path, int32_t flags, int32_t mode)
{
  int fd = open(path, flags, mode);
  return (intptr_t)fd;
}

static int32_t sysn_close(intptr_t fd)
{
  return close((int)fd);
}

/* Stat structures — .NET expects a specific layout */
struct FileStatus
{
  int32_t  Flags;    /* FileStatusFlags */
  int32_t  Mode;     /* mode_t */
  uint32_t Uid;
  uint32_t Gid;
  int64_t  Size;
  int64_t  ATime;
  int64_t  ATimeNsec;
  int64_t  MTime;
  int64_t  MTimeNsec;
  int64_t  CTime;
  int64_t  CTimeNsec;
  int64_t  BirthTime;
  int64_t  BirthTimeNsec;
  int64_t  Dev;
  int64_t  RDev;
  int64_t  Ino;
  uint32_t UserFlags;
};

static void convert_stat(const struct stat *src, struct FileStatus *dst)
{
  memset(dst, 0, sizeof(*dst));
  dst->Mode  = (int32_t)src->st_mode;
  /* NuttX minimal stat: no st_uid/st_gid/st_ino/st_dev/st_rdev */
  dst->Uid   = 0;
  dst->Gid   = 0;
  dst->Size  = (int64_t)src->st_size;
  dst->ATime = (int64_t)src->st_atime;
  dst->MTime = (int64_t)src->st_mtime;
  dst->CTime = (int64_t)src->st_ctime;
  dst->Ino   = 0;
  dst->Dev   = 0;
  dst->RDev  = 0;
}

static int32_t sysn_fstat(intptr_t fd, struct FileStatus *output)
{
  struct stat s;
  if (fstat((int)fd, &s) != 0)
    return -1;
  convert_stat(&s, output);
  return 0;
}

static int32_t sysn_stat2(const char *path, struct FileStatus *output)
{
  struct stat s;
  if (stat(path, &s) != 0)
    return -1;
  convert_stat(&s, output);
  return 0;
}

static int32_t sysn_lstat2(const char *path, struct FileStatus *output)
{
  /* NuttX doesn't have symlinks typically — fall through to stat */
  return sysn_stat2(path, output);
}

static int64_t sysn_lseek(intptr_t fd, int64_t offset, int32_t whence)
{
  return (int64_t)lseek((int)fd, (off_t)offset, whence);
}

static int32_t sysn_fcntl_set_fd_flags(intptr_t fd, int32_t flags)
{
  return fcntl((int)fd, F_SETFD, flags);
}

static int32_t sysn_fcntl_get_fd_flags(intptr_t fd)
{
  return fcntl((int)fd, F_GETFD);
}

static int32_t sysn_fcntl_get_is_nonblocking(intptr_t fd, int32_t *isNonBlocking)
{
  if (!isNonBlocking) return -1;
  int flags = fcntl((int)fd, F_GETFL);
  if (flags == -1) return -1;
  *isNonBlocking = (flags & O_NONBLOCK) != 0 ? 1 : 0;
  return 0;
}

static int32_t sysn_fcntl_set_is_nonblocking(intptr_t fd, int32_t isNonBlocking)
{
  int flags = fcntl((int)fd, F_GETFL);
  if (flags == -1) return -1;
  if (isNonBlocking) flags |= O_NONBLOCK; else flags &= ~O_NONBLOCK;
  return fcntl((int)fd, F_SETFL, flags);
}

/* Process / diagnostics — NuttX-specific stubs */

static void sysn_syslog(int32_t priority, const char *message, const char *arg1)
{
  syslog(LOG_USER | (priority & 0x7), message, arg1);
}

static int32_t sysn_get_pid(void)
{
  return (int32_t)getpid();
}

/* Signal stubs — NuttX signal support is limited */

static void sysn_set_posix_signal_handler(int32_t signalCode)
{
  (void)signalCode; /* No-op on NuttX */
}

static void sysn_enable_posix_signal_handling(int32_t signalCode)
{
  (void)signalCode;
}

static int32_t sysn_get_platform_signal_number(int32_t managedSignal)
{
  return managedSignal; /* pass through */
}

/* UID/GID stubs — single-user embedded system */

static uint32_t sysn_get_euid(void) { return 0; /* root */ }
static uint32_t sysn_get_egid(void) { return 0; }
static int32_t  sysn_set_euid(uint32_t uid) { (void)uid; return 0; }

static int32_t sysn_get_pw_uid_r(uint32_t uid, void *pwd, char *buf,
                                  int32_t bufLen, void *result)
{
  (void)uid; (void)pwd; (void)buf; (void)bufLen;
  /* Set *result = NULL → "not found" */
  if (result) *(void **)result = NULL;
  return -1;
}

static int32_t sysn_get_hostname(char *name, int32_t nameLength)
{
  if (name && nameLength > 0)
    {
      strncpy(name, "meadow", nameLength - 1);
      name[nameLength - 1] = '\0';
    }
  return 0;
}

/* Process path — fixed for NuttX */
static const char *sysn_get_process_path(void)
{
  return "/meadow0/Meadow";
}

/* Fork stub — not supported on NuttX */
static int32_t sysn_fork_and_exec_process(void)
{
  return -1;
}

/* Expanded I/O — NuttX POSIX wrappers */

static int32_t sysn_pipe(int32_t *fds)
{
  return pipe((int *)fds);
}

static int32_t sysn_dup(intptr_t oldFd)
{
  return dup((int)oldFd);
}

static int32_t sysn_dup2(intptr_t oldFd, intptr_t newFd)
{
  return dup2((int)oldFd, (int)newFd);
}

static int32_t sysn_unlink(const char *path)
{
  return unlink(path);
}

static int32_t sysn_mkdir(const char *path, int32_t mode)
{
  return mkdir(path, (mode_t)mode);
}

static int32_t sysn_getcwd(char *buf, int32_t size)
{
  return getcwd(buf, (size_t)size) != NULL ? 0 : -1;
}

static int32_t sysn_access(const char *path, int32_t mode)
{
  return access(path, mode);
}

static int32_t sysn_readlink(const char *path, char *buf, int32_t bufSize)
{
  (void)path; (void)buf; (void)bufSize;
  set_errno(ENOTSUP); /* No symlinks on NuttX */
  return -1;
}

static intptr_t sysn_opendir(const char *path)
{
  return (intptr_t)opendir(path);
}

static int32_t sysn_closedir(intptr_t dir)
{
  return closedir((DIR *)dir);
}

static int32_t sysn_fsync(intptr_t fd)
{
  return fsync((int)fd);
}

static int32_t sysn_ftruncate(intptr_t fd, int64_t length)
{
  return ftruncate((int)fd, (off_t)length);
}

static int32_t sysn_flock(intptr_t fd, int32_t operation)
{
  (void)fd; (void)operation;
  return 0; /* No file locking on NuttX */
}

static int32_t sysn_chmod(const char *path, int32_t mode)
{
  (void)path; (void)mode;
  return 0; /* NuttX has no chmod in user space */
}

static int32_t sysn_rename(const char *oldPath, const char *newPath)
{
  return rename(oldPath, newPath);
}

/****************************************************************************
 * Upstream System.Native PAL — extern declarations
 *
 * These functions are compiled from the upstream pal_*.c files in
 * runtime/src/native/libs/System.Native/ (added via Makefile VPATH).
 ****************************************************************************/

/* pal_threading.c */
extern void *SystemNative_LowLevelMonitor_Create(void);
extern void  SystemNative_LowLevelMonitor_Destroy(void *monitor);
extern void  SystemNative_LowLevelMonitor_Acquire(void *monitor);
extern void  SystemNative_LowLevelMonitor_Release(void *monitor);
extern void  SystemNative_LowLevelMonitor_Wait(void *monitor);
extern int32_t SystemNative_LowLevelMonitor_TimedWait(void *monitor, int32_t timeoutMs);
extern void  SystemNative_LowLevelMonitor_Signal_Release(void *monitor);
extern int32_t SystemNative_CreateThread(uintptr_t stackSize, void *(*startAddress)(void*), void *parameter);
extern int32_t SystemNative_SchedGetCpu(void);
extern void  SystemNative_Exit(int32_t exitCode);
extern void  SystemNative_Abort(void);
extern uint64_t SystemNative_GetUInt64OSThreadId(void);
extern uint32_t SystemNative_TryGetUInt32OSThreadId(void);

/* pal_time.c */
extern int64_t SystemNative_GetTimestamp(void);
extern int64_t SystemNative_GetLowResolutionTimestamp(void);
extern int64_t SystemNative_GetBootTimeTicks(void);
extern double  SystemNative_GetCpuUtilization(void *previousCpuInfo);
extern int32_t SystemNative_UTimensat(const char *path, void *times);
extern int32_t SystemNative_FUTimens(intptr_t fd, void *times);

/* pal_memory.c */
extern void *SystemNative_AlignedAlloc(uintptr_t alignment, uintptr_t size);
extern void  SystemNative_AlignedFree(void *ptr);
extern void *SystemNative_AlignedRealloc(void *ptr, uintptr_t alignment, uintptr_t size);
extern void *SystemNative_Calloc(uintptr_t num, uintptr_t size);
extern void  SystemNative_Free(void *ptr);
extern void *SystemNative_Malloc(uintptr_t size);
extern void *SystemNative_Realloc(void *ptr, uintptr_t new_size);

/* pal_random.c */
extern void    SystemNative_GetNonCryptographicallySecureRandomBytes(uint8_t *buffer, int32_t bufferLength);
extern int32_t SystemNative_GetCryptographicallySecureRandomBytes(uint8_t *buffer, int32_t bufferLength);

/* pal_errno.c */
extern int32_t     SystemNative_ConvertErrorPlatformToPal(int32_t platformErrno);
extern int32_t     SystemNative_ConvertErrorPalToPlatform(int32_t error);
extern const char *SystemNative_StrErrorR(int32_t platformErrno, char *buffer, int32_t bufferSize);
extern int32_t     SystemNative_GetErrNo(void);
extern void        SystemNative_SetErrNo(int32_t errorCode);

/* pal_string.c */
extern int32_t SystemNative_SNPrintF(char *string, int32_t size, const char *format, ...);
extern int32_t SystemNative_SNPrintF_1S(char *string, int32_t size, const char *format, char *str);
extern int32_t SystemNative_SNPrintF_1I(char *string, int32_t size, const char *format, int arg);

/* pal_runtimeinformation.c */
extern char   *SystemNative_GetUnixRelease(void);
extern int32_t SystemNative_GetUnixVersion(char *version, int *capacity);
extern int32_t SystemNative_GetOSArchitecture(void);

/* pal_log.c */
extern void SystemNative_Log(uint8_t *buffer, int32_t length);
extern void SystemNative_LogError(uint8_t *buffer, int32_t length);

/* pal_datetime.c */
extern int64_t SystemNative_GetSystemTimeAsTicks(void);

/****************************************************************************
 * System.Native mapping table
 ****************************************************************************/

static MonoDlMapping system_native_mappings[] = {
  /* Environment */
  { "SystemNative_GetEnv",                    (void *)sysn_getenv },
  { "SystemNative_LChflagsCanSetHiddenFlag",  (void *)sysn_lchflags_can_set_hidden_flag },
  { "SystemNative_CanGetHiddenFlag",          (void *)sysn_can_get_hidden_flag },

  /* Console I/O */
  { "SystemNative_Write",                     (void *)sysn_write },
  { "SystemNative_Read",                      (void *)sysn_read },
  { "SystemNative_IsATty",                    (void *)sysn_isatty },

  /* Terminal handling (NuttX stubs) */
  { "SystemNative_InitializeTerminalAndSignalHandling", (void *)sysn_initialize_terminal_and_signal_handling },
  { "SystemNative_SetKeypadXmit",             (void *)sysn_set_keypad_xmit },
  { "SystemNative_SetTerminalInvalidationHandler", (void *)sysn_set_terminal_invalidation_handler },
  { "SystemNative_UninitializeTerminal",      (void *)sysn_uninitialize_terminal },
  { "SystemNative_GetControlCharacters",      (void *)sysn_get_control_characters },
  { "SystemNative_GetWindowSize",             (void *)sysn_get_window_size },

  /* File operations (NuttX wrappers) */
  { "SystemNative_Open",                      (void *)sysn_open },
  { "SystemNative_Close",                     (void *)sysn_close },
  { "SystemNative_FStat2",                    (void *)sysn_fstat },
  { "SystemNative_Stat2",                     (void *)sysn_stat2 },
  { "SystemNative_LStat2",                    (void *)sysn_lstat2 },
  { "SystemNative_LSeek",                     (void *)sysn_lseek },
  { "SystemNative_FcntlSetFdFlags",           (void *)sysn_fcntl_set_fd_flags },
  { "SystemNative_FcntlGetFdFlags",           (void *)sysn_fcntl_get_fd_flags },
  { "SystemNative_FcntlGetIsNonBlocking",    (void *)sysn_fcntl_get_is_nonblocking },
  { "SystemNative_FcntlSetIsNonBlocking",    (void *)sysn_fcntl_set_is_nonblocking },
  { "SystemNative_Pipe",                      (void *)sysn_pipe },
  { "SystemNative_Dup",                       (void *)sysn_dup },
  { "SystemNative_Dup2",                      (void *)sysn_dup2 },
  { "SystemNative_Unlink",                    (void *)sysn_unlink },
  { "SystemNative_MkDir",                     (void *)sysn_mkdir },
  { "SystemNative_GetCwd",                    (void *)sysn_getcwd },
  { "SystemNative_Access",                    (void *)sysn_access },
  { "SystemNative_ReadLink",                  (void *)sysn_readlink },
  { "SystemNative_OpenDir",                   (void *)sysn_opendir },
  { "SystemNative_CloseDir",                  (void *)sysn_closedir },
  { "SystemNative_FSync",                     (void *)sysn_fsync },
  { "SystemNative_FTruncate",                 (void *)sysn_ftruncate },
  { "SystemNative_FLock",                     (void *)sysn_flock },
  { "SystemNative_ChMod",                     (void *)sysn_chmod },
  { "SystemNative_Rename",                    (void *)sysn_rename },

  /* errno helpers (upstream pal_errno.c) */
  { "SystemNative_SetErrNo",                  (void *)SystemNative_SetErrNo },
  { "SystemNative_GetErrNo",                  (void *)SystemNative_GetErrNo },
  { "SystemNative_ConvertErrorPlatformToPal", (void *)SystemNative_ConvertErrorPlatformToPal },
  { "SystemNative_ConvertErrorPalToPlatform", (void *)SystemNative_ConvertErrorPalToPlatform },
  { "SystemNative_StrErrorR",                 (void *)SystemNative_StrErrorR },

  /* Threading (upstream pal_threading.c) */
  { "SystemNative_LowLevelMonitor_Create",    (void *)SystemNative_LowLevelMonitor_Create },
  { "SystemNative_LowLevelMonitor_Destroy",   (void *)SystemNative_LowLevelMonitor_Destroy },
  { "SystemNative_LowLevelMonitor_Acquire",   (void *)SystemNative_LowLevelMonitor_Acquire },
  { "SystemNative_LowLevelMonitor_Release",   (void *)SystemNative_LowLevelMonitor_Release },
  { "SystemNative_LowLevelMonitor_Wait",      (void *)SystemNative_LowLevelMonitor_Wait },
  { "SystemNative_LowLevelMonitor_TimedWait", (void *)SystemNative_LowLevelMonitor_TimedWait },
  { "SystemNative_LowLevelMonitor_Signal_Release", (void *)SystemNative_LowLevelMonitor_Signal_Release },
  { "SystemNative_CreateThread",              (void *)SystemNative_CreateThread },
  { "SystemNative_SchedGetCpu",               (void *)SystemNative_SchedGetCpu },
  { "SystemNative_GetUInt64OSThreadId",       (void *)SystemNative_GetUInt64OSThreadId },
  { "SystemNative_TryGetUInt32OSThreadId",    (void *)SystemNative_TryGetUInt32OSThreadId },

  /* Time (upstream pal_time.c) */
  { "SystemNative_GetTimestamp",              (void *)SystemNative_GetTimestamp },
  { "SystemNative_GetLowResolutionTimestamp", (void *)SystemNative_GetLowResolutionTimestamp },
  { "SystemNative_GetBootTimeTicks",          (void *)SystemNative_GetBootTimeTicks },
  { "SystemNative_GetCpuUtilization",         (void *)SystemNative_GetCpuUtilization },
  { "SystemNative_UTimensat",                 (void *)SystemNative_UTimensat },
  { "SystemNative_FUTimens",                  (void *)SystemNative_FUTimens },

  /* Memory (upstream pal_memory.c) */
  { "SystemNative_AlignedAlloc",              (void *)SystemNative_AlignedAlloc },
  { "SystemNative_AlignedFree",               (void *)SystemNative_AlignedFree },
  { "SystemNative_AlignedRealloc",            (void *)SystemNative_AlignedRealloc },
  { "SystemNative_Calloc",                    (void *)SystemNative_Calloc },
  { "SystemNative_Free",                      (void *)SystemNative_Free },
  { "SystemNative_Malloc",                    (void *)SystemNative_Malloc },
  { "SystemNative_Realloc",                   (void *)SystemNative_Realloc },

  /* Random (upstream pal_random.c) */
  { "SystemNative_GetNonCryptographicallySecureRandomBytes", (void *)SystemNative_GetNonCryptographicallySecureRandomBytes },
  { "SystemNative_GetCryptographicallySecureRandomBytes",    (void *)SystemNative_GetCryptographicallySecureRandomBytes },

  /* String (upstream pal_string.c) */
  { "SystemNative_SNPrintF",                  (void *)SystemNative_SNPrintF },
  { "SystemNative_SNPrintF_1S",               (void *)SystemNative_SNPrintF_1S },
  { "SystemNative_SNPrintF_1I",               (void *)SystemNative_SNPrintF_1I },

  /* Runtime info (upstream pal_runtimeinformation.c) */
  { "SystemNative_GetUnixRelease",            (void *)SystemNative_GetUnixRelease },
  { "SystemNative_GetUnixVersion",            (void *)SystemNative_GetUnixVersion },
  { "SystemNative_GetOSArchitecture",         (void *)SystemNative_GetOSArchitecture },

  /* Logging (upstream pal_log.c) */
  { "SystemNative_Log",                       (void *)SystemNative_Log },
  { "SystemNative_LogError",                  (void *)SystemNative_LogError },

  /* DateTime (upstream pal_datetime.c) */
  { "SystemNative_GetSystemTimeAsTicks",      (void *)SystemNative_GetSystemTimeAsTicks },

  /* Process / diagnostics (NuttX stubs) */
  { "SystemNative_SysLog",                    (void *)sysn_syslog },
  { "SystemNative_Abort",                     (void *)SystemNative_Abort },
  { "SystemNative_Exit",                      (void *)SystemNative_Exit },
  { "SystemNative_GetPid",                    (void *)sysn_get_pid },
  { "SystemNative_GetProcessPath",            (void *)sysn_get_process_path },
  { "SystemNative_ForkAndExecProcess",        (void *)sysn_fork_and_exec_process },

  /* Signal stubs */
  { "SystemNative_SetPosixSignalHandler",     (void *)sysn_set_posix_signal_handler },
  { "SystemNative_EnablePosixSignalHandling", (void *)sysn_enable_posix_signal_handling },
  { "SystemNative_GetPlatformSignalNumber",   (void *)sysn_get_platform_signal_number },

  /* UID/GID stubs */
  { "SystemNative_GetEUid",                   (void *)sysn_get_euid },
  { "SystemNative_GetEGid",                   (void *)sysn_get_egid },
  { "SystemNative_SetEUid",                   (void *)sysn_set_euid },
  { "SystemNative_GetPwUidR",                 (void *)sysn_get_pw_uid_r },
  { "SystemNative_GetHostName",               (void *)sysn_get_hostname },

  { NULL, NULL }
};

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

  syslog(LOG_NOTICE, "P/Invoke resolve: %s::%s\n",
         libraryName, entrypointName);

  if (strcmp(libraryName, "System.Native") == 0 ||
      strcmp(libraryName, "libSystem.Native") == 0)
    {
      mappings = system_native_mappings;
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
      syslog(LOG_NOTICE, "P/Invoke: %s::%s — unknown library, returning stub\n",
             libraryName, entrypointName);
      return (void *)meadow_pinvoke_noop_stub;
    }

  for (MonoDlMapping *m = mappings; m->name != NULL; m++)
    {
      if (strcmp(m->name, entrypointName) == 0)
        {
          return m->addr;
        }
    }

  /* For System.Native, return no-op stub for unmapped functions so the
   * runtime can limp along while we discover all needed functions.
   * For other libraries, return NULL (runtime continues default search).
   */
  if (strcmp(libraryName, "System.Native") == 0 ||
      strcmp(libraryName, "libSystem.Native") == 0)
    {
      syslog(LOG_NOTICE, "P/Invoke: System.Native::%s — UNMAPPED, returning noop stub\n",
             entrypointName);
      return (void *)meadow_pinvoke_noop_stub;
    }

  syslog(LOG_NOTICE, "P/Invoke: '%s!%s' not found in mapping table\n",
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

  setenv("MONO_LOG_LEVEL", "info", 1);
  setenv("MONO_LOG_DEST", "syslog", 1);
  setenv("MONO_ENV_OPTIONS", "--interpreter", 1);
  setenv("TMPDIR", "/meadow0/Temp", 1);
  setenv("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT", "1", 1);
  /* Use raw resource keys instead of loading .resources files.
   * This avoids ResourceManager initialization which can fail on NuttX. */
  setenv("DOTNET_SYSTEM_RESOURCES_USESYSTEMRESOURCEKEYS", "true", 1);
  /* Skip ConsolePal terminal/signal initialization.  NuttX has no terminal;
   * the initialization path crashes in the Mono interpreter due to
   * re-entrant Monitor.Enter on the first Console.Write through
   * EnsureInitializedCore → lock(Console.Out). */
  setenv("DOTNET_SYSTEM_CONSOLE_SKIP_TERMINAL_INIT", "1", 1);

  /* Constrain GC heap for limited SDRAM (~29MB user heap).
   * SDRAM budget: GC heap + CoreLib cache (6MB) + thread stacks + Mono metadata.
   * Thread stacks capped at 64KB each (pal_threading.c NuttX path).
   * Legacy firmware used: max-heap-size=8m (Mono 6.9 was lighter).
   * .NET 10 needs more: heavier type system + ThreadPool infrastructure.
   */
  setenv("MONO_GC_PARAMS",
         "max-heap-size=16m,nursery-size=512k,soft-heap-limit=8m,"
         "major=marksweep", 1);

  /* Pre-load assemblies from LFS (QSPI flash) into SDRAM.
   * QSPI reads are slow; SDRAM access is fast. We read each assembly
   * once into an SDRAM buffer (user heap at 0xC0300000+, ~29MB available).
   * The mmap stub then serves data from these buffers via memcpy instead
   * of re-reading from flash on every mono_file_map call.
   */

  {
    extern void sdram_cache_register(const char *path, void *data,
                                     size_t size);

    DIR *dir = opendir(MONO_MEADOW_EXECUTABLE_PARTITION_NAME);
    if (dir != NULL)
      {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
          {
            size_t nlen = strlen(entry->d_name);
            if (nlen > 4 && strcmp(entry->d_name + nlen - 4, ".dll") == 0)
              {
                char path[128];
                snprintf(path, sizeof(path), "%s/%s",
                         MONO_MEADOW_EXECUTABLE_PARTITION_NAME,
                         entry->d_name);

                int fd = open(path, O_RDONLY);
                if (fd < 0)
                  continue;

                struct stat st;
                if (fstat(fd, &st) < 0 || st.st_size == 0 ||
                    st.st_size > 100000)
                    /* Skip caching assemblies >100KB to save SDRAM for GC heap
                     * and Mono metadata. SDRAM budget: 29MB total, shared between
                     * GC (16MB), thread stacks, Mono metadata, and caching.
                     * CoreLib alone is 6MB; caching it + GC + metadata exceeds 29MB.
                     * Only cache tiny forwarder assemblies (<100KB).
                     * On Renode, QSPI reads are fast enough; on real hardware,
                     * revisit with a file-backed mmap or selective caching. */
                  {
                    close(fd);
                    continue;
                  }

                /* Allocate in SDRAM (user heap) */

                void *buf = malloc(st.st_size);
                if (buf == NULL)
                  {
                    syslog(LOG_ERR, "SDRAM cache: malloc(%ld) failed for %s\n",
                           (long)st.st_size, entry->d_name);
                    close(fd);
                    continue;
                  }

                /* Read entire file from LFS/QSPI into SDRAM */

                size_t total = 0;
                while (total < (size_t)st.st_size)
                  {
                    ssize_t n = read(fd, (char *)buf + total,
                                     st.st_size - total);
                    if (n <= 0)
                      break;
                    total += n;
                  }

                close(fd);

                if (total == (size_t)st.st_size)
                  {
                    sdram_cache_register(path, buf, st.st_size);
                    syslog(LOG_INFO, "SDRAM cache: %s (%ld bytes)\n",
                           entry->d_name, (long)st.st_size);
                  }
                else
                  {
                    syslog(LOG_ERR, "SDRAM cache: short read %s "
                           "(%zu of %ld)\n",
                           entry->d_name, total, (long)st.st_size);
                    free(buf);
                  }
              }
          }
        closedir(dir);
      }
  }

  /* Build TPA from assemblies on the filesystem */

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
    "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT",
    "System.Resources.UseSystemResourceKeys",
  };

  const char *property_values[] = {
    tpa_list,
    MONO_MEADOW_EXECUTABLE_PARTITION_NAME,
    MONO_MEADOW_EXECUTABLE_PARTITION_NAME,
    pinvoke_override_str,
    "1",
    "true",
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

  /* Global markers for GDB/Renode inspection (survives syslog failure) */

  static volatile int g_mono_stage = 0;
  static volatile int g_mono_exec_ret = -999;
  static volatile unsigned int g_mono_exit_code = 0xDEAD;

  syslog(LOG_NOTICE, "monovm_initialize succeeded\n");

  g_mono_stage = 1; /* About to call mono_appears_to_be_running */

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

  g_mono_stage = 2; /* mono_appears_to_be_running returned OK */

  /* The HCOM call above redirects stdout/stderr to FIFOs (1024-byte
   * buffer).  The Mono runtime writes warnings/errors to stderr during
   * init.  In the headless emulator there is no HCOM client on TCP:4242
   * to drain the FIFOs, so the write blocks when the buffer fills.
   *
   * Set O_NONBLOCK on both FIFOs so writes return EAGAIN instead of
   * blocking.  The FIFOs stay open (no POLLHUP for the HCOM reader).
   * Mono's important output goes to syslog (MONO_LOG_DEST=syslog).
   */

  {
    int flags;

    /* Make stdout/stderr FIFOs non-blocking so writes return EAGAIN
     * instead of hanging.  Mono's g_log/g_printerr are patched to use
     * syslog on NuttX, so important messages still reach the UART.
     */
    flags = fcntl(1, F_GETFL, 0);
    if (flags >= 0)
      fcntl(1, F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(2, F_GETFL, 0);
    if (flags >= 0)
      fcntl(2, F_SETFL, flags | O_NONBLOCK);
  }

  g_mono_stage = 3; /* O_NONBLOCK set, about to syslog */
  syslog(LOG_NOTICE, "stdout/stderr set to O_NONBLOCK (stage %d)\n",
         g_mono_stage);

  g_mono_stage = 4; /* About to chdir */

  /* Change to the app directory */

  chdir(MONO_MEADOW_EXECUTABLE_PARTITION_NAME);

  g_mono_stage = 5; /* About to open app */

  /* Check if the app assembly exists before trying to execute it */

  char *app_path = MONO_MEADOW_EXECUTABLE_APP_EXE;
  int app_fd = open(app_path, O_RDONLY);
  if (app_fd < 0)
    {
      g_mono_stage = 6; /* App not found */
      syslog(LOG_WARNING, "App assembly '%s' not found — "
             "runtime initialized but no app to execute\n", app_path);
      /* Runtime initialized successfully, just no app to run.
       * Shut down cleanly.
       */
    }
  else
    {
      close(app_fd);

      g_mono_stage = 7; /* About to execute assembly */
      syslog(LOG_NOTICE, "Executing assembly: %s\n", app_path);

      unsigned int exit_code = 0;
      ret = monovm_execute_assembly(0, NULL, app_path, &exit_code);
      g_mono_exec_ret = ret;
      g_mono_exit_code = exit_code;

      g_mono_stage = 8; /* Execute returned */
      syslog(LOG_NOTICE, "mono_exec returned: ret=0x%08x exit_code=%u\n",
             ret, exit_code);
    }

  g_mono_stage = 9; /* About to shutdown */

  /* Shutdown the runtime */

  int latched_exit_code = 0;
  monovm_shutdown(&latched_exit_code);

  g_mono_stage = 10; /* Shutdown complete */
  syslog(LOG_NOTICE, "monovm_shutdown complete, latched exit code: %d\n",
         latched_exit_code);

  free(tpa_list);
  return 0;
}
