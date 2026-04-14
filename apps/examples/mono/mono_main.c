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

/* Upstream pal_io.c function declarations — we can't include pal_io.h directly
 * because pal_io_common.h uses `errno = X` which NuttX doesn't support (errno
 * is a macro expanding to get_errno(), not an assignable lvalue). Instead, we
 * declare the subset of functions needed for the P/Invoke mapping table. */
#include <dirent.h>
/* Forward declarations for pal_io.c types (defined in pal_io.h) */
typedef struct {
    int32_t  Flags;
    int32_t  Mode;
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
} FileStatus;

typedef struct {
    const char *Name;
    int32_t NameLength;
    int32_t InodeType;
} DirectoryEntry;

/* pal_io.c functions (from libSystem.Native.a) */
extern int32_t  SystemNative_Stat(const char *path, FileStatus *output);
extern int32_t  SystemNative_FStat(intptr_t fd, FileStatus *output);
extern int32_t  SystemNative_LStat(const char *path, FileStatus *output);
extern intptr_t SystemNative_Open(const char *path, int32_t flags, int32_t mode);
extern int32_t  SystemNative_Close(intptr_t fd);
extern intptr_t SystemNative_Dup(intptr_t oldfd);
extern int32_t  SystemNative_Unlink(const char *path);
extern int32_t  SystemNative_ReadDir(DIR *dir, DirectoryEntry *outputEntry);
extern DIR     *SystemNative_OpenDir(const char *path);
extern int32_t  SystemNative_CloseDir(DIR *dir);
extern int32_t  SystemNative_Pipe(int32_t pipefd[2], int32_t flags);
extern int32_t  SystemNative_FcntlSetFD(intptr_t fd, int32_t flags);
extern int32_t  SystemNative_FcntlGetFD(intptr_t fd);
extern int32_t  SystemNative_FcntlCanGetSetPipeSz(void);
extern int32_t  SystemNative_FcntlGetPipeSz(intptr_t fd);
extern int32_t  SystemNative_FcntlSetPipeSz(intptr_t fd, int32_t size);
extern int32_t  SystemNative_FcntlSetIsNonBlocking(intptr_t fd, int32_t isNonBlocking);
extern int32_t  SystemNative_FcntlGetIsNonBlocking(intptr_t fd, int32_t *isNonBlocking);
extern int32_t  SystemNative_MkDir(const char *path, int32_t mode);
extern int32_t  SystemNative_ChMod(const char *path, int32_t mode);
extern int32_t  SystemNative_FChMod(intptr_t fd, int32_t mode);
extern int32_t  SystemNative_FSync(intptr_t fd);
extern int32_t  SystemNative_FLock(intptr_t fd, int32_t operation);
extern int32_t  SystemNative_ChDir(const char *path);
extern int32_t  SystemNative_Access(const char *path, int32_t mode);
extern int64_t  SystemNative_LSeek(intptr_t fd, int64_t offset, int32_t whence);
extern int32_t  SystemNative_Link(const char *source, const char *linkTarget);
extern int32_t  SystemNative_SymLink(const char *target, const char *linkPath);
extern void     SystemNative_GetDeviceIdentifiers(uint64_t dev, uint32_t *majorNumber, uint32_t *minorNumber);
extern int32_t  SystemNative_MkNod(const char *pathName, uint32_t mode, uint32_t major, uint32_t minor);
extern int32_t  SystemNative_MkFifo(const char *pathName, uint32_t mode);
extern char    *SystemNative_MkdTemp(char *pathTemplate);
extern intptr_t SystemNative_MksTemps(char *pathTemplate, int32_t suffixLength);
extern void    *SystemNative_MMap(void *address, uint64_t length, int32_t protection, int32_t flags, intptr_t fd, int64_t offset);
extern int32_t  SystemNative_MUnmap(void *address, uint64_t length);
extern int32_t  SystemNative_MProtect(void *address, uint64_t length, int32_t protection);
extern int32_t  SystemNative_MAdvise(void *address, uint64_t length, int32_t advice);
extern int32_t  SystemNative_MSync(void *address, uint64_t length, int32_t flags);
extern int64_t  SystemNative_SysConf(int32_t name);
extern int32_t  SystemNative_FTruncate(intptr_t fd, int64_t length);
extern int32_t  SystemNative_Poll(void *pollEvents, uint32_t eventCount, int32_t milliseconds, uint32_t *triggered);
extern int32_t  SystemNative_PosixFAdvise(intptr_t fd, int64_t offset, int64_t length, int32_t advice);
extern int32_t  SystemNative_FAllocate(intptr_t fd, int64_t offset, int64_t length);
extern int32_t  SystemNative_Read(intptr_t fd, void *buffer, int32_t bufferSize);
extern int32_t  SystemNative_ReadFromNonblocking(intptr_t fd, void *buffer, int32_t bufferSize);
extern int32_t  SystemNative_Write(intptr_t fd, const void *buffer, int32_t bufferSize);
extern int32_t  SystemNative_WriteToNonblocking(intptr_t fd, const void *buffer, int32_t bufferSize);
extern int32_t  SystemNative_ReadLink(const char *path, char *buffer, int32_t bufferSize);
extern int32_t  SystemNative_Rename(const char *oldPath, const char *newPath);
extern int32_t  SystemNative_RmDir(const char *path);
extern void     SystemNative_Sync(void);
extern int32_t  SystemNative_CopyFile(intptr_t sourceFd, intptr_t destinationFd, int64_t sourceLength);
extern intptr_t SystemNative_INotifyInit(void);
extern int32_t  SystemNative_INotifyAddWatch(intptr_t fd, const char *pathName, uint32_t mask);
extern int32_t  SystemNative_INotifyRemoveWatch(intptr_t fd, int32_t wd);
extern char    *SystemNative_RealPath(const char *path);
extern uint32_t SystemNative_FileSystemSupportsLocking(intptr_t fd, int32_t lockOperation, int32_t accessWrite);
extern int32_t  SystemNative_LockFileRegion(intptr_t fd, int64_t offset, int64_t length, int16_t lockType);
extern int32_t  SystemNative_LChflags(const char *path, uint32_t flags);
extern int32_t  SystemNative_FChflags(intptr_t fd, uint32_t flags);
extern int32_t  SystemNative_LChflagsCanSetHiddenFlag(void);
extern int32_t  SystemNative_CanGetHiddenFlag(void);
extern int32_t  SystemNative_PRead(intptr_t fd, void *buffer, int32_t bufferSize, int64_t fileOffset);
extern int32_t  SystemNative_PWrite(intptr_t fd, void *buffer, int32_t bufferSize, int64_t fileOffset);

/* pal_networking.c functions (from libSystem.Native.a) */
extern int32_t  SystemNative_GetHostEntryForName(const uint8_t *address, int32_t addressFamily, void *entry);
extern void     SystemNative_FreeHostEntry(void *entry);
extern int32_t  SystemNative_GetNameInfo(const uint8_t *address, int32_t addressLength, int8_t isIPv6,
                    uint8_t *host, int32_t hostLength, uint8_t *service, int32_t serviceLength, int32_t flags);
extern int32_t  SystemNative_GetDomainName(uint8_t *name, int32_t nameLength);
extern int32_t  SystemNative_GetHostName(uint8_t *name, int32_t nameLength);
extern int32_t  SystemNative_GetSocketAddressSizes(int32_t *ipv4, int32_t *ipv6, int32_t *uds, int32_t *max);
extern int32_t  SystemNative_GetAddressFamily(const uint8_t *sa, int32_t saLen, int32_t *af);
extern int32_t  SystemNative_SetAddressFamily(uint8_t *sa, int32_t saLen, int32_t af);
extern int32_t  SystemNative_GetPort(const uint8_t *sa, int32_t saLen, uint16_t *port);
extern int32_t  SystemNative_SetPort(uint8_t *sa, int32_t saLen, uint16_t port);
extern int32_t  SystemNative_GetIPv4Address(const uint8_t *sa, int32_t saLen, uint32_t *address);
extern int32_t  SystemNative_SetIPv4Address(uint8_t *sa, int32_t saLen, uint32_t address);
extern int32_t  SystemNative_GetIPv6Address(const uint8_t *sa, int32_t saLen, uint8_t *addr, int32_t addrLen, uint32_t *scopeId);
extern int32_t  SystemNative_SetIPv6Address(uint8_t *sa, int32_t saLen, uint8_t *addr, int32_t addrLen, uint32_t scopeId);
extern int32_t  SystemNative_GetControlMessageBufferSize(int32_t isIPv4, int32_t isIPv6);
extern int32_t  SystemNative_TryGetIPPacketInformation(void *messageHeader, int32_t isIPv4, void *packetInfo);
extern int32_t  SystemNative_GetIPv4MulticastOption(intptr_t socket, int32_t multicastOption, void *option);
extern int32_t  SystemNative_SetIPv4MulticastOption(intptr_t socket, int32_t multicastOption, void *option);
extern int32_t  SystemNative_GetIPv6MulticastOption(intptr_t socket, int32_t multicastOption, void *option);
extern int32_t  SystemNative_SetIPv6MulticastOption(intptr_t socket, int32_t multicastOption, void *option);
extern int32_t  SystemNative_GetLingerOption(intptr_t socket, void *option);
extern int32_t  SystemNative_SetLingerOption(intptr_t socket, void *option);
extern int32_t  SystemNative_SetReceiveTimeout(intptr_t socket, int32_t millisecondsTimeout);
extern int32_t  SystemNative_SetSendTimeout(intptr_t socket, int32_t millisecondsTimeout);
extern int32_t  SystemNative_Receive(intptr_t socket, void *buffer, int32_t bufferLen, int32_t flags, int32_t *received);
extern int32_t  SystemNative_ReceiveMessage(intptr_t socket, void *messageHeader, int32_t flags, int64_t *received);
extern int32_t  SystemNative_ReceiveSocketError(intptr_t socket, void *messageHeader);
extern int32_t  SystemNative_Send(intptr_t socket, void *buffer, int32_t bufferLen, int32_t flags, int32_t *sent);
extern int32_t  SystemNative_SendMessage(intptr_t socket, void *messageHeader, int32_t flags, int64_t *sent);
extern int32_t  SystemNative_Accept(intptr_t socket, uint8_t *socketAddress, int32_t *socketAddressLen, intptr_t *acceptedSocket);
extern int32_t  SystemNative_Bind(intptr_t socket, int32_t protocolType, uint8_t *socketAddress, int32_t socketAddressLen);
extern int32_t  SystemNative_Connect(intptr_t socket, uint8_t *socketAddress, int32_t socketAddressLen);
extern int32_t  SystemNative_Connectx(intptr_t socket, uint8_t *socketAddress, int32_t socketAddressLen, uint8_t *data, int32_t dataLen, int32_t tfo, int *sent);
extern int32_t  SystemNative_GetPeerName(intptr_t socket, uint8_t *socketAddress, int32_t *socketAddressLen);
extern int32_t  SystemNative_GetSockName(intptr_t socket, uint8_t *socketAddress, int32_t *socketAddressLen);
extern int32_t  SystemNative_Listen(intptr_t socket, int32_t backlog);
extern int32_t  SystemNative_Shutdown(intptr_t socket, int32_t socketShutdown);
extern int32_t  SystemNative_GetSocketErrorOption(intptr_t socket, int32_t *error);
extern int32_t  SystemNative_GetSockOpt(intptr_t socket, int32_t optLevel, int32_t optName, uint8_t *optValue, int32_t *optLen);
extern int32_t  SystemNative_GetRawSockOpt(intptr_t socket, int32_t optLevel, int32_t optName, uint8_t *optValue, int32_t *optLen);
extern int32_t  SystemNative_SetSockOpt(intptr_t socket, int32_t optLevel, int32_t optName, uint8_t *optValue, int32_t optLen);
extern int32_t  SystemNative_SetRawSockOpt(intptr_t socket, int32_t optLevel, int32_t optName, uint8_t *optValue, int32_t optLen);
extern int32_t  SystemNative_Socket(int32_t addressFamily, int32_t socketType, int32_t protocolType, intptr_t *createdSocket);
extern int32_t  SystemNative_GetSocketType(intptr_t socket, int32_t *af, int32_t *type, int32_t *proto, int32_t *isListening);
extern int32_t  SystemNative_GetAtOutOfBandMark(intptr_t socket, int32_t *available);
extern int32_t  SystemNative_GetBytesAvailable(intptr_t socket, int32_t *available);
extern int32_t  SystemNative_GetWasiSocketDescriptor(intptr_t socket, void **entry);
extern int32_t  SystemNative_CreateSocketEventPort(intptr_t *port);
extern int32_t  SystemNative_CloseSocketEventPort(intptr_t port);
extern int32_t  SystemNative_CreateSocketEventBuffer(int32_t count, void **buffer);
extern int32_t  SystemNative_FreeSocketEventBuffer(void *buffer);
extern int32_t  SystemNative_TryChangeSocketEventRegistration(intptr_t port, intptr_t socket, int32_t currentEvents, int32_t newEvents, uintptr_t data);
extern int32_t  SystemNative_WaitForSocketEvents(intptr_t port, void *buffer, int32_t *count);
extern int32_t  SystemNative_PlatformSupportsDualModeIPv4PacketInfo(void);
extern void     SystemNative_GetDomainSocketSizes(int32_t *pathOffset, int32_t *pathSize, int32_t *addressSize);
extern int32_t  SystemNative_GetMaximumAddressSize(void);
extern int32_t  SystemNative_SendFile(intptr_t out_fd, intptr_t in_fd, int64_t offset, int64_t count, int64_t *sent);
extern int32_t  SystemNative_Disconnect(intptr_t socket);
extern uint32_t SystemNative_InterfaceNameToIndex(char *interfaceName);
extern int32_t  SystemNative_Select(int *readFds, int readFdsCount, int *writeFds, int writeFdsCount, int *errorFds, int errorFdsCount, int32_t microseconds, int32_t maxFd, int *triggered);

/* pal_interfaceaddresses.c */
typedef void (*IPv4AddressFound)(void*, const char*, void*);
typedef void (*IPv6AddressFound)(void*, const char*, void*, uint32_t*);
typedef void (*LinkLayerAddressFound)(void*, const char*, void*);
typedef void (*GatewayAddressFound)(void*, void*);
extern int32_t  SystemNative_EnumerateInterfaceAddresses(void *context, IPv4AddressFound onIpv4Found, IPv6AddressFound onIpv6Found, LinkLayerAddressFound onLinkLayerFound);
extern int32_t  SystemNative_GetNetworkInterfaces(int32_t *interfaceCount, void **interfaces, int32_t *addressCount, void **addressList);
extern int32_t  SystemNative_EnumerateGatewayAddressesForInterface(void *context, uint32_t interfaceIndex, GatewayAddressFound onGatewayFound);

/* pal_networkchange.c */
typedef void (*NetworkChangeEvent)(intptr_t, int32_t);
extern int32_t  SystemNative_CreateNetworkChangeListenerSocket(intptr_t *retSocket);
extern int32_t  SystemNative_ReadEvents(intptr_t sock, NetworkChangeEvent onNetworkChange);

/* pal_networkstatistics.c (upstream, returns ENOTSUP on NuttX) */
extern int32_t  SystemNative_GetTcpGlobalStatistics(void *retStats);
extern int32_t  SystemNative_GetIPv4GlobalStatistics(void *retStats);
extern int32_t  SystemNative_GetUdpGlobalStatistics(void *retStats);
extern int32_t  SystemNative_GetIcmpv4GlobalStatistics(void *retStats);
extern int32_t  SystemNative_GetIcmpv6GlobalStatistics(void *retStats);
extern int32_t  SystemNative_GetEstimatedTcpConnectionCount(void);
extern int32_t  SystemNative_GetActiveTcpConnectionInfos(void *infos, int32_t *infoCount);
extern int32_t  SystemNative_GetEstimatedUdpListenerCount(void);
extern int32_t  SystemNative_GetActiveUdpListeners(void *infos, int32_t *infoCount);
extern int32_t  SystemNative_GetNativeIPInterfaceStatistics(char *interfaceName, void *retStats);
extern int32_t  SystemNative_GetNumRoutes(void);
extern int32_t  SystemNative_MapTcpState(int32_t tcpState);

/* Additional libSystem.Native.a functions not yet mapped */
extern void     SystemNative_CreateAutoreleasePool(void);
extern void     SystemNative_DrainAutoreleasePool(void);
extern char    *SystemNative_GetDefaultTimeZone(void);
extern void     SystemNative_GetTimeZoneData(const char *tzId, void **rawData, int32_t *length);
extern int32_t  SystemNative_GetPeerID(intptr_t socket, void *peerID);
extern int32_t  SystemNative_iOSSupportVersion(void);
extern int32_t  SystemNative_IsMemfdSupported(void);
extern intptr_t SystemNative_MemfdCreate(const char *name, int32_t flags);
extern int64_t  SystemNative_PReadV(intptr_t fd, void *vectors, int32_t vectorCount);
extern int64_t  SystemNative_PWriteV(intptr_t fd, void *vectors, int32_t vectorCount);
extern int32_t  SystemNative_ReadProcessInfo(int32_t pid, void *procInfo);
extern int32_t  SystemNative_ReadThreadInfo(void *threadInfo);
extern char    *SystemNative_SearchPath(int32_t folderId);
extern char    *SystemNative_SearchPath_TempDirectory(void);
extern intptr_t SystemNative_ShmOpen(const char *name, int32_t flags, int32_t mode);
extern int32_t  SystemNative_ShmUnlink(const char *name);

/* pal_environment.c functions (from libSystem.Native.a) */
extern char    *SystemNative_GetEnv(const char *variable);
extern char   **SystemNative_GetEnviron(void);
extern void     SystemNative_FreeEnviron(char **envp);

/* pal_process.c functions (from libSystem.Native.a) */
extern int32_t  SystemNative_ForkAndExecProcess(const char *filename, char *const argv[],
                    char *const envp[], const char *cwd, int32_t redirectStdin,
                    int32_t redirectStdout, int32_t redirectStderr, int32_t setCredentials,
                    uint32_t userId, uint32_t groupId, uint32_t *groups, int32_t groupsLength,
                    int32_t *childPid, int32_t *stdinFd, int32_t *stdoutFd, int32_t *stderrFd);
extern int32_t  SystemNative_GetRLimit(int32_t resourceType, void *limits);
extern int32_t  SystemNative_SetRLimit(int32_t resourceType, const void *limits);
extern int32_t  SystemNative_Kill(int32_t pid, int32_t signal);
extern int32_t  SystemNative_GetPid(void);
extern int32_t  SystemNative_GetSid(int32_t pid);
extern void     SystemNative_SysLog(int32_t priority, const char *message, const char *arg1);
extern int32_t  SystemNative_WaitIdAnyExitedNoHangNoWait(void);
extern int32_t  SystemNative_WaitPidExitedNoHang(int32_t pid, int32_t *exitCode);
extern int64_t  SystemNative_PathConf(const char *path, int32_t name);
extern int32_t  SystemNative_GetPriority(int32_t which, int32_t who);
extern int32_t  SystemNative_SetPriority(int32_t which, int32_t who, int32_t nice);
extern char    *SystemNative_GetCwd(char *buffer, int32_t bufferSize);
extern int32_t  SystemNative_SchedSetAffinity(int32_t pid, intptr_t *mask);
extern int32_t  SystemNative_SchedGetAffinity(int32_t pid, intptr_t *mask);
extern char    *SystemNative_GetProcessPath(void);

/****************************************************************************
 * P/Invoke mapping tables
 *
 * Consumed by meadow_pinvoke_override() to resolve DllImport calls.
 ****************************************************************************/

typedef struct {
  const char *name;
  void *addr;
} MonoDlMapping;

#include "mappings-meadow.h"
#include "mappings-crypto-native.h"

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

/* UPD emulation shim removed — emulation belongs in Renode peripheral drivers.
 *
 * Root cause of the EACCES was: UPD driver only defines .open/.close/.ioctl
 * (no .read/.write) in file_operations. NuttX O_RDONLY=1, so opening with
 * flags=0 (DriverFlags.DontCare) works fine — the VFS inode_checkflags()
 * only rejects when a read/write mode is requested but the driver lacks
 * the corresponding handler. Meadow.Core always opens with flags=0.
 */


/****************************************************************************
 * System.Native P/Invoke implementations
 *
 * These implement the subset of System.Native functions called during
 * runtime initialization and Hello World execution.
 ****************************************************************************/


/* Direct syslog write for managed code — bypasses HCOM, goes straight to USART1.
 * Called from managed via [DllImport("System.Native")] SystemNative_SyslogWrite. */
static void sysn_syslog_write(const void *buffer, int32_t length)
{
  if (buffer && length > 0 && length < 1024)
    {
      char tmp[1025];
      memcpy(tmp, buffer, length);
      tmp[length] = '\0';
      syslog(LOG_ERR, "%s", tmp);
    }
}


/* Console I/O — wraps NuttX POSIX calls */

static int32_t sysn_write(intptr_t fd, const void *buffer, int32_t bufferSize)
{
  /* stdout/stderr → HCOM FIFO → MonoStdxxx thread → UART4 → CLI.
   * FIFOs are O_NONBLOCK: write returns EAGAIN if the 1KB buffer is full
   * (no CLI client connected, or reader can't keep up).  Retry briefly
   * to give the MonoStdxxx thread time to drain. */

  /* Note: syslog mirroring removed — hcom_logging_syslog has a semaphore leak
   * that permanently deadlocks syslog when malloc fails. Use SyslogWrite P/Invoke
   * for direct USART1 output from managed code instead. */

  ssize_t count;
  int retries = 5;

  do {
    count = write((int)fd, buffer, (size_t)bufferSize);
    if (count >= 0)
      return (int32_t)count;
    if (get_errno() == EINTR)
      continue;
    if (get_errno() == EAGAIN && --retries > 0)
      {
        usleep(1000); /* 1ms — let MonoStdxxx thread drain the FIFO */
        continue;
      }
    break;
  } while (1);

  /* EAGAIN: FIFO full / no CLI client draining.  Return bufferSize to
   * prevent managed IOException — data is lost but app continues. */

  if (get_errno() == EAGAIN)
      return bufferSize;

  return (int32_t)count;
}

static int32_t sysn_isatty(intptr_t fd)
{
  (void)fd;
  /* After HCOM redirect, fd 1/2 are FIFOs (/dev/monostdout, /dev/monostderr),
   * not terminals.  Returning 0 makes .NET ConsolePal use the simple stream
   * write path instead of trying tcgetattr/terminal init (which crashes on FIFOs). */
  return 0;
}

/* Terminal handling stubs — NuttX Meadow has no real terminal */

static int32_t sysn_initialize_terminal_and_signal_handling(void)
{
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

/* Dup2 — not in upstream pal_io.c */
static int32_t sysn_dup2(intptr_t oldFd, intptr_t newFd)
{
  return dup2((int)oldFd, (int)newFd);
}

static void sysn_disable_posix_signal_handling(int32_t signalCode)
{
  (void)signalCode;
}

static int32_t sysn_handle_noncanceled_posix_signal(int32_t signalCode)
{
  (void)signalCode;
  return 1; /* Handled */
}

static int32_t sysn_get_groups(int32_t gidsetsize, uint32_t *grouplist)
{
  (void)gidsetsize; (void)grouplist;
  return 0; /* No supplementary groups */
}



/****************************************************************************
 * Upstream System.Native PAL — extern declarations
 *
 * These functions are compiled into libSystem.Native.a via CMake
 * (runtime/src/native/libs/System.Native/).
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

/****************************************************************************
 * System.Globalization.Native stubs
 *
 * With DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1, most globalization paths
 * should be skipped. However, some code paths (e.g. CompareInfo.InitSort)
 * still attempt ICU calls. Provide minimal stubs that return success with
 * dummy values so the managed code doesn't crash on null handles.
 ****************************************************************************/

/* Dummy sort handle — just a non-null sentinel */
static int g_dummy_sort_handle;

static int glob_get_sort_handle(const char *localeName, void **ppSortHandle)
{
  syslog(LOG_ERR, "GLOB: GetSortHandle('%s') called — should not happen in invariant mode!\n",
         localeName ? localeName : "(null)");
  if (ppSortHandle)
    *ppSortHandle = NULL;
  return 3; /* UnknownError */
}

static void glob_close_sort_handle(void *pSortHandle)
{
  /* no-op — dummy handle */
}

static int glob_load_icu(void)
{
  syslog(LOG_ERR, "GLOB: LoadICU called — invariant mode should skip this!\n");
  /* Return 1 = success to prevent FailFast.
   * On NuttX we have no ICU, but returning success prevents
   * Environment.FailFast("Couldn't find a valid ICU package...") */
  return 1;
}

/* GetDefaultLocaleName: write empty string (invariant mode). Return length. */
static int32_t glob_get_default_locale_name(uint16_t *value, int32_t valueLength)
{
  if (value && valueLength > 0)
    value[0] = 0;
  return 0;
}

/* GetLocaleName: copy empty string (invariant mode). Return length. */
static int32_t glob_get_locale_name(const uint16_t *localeName, uint16_t *value,
                                    int32_t valueLength)
{
  (void)localeName;
  if (value && valueLength > 0)
    value[0] = 0;
  return 0;
}

static MonoDlMapping globalization_native_mappings[] = {
  { "GlobalizationNative_GetSortHandle",        (void *)glob_get_sort_handle },
  { "GlobalizationNative_CloseSortHandle",      (void *)glob_close_sort_handle },
  { "GlobalizationNative_LoadICU",              (void *)glob_load_icu },
  { "GlobalizationNative_GetDefaultLocaleName", (void *)glob_get_default_locale_name },
  { "GlobalizationNative_GetLocaleName",        (void *)glob_get_locale_name },
  { NULL, NULL }
};

static MonoDlMapping system_native_mappings[] = {
  /* ---- test syslog (direct USART1 output for Renode) ---- */
  { "SystemNative_SyslogWrite",               (void *)sysn_syslog_write },
  /* ---- pal_io.c (upstream, from libSystem.Native.a) ---- */
  { "SystemNative_Open",                      (void *)SystemNative_Open },
  { "SystemNative_Close",                     (void *)SystemNative_Close },
  { "SystemNative_FStat2",                    (void *)SystemNative_FStat },
  { "SystemNative_FStat",                     (void *)SystemNative_FStat },
  { "SystemNative_Stat2",                     (void *)SystemNative_Stat },
  { "SystemNative_Stat",                      (void *)SystemNative_Stat },
  { "SystemNative_LStat2",                    (void *)SystemNative_LStat },
  { "SystemNative_LStat",                     (void *)SystemNative_LStat },
  { "SystemNative_LSeek",                     (void *)SystemNative_LSeek },
  { "SystemNative_Read",                      (void *)SystemNative_Read },
  { "SystemNative_ReadFromNonblocking",       (void *)SystemNative_ReadFromNonblocking },
  { "SystemNative_Write",                     (void *)sysn_write },  /* HCOM FIFO retry */
  { "SystemNative_WriteToNonblocking",        (void *)SystemNative_WriteToNonblocking },
  { "SystemNative_FcntlSetFD",               (void *)SystemNative_FcntlSetFD },
  { "SystemNative_FcntlGetFD",               (void *)SystemNative_FcntlGetFD },
  { "SystemNative_FcntlSetFdFlags",           (void *)SystemNative_FcntlSetFD },     /* legacy name */
  { "SystemNative_FcntlGetFdFlags",           (void *)SystemNative_FcntlGetFD },     /* legacy name */
  { "SystemNative_FcntlGetIsNonBlocking",     (void *)SystemNative_FcntlGetIsNonBlocking },
  { "SystemNative_FcntlSetIsNonBlocking",     (void *)SystemNative_FcntlSetIsNonBlocking },
  { "SystemNative_FcntlCanGetSetPipeSz",      (void *)SystemNative_FcntlCanGetSetPipeSz },
  { "SystemNative_FcntlGetPipeSz",            (void *)SystemNative_FcntlGetPipeSz },
  { "SystemNative_FcntlSetPipeSz",            (void *)SystemNative_FcntlSetPipeSz },
  { "SystemNative_Pipe",                      (void *)SystemNative_Pipe },
  { "SystemNative_Dup",                       (void *)SystemNative_Dup },
  { "SystemNative_Dup2",                      (void *)sysn_dup2 },  /* no upstream */
  { "SystemNative_Unlink",                    (void *)SystemNative_Unlink },
  { "SystemNative_MkDir",                     (void *)SystemNative_MkDir },
  { "SystemNative_Access",                    (void *)SystemNative_Access },
  { "SystemNative_ReadLink",                  (void *)SystemNative_ReadLink },
  { "SystemNative_RealPath",                  (void *)SystemNative_RealPath },
  { "SystemNative_OpenDir",                   (void *)SystemNative_OpenDir },
  { "SystemNative_ReadDir",                   (void *)SystemNative_ReadDir },
  { "SystemNative_CloseDir",                  (void *)SystemNative_CloseDir },
  { "SystemNative_FSync",                     (void *)SystemNative_FSync },
  { "SystemNative_FTruncate",                 (void *)SystemNative_FTruncate },
  { "SystemNative_FLock",                     (void *)SystemNative_FLock },
  { "SystemNative_ChMod",                     (void *)SystemNative_ChMod },
  { "SystemNative_FChMod",                    (void *)SystemNative_FChMod },
  { "SystemNative_ChDir",                     (void *)SystemNative_ChDir },
  { "SystemNative_RmDir",                     (void *)SystemNative_RmDir },
  { "SystemNative_Rename",                    (void *)SystemNative_Rename },
  { "SystemNative_Sync",                      (void *)SystemNative_Sync },
  { "SystemNative_Link",                      (void *)SystemNative_Link },
  { "SystemNative_SymLink",                   (void *)SystemNative_SymLink },
  { "SystemNative_MkNod",                     (void *)SystemNative_MkNod },
  { "SystemNative_MkFifo",                    (void *)SystemNative_MkFifo },
  { "SystemNative_MkdTemp",                   (void *)SystemNative_MkdTemp },
  { "SystemNative_MksTemps",                  (void *)SystemNative_MksTemps },
  { "SystemNative_GetDeviceIdentifiers",      (void *)SystemNative_GetDeviceIdentifiers },
  { "SystemNative_PRead",                     (void *)SystemNative_PRead },
  { "SystemNative_PWrite",                    (void *)SystemNative_PWrite },
  { "SystemNative_Poll",                      (void *)SystemNative_Poll },
  { "SystemNative_PosixFAdvise",              (void *)SystemNative_PosixFAdvise },
  { "SystemNative_FAllocate",                 (void *)SystemNative_FAllocate },
  { "SystemNative_CopyFile",                  (void *)SystemNative_CopyFile },
  { "SystemNative_SysConf",                   (void *)SystemNative_SysConf },
  { "SystemNative_LChflagsCanSetHiddenFlag",  (void *)SystemNative_LChflagsCanSetHiddenFlag },
  { "SystemNative_CanGetHiddenFlag",          (void *)SystemNative_CanGetHiddenFlag },
  { "SystemNative_LChflags",                  (void *)SystemNative_LChflags },
  { "SystemNative_FChflags",                  (void *)SystemNative_FChflags },
  { "SystemNative_FileSystemSupportsLocking", (void *)SystemNative_FileSystemSupportsLocking },
  { "SystemNative_LockFileRegion",            (void *)SystemNative_LockFileRegion },
  { "SystemNative_MMap",                      (void *)SystemNative_MMap },
  { "SystemNative_MUnmap",                    (void *)SystemNative_MUnmap },
  { "SystemNative_MProtect",                  (void *)SystemNative_MProtect },
  { "SystemNative_MAdvise",                   (void *)SystemNative_MAdvise },
  { "SystemNative_MSync",                     (void *)SystemNative_MSync },
  { "SystemNative_INotifyInit",               (void *)SystemNative_INotifyInit },
  { "SystemNative_INotifyAddWatch",           (void *)SystemNative_INotifyAddWatch },
  { "SystemNative_INotifyRemoveWatch",        (void *)SystemNative_INotifyRemoveWatch },

  /* ---- pal_errno.c (upstream) ---- */
  { "SystemNative_SetErrNo",                  (void *)SystemNative_SetErrNo },
  { "SystemNative_GetErrNo",                  (void *)SystemNative_GetErrNo },
  { "SystemNative_ConvertErrorPlatformToPal", (void *)SystemNative_ConvertErrorPlatformToPal },
  { "SystemNative_ConvertErrorPalToPlatform", (void *)SystemNative_ConvertErrorPalToPlatform },
  { "SystemNative_StrErrorR",                 (void *)SystemNative_StrErrorR },

  /* ---- pal_threading.c (upstream) ---- */
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

  /* ---- pal_time.c (upstream) ---- */
  { "SystemNative_GetTimestamp",              (void *)SystemNative_GetTimestamp },
  { "SystemNative_GetLowResolutionTimestamp", (void *)SystemNative_GetLowResolutionTimestamp },
  { "SystemNative_GetBootTimeTicks",          (void *)SystemNative_GetBootTimeTicks },
  { "SystemNative_GetCpuUtilization",         (void *)SystemNative_GetCpuUtilization },
  { "SystemNative_UTimensat",                 (void *)SystemNative_UTimensat },
  { "SystemNative_FUTimens",                  (void *)SystemNative_FUTimens },

  /* ---- pal_memory.c (upstream) ---- */
  { "SystemNative_AlignedAlloc",              (void *)SystemNative_AlignedAlloc },
  { "SystemNative_AlignedFree",               (void *)SystemNative_AlignedFree },
  { "SystemNative_AlignedRealloc",            (void *)SystemNative_AlignedRealloc },
  { "SystemNative_Calloc",                    (void *)SystemNative_Calloc },
  { "SystemNative_Free",                      (void *)SystemNative_Free },
  { "SystemNative_Malloc",                    (void *)SystemNative_Malloc },
  { "SystemNative_Realloc",                   (void *)SystemNative_Realloc },

  /* ---- pal_random.c (upstream) ---- */
  { "SystemNative_GetNonCryptographicallySecureRandomBytes", (void *)SystemNative_GetNonCryptographicallySecureRandomBytes },
  { "SystemNative_GetCryptographicallySecureRandomBytes",    (void *)SystemNative_GetCryptographicallySecureRandomBytes },

  /* ---- pal_string.c (upstream) ---- */
  { "SystemNative_SNPrintF",                  (void *)SystemNative_SNPrintF },
  { "SystemNative_SNPrintF_1S",               (void *)SystemNative_SNPrintF_1S },
  { "SystemNative_SNPrintF_1I",               (void *)SystemNative_SNPrintF_1I },

  /* ---- pal_runtimeinformation.c (upstream) ---- */
  { "SystemNative_GetUnixRelease",            (void *)SystemNative_GetUnixRelease },
  { "SystemNative_GetUnixVersion",            (void *)SystemNative_GetUnixVersion },
  { "SystemNative_GetOSArchitecture",         (void *)SystemNative_GetOSArchitecture },

  /* ---- pal_log.c (upstream) ---- */
  { "SystemNative_Log",                       (void *)SystemNative_Log },
  { "SystemNative_LogError",                  (void *)SystemNative_LogError },

  /* ---- pal_datetime.c (upstream) ---- */
  { "SystemNative_GetSystemTimeAsTicks",      (void *)SystemNative_GetSystemTimeAsTicks },

  /* ---- pal_networking.c (upstream) ---- */
  { "SystemNative_GetHostEntryForName",       (void *)SystemNative_GetHostEntryForName },
  { "SystemNative_FreeHostEntry",             (void *)SystemNative_FreeHostEntry },
  { "SystemNative_GetNameInfo",               (void *)SystemNative_GetNameInfo },
  { "SystemNative_GetDomainName",             (void *)SystemNative_GetDomainName },
  { "SystemNative_GetHostName",               (void *)SystemNative_GetHostName },
  { "SystemNative_GetSocketAddressSizes",     (void *)SystemNative_GetSocketAddressSizes },
  { "SystemNative_GetAddressFamily",          (void *)SystemNative_GetAddressFamily },
  { "SystemNative_SetAddressFamily",          (void *)SystemNative_SetAddressFamily },
  { "SystemNative_GetPort",                   (void *)SystemNative_GetPort },
  { "SystemNative_SetPort",                   (void *)SystemNative_SetPort },
  { "SystemNative_GetIPv4Address",            (void *)SystemNative_GetIPv4Address },
  { "SystemNative_SetIPv4Address",            (void *)SystemNative_SetIPv4Address },
  { "SystemNative_GetIPv6Address",            (void *)SystemNative_GetIPv6Address },
  { "SystemNative_SetIPv6Address",            (void *)SystemNative_SetIPv6Address },
  { "SystemNative_GetControlMessageBufferSize", (void *)SystemNative_GetControlMessageBufferSize },
  { "SystemNative_TryGetIPPacketInformation", (void *)SystemNative_TryGetIPPacketInformation },
  { "SystemNative_GetIPv4MulticastOption",    (void *)SystemNative_GetIPv4MulticastOption },
  { "SystemNative_SetIPv4MulticastOption",    (void *)SystemNative_SetIPv4MulticastOption },
  { "SystemNative_GetIPv6MulticastOption",    (void *)SystemNative_GetIPv6MulticastOption },
  { "SystemNative_SetIPv6MulticastOption",    (void *)SystemNative_SetIPv6MulticastOption },
  { "SystemNative_GetLingerOption",           (void *)SystemNative_GetLingerOption },
  { "SystemNative_SetLingerOption",           (void *)SystemNative_SetLingerOption },
  { "SystemNative_SetReceiveTimeout",         (void *)SystemNative_SetReceiveTimeout },
  { "SystemNative_SetSendTimeout",            (void *)SystemNative_SetSendTimeout },
  { "SystemNative_Receive",                   (void *)SystemNative_Receive },
  { "SystemNative_ReceiveMessage",            (void *)SystemNative_ReceiveMessage },
  { "SystemNative_ReceiveSocketError",        (void *)SystemNative_ReceiveSocketError },
  { "SystemNative_Send",                      (void *)SystemNative_Send },
  { "SystemNative_SendMessage",               (void *)SystemNative_SendMessage },
  { "SystemNative_Accept",                    (void *)SystemNative_Accept },
  { "SystemNative_Bind",                      (void *)SystemNative_Bind },
  { "SystemNative_Connect",                   (void *)SystemNative_Connect },
  { "SystemNative_Connectx",                  (void *)SystemNative_Connectx },
  { "SystemNative_GetPeerName",               (void *)SystemNative_GetPeerName },
  { "SystemNative_GetSockName",               (void *)SystemNative_GetSockName },
  { "SystemNative_Listen",                    (void *)SystemNative_Listen },
  { "SystemNative_Shutdown",                  (void *)SystemNative_Shutdown },
  { "SystemNative_GetSocketErrorOption",      (void *)SystemNative_GetSocketErrorOption },
  { "SystemNative_GetSockOpt",               (void *)SystemNative_GetSockOpt },
  { "SystemNative_GetRawSockOpt",            (void *)SystemNative_GetRawSockOpt },
  { "SystemNative_SetSockOpt",               (void *)SystemNative_SetSockOpt },
  { "SystemNative_SetRawSockOpt",            (void *)SystemNative_SetRawSockOpt },
  { "SystemNative_Socket",                    (void *)SystemNative_Socket },
  { "SystemNative_GetSocketType",             (void *)SystemNative_GetSocketType },
  { "SystemNative_GetAtOutOfBandMark",        (void *)SystemNative_GetAtOutOfBandMark },
  { "SystemNative_GetBytesAvailable",         (void *)SystemNative_GetBytesAvailable },
  { "SystemNative_GetWasiSocketDescriptor",   (void *)SystemNative_GetWasiSocketDescriptor },
  { "SystemNative_CreateSocketEventPort",     (void *)SystemNative_CreateSocketEventPort },
  { "SystemNative_CloseSocketEventPort",      (void *)SystemNative_CloseSocketEventPort },
  { "SystemNative_CreateSocketEventBuffer",   (void *)SystemNative_CreateSocketEventBuffer },
  { "SystemNative_FreeSocketEventBuffer",     (void *)SystemNative_FreeSocketEventBuffer },
  { "SystemNative_TryChangeSocketEventRegistration", (void *)SystemNative_TryChangeSocketEventRegistration },
  { "SystemNative_WaitForSocketEvents",       (void *)SystemNative_WaitForSocketEvents },
  { "SystemNative_PlatformSupportsDualModeIPv4PacketInfo", (void *)SystemNative_PlatformSupportsDualModeIPv4PacketInfo },
  { "SystemNative_GetDomainSocketSizes",      (void *)SystemNative_GetDomainSocketSizes },
  { "SystemNative_GetMaximumAddressSize",     (void *)SystemNative_GetMaximumAddressSize },
  { "SystemNative_SendFile",                  (void *)SystemNative_SendFile },
  { "SystemNative_Disconnect",                (void *)SystemNative_Disconnect },
  { "SystemNative_InterfaceNameToIndex",      (void *)SystemNative_InterfaceNameToIndex },
  { "SystemNative_Select",                    (void *)SystemNative_Select },

  /* pal_interfaceaddresses.c (upstream, uses NuttX getifaddrs) */
  { "SystemNative_EnumerateInterfaceAddresses", (void *)SystemNative_EnumerateInterfaceAddresses },
  { "SystemNative_GetNetworkInterfaces",      (void *)SystemNative_GetNetworkInterfaces },
  { "SystemNative_EnumerateGatewayAddressesForInterface", (void *)SystemNative_EnumerateGatewayAddressesForInterface },

  /* pal_networkchange.c (upstream, returns ENOTSUP on NuttX) */
  { "SystemNative_CreateNetworkChangeListenerSocket", (void *)SystemNative_CreateNetworkChangeListenerSocket },
  { "SystemNative_ReadEvents",                (void *)SystemNative_ReadEvents },

  /* pal_networkstatistics.c (upstream, returns ENOTSUP on NuttX) */
  { "SystemNative_GetTcpGlobalStatistics",    (void *)SystemNative_GetTcpGlobalStatistics },
  { "SystemNative_GetIPv4GlobalStatistics",   (void *)SystemNative_GetIPv4GlobalStatistics },
  { "SystemNative_GetUdpGlobalStatistics",    (void *)SystemNative_GetUdpGlobalStatistics },
  { "SystemNative_GetIcmpv4GlobalStatistics", (void *)SystemNative_GetIcmpv4GlobalStatistics },
  { "SystemNative_GetIcmpv6GlobalStatistics", (void *)SystemNative_GetIcmpv6GlobalStatistics },
  { "SystemNative_GetEstimatedTcpConnectionCount", (void *)SystemNative_GetEstimatedTcpConnectionCount },
  { "SystemNative_GetActiveTcpConnectionInfos", (void *)SystemNative_GetActiveTcpConnectionInfos },
  { "SystemNative_GetEstimatedUdpListenerCount", (void *)SystemNative_GetEstimatedUdpListenerCount },
  { "SystemNative_GetActiveUdpListeners",     (void *)SystemNative_GetActiveUdpListeners },
  { "SystemNative_GetNativeIPInterfaceStatistics", (void *)SystemNative_GetNativeIPInterfaceStatistics },
  { "SystemNative_GetNumRoutes",              (void *)SystemNative_GetNumRoutes },
  { "SystemNative_MapTcpState",               (void *)SystemNative_MapTcpState },

  /* Additional upstream functions (from libSystem.Native.a) */
  { "SystemNative_CreateAutoreleasePool",     (void *)SystemNative_CreateAutoreleasePool },
  { "SystemNative_DrainAutoreleasePool",      (void *)SystemNative_DrainAutoreleasePool },
  { "SystemNative_GetDefaultTimeZone",        (void *)SystemNative_GetDefaultTimeZone },
  { "SystemNative_GetTimeZoneData",           (void *)SystemNative_GetTimeZoneData },
  { "SystemNative_GetPeerID",                 (void *)SystemNative_GetPeerID },
  { "SystemNative_iOSSupportVersion",         (void *)SystemNative_iOSSupportVersion },
  { "SystemNative_IsMemfdSupported",          (void *)SystemNative_IsMemfdSupported },
  { "SystemNative_MemfdCreate",               (void *)SystemNative_MemfdCreate },
  { "SystemNative_PReadV",                    (void *)SystemNative_PReadV },
  { "SystemNative_PWriteV",                   (void *)SystemNative_PWriteV },
  { "SystemNative_ReadProcessInfo",           (void *)SystemNative_ReadProcessInfo },
  { "SystemNative_ReadThreadInfo",            (void *)SystemNative_ReadThreadInfo },
  { "SystemNative_SearchPath",                (void *)SystemNative_SearchPath },
  { "SystemNative_SearchPath_TempDirectory",  (void *)SystemNative_SearchPath_TempDirectory },
  { "SystemNative_ShmOpen",                   (void *)SystemNative_ShmOpen },
  { "SystemNative_ShmUnlink",                 (void *)SystemNative_ShmUnlink },

  /* ---- NuttX-specific stubs (no upstream equivalent) ---- */

  /* Console / terminal */
  { "SystemNative_IsATty",                    (void *)sysn_isatty },
  { "SystemNative_InitializeTerminalAndSignalHandling", (void *)sysn_initialize_terminal_and_signal_handling },
  { "SystemNative_SetKeypadXmit",             (void *)sysn_set_keypad_xmit },
  { "SystemNative_SetTerminalInvalidationHandler", (void *)sysn_set_terminal_invalidation_handler },
  { "SystemNative_UninitializeTerminal",      (void *)sysn_uninitialize_terminal },
  { "SystemNative_GetControlCharacters",      (void *)sysn_get_control_characters },
  { "SystemNative_GetWindowSize",             (void *)sysn_get_window_size },

  /* ---- pal_environment.c (upstream, with diag wrapper) ---- */
  { "SystemNative_GetEnv",                    (void *)SystemNative_GetEnv },
  { "SystemNative_GetEnviron",                (void *)SystemNative_GetEnviron },
  { "SystemNative_FreeEnviron",               (void *)SystemNative_FreeEnviron },

  /* ---- pal_process.c (upstream) ---- */
  { "SystemNative_SysLog",                    (void *)SystemNative_SysLog },
  { "SystemNative_Abort",                     (void *)SystemNative_Abort },
  { "SystemNative_Exit",                      (void *)SystemNative_Exit },
  { "SystemNative_GetPid",                    (void *)SystemNative_GetPid },
  { "SystemNative_GetProcessPath",            (void *)SystemNative_GetProcessPath },
  { "SystemNative_ForkAndExecProcess",        (void *)SystemNative_ForkAndExecProcess },
  { "SystemNative_GetSid",                    (void *)SystemNative_GetSid },
  { "SystemNative_Kill",                      (void *)SystemNative_Kill },
  { "SystemNative_GetRLimit",                 (void *)SystemNative_GetRLimit },
  { "SystemNative_SetRLimit",                 (void *)SystemNative_SetRLimit },
  { "SystemNative_WaitIdAnyExitedNoHangNoWait", (void *)SystemNative_WaitIdAnyExitedNoHangNoWait },
  { "SystemNative_WaitPidExitedNoHang",       (void *)SystemNative_WaitPidExitedNoHang },
  { "SystemNative_PathConf",                  (void *)SystemNative_PathConf },
  { "SystemNative_GetPriority",               (void *)SystemNative_GetPriority },
  { "SystemNative_SetPriority",               (void *)SystemNative_SetPriority },
  { "SystemNative_GetCwd",                    (void *)SystemNative_GetCwd },
  { "SystemNative_SchedSetAffinity",          (void *)SystemNative_SchedSetAffinity },
  { "SystemNative_SchedGetAffinity",          (void *)SystemNative_SchedGetAffinity },

  /* Signals */
  { "SystemNative_SetPosixSignalHandler",     (void *)sysn_set_posix_signal_handler },
  { "SystemNative_EnablePosixSignalHandling", (void *)sysn_enable_posix_signal_handling },
  { "SystemNative_DisablePosixSignalHandling",(void *)sysn_disable_posix_signal_handling },
  { "SystemNative_HandleNonCanceledPosixSignal",(void *)sysn_handle_noncanceled_posix_signal },
  { "SystemNative_GetPlatformSignalNumber",   (void *)sysn_get_platform_signal_number },

  /* UID/GID */
  { "SystemNative_GetEUid",                   (void *)sysn_get_euid },
  { "SystemNative_GetEGid",                   (void *)sysn_get_egid },
  { "SystemNative_SetEUid",                   (void *)sysn_set_euid },
  { "SystemNative_GetPwUidR",                 (void *)sysn_get_pw_uid_r },
  { "SystemNative_GetGroups",                 (void *)sysn_get_groups },

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
  else if (strcmp(libraryName, "System.Globalization.Native") == 0 ||
           strcmp(libraryName, "libSystem.Globalization.Native") == 0)
    {
      mappings = globalization_native_mappings;
    }
  else if (strcmp(libraryName, "System.Security.Cryptography.Native.OpenSsl") == 0 ||
           strcmp(libraryName, "libSystem.Security.Cryptography.Native.OpenSsl") == 0)
    {
      mappings = crypto_native_mappings;
    }
#if defined (CONFIG_EXAMPLES_MEADOW_SQLITE)
  else if (strcmp(libraryName, "sqlite3") == 0 ||
           strcmp(libraryName, "libsqlite3") == 0)
    {
      /* TODO: sqlite mappings not yet ported */
      return NULL;
    }
#endif
  else
    {
      printf("P/Invoke: unknown library '%s' (looking for '%s')\n",
             libraryName, entrypointName);
      return NULL;
    }

  for (MonoDlMapping *m = mappings; m->name != NULL; m++)
    {
      if (strcmp(m->name, entrypointName) == 0)
        {
          return m->addr;
        }
    }

  printf("P/Invoke: %s::%s — NOT FOUND (will throw EntryPointNotFoundException)\n",
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
             base_path, get_errno());
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
  int pass2_count = 0;
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
          pass2_count++;
        }
    }

  closedir(dir);
  syslog(LOG_ERR, "TPA: Found %d assemblies (pass2: %d) in '%s'\n",
         count, pass2_count, base_path);
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
  /* JIT mode: meadow.config.yaml can override with --interp if needed.
   * The ARM/Thumb2 JIT codegen bugs (Dictionary OverflowException) are
   * worked around in AppContext.cs (catch block). */
  setenv("TMPDIR", "/meadow0/Temp", 1);
  setenv("DOTNET_SYSTEM_GLOBALIZATION_INVARIANT", "1", 1);
  /* Use raw resource keys instead of loading .resources files.
   * This avoids ResourceManager initialization which can fail on NuttX.
   * Disabling this causes mono_get_restore_context() assertion failure
   * because interpreter-only mode doesn't set up restore_context_func. */
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
         "max-heap-size=8m,nursery-size=512k,soft-heap-limit=4m,"
         "major=marksweep", 1);

  /* Also set via direct API — NuttX getenv() may not see setenv() in
   * protected mode (kernel-managed env vs user-space libc). */
  {
    extern void mono_gc_params_set(const char *options);
    mono_gc_params_set("max-heap-size=8m,nursery-size=512k,"
                       "soft-heap-limit=4m,major=marksweep");
  }

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
    "System.Globalization.Invariant",
    "System.Resources.UseSystemResourceKeys",
    "TRUSTED_PLATFORM_ASSEMBLIES",
    "APP_PATHS",
    "NATIVE_DLL_SEARCH_DIRECTORIES",
    "PINVOKE_OVERRIDE",
    "APP_CONTEXT_BASE_DIRECTORY",
  };

  const char *property_values[] = {
    "true",
    "true",
    tpa_list,
    MONO_MEADOW_EXECUTABLE_PARTITION_NAME,
    MONO_MEADOW_EXECUTABLE_PARTITION_NAME,
    pinvoke_override_str,
    MONO_MEADOW_EXECUTABLE_PARTITION_NAME "/",
  };

  int property_count = sizeof(property_keys) / sizeof(property_keys[0]);

  syslog(LOG_INFO, "Calling monovm_initialize with %d properties...\n",
         property_count);
  syslog(LOG_INFO, "  TPA: %s\n", tpa_list);
  syslog(LOG_INFO, "  APP_PATHS: %s\n", MONO_MEADOW_EXECUTABLE_PARTITION_NAME);
  syslog(LOG_INFO, "  PINVOKE_OVERRIDE: %s\n", pinvoke_override_str);

  /* UPD driver note: Meadow.Core opens /dev/upd with DriverFlags.DontCare=0.
   * This works because NuttX O_RDONLY=1 (not POSIX 0), so flags=0 passes
   * inode_checkflags(). The UPD driver only has .open/.close/.ioctl — no
   * .read/.write — so O_RDONLY(1) or O_RDWR(3) would return EACCES. */

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

  /* Ensure stdout/stderr are redirected to HCOM FIFOs.
   *
   * The HCOM subsystem creates /dev/monostdout and /dev/monostderr FIFOs
   * and starts a MonoStdxxx reader thread that polls them, forwarding
   * data over UART4 → TCP:4242 → Meadow CLI ("meadow listen").
   *
   * Normally hcom_mono_ctrl_mono_appears_to_be_running() does the dup2
   * redirect, but in the Renode emulator a hook skips that function
   * (it also touches GPIOs and BBR registers that don't exist in emu).
   * We do the redirect explicitly here so it works in both environments.
   *
   * Note: hcom_mono_open_mono_fifo has a bug — it always dup2's to
   * STDOUT_FILENO regardless of the stdxxxFileNo parameter.  We fix
   * that here by dup2'ing to the correct fd.
   */

  {
    int fd, flags;
    struct stat st;

    /* Check if stdout is already a FIFO (redirect already happened) */
    if (fstat(STDOUT_FILENO, &st) < 0 || !S_ISFIFO(st.st_mode))
      {
        syslog(LOG_NOTICE, "stdout is not a FIFO — redirecting to HCOM\n");

        fd = open("/dev/monostdout", O_WRONLY | O_NONBLOCK);
        if (fd >= 0)
          {
            dup2(fd, STDOUT_FILENO);
            if (fd > STDERR_FILENO)
              close(fd);
            syslog(LOG_NOTICE, "stdout → /dev/monostdout (HCOM)\n");
          }
        else
          {
            syslog(LOG_WARNING, "open /dev/monostdout failed: errno=%d "
                   "(HCOM reader not running?)\n", get_errno());
          }

        fd = open("/dev/monostderr", O_WRONLY | O_NONBLOCK);
        if (fd >= 0)
          {
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO)
              close(fd);
            syslog(LOG_NOTICE, "stderr → /dev/monostderr (HCOM)\n");
          }
        else
          {
            syslog(LOG_WARNING, "open /dev/monostderr failed: errno=%d\n",
                   get_errno());
          }
      }
    else
      {
        syslog(LOG_NOTICE, "stdout already a FIFO — HCOM redirect OK\n");
      }

    /* Make FIFOs non-blocking so writes return EAGAIN instead of hanging
     * when the FIFO buffer fills (no CLI client, or reader can't keep up).
     * Mono runtime output also goes to syslog (MONO_LOG_DEST=syslog).
     */

    flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
    if (flags >= 0)
      fcntl(STDOUT_FILENO, F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(STDERR_FILENO, F_GETFL, 0);
    if (flags >= 0)
      fcntl(STDERR_FILENO, F_SETFL, flags | O_NONBLOCK);
  }

  g_mono_stage = 3; /* HCOM redirect + O_NONBLOCK set */
  syslog(LOG_NOTICE, "stdout/stderr HCOM redirect complete (stage %d)\n",
         g_mono_stage);

  g_mono_stage = 4; /* About to chdir */

  /* Change to the app directory */

  chdir(MONO_MEADOW_EXECUTABLE_PARTITION_NAME);

  g_mono_stage = 5; /* About to open app */

  /* Check if the app assembly exists before trying to execute it */

  char *app_path = MONO_MEADOW_EXECUTABLE_APP_EXE;
  syslog(LOG_NOTICE, "Entry assembly: %s\n", app_path);
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

      /* Diagnostic: verify critical assembly files are accessible and check stat modes */
      {
        const char *check_files[] = {
          "/meadow0/Microsoft.Win32.Primitives.dll",
          "/meadow0/System.Threading.ThreadPool.dll",
          "/meadow0/System.Console.dll",
          "/meadow0/System.Runtime.dll",
          NULL
        };
        for (int ci = 0; check_files[ci]; ci++)
          {
            struct stat cst;
            int sr = stat(check_files[ci], &cst);
            if (sr == 0)
              {
                syslog(LOG_ERR,
                       "FILE CHECK: %s => size=%d, mode=0x%x, S_ISREG=%d\n",
                       check_files[ci], (int)cst.st_size,
                       (unsigned)cst.st_mode, S_ISREG(cst.st_mode) ? 1 : 0);
                /* Dump first 256 bytes to verify file content identity */
                int dfd = open(check_files[ci], O_RDONLY);
                if (dfd >= 0) {
                  uint8_t hdr[256];
                  ssize_t nr = read(dfd, hdr, sizeof(hdr));
                  close(dfd);
                  if (nr > 0) {
                    /* Find assembly name string - scan for "Microsoft" or "System" after PE header */
                    syslog(LOG_ERR, "FILE DUMP: %s first 16 bytes: %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x\n",
                           check_files[ci],
                           hdr[0], hdr[1], hdr[2], hdr[3],
                           hdr[4], hdr[5], hdr[6], hdr[7],
                           hdr[8], hdr[9], hdr[10], hdr[11],
                           hdr[12], hdr[13], hdr[14], hdr[15]);
                  }
                }
              }
            else
              {
                syslog(LOG_ERR, "FILE CHECK: %s => stat FAILED (errno=%d)\n",
                       check_files[ci], get_errno());
              }
          }
      }

      syslog(LOG_NOTICE, "Executing assembly: %s\n", app_path);

      /* Pass --root so MeadowOS.FindAppType searches for App.dll on disk */
      unsigned int exit_code = 0;
      const char *managed_args[] = { "--root", MONO_MEADOW_EXECUTABLE_PARTITION_NAME };
      ret = monovm_execute_assembly(2, managed_args, app_path, &exit_code);
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
