/* symtab.inc: Auto-generated symbol table.  Do not edit */

#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <nuttx/binfmt/symtab.h>

#include <debug.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <nuttx/syslog/syslog.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <aio.h>
#include <stdio.h>
#include <fixedmath.h>
#include <libgen.h>
#include <sys/boardctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <time.h>
#include <nuttx/clock.h>
#include <dirent.h>
#include <crc32.h>
#include <queue.h>
#include <netinet/ether.h>
#include <nuttx/binfmt/binfmt.h>
#include <fcntl.h>
#include <strings.h>
#include <nuttx/fs/fs.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <errno.h>
#include <sys/random.h>
#include <inttypes.h>
#include <nuttx/module.h>
#include <sys/ioctl.h>
#include <wctype.h>
#include <signal.h>
#include <nuttx/lib/regex.h>
#include <wchar.h>
#include <string.h>
#include <nuttx/drivers/drivers.h>
#include <sys/mount.h>
#include <mqueue.h>
#include <nuttx/arch.h>
#include <poll.h>
#include <spawn.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <sched.h>
#include <nuttx/sched.h>
#include <sys/select.h>
#include <semaphore.h>
#include <nuttx/semaphore.h>
#include <sys/sendfile.h>
#include <locale.h>
#include <syslog.h>
#include <sys/shm.h>
#include <sys/utsname.h>
#include <assert.h>
#include <sys/wait.h>

const struct symtab_s g_symtab[] =
{
#if !defined(CONFIG_CPP_HAVE_VARARGS) && defined(CONFIG_DEBUG_ERROR)
  { "_err", (FAR const void *)_err },
#endif
  { "_exit", (FAR const void *)_exit },
#if defined(CONFIG_NET_IPv4) && !defined(CONFIG_CAN_PASS_STRUCTS)
  { "_inet_ntoa", (FAR const void *)_inet_ntoa },
#endif
#if !defined(CONFIG_CPP_HAVE_VARARGS) && defined(CONFIG_DEBUG_INFO)
  { "_info", (FAR const void *)_info },
#endif
  { "_vsyslog", (FAR const void *)_vsyslog },
#if !defined(CONFIG_CPP_HAVE_VARARGS) && defined(CONFIG_DEBUG_WARN)
  { "_warn", (FAR const void *)_warn },
#endif
  { "abort", (FAR const void *)abort },
  { "abs", (FAR const void *)abs },
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "accept", (FAR const void *)accept },
#endif
#if defined(CONFIG_CLOCK_TIMEKEEPING)
  { "adjtime", (FAR const void *)adjtime },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_cancel", (FAR const void *)aio_cancel },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_error", (FAR const void *)aio_error },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_fsync", (FAR const void *)aio_fsync },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_read", (FAR const void *)aio_read },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_return", (FAR const void *)aio_return },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_suspend", (FAR const void *)aio_suspend },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_write", (FAR const void *)aio_write },
#endif
  { "asprintf", (FAR const void *)asprintf },
#if defined(CONFIG_SCHED_ATEXIT)
  { "atexit", (FAR const void *)atexit },
#endif
#if !defined(CONFIG_HAVE_LONG_LONG)
  { "b16atan2", (FAR const void *)b16atan2 },
#endif
  { "b16cos", (FAR const void *)b16cos },
#if !defined(CONFIG_HAVE_LONG_LONG)
  { "b16divb16", (FAR const void *)b16divb16 },
#endif
#if !defined(CONFIG_HAVE_LONG_LONG)
  { "b16mulb16", (FAR const void *)b16mulb16 },
#endif
  { "b16sin", (FAR const void *)b16sin },
#if !defined(CONFIG_HAVE_LONG_LONG)
  { "b16sqr", (FAR const void *)b16sqr },
#endif
  { "basename", (FAR const void *)basename },
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "bind", (FAR const void *)bind },
#endif
#if defined(CONFIG_LIB_BOARDCTL)
  { "boardctl", (FAR const void *)boardctl },
#endif
  { "cacheflush", (FAR const void *)cacheflush },
#if CONFIG_NFILE_DESCRIPTORS > 0 && defined(CONFIG_SERIAL_TERMIOS)
  { "cfgetspeed", (FAR const void *)cfgetspeed },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && defined(CONFIG_SERIAL_TERMIOS)
  { "cfsetspeed", (FAR const void *)cfsetspeed },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && !defined(CONFIG_DISABLE_ENVIRON)
  { "chdir", (FAR const void *)chdir },
#endif
#if !defined(CONFIG_DISABLE_ENVIRON)
  { "clearenv", (FAR const void *)clearenv },
#endif
  { "clock_getres", (FAR const void *)clock_getres },
  { "clock_gettime", (FAR const void *)clock_gettime },
  { "clock_settime", (FAR const void *)clock_settime },
#if !defined(__HAVE_KERNEL_GLOBALS)
  { "clock_systimer", (FAR const void *)clock_systimer },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0
  { "close", (FAR const void *)close },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "closedir", (FAR const void *)closedir },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "connect", (FAR const void *)connect },
#endif
  { "crc32", (FAR const void *)crc32 },
  { "crc32part", (FAR const void *)crc32part },
  { "dirname", (FAR const void *)dirname },
  { "dq_addafter", (FAR const void *)dq_addafter },
  { "dq_addbefore", (FAR const void *)dq_addbefore },
  { "dq_addfirst", (FAR const void *)dq_addfirst },
  { "dq_addlast", (FAR const void *)dq_addlast },
  { "dq_rem", (FAR const void *)dq_rem },
  { "dq_remfirst", (FAR const void *)dq_remfirst },
  { "dq_remlast", (FAR const void *)dq_remlast },
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "dup", (FAR const void *)dup },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "dup2", (FAR const void *)dup2 },
#endif
  { "ether_ntoa", (FAR const void *)ether_ntoa },
#if !defined(CONFIG_BINFMT_DISABLE) && !defined(CONFIG_BUILD_KERNEL)
  { "exec", (FAR const void *)exec },
#endif
#if !defined(CONFIG_BINFMT_DISABLE) && defined(CONFIG_LIBC_EXECFUNCS)
  { "execv", (FAR const void *)execv },
#endif
  { "exit", (FAR const void *)exit },
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fclose", (FAR const void *)fclose },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "fcntl", (FAR const void *)fcntl },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fdopen", (FAR const void *)fdopen },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fflush", (FAR const void *)fflush },
#endif
  { "ffs", (FAR const void *)ffs },
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fgetc", (FAR const void *)fgetc },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fgetpos", (FAR const void *)fgetpos },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fgets", (FAR const void *)fgets },
#endif
  { "fileno", (FAR const void *)fileno },
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fopen", (FAR const void *)fopen },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fprintf", (FAR const void *)fprintf },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fputc", (FAR const void *)fputc },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fputs", (FAR const void *)fputs },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fread", (FAR const void *)fread },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fs_fdopen", (FAR const void *)fs_fdopen },
#endif
#if defined(CONFIG_LIBC_IOCTL_VARIADIC) && (CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0)
  { "fs_ioctl", (FAR const void *)fs_ioctl },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fseek", (FAR const void *)fseek },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fsetpos", (FAR const void *)fsetpos },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "fstat", (FAR const void *)fstat },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "fstatfs", (FAR const void *)fstatfs },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "fsync", (FAR const void *)fsync },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "ftell", (FAR const void *)ftell },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "fwrite", (FAR const void *)fwrite },
#endif
#if !defined(__DIRECT_ERRNO_ACCESS)
  { "get_errno", (FAR const void *)get_errno },
#endif
#if defined(__DIRECT_ERRNO_ACCESS)
  { "get_errno_ptr", (FAR const void *)get_errno_ptr },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && !defined(CONFIG_DISABLE_ENVIRON)
  { "getcwd", (FAR const void *)getcwd },
#endif
#if !defined(CONFIG_DISABLE_ENVIRON)
  { "getenv", (FAR const void *)getenv },
#endif
#if defined(CONFIG_LIBC_NETDB)
  { "gethostname", (FAR const void *)gethostname },
#endif
  { "getopt", (FAR const void *)getopt },
  { "getoptargp", (FAR const void *)getoptargp },
  { "getoptindp", (FAR const void *)getoptindp },
  { "getoptoptp", (FAR const void *)getoptoptp },
  { "getpid", (FAR const void *)getpid },
#if defined(CONFIG_CRYPTO_RANDOM_POOL)
  { "getrandom", (FAR const void *)getrandom },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "gets", (FAR const void *)gets },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "getsockopt", (FAR const void *)getsockopt },
#endif
  { "gmtime", (FAR const void *)gmtime },
  { "gmtime_r", (FAR const void *)gmtime_r },
  { "htonl", (FAR const void *)htonl },
  { "htons", (FAR const void *)htons },
  { "imaxabs", (FAR const void *)imaxabs },
  { "inet_addr", (FAR const void *)inet_addr },
#if defined(CONFIG_NET_IPv4) && defined(CONFIG_CAN_PASS_STRUCTS)
  { "inet_ntoa", (FAR const void *)inet_ntoa },
#endif
  { "inet_ntop", (FAR const void *)inet_ntop },
  { "inet_pton", (FAR const void *)inet_pton },
#if defined(CONFIG_MODULE)
  { "insmod", (FAR const void *)insmod },
#endif
#if !defined(CONFIG_LIBC_IOCTL_VARIADIC) && (CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0)
  { "ioctl", (FAR const void *)ioctl },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswalnum", (FAR const void *)iswalnum },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswalpha", (FAR const void *)iswalpha },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswblank", (FAR const void *)iswblank },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswcntrl", (FAR const void *)iswcntrl },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswctype", (FAR const void *)iswctype },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswdigit", (FAR const void *)iswdigit },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswgraph", (FAR const void *)iswgraph },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswlower", (FAR const void *)iswlower },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswprint", (FAR const void *)iswprint },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswpunct", (FAR const void *)iswpunct },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswspace", (FAR const void *)iswspace },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswupper", (FAR const void *)iswupper },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "iswxdigit", (FAR const void *)iswxdigit },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "kill", (FAR const void *)kill },
#endif
  { "labs", (FAR const void *)labs },
  { "lib_dumpbuffer", (FAR const void *)lib_dumpbuffer },
#if defined(CONFIG_PSEUDOFS_SOFTLINKS)
  { "link", (FAR const void *)link },
#endif
#if defined(CONFIG_FS_AIO)
  { "lio_listio", (FAR const void *)lio_listio },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "listen", (FAR const void *)listen },
#endif
#if defined(CONFIG_HAVE_LONG_LONG)
  { "llabs", (FAR const void *)llabs },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "lseek", (FAR const void *)lseek },
#endif
  { "match", (FAR const void *)match },
#if defined(CONFIG_LIBC_WCHAR)
  { "mbrtowc", (FAR const void *)mbrtowc },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "mbsnrtowcs", (FAR const void *)mbsnrtowcs },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "mbtowc", (FAR const void *)mbtowc },
#endif
  { "memccpy", (FAR const void *)memccpy },
  { "memchr", (FAR const void *)memchr },
  { "memcmp", (FAR const void *)memcmp },
  { "memcpy", (FAR const void *)memcpy },
  { "memmove", (FAR const void *)memmove },
  { "memset", (FAR const void *)memset },
#if CONFIG_NFILE_DESCRIPTORS > 0 && !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "mkdir", (FAR const void *)mkdir },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "mkfifo", (FAR const void *)mkfifo },
#endif
#if defined(CONFIG_PIPES) && CONFIG_DEV_FIFO_SIZE > 0
  { "mkfifo2", (FAR const void *)mkfifo2 },
#endif
  { "mktime", (FAR const void *)mktime },
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "mmap", (FAR const void *)mmap },
#endif
#if defined(CONFIG_MODULE)
  { "modhandle", (FAR const void *)modhandle },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && !defined(CONFIG_DISABLE_MOUNTPOINT) && defined(CONFIG_FS_READABLE)
  { "mount", (FAR const void *)mount },
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_close", (FAR const void *)mq_close },
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_getattr", (FAR const void *)mq_getattr },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS) && !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_notify", (FAR const void *)mq_notify },
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_open", (FAR const void *)mq_open },
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_receive", (FAR const void *)mq_receive },
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_send", (FAR const void *)mq_send },
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_setattr", (FAR const void *)mq_setattr },
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_timedreceive", (FAR const void *)mq_timedreceive },
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_timedsend", (FAR const void *)mq_timedsend },
#endif
#if !defined(CONFIG_DISABLE_MQUEUE)
  { "mq_unlink", (FAR const void *)mq_unlink },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "munmap", (FAR const void *)munmap },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "nanosleep", (FAR const void *)nanosleep },
#endif
  { "ntohl", (FAR const void *)ntohl },
  { "ntohs", (FAR const void *)ntohs },
#if defined(CONFIG_SCHED_ONEXIT)
  { "on_exit", (FAR const void *)on_exit },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "open", (FAR const void *)open },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "opendir", (FAR const void *)opendir },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "perror", (FAR const void *)perror },
#endif
#if defined(CONFIG_BUILD_KERNEL)
  { "pgalloc", (FAR const void *)pgalloc },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "pipe", (FAR const void *)pipe },
#endif
#if defined(CONFIG_PIPES) && CONFIG_DEV_PIPE_SIZE > 0
  { "pipe2", (FAR const void *)pipe2 },
#endif
#if !defined(CONFIG_DISABLE_POLL) && (CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0)
  { "poll", (FAR const void *)poll },
#endif
#if !defined(CONFIG_BINFMT_DISABLE) && defined(CONFIG_LIBC_EXECFUNCS) && !defined(CONFIG_BINFMT_EXEPATH)
  { "posix_spawn", (FAR const void *)posix_spawn },
#endif
#if !defined(CONFIG_BINFMT_DISABLE) && defined(CONFIG_LIBC_EXECFUNCS) && defined(CONFIG_BINFMT_EXEPATH)
  { "posix_spawnp", (FAR const void *)posix_spawnp },
#endif
#if CONFIG_TASK_NAME_SIZE > 0
  { "prctl", (FAR const void *)prctl },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0
  { "pread", (FAR const void *)pread },
#endif
  { "printf", (FAR const void *)printf },
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_destroy", (FAR const void *)pthread_attr_destroy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_getinheritsched", (FAR const void *)pthread_attr_getinheritsched },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_getschedparam", (FAR const void *)pthread_attr_getschedparam },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_getschedpolicy", (FAR const void *)pthread_attr_getschedpolicy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_getstacksize", (FAR const void *)pthread_attr_getstacksize },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_init", (FAR const void *)pthread_attr_init },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_setinheritsched", (FAR const void *)pthread_attr_setinheritsched },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_setschedparam", (FAR const void *)pthread_attr_setschedparam },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_setschedpolicy", (FAR const void *)pthread_attr_setschedpolicy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_attr_setstacksize", (FAR const void *)pthread_attr_setstacksize },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_barrier_destroy", (FAR const void *)pthread_barrier_destroy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_barrier_init", (FAR const void *)pthread_barrier_init },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_barrier_wait", (FAR const void *)pthread_barrier_wait },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_barrierattr_destroy", (FAR const void *)pthread_barrierattr_destroy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_barrierattr_getpshared", (FAR const void *)pthread_barrierattr_getpshared },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_barrierattr_init", (FAR const void *)pthread_barrierattr_init },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_barrierattr_setpshared", (FAR const void *)pthread_barrierattr_setpshared },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_cancel", (FAR const void *)pthread_cancel },
#endif
#if defined(CONFIG_PTHREAD_CLEANUP)
  { "pthread_cleanup_pop", (FAR const void *)pthread_cleanup_pop },
#endif
#if defined(CONFIG_PTHREAD_CLEANUP)
  { "pthread_cleanup_push", (FAR const void *)pthread_cleanup_push },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_cond_broadcast", (FAR const void *)pthread_cond_broadcast },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_cond_destroy", (FAR const void *)pthread_cond_destroy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_cond_init", (FAR const void *)pthread_cond_init },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_cond_signal", (FAR const void *)pthread_cond_signal },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS) && !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_cond_timedwait", (FAR const void *)pthread_cond_timedwait },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_cond_wait", (FAR const void *)pthread_cond_wait },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_condattr_destroy", (FAR const void *)pthread_condattr_destroy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_condattr_init", (FAR const void *)pthread_condattr_init },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_create", (FAR const void *)pthread_create },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_detach", (FAR const void *)pthread_detach },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_exit", (FAR const void *)pthread_exit },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_get_stackaddr_np", (FAR const void *)pthread_get_stackaddr_np },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_get_stacksize_np", (FAR const void *)pthread_get_stacksize_np },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD) && defined(CONFIG_SMP)
  { "pthread_getaffinity_np", (FAR const void *)pthread_getaffinity_np },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_getschedparam", (FAR const void *)pthread_getschedparam },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_getspecific", (FAR const void *)pthread_getspecific },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_join", (FAR const void *)pthread_join },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_key_create", (FAR const void *)pthread_key_create },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_key_delete", (FAR const void *)pthread_key_delete },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS) && !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_kill", (FAR const void *)pthread_kill },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD) && !defined(CONFIG_PTHREAD_MUTEX_UNSAFE)
  { "pthread_mutex_consistent", (FAR const void *)pthread_mutex_consistent },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_destroy", (FAR const void *)pthread_mutex_destroy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_init", (FAR const void *)pthread_mutex_init },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_lock", (FAR const void *)pthread_mutex_lock },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_trylock", (FAR const void *)pthread_mutex_trylock },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_unlock", (FAR const void *)pthread_mutex_unlock },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutexattr_destroy", (FAR const void *)pthread_mutexattr_destroy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutexattr_getpshared", (FAR const void *)pthread_mutexattr_getpshared },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD) && defined(CONFIG_PTHREAD_MUTEX_TYPES)
  { "pthread_mutexattr_gettype", (FAR const void *)pthread_mutexattr_gettype },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutexattr_init", (FAR const void *)pthread_mutexattr_init },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutexattr_setpshared", (FAR const void *)pthread_mutexattr_setpshared },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD) && defined(CONFIG_PTHREAD_MUTEX_TYPES)
  { "pthread_mutexattr_settype", (FAR const void *)pthread_mutexattr_settype },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_once", (FAR const void *)pthread_once },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD) && defined(CONFIG_SMP)
  { "pthread_setaffinity_np", (FAR const void *)pthread_setaffinity_np },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_setschedparam", (FAR const void *)pthread_setschedparam },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_setschedprio", (FAR const void *)pthread_setschedprio },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_setspecific", (FAR const void *)pthread_setspecific },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS) && !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_sigmask", (FAR const void *)pthread_sigmask },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_yield", (FAR const void *)pthread_yield },
#endif
#if !defined(CONFIG_DISABLE_ENVIRON)
  { "putenv", (FAR const void *)putenv },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "puts", (FAR const void *)puts },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0
  { "pwrite", (FAR const void *)pwrite },
#endif
  { "qsort", (FAR const void *)qsort },
  { "rand", (FAR const void *)rand },
#if CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0
  { "read", (FAR const void *)read },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "readdir", (FAR const void *)readdir },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "readdir_r", (FAR const void *)readdir_r },
#endif
#if defined(CONFIG_PSEUDOFS_SOFTLINKS)
  { "readlink", (FAR const void *)readlink },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "recv", (FAR const void *)recv },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "recvfrom", (FAR const void *)recvfrom },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "rename", (FAR const void *)rename },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "rewinddir", (FAR const void *)rewinddir },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "rmdir", (FAR const void *)rmdir },
#endif
#if defined(CONFIG_MODULE)
  { "rmmod", (FAR const void *)rmmod },
#endif
  { "sched_get_priority_max", (FAR const void *)sched_get_priority_max },
  { "sched_get_priority_min", (FAR const void *)sched_get_priority_min },
  { "sched_getparam", (FAR const void *)sched_getparam },
  { "sched_getscheduler", (FAR const void *)sched_getscheduler },
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "sched_getstreams", (FAR const void *)sched_getstreams },
#endif
  { "sched_lock", (FAR const void *)sched_lock },
  { "sched_lockcount", (FAR const void *)sched_lockcount },
  { "sched_rr_get_interval", (FAR const void *)sched_rr_get_interval },
  { "sched_setparam", (FAR const void *)sched_setparam },
  { "sched_setscheduler", (FAR const void *)sched_setscheduler },
  { "sched_unlock", (FAR const void *)sched_unlock },
  { "sched_yield", (FAR const void *)sched_yield },
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "seekdir", (FAR const void *)seekdir },
#endif
#if !defined(CONFIG_DISABLE_POLL) && (CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0)
  { "select", (FAR const void *)select },
#endif
#if defined(CONFIG_FS_NAMED_SEMAPHORES)
  { "sem_close", (FAR const void *)sem_close },
#endif
  { "sem_destroy", (FAR const void *)sem_destroy },
  { "sem_getvalue", (FAR const void *)sem_getvalue },
  { "sem_init", (FAR const void *)sem_init },
#if defined(CONFIG_FS_NAMED_SEMAPHORES)
  { "sem_open", (FAR const void *)sem_open },
#endif
  { "sem_post", (FAR const void *)sem_post },
#if defined(CONFIG_PRIORITY_INHERITANCE)
  { "sem_setprotocol", (FAR const void *)sem_setprotocol },
#endif
  { "sem_timedwait", (FAR const void *)sem_timedwait },
  { "sem_trywait", (FAR const void *)sem_trywait },
#if defined(CONFIG_FS_NAMED_SEMAPHORES)
  { "sem_unlink", (FAR const void *)sem_unlink },
#endif
  { "sem_wait", (FAR const void *)sem_wait },
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "send", (FAR const void *)send },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && defined(CONFIG_NET_SENDFILE)
  { "sendfile", (FAR const void *)sendfile },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0
  { "sendfile", (FAR const void *)sendfile },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "sendto", (FAR const void *)sendto },
#endif
#if !defined(__DIRECT_ERRNO_ACCESS)
  { "set_errno", (FAR const void *)set_errno },
#endif
#if !defined(CONFIG_DISABLE_ENVIRON)
  { "setenv", (FAR const void *)setenv },
#endif
#if defined(CONFIG_LIBC_NETDB)
  { "sethostname", (FAR const void *)sethostname },
#endif
  { "setlocale", (FAR const void *)setlocale },
  { "setlogmask", (FAR const void *)setlogmask },
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "setsockopt", (FAR const void *)setsockopt },
#endif
#if defined(CONFIG_MM_SHM)
  { "shmat", (FAR const void *)shmat },
#endif
#if defined(CONFIG_MM_SHM)
  { "shmctl", (FAR const void *)shmctl },
#endif
#if defined(CONFIG_MM_SHM)
  { "shmdt", (FAR const void *)shmdt },
#endif
#if defined(CONFIG_MM_SHM)
  { "shmget", (FAR const void *)shmget },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigaction", (FAR const void *)sigaction },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigaddset", (FAR const void *)sigaddset },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigdelset", (FAR const void *)sigdelset },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigemptyset", (FAR const void *)sigemptyset },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigfillset", (FAR const void *)sigfillset },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigismember", (FAR const void *)sigismember },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigpending", (FAR const void *)sigpending },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigprocmask", (FAR const void *)sigprocmask },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigqueue", (FAR const void *)sigqueue },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigsuspend", (FAR const void *)sigsuspend },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigtimedwait", (FAR const void *)sigtimedwait },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sigwaitinfo", (FAR const void *)sigwaitinfo },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "sleep", (FAR const void *)sleep },
#endif
  { "snprintf", (FAR const void *)snprintf },
#if CONFIG_NSOCKET_DESCRIPTORS > 0 && defined(CONFIG_NET)
  { "socket", (FAR const void *)socket },
#endif
  { "sprintf", (FAR const void *)sprintf },
  { "sq_addafter", (FAR const void *)sq_addafter },
  { "sq_addfirst", (FAR const void *)sq_addfirst },
  { "sq_addlast", (FAR const void *)sq_addlast },
  { "sq_rem", (FAR const void *)sq_rem },
  { "sq_remafter", (FAR const void *)sq_remafter },
  { "sq_remfirst", (FAR const void *)sq_remfirst },
  { "sq_remlast", (FAR const void *)sq_remlast },
  { "srand", (FAR const void *)srand },
  { "sscanf", (FAR const void *)sscanf },
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "stat", (FAR const void *)stat },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "statfs", (FAR const void *)statfs },
#endif
  { "strcasecmp", (FAR const void *)strcasecmp },
  { "strcasestr", (FAR const void *)strcasestr },
  { "strcat", (FAR const void *)strcat },
  { "strchr", (FAR const void *)strchr },
  { "strcmp", (FAR const void *)strcmp },
#if CONFIG_LIBC_LOCALE
  { "strcoll", (FAR const void *)strcoll },
#endif
  { "strcpy", (FAR const void *)strcpy },
  { "strcspn", (FAR const void *)strcspn },
  { "strdup", (FAR const void *)strdup },
  { "strerror", (FAR const void *)strerror },
  { "strerror_r", (FAR const void *)strerror_r },
  { "strftime", (FAR const void *)strftime },
  { "strlen", (FAR const void *)strlen },
  { "strncasecmp", (FAR const void *)strncasecmp },
  { "strncat", (FAR const void *)strncat },
  { "strncmp", (FAR const void *)strncmp },
  { "strncpy", (FAR const void *)strncpy },
  { "strndup", (FAR const void *)strndup },
  { "strnlen", (FAR const void *)strnlen },
  { "strpbrk", (FAR const void *)strpbrk },
  { "strrchr", (FAR const void *)strrchr },
  { "strspn", (FAR const void *)strspn },
  { "strstr", (FAR const void *)strstr },
  { "strtod", (FAR const void *)strtod },
  { "strtok", (FAR const void *)strtok },
  { "strtok_r", (FAR const void *)strtok_r },
  { "strtol", (FAR const void *)strtol },
#if defined(CONFIG_HAVE_LONG_LONG)
  { "strtoll", (FAR const void *)strtoll },
#endif
  { "strtoul", (FAR const void *)strtoul },
#if defined(CONFIG_HAVE_LONG_LONG)
  { "strtoull", (FAR const void *)strtoull },
#endif
#if defined(CONFIG_LIBC_LOCALE)
  { "strxfrm", (FAR const void *)strxfrm },
#endif
  { "swab", (FAR const void *)swab },
#if defined(CONFIG_LIBC_WCHAR)
  { "swprintf", (FAR const void *)swprintf },
#endif
  { "syslog", (FAR const void *)syslog },
#if !defined(CONFIG_BUILD_KERNEL)
  { "task_create", (FAR const void *)task_create },
#endif
  { "task_delete", (FAR const void *)task_delete },
  { "task_restart", (FAR const void *)task_restart },
  { "task_setcancelstate", (FAR const void *)task_setcancelstate },
#if defined(CONFIG_CANCELLATION_POINTS)
  { "task_setcanceltype", (FAR const void *)task_setcanceltype },
#endif
#if defined(CONFIG_CANCELLATION_POINTS)
  { "task_testcancel", (FAR const void *)task_testcancel },
#endif
#if defined(CONFIG_SERIAL_TERMIOS)
  { "tcdrain", (FAR const void *)tcdrain },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && defined(CONFIG_SERIAL_TERMIOS)
  { "tcflush", (FAR const void *)tcflush },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && defined(CONFIG_SERIAL_TERMIOS)
  { "tcgetattr", (FAR const void *)tcgetattr },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && defined(CONFIG_SERIAL_TERMIOS)
  { "tcsetattr", (FAR const void *)tcsetattr },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "telldir", (FAR const void *)telldir },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0
  { "telldir", (FAR const void *)telldir },
#endif
  { "time", (FAR const void *)time },
#if !defined(CONFIG_DISABLE_POSIX_TIMERS)
  { "timer_create", (FAR const void *)timer_create },
#endif
#if !defined(CONFIG_DISABLE_POSIX_TIMERS)
  { "timer_delete", (FAR const void *)timer_delete },
#endif
#if !defined(CONFIG_DISABLE_POSIX_TIMERS)
  { "timer_getoverrun", (FAR const void *)timer_getoverrun },
#endif
#if !defined(CONFIG_DISABLE_POSIX_TIMERS)
  { "timer_gettime", (FAR const void *)timer_gettime },
#endif
#if !defined(CONFIG_DISABLE_POSIX_TIMERS)
  { "timer_settime", (FAR const void *)timer_settime },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "towlower", (FAR const void *)towlower },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "towupper", (FAR const void *)towupper },
#endif
#if !defined(CONFIG_HAVE_LONG_LONG)
  { "ub16divub16", (FAR const void *)ub16divub16 },
#endif
#if !defined(CONFIG_HAVE_LONG_LONG)
  { "ub16mulub16", (FAR const void *)ub16mulub16 },
#endif
#if !defined(CONFIG_HAVE_LONG_LONG)
  { "ub16sqr", (FAR const void *)ub16sqr },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "umount2", (FAR const void *)umount2 },
#endif
  { "uname", (FAR const void *)uname },
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "ungetc", (FAR const void *)ungetc },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "unlink", (FAR const void *)unlink },
#endif
#if !defined(CONFIG_DISABLE_ENVIRON)
  { "unsetenv", (FAR const void *)unsetenv },
#endif
  { "up_assert", (FAR const void *)up_assert },
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "usleep", (FAR const void *)usleep },
#endif
  { "vasprintf", (FAR const void *)vasprintf },
#if defined(CONFIG_ARCH_HAVE_VFORK)
  { "vfork", (FAR const void *)vfork },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "vfprintf", (FAR const void *)vfprintf },
#endif
#if CONFIG_NFILE_DESCRIPTORS > 0 && CONFIG_NFILE_STREAMS > 0
  { "vprintf", (FAR const void *)vprintf },
#endif
  { "vsnprintf", (FAR const void *)vsnprintf },
  { "vsprintf", (FAR const void *)vsprintf },
  { "vsscanf", (FAR const void *)vsscanf },
  { "vsyslog", (FAR const void *)vsyslog },
#if defined(CONFIG_SCHED_WAITPID) && defined(CONFIG_SCHED_HAVE_PARENT)
  { "wait", (FAR const void *)wait },
#endif
#if defined(CONFIG_SCHED_WAITPID) && defined(CONFIG_SCHED_HAVE_PARENT)
  { "waitid", (FAR const void *)waitid },
#endif
#if defined(CONFIG_SCHED_WAITPID)
  { "waitpid", (FAR const void *)waitpid },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcrtomb", (FAR const void *)wcrtomb },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcscmp", (FAR const void *)wcscmp },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcscoll", (FAR const void *)wcscoll },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcsftime", (FAR const void *)wcsftime },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcslcpy", (FAR const void *)wcslcpy },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcslen", (FAR const void *)wcslen },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcsnrtombs", (FAR const void *)wcsnrtombs },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcstod", (FAR const void *)wcstod },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcstof", (FAR const void *)wcstof },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcstol", (FAR const void *)wcstol },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcstold", (FAR const void *)wcstold },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcstoll", (FAR const void *)wcstoll },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcstoul", (FAR const void *)wcstoul },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wcsxfrm", (FAR const void *)wcsxfrm },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wctob", (FAR const void *)wctob },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wctomb", (FAR const void *)wctomb },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wctype", (FAR const void *)wctype },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wmemchr", (FAR const void *)wmemchr },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wmemcmp", (FAR const void *)wmemcmp },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wmemcpy", (FAR const void *)wmemcpy },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wmemmove", (FAR const void *)wmemmove },
#endif
#if defined(CONFIG_LIBC_WCHAR)
  { "wmemset", (FAR const void *)wmemset },
#endif
#if CONFIG_NSOCKET_DESCRIPTORS > 0 || CONFIG_NFILE_DESCRIPTORS > 0
  { "write", (FAR const void *)write }
#endif
};

#define NSYMBOLS (sizeof(g_symtab) / sizeof (struct symtab_s))
