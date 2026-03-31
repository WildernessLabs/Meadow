/****************************************************************************
 * examples/mono/mono_nuttx_stubs.c
 *
 *   Copyright (C) 2026 Wilderness Labs. All rights reserved.
 *
 *   Stub implementations for symbols required by .NET 10 libmonosgen-2.0.a
 *   that are not available in NuttX or need platform-specific implementations.
 *
 ****************************************************************************/

#include <nuttx/config.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/****************************************************************************
 * Mono native signal/crash handler stubs
 *
 * These are normally provided by mini-posix.c or mini-windows.c.
 * On NuttX, we provide minimal stubs since we handle crashes via
 * NuttX's own fault handlers.
 ****************************************************************************/

typedef void MonoContext;
typedef void MonoThreadHandle;
typedef void MonoThreadUnwindState;

void mono_dump_native_crash_info(const char *signal, MonoContext *mctx,
                                 void *info)
{
  syslog(LOG_ERR, "MONO CRASH: signal=%s\n", signal ? signal : "unknown");
}

void mono_post_native_crash_handler(const char *signal, MonoContext *mctx,
                                    void *info, int crash_chaining)
{
  syslog(LOG_ERR, "MONO POST-CRASH: signal=%s, chaining=%d\n",
         signal ? signal : "unknown", crash_chaining);
}

int mono_chain_signal(int signal, void *siginfo, void *ctx,
                      int chain_to_other)
{
  return 0;
}

void mono_chain_signal_to_default_sigsegv_handler(void)
{
  syslog(LOG_ERR, "MONO: SIGSEGV chain requested (not supported on NuttX)\n");
}

void mono_runtime_install_handlers(void)
{
  /* NuttX handles signals/faults through its own mechanism */
}

void mono_runtime_setup_stat_profiler(void)
{
  /* Statistical profiler not supported on NuttX */
}

void mono_init_native_crash_info(void)
{
  /* No native crash info infrastructure on NuttX */
}

/* mono_setmmapjit now provided by libmonosgen (mono-mmap.c) with HAVE_MMAP=1 */

/* mmap/munmap stubs — NuttX doesn't have mmap in user space, but Mono
 * requires it (HAVE_MMAP=1 for the fileio fallback path). We implement
 * mmap using posix_memalign + read, and munmap as a no-op.
 *
 * For file-backed mappings, the SDRAM assembly cache is checked first.
 * If the file was pre-loaded into SDRAM at startup, data is served from
 * the cache via memcpy (fast). Otherwise, falls back to read() from the
 * filesystem (slow — goes through LFS → QSPI).
 */

#ifndef MAP_FAILED
#define MAP_FAILED ((void *)-1)
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE 0x02
#endif

/****************************************************************************
 * SDRAM assembly cache — pre-loaded file data served by mmap
 *
 * Files are read from LFS/QSPI into SDRAM buffers once at startup by
 * mono_main.c. The mmap stub checks the cache by fd (via fstat to match
 * inode/size) and serves data directly from SDRAM instead of re-reading
 * from flash.
 ****************************************************************************/

#define SDRAM_CACHE_MAX_FILES 8

struct sdram_cache_entry
{
  const char *path;      /* Full filesystem path (for logging) */
  void       *data;      /* SDRAM buffer with file contents */
  size_t      size;      /* File size in bytes */
  int         active;    /* Entry is valid */
};

static struct sdram_cache_entry _sdram_cache[SDRAM_CACHE_MAX_FILES];
static int _sdram_cache_count = 0;

/* Register a pre-loaded file in the SDRAM cache.
 * Called from mono_main.c after reading the file into an SDRAM buffer.
 */

void sdram_cache_register(const char *path, void *data, size_t size)
{
  if (_sdram_cache_count >= SDRAM_CACHE_MAX_FILES)
    return;

  struct sdram_cache_entry *e = &_sdram_cache[_sdram_cache_count++];
  e->path   = path;
  e->data   = data;
  e->size   = size;
  e->active = 1;
}

/* fd-to-path tracking — allows mmap to identify which file an fd belongs to.
 * mono_file_map_open() calls open(); we intercept via sdram_cache_track_fd().
 */

#define SDRAM_FD_MAP_MAX 32

struct sdram_fd_entry
{
  int   fd;
  char  path[128];
};

static struct sdram_fd_entry _fd_map[SDRAM_FD_MAP_MAX];
static int _fd_map_count = 0;

void sdram_cache_track_fd(int fd, const char *path)
{
  if (_fd_map_count >= SDRAM_FD_MAP_MAX || fd < 0)
    return;
  struct sdram_fd_entry *e = &_fd_map[_fd_map_count++];
  e->fd = fd;
  strncpy(e->path, path, sizeof(e->path) - 1);
  e->path[sizeof(e->path) - 1] = '\0';
}

/* Look up an fd in the SDRAM cache by matching file path.
 * First find the path from fd tracking, then match against cached entries.
 */

static struct sdram_cache_entry *sdram_cache_lookup_by_fd(int fd)
{
  /* Find path for this fd */
  const char *path = NULL;
  for (int i = 0; i < _fd_map_count; i++)
    {
      if (_fd_map[i].fd == fd)
        {
          path = _fd_map[i].path;
          break;
        }
    }
  if (!path)
    return NULL;

  /* Match by path */
  for (int i = 0; i < _sdram_cache_count; i++)
    {
      if (_sdram_cache[i].active && strcmp(_sdram_cache[i].path, path) == 0)
        return &_sdram_cache[i];
    }
  return NULL;
}

static size_t _mmap_total = 0;
static int _mmap_count = 0;

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset)
{
  void *ptr = NULL;

  if (length == 0)
    return MAP_FAILED;

  _mmap_count++;
  _mmap_total += length;
  syslog(LOG_ERR, "mmap #%d: %ld bytes (total %ld KB, %s)\n",
         _mmap_count, (long)length, (long)(_mmap_total / 1024),
         (flags & MAP_ANONYMOUS) ? "anon" : "file");

  /* Allocate page-aligned memory in SDRAM (user heap) */
  if (posix_memalign(&ptr, 4096, length) != 0)
    return MAP_FAILED;

  if (flags & MAP_ANONYMOUS)
    {
      /* Anonymous mapping — just zero the memory */
      memset(ptr, 0, length);
    }
  else
    {
      /* File-backed mapping — check SDRAM cache first (matched by fd → path) */
      struct sdram_cache_entry *cached = sdram_cache_lookup_by_fd(fd);
      if (cached && offset + length <= cached->size)
        {
          /* Cache hit: memcpy from SDRAM (fast) */
          memcpy(ptr, (char *)cached->data + offset, length);
        }
      else if (cached && offset < cached->size)
        {
          /* Partial cache hit: copy available data, zero the rest */
          size_t avail = cached->size - offset;
          memcpy(ptr, (char *)cached->data + offset, avail);
          memset((char *)ptr + avail, 0, length - avail);
        }
      else
        {
          /* Cache miss: read from filesystem (slow path) */
          off_t saved = lseek(fd, 0, SEEK_CUR);
          lseek(fd, offset, SEEK_SET);

          size_t total = 0;
          while (total < length)
            {
              ssize_t n = read(fd, (char *)ptr + total, length - total);
              if (n <= 0)
                break;
              total += n;
            }

          if (total < length)
            memset((char *)ptr + total, 0, length - total);

          lseek(fd, saved, SEEK_SET);
        }
    }

  return ptr;
}

int munmap(void *addr, size_t length)
{
  /* mono_valloc/mono_vfree now bypass mmap/munmap on NuttX (handled in
   * mono-mmap.c via memalign/free directly), so the main callers of this
   * stub are mono_file_map/mono_file_unmap for assembly file mappings.
   * Those pass the exact pointer from posix_memalign, so free() is safe. */
  free(addr);
  (void)length;
  return 0;
}

int mprotect(void *addr, size_t len, int prot)
{
  return 0; /* no-op — no memory protection on Cortex-M7 */
}

int posix_madvise(void *addr, size_t len, int advice)
{
  return 0; /* no-op — no memory advice on NuttX */
}

int mono_thread_state_init_from_handle(MonoThreadUnwindState *tctx,
                                       MonoThreadHandle *info,
                                       void *sigctx)
{
  /* Thread state init from handle not supported on NuttX */
  return 0;
}

/****************************************************************************
 * POSIX stubs not available in NuttX
 ****************************************************************************/

struct rlimit {
  unsigned long rlim_cur;
  unsigned long rlim_max;
};

int getrlimit(int resource, struct rlimit *rlim)
{
  if (rlim)
    {
      rlim->rlim_cur = 0;
      rlim->rlim_max = 0;
    }

  return -1;
}

int setrlimit(int resource, const struct rlimit *rlim)
{
  return -1;
}

int gettid(void)
{
  return (int)getpid();
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
  void *p;

  if (alignment < sizeof(void *) || (alignment & (alignment - 1)) != 0)
    {
      return 22; /* EINVAL */
    }

  p = memalign(alignment, size);
  if (p == NULL)
    {
      return 12; /* ENOMEM */
    }

  *memptr = p;
  return 0;
}

/****************************************************************************
 * libc functions that may be missing in NuttX user space
 ****************************************************************************/

int putchar(int c)
{
  /* Discard — stdout may be a blocking FIFO */
  (void)c;
  return c;
}

/* srand48/lrand48 — POSIX random number functions not in legacy NuttX.
 * Mono's minipal random.c uses these as a fallback randomness source.
 * Wrap to srand()/rand() which are available.
 */

void srand48(long int seedval)
{
  srand((unsigned int)seedval);
}

long int lrand48(void)
{
  /* rand() returns 0..RAND_MAX; lrand48 returns 0..2^31-1 */
  return (long int)rand();
}

/****************************************************************************
 * NuttX internal functions needed by mono
 ****************************************************************************/

/* __errno — NuttX user-space errno access.
 * Returns a pointer so Mono can both read and write errno as an lvalue
 * (*__errno() = val). In protected mode, NuttX's actual errno is only
 * accessible via get_errno()/set_errno() syscalls, not as a direct pointer.
 * We use a static variable here; Mono's libc wrappers will sync it
 * with NuttX's errno as needed.
 */
static int _mono_errno_val;
int *__errno(void)
{
  return &_mono_errno_val;
}

/* __assert — NuttX assert handler */
void __assert(const char *file, int line, const char *expr)
{
  syslog(LOG_EMERG, "ASSERT: %s:%d: %s\n", file, line, expr);
  for (;;); /* hang rather than crash unpredictably */
}

/* abort — intercept to log before dying */
void abort(void)
{
  syslog(LOG_EMERG, "ABORT called! LR=%p\n",
         __builtin_return_address(0));
  for (;;); /* hang so we can debug */
}

/* exit — intercept to log task exit */
void _exit(int status)
{
  syslog(LOG_EMERG, "EXIT(%d) called! LR=%p\n",
         status, __builtin_return_address(0));
  for (;;); /* hang so we can debug */
}

/* lib_get_stream — NuttX internal for FILE* by fd index.
 * libmonosgen references this. Return NULL for now. */
void *lib_get_stream(int fd)
{
  return NULL;
}

/* up_invalidate_icache — ARM I-cache invalidation for JIT.
 * In NuttX user-space, we may not have direct access to the kernel
 * cache API. Use ARM DSB/ISB instructions directly. */
void up_invalidate_icache(unsigned long start, unsigned long end)
{
  (void)start;
  (void)end;
  __asm__ __volatile__("dsb sy\n\tisb sy\n\t" ::: "memory");
}

/****************************************************************************
 * Standard C library stubs
 *
 * These are standard libc functions that NuttX may define as macros
 * in headers but not provide as linkable function symbols in user-space.
 ****************************************************************************/

#undef atoi
int atoi(const char *nptr)
{
  int result = 0;
  int sign = 1;

  while (*nptr == ' ' || *nptr == '\t') nptr++;
  if (*nptr == '-') { sign = -1; nptr++; }
  else if (*nptr == '+') { nptr++; }
  while (*nptr >= '0' && *nptr <= '9')
    {
      result = result * 10 + (*nptr - '0');
      nptr++;
    }

  return sign * result;
}

#undef isspace
int isspace(int c)
{
  return c == ' ' || c == '\t' || c == '\n' ||
         c == '\r' || c == '\f' || c == '\v';
}

#undef isxdigit
int isxdigit(int c)
{
  return (c >= '0' && c <= '9') ||
         (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

#undef toupper
int toupper(int c)
{
  return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

#undef localtime_r
extern struct tm *gmtime_r(const time_t *timer, struct tm *result);
struct tm *localtime_r(const time_t *timer, struct tm *result)
{
  return gmtime_r(timer, result);
}

/****************************************************************************
 * Dynamic loading stubs
 *
 * Mono calls dlopen(NULL) to get a handle to the main program (POSIX
 * standard behavior). NuttX's dlopen doesn't handle NULL — it dereferences
 * file[0] immediately. Return a sentinel handle for NULL.
 ****************************************************************************/

#include <dlfcn.h>

static int _dlopen_self_sentinel;

/* Override NuttX dlopen to handle NULL (self-reference) */
void *dlopen(const char *file, int mode)
{
  if (file == NULL)
    return &_dlopen_self_sentinel;

  /* For named libraries, return the sentinel too — NuttX doesn't
   * support loading shared objects at runtime. Mono will fall back
   * to its pinvoke_override mechanism.
   */
  return &_dlopen_self_sentinel;
}

void *dlsym(void *handle, const char *name)
{
  /* Symbol lookup not supported on NuttX — Mono uses pinvoke_override */
  (void)handle;
  (void)name;
  return NULL;
}

int dlclose(void *handle)
{
  (void)handle;
  return 0;
}

char *dlerror(void)
{
  return "dlopen/dlsym not supported on NuttX";
}

/****************************************************************************
 * POSIX file stubs
 ****************************************************************************/

int lstat(const char *path, void *buf)
{
  /* Fall back to stat — NuttX doesn't have symlinks */
  extern int stat(const char *, void *);
  return stat(path, buf);
}

long readlink(const char *path, char *buf, unsigned long bufsiz)
{
  /* NuttX doesn't support symlinks */
  return -1;
}

char *realpath(const char *path, char *resolved_path)
{
  /* Simplified: just copy the path */
  if (path == NULL) return NULL;
  if (resolved_path == NULL)
    {
      resolved_path = (char *)malloc(strlen(path) + 1);
      if (resolved_path == NULL) return NULL;
    }

  strcpy(resolved_path, path);
  return resolved_path;
}

char *mkdtemp(char *template)
{
  /* Simple stub — not fully POSIX compliant */
  (void)template;
  return NULL;
}

/* 64-bit atomic CAS — ARM Cortex-M7 doesn't have native 64-bit atomics,
 * provide a stub using critical section */
extern unsigned long long __sync_val_compare_and_swap_8(
  volatile void *ptr, unsigned long long oldval, unsigned long long newval)
{
  /* Simple non-atomic fallback — sufficient for single-core Cortex-M7 */
  unsigned long long *p = (unsigned long long *)ptr;
  unsigned long long prev = *p;
  if (prev == oldval)
    {
      *p = newval;
    }
  return prev;
}

/****************************************************************************
 * Pre-existing missing symbols (not related to mono upgrade)
 ****************************************************************************/

int pppd(int argc, char *argv[])
{
  return -1;
}

int ntpc_start(void)
{
  return -1;
}

/****************************************************************************
 * Math stubs
 *
 * NuttX math.h may define these as macros but not provide implementations.
 * Undef any macros first, then provide real function bodies.
 ****************************************************************************/

extern double pow(double, double);

#undef cbrt
double cbrt(double x)
{
  if (x == 0.0) return 0.0;
  return (x > 0.0) ? pow(x, 1.0/3.0) : -pow(-x, 1.0/3.0);
}

#undef fmax
double fmax(double x, double y)
{
  return (x > y) ? x : y;
}

#undef fmaxf
float fmaxf(float x, float y)
{
  return (x > y) ? x : y;
}

#undef fmin
double fmin(double x, double y)
{
  return (x < y) ? x : y;
}

#undef fminf
float fminf(float x, float y)
{
  return (x < y) ? x : y;
}
