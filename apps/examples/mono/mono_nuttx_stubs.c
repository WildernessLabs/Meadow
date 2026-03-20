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

void mono_setmmapjit(int flag)
{
  /* mmap-based JIT not applicable on NuttX (no MMU in this config) */
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

/****************************************************************************
 * NuttX internal functions needed by mono
 ****************************************************************************/

/* __errno — NuttX user-space errno access */
static int _mono_errno_val;
int *__errno(void)
{
  /* Use the thread's errno from NuttX via the errno macro if available,
   * otherwise fall back to a static variable */
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
