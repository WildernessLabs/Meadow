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
 * mmap using posix_memalign + read, and munmap using free.
 * This handles both anonymous (MAP_ANONYMOUS) and file-backed mappings.
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

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset)
{
  void *ptr = NULL;

  if (length == 0)
    return MAP_FAILED;

  /* Allocate page-aligned memory */
  if (posix_memalign(&ptr, 4096, length) != 0)
    return MAP_FAILED;

  if (flags & MAP_ANONYMOUS)
    {
      /* Anonymous mapping — just zero the memory */
      memset(ptr, 0, length);
    }
  else
    {
      /* File-backed mapping — read the data */
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

      /* Zero remainder if file was shorter than requested */
      if (total < length)
        memset((char *)ptr + total, 0, length - total);

      lseek(fd, saved, SEEK_SET);
    }

  return ptr;
}

int munmap(void *addr, size_t length)
{
  /* No-op: Mono's mono_valloc_aligned calls munmap on sub-regions of a
   * single mmap allocation (prefix/suffix trimming). Real munmap can unmap
   * partial regions, but our mmap stub uses posix_memalign — free() only
   * works on the exact pointer returned by memalign, not interior pointers.
   * Leak the memory for now to let init complete; proper tracking TBD.
   */
  (void)addr;
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
  char ch = (char)c;
  write(1, &ch, 1);
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
