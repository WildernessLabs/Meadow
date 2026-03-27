# Track 08-PAL: System.Native PAL Port for NuttX

## Goal

Replace ~102 hand-maintained stubs in `mono_main.c` with the upstream `pal_*.c` source files from `runtime/src/native/libs/System.Native/`, compiled directly with `#ifdef __NuttX__` guards. This gives us correct, complete implementations instead of returning 0 for unknown functions.

## Why Now

The catch-all no-op stub silently returns 0 for ~160 unimplemented functions. This masks bugs and will cause hard-to-diagnose failures as we expand beyond Blinky (Track 09 tests, Track 11 networking). Porting the upstream source gives us:
- Correct error handling and errno propagation
- Complete coverage of all 265 System.Native functions
- Maintainable code that tracks upstream changes
- Foundation for TLS/networking (Track 11)

## Current State

- **102 functions** stubbed in `apps/examples/mono/mono_main.c` (lines ~100-1500)
- **~160 functions** served by catch-all no-op returning 0
- **`__NuttX__` guards already exist** in upstream pal_time.c, pal_errno.c, pal_memory.c, pal_threading.c
- No upstream CMake support for NuttX target yet

## Architecture Decision: Compile Upstream vs. Maintain Stubs

**Chosen: Compile upstream pal_*.c files with NuttX conditionals.**

Rationale: The upstream code handles edge cases, error codes, and platform differences that our stubs don't. Adding `#ifdef __NuttX__` blocks follows the existing pattern (see pal_time.c, pal_errno.c). The alternative — maintaining 265 parallel stubs — is a maintenance burden that grows with each runtime update.

Integration: The PAL .c files compile into `libmono-component-*` or get linked into the NuttX firmware build via CMakeLists.txt. The P/Invoke override in `mono_main.c` maps "libSystem.Native" calls to these compiled-in functions instead of stub implementations.

---

## Phase 1: Build System Integration

**Goal**: Compile upstream pal_*.c files into the NuttX firmware build.

### 1A. Add NuttX target to System.Native CMakeLists.txt

Add `CLR_CMAKE_TARGET_NUTTX` flag alongside existing `CLR_CMAKE_TARGET_BROWSER`, `CLR_CMAKE_TARGET_WASI`, etc.

File: `runtime/src/native/libs/System.Native/CMakeLists.txt`

Core files (always compiled):
- pal_errno.c, pal_memory.c, pal_random.c, pal_runtimeinformation.c, pal_string.c, pal_time.c, pal_datetime.c

NuttX-specific selections:
- pal_io.c (with `__NuttX__` guards for missing features)
- pal_threading.c (already has NuttX support)
- pal_uid.c (stub most functions — single-user system)
- pal_signal.c (reduced signal support on NuttX)
- pal_process.c (no fork/exec — stub)
- pal_environment.c
- pal_console.c (reduced — no full terminal)

Skip for NuttX:
- pal_networking.c (Phase 3)
- pal_ifaddrs.c, pal_interfaceaddresses.c (Phase 3)
- pal_networkchange.c, pal_networkstatistics.c (Phase 3)
- pal_mount.c (NuttX mount model differs significantly)
- pal_iossupportversion.c, pal_autoreleasepool.c (Apple-only)
- pal_*_wasm.c, pal_*_wasi.c, pal_*_browser.c (other platforms)

### 1B. Integration with NuttX firmware build

The compiled PAL object files need to be linked into the firmware. Two approaches:

**Option A**: Compile as part of the mono library build (CMake), link via `libmono-component-System.Native.a`
**Option B**: Compile the .c files directly in the NuttX apps/examples/mono Makefile

Option A is preferred — keeps PAL code in the runtime repo where it belongs.

### 1C. Update P/Invoke override

Modify `meadow_pinvoke_override()` in `mono_main.c` to map function names to the compiled upstream implementations instead of local stubs. The mapping table already exists — just change the function pointers.

### Files modified (Phase 1)
- `runtime/src/native/libs/System.Native/CMakeLists.txt` — Add NuttX target
- `runtime/src/mono/CMakeLists.txt` — Link System.Native into mono library for NuttX
- `Meadow/apps/examples/mono/mono_main.c` — Remove stubs, update mapping to upstream functions

---

## Phase 2: pal_io.c Port (69 functions)

**Goal**: Full file I/O through upstream code. This is the highest-impact module.

### NuttX Compatibility Matrix for pal_io.c

| Function Group | Count | NuttX Support | Action |
|---------------|-------|--------------|--------|
| Basic I/O (Open, Close, Read, Write, LSeek) | 5 | Full | Direct compile |
| Stat family (Stat, LStat, FStat) | 3 | Full (struct differences) | `__NuttX__` guards for struct stat fields |
| Directory ops (OpenDir, ReadDir, CloseDir) | 3 | Full | Direct compile |
| File ops (MkDir, RmDir, Unlink, Rename, ChMod, FChMod) | 6 | Full | Direct compile |
| Pipe/Poll (Pipe, Poll) | 2 | Full | Direct compile |
| Fcntl family (GetFD, SetFD, IsNonBlocking, etc.) | 6 | Partial | NuttX fcntl is limited; stub pipe size ops |
| Dup/Dup2 | 2 | Full | Direct compile |
| FSync/FTruncate | 2 | Full | Direct compile |
| PRead/PWrite/PReadV/PWriteV | 4 | Full (NuttX has pread/pwrite) | Direct compile |
| Access/ChDir/GetCwd | 3 | Full | Direct compile |
| Link/SymLink/ReadLink | 3 | Stub | NuttX has no symlinks; return ENOSYS |
| MkFifo/MkNod | 2 | Partial | MkFifo works, MkNod limited |
| Mmap family (MProtect, MSync, MUnmap, MAdvise) | 4 | Stub | NuttX mmap is very limited |
| INotify (Init, AddWatch, RemoveWatch) | 3 | Stub | No inotify on NuttX |
| Memfd/Shm (MemfdCreate, ShmOpen, ShmUnlink) | 3 | Stub | No memfd on NuttX |
| FAllocate/PosixFAdvise | 2 | Stub | No fallocate on NuttX |
| CopyFile | 1 | Stub | No sendfile/copy_file_range; use read+write fallback |
| MksTemps | 1 | Partial | NuttX has mkstemp but not mkstemps |
| Misc (SysConf, FLock, LockFileRegion, Sync) | 4 | Partial | sysconf limited; flock no-op |
| Flags (LChflags, FChflags, CanGetHiddenFlag, LChflagsCanSetHiddenFlag) | 4 | Stub | No UF_HIDDEN on NuttX |
| Process/Thread info (ReadProcessInfo, ReadThreadInfo) | 2 | Stub | No /proc on NuttX |
| DeviceIdentifiers/PeerID | 2 | Partial | |

### NuttX-specific `#ifdef __NuttX__` blocks needed

1. **struct stat differences**: NuttX stat is minimal — missing st_uid, st_gid, st_ino, st_dev, st_rdev, st_blocks, st_blksize. Set these to 0 in the conversion function.
2. **No inotify**: Return ENOSYS for INotify* functions.
3. **No mmap**: Return ENOSYS for mmap-related functions.
4. **No memfd/shm**: Return ENOSYS.
5. **No symlinks**: ReadLink, SymLink, Link return ENOSYS.
6. **Limited fcntl**: Pipe size operations return ENOSYS.
7. **No /proc**: ReadProcessInfo, ReadThreadInfo return empty/error.

### Files modified (Phase 2)
- `runtime/src/native/libs/System.Native/pal_io.c` — Add `#ifdef __NuttX__` blocks (~15 locations)

---

## Phase 3: pal_networking.c Port (57 functions)

**Goal**: BSD socket API through upstream code. Required for Track 11 (TLS/Networking).

### NuttX Compatibility Matrix for pal_networking.c

| Function Group | Count | NuttX Support | Action |
|---------------|-------|--------------|--------|
| Socket basics (Socket, Bind, Listen, Accept, Connect) | 5 | Full | Direct compile |
| Send/Receive (Send, Receive, SendMessage, ReceiveMessage) | 4 | Full | Direct compile |
| Socket options (Get/SetSockOpt, Get/SetRawSockOpt) | 4 | Full | Direct compile |
| Address ops (GetSockName, GetPeerName, GetPort, SetPort, etc.) | 8 | Full | Direct compile |
| Name resolution (GetHostEntryForName, GetNameInfo, FreeHostEntry) | 3 | Partial | NuttX DNS is basic |
| Select/Poll | 1 | Full | Direct compile |
| IPv4/IPv6 multicast | 4 | Partial | NuttX multicast limited |
| Linger/Timeout options | 4 | Full | Direct compile |
| Shutdown/Disconnect | 2 | Full | Direct compile |
| Event port (epoll/kqueue) | 4 | Partial | NuttX has poll, not epoll; use poll fallback |
| SendFile | 1 | Stub | No sendfile on NuttX; fallback to read+write |
| Connectx (Apple-only) | 1 | Stub | Not on NuttX |
| Domain sockets | 2 | Full | NuttX has AF_LOCAL |
| Advanced (GetBytesAvailable, AtOutOfBandMark, etc.) | 4 | Partial | |
| GetHostName/GetDomainName | 2 | Full | Direct compile |
| Misc (InterfaceNameToIndex, PlatformSupportsDualMode, etc.) | 3 | Partial | |

### NuttX-specific concerns

1. **No epoll**: NuttX uses poll(). The upstream code has a `#ifdef HAVE_KQUEUE` / `#ifdef HAVE_EPOLL` split. Add `#ifdef __NuttX__` to use poll-based event waiting.
2. **Limited multicast**: Some multicast socket options may not be available.
3. **DNS resolution**: NuttX DNS is basic — getaddrinfo/getnameinfo may have limitations.
4. **No sendfile**: Fallback to read+write loop.
5. **Struct sockaddr layout**: Verify NuttX sockaddr matches expected layout.

### Files modified (Phase 3)
- `runtime/src/native/libs/System.Native/pal_networking.c` — Add `#ifdef __NuttX__` blocks (~10 locations)
- `runtime/src/native/libs/System.Native/pal_interfaceaddresses.c` — NuttX getifaddrs stubs
- `runtime/src/native/libs/System.Native/pal_networkstatistics.c` — Stub or implement with NuttX APIs

---

## Phase 4: Remaining PAL Modules

Port the remaining modules with NuttX guards:

| Module | Priority | Complexity | Notes |
|--------|----------|------------|-------|
| pal_process.c | Medium | Medium | No fork/exec; stub ForkAndExecProcess, keep GetProcessPath |
| pal_signal.c | Medium | Medium | NuttX signals are limited; stub most handlers |
| pal_console.c | Low | Low | Reduced terminal support; IsATty, GetWindowSize |
| pal_mount.c | Low | Medium | NuttX mount model differs; stub GetAllMountPoints |
| pal_uid.c | Low | Low | Single-user; return root for everything |

### Files modified (Phase 4)
- Multiple pal_*.c files — `#ifdef __NuttX__` blocks

---

## Phase 5: Remove Stubs from mono_main.c

**Goal**: Delete all 102 stub functions from mono_main.c, leaving only:
1. The P/Invoke override dispatcher (maps function names to upstream implementations)
2. NuttX-specific functions not in System.Native (e.g., HCOM, UPD driver mappings)
3. The monovm initialization and startup code

This dramatically simplifies mono_main.c and eliminates the maintenance burden.

---

## Verification Strategy

| Phase | Test | Success Criteria |
|-------|------|-----------------|
| 1 | Firmware builds with upstream PAL linked | Clean compile, no undefined symbols |
| 2 | Blinky still works with upstream pal_io | Same HCOM output as before (10 LED cycles) |
| 3 | Simple socket test app | Can create socket, bind, connect (loopback) |
| 4 | Full test suite (Track 09) | No regressions from stub removal |
| 5 | mono_main.c reduced to <500 lines | Clean, maintainable firmware entry point |

## Risk Assessment

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| NuttX struct layout mismatches | Medium | Compare sizeof/offsetof at build time; add static_assert |
| Missing POSIX functions on NuttX | Medium | Stub with ENOSYS; document gaps |
| Build system integration complexity | Medium | Start with Option B (compile in NuttX Makefile) if CMake integration is difficult |
| Upstream PAL code assumes features NuttX lacks | High | Systematic `#ifdef __NuttX__` audit of each function |
| Performance regression from full PAL vs. thin stubs | Low | PAL functions are thin wrappers; minimal overhead |

## Dependencies

- **Track 08 (Hardware Validation)**: Can proceed in parallel — hardware testing uses existing stubs
- **Track 09 (Tests)**: Benefits from complete PAL — test failures may be masked by stub behavior
- **Track 11 (TLS/Networking)**: Blocked on Phase 3 (pal_networking.c port)
