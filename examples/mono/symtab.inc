/* ../../apps/examples/mono/symtab.inc: Auto-generated symbol table.  Do not edit */

#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <nuttx/binfmt/symtab.h>

#include <unistd.h>
#include <sys/time.h>
#include <aio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/boardctl.h>
#include <sys/mman.h>
#include <time.h>
#include <dirent.h>
#include <nuttx/binfmt/binfmt.h>
#include <fcntl.h>
#include <nuttx/fs/fs.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <errno.h>
#include <sys/random.h>
#include <net/if.h>
#include <nuttx/module.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <nuttx/drivers/drivers.h>
#include <sys/mount.h>
#include <mqueue.h>
#include <nuttx/syslog/syslog.h>
#include <nuttx/arch.h>
#include <poll.h>
#include <sys/prctl.h>
#include <sys/select.h>
#include <spawn.h>
#include <pthread.h>
#include <stdio.h>
#include <sched.h>
#include <nuttx/sched.h>
#include <semaphore.h>
#include <nuttx/semaphore.h>
#include <sys/sendfile.h>
#include <sys/shm.h>
#include <termios.h>
#include <sys/utsname.h>
#include <assert.h>
#include <sys/wait.h>

const struct symtab_s g_symtab[] =
{
  { "_exit", (FAR const void *)_exit },
#if defined(CONFIG_CLOCK_TIMEKEEPING)
  { "adjtime", (FAR const void *)adjtime },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_cancel", (FAR const void *)aio_cancel },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_fsync", (FAR const void *)aio_fsync },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_read", (FAR const void *)aio_read },
#endif
#if defined(CONFIG_FS_AIO)
  { "aio_write", (FAR const void *)aio_write },
#endif
#if defined(CONFIG_NET)
  { "accept", (FAR const void *)accept },
#endif
#if defined(CONFIG_SCHED_ATEXIT)
  { "atexit", (FAR const void *)atexit },
#endif
#if defined(CONFIG_NET)
  { "bind", (FAR const void *)bind },
#endif
#if defined(CONFIG_LIB_BOARDCTL)
  { "boardctl", (FAR const void *)boardctl },
#endif
  { "cacheflush", (FAR const void *)cacheflush },
#if !defined(CONFIG_DISABLE_ENVIRON)
  { "clearenv", (FAR const void *)clearenv },
#endif
  { "clock", (FAR const void *)clock },
  { "clock_getres", (FAR const void *)clock_getres },
  { "clock_gettime", (FAR const void *)clock_gettime },
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "clock_nanosleep", (FAR const void *)clock_nanosleep },
#endif
  { "clock_settime", (FAR const void *)clock_settime },
  { "close", (FAR const void *)close },
  { "closedir", (FAR const void *)closedir },
#if defined(CONFIG_NET)
  { "connect", (FAR const void *)connect },
#endif
  { "dup", (FAR const void *)dup },
  { "dup2", (FAR const void *)dup2 },
#if !defined(CONFIG_BINFMT_DISABLE) && !defined(CONFIG_BUILD_KERNEL)
  { "exec", (FAR const void *)exec },
#endif
#if !defined(CONFIG_BINFMT_DISABLE) && defined(CONFIG_LIBC_EXECFUNCS)
  { "execv", (FAR const void *)execv },
#endif
  { "exit", (FAR const void *)exit },
  { "fcntl", (FAR const void *)fcntl },
#if CONFIG_NFILE_STREAMS > 0
  { "fs_fdopen", (FAR const void *)fs_fdopen },
#endif
#if defined(CONFIG_LIBC_IOCTL_VARIADIC)
  { "fs_ioctl", (FAR const void *)fs_ioctl },
#endif
  { "fstat", (FAR const void *)fstat },
  { "fstatfs", (FAR const void *)fstatfs },
#if !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "fsync", (FAR const void *)fsync },
#endif
#if !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "ftruncate", (FAR const void *)ftruncate },
#endif
#if !defined(__DIRECT_ERRNO_ACCESS)
  { "get_errno", (FAR const void *)get_errno },
#endif
#if defined(__DIRECT_ERRNO_ACCESS)
  { "get_errno_ptr", (FAR const void *)get_errno_ptr },
#endif
#if !defined(CONFIG_DISABLE_ENVIRON)
  { "getenv", (FAR const void *)getenv },
#endif
#if defined(CONFIG_NET)
  { "getpeername", (FAR const void *)getpeername },
#endif
  { "getpid", (FAR const void *)getpid },
#if defined(CONFIG_CRYPTO_RANDOM_POOL)
  { "getrandom", (FAR const void *)getrandom },
#endif
#if defined(CONFIG_NET)
  { "getsockname", (FAR const void *)getsockname },
#endif
#if defined(CONFIG_NET)
  { "getsockopt", (FAR const void *)getsockopt },
#endif
#if defined(CONFIG_NETDEV_IFINDEX)
  { "if_indextoname", (FAR const void *)if_indextoname },
#endif
#if defined(CONFIG_NETDEV_IFINDEX)
  { "if_nametoindex", (FAR const void *)if_nametoindex },
#endif
#if defined(CONFIG_MODULE)
  { "insmod", (FAR const void *)insmod },
#endif
#if !defined(CONFIG_LIBC_IOCTL_VARIADIC)
  { "ioctl", (FAR const void *)ioctl },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS)
  { "kill", (FAR const void *)kill },
#endif
#if defined(CONFIG_PSEUDOFS_SOFTLINKS)
  { "link", (FAR const void *)link },
#endif
#if defined(CONFIG_NET)
  { "listen", (FAR const void *)listen },
#endif
  { "lseek", (FAR const void *)lseek },
#if !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "mkdir", (FAR const void *)mkdir },
#endif
#if defined(CONFIG_PIPES) && CONFIG_DEV_FIFO_SIZE > 0
  { "mkfifo2", (FAR const void *)mkfifo2 },
#endif
  { "mmap", (FAR const void *)mmap },
  { "munmap", (FAR const void *)munmap },
#if defined(CONFIG_MODULE)
  { "modhandle", (FAR const void *)modhandle },
#endif
#if !defined(CONFIG_DISABLE_MOUNTPOINT) && defined(CONFIG_FS_READABLE)
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
  { "nx_vsyslog", (FAR const void *)nx_vsyslog },
#if defined(CONFIG_SCHED_ONEXIT)
  { "on_exit", (FAR const void *)on_exit },
#endif
  { "open", (FAR const void *)open },
  { "opendir", (FAR const void *)opendir },
#if defined(CONFIG_BUILD_KERNEL)
  { "pgalloc", (FAR const void *)pgalloc },
#endif
#if defined(CONFIG_PIPES) && CONFIG_DEV_PIPE_SIZE > 0
  { "pipe2", (FAR const void *)pipe2 },
#endif
#if !defined(CONFIG_DISABLE_POLL)
  { "poll", (FAR const void *)poll },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS) && !defined(CONFIG_DISABLE_POLL)
  { "ppoll", (FAR const void *)ppoll },
#endif
#if CONFIG_TASK_NAME_SIZE > 0
  { "prctl", (FAR const void *)prctl },
#endif
  { "pread", (FAR const void *)pread },
#if !defined(CONFIG_DISABLE_SIGNALS) && !defined(CONFIG_DISABLE_POLL)
  { "pselect", (FAR const void *)pselect },
#endif
  { "pwrite", (FAR const void *)pwrite },
#if !defined(CONFIG_BINFMT_DISABLE) && defined(CONFIG_LIBC_EXECFUNCS) && defined(CONFIG_LIB_ENVPATH)
  { "posix_spawnp", (FAR const void *)posix_spawnp },
#endif
#if !defined(CONFIG_BINFMT_DISABLE) && defined(CONFIG_LIBC_EXECFUNCS) && !defined(CONFIG_LIB_ENVPATH)
  { "posix_spawn", (FAR const void *)posix_spawn },
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
  { "pthread_cond_signal", (FAR const void *)pthread_cond_signal },
#endif
#if !defined(CONFIG_DISABLE_SIGNALS) && !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_cond_timedwait", (FAR const void *)pthread_cond_timedwait },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_cond_wait", (FAR const void *)pthread_cond_wait },
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
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_destroy", (FAR const void *)pthread_mutex_destroy },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_init", (FAR const void *)pthread_mutex_init },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_timedlock", (FAR const void *)pthread_mutex_timedlock },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_trylock", (FAR const void *)pthread_mutex_trylock },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD)
  { "pthread_mutex_unlock", (FAR const void *)pthread_mutex_unlock },
#endif
#if !defined(CONFIG_DISABLE_PTHREAD) && !defined(CONFIG_PTHREAD_MUTEX_UNSAFE)
  { "pthread_mutex_consistent", (FAR const void *)pthread_mutex_consistent },
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
#if !defined(CONFIG_DISABLE_ENVIRON)
  { "putenv", (FAR const void *)putenv },
#endif
  { "read", (FAR const void *)read },
  { "readdir", (FAR const void *)readdir },
#if defined(CONFIG_PSEUDOFS_SOFTLINKS)
  { "readlink", (FAR const void *)readlink },
#endif
#if defined(CONFIG_NET)
  { "recv", (FAR const void *)recv },
#endif
#if defined(CONFIG_NET)
  { "recvfrom", (FAR const void *)recvfrom },
#endif
#if !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "rename", (FAR const void *)rename },
#endif
  { "rewinddir", (FAR const void *)rewinddir },
#if !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "rmdir", (FAR const void *)rmdir },
#endif
#if defined(CONFIG_MODULE)
  { "rmmod", (FAR const void *)rmmod },
#endif
  { "sched_getparam", (FAR const void *)sched_getparam },
  { "sched_getscheduler", (FAR const void *)sched_getscheduler },
#if CONFIG_NFILE_STREAMS > 0
  { "sched_getstreams", (FAR const void *)sched_getstreams },
#endif
  { "sched_lock", (FAR const void *)sched_lock },
  { "sched_lockcount", (FAR const void *)sched_lockcount },
  { "sched_rr_get_interval", (FAR const void *)sched_rr_get_interval },
  { "sched_setparam", (FAR const void *)sched_setparam },
  { "sched_setscheduler", (FAR const void *)sched_setscheduler },
  { "sched_unlock", (FAR const void *)sched_unlock },
  { "sched_yield", (FAR const void *)sched_yield },
  { "seekdir", (FAR const void *)seekdir },
#if !defined(CONFIG_DISABLE_POLL)
  { "select", (FAR const void *)select },
#endif
#if defined(CONFIG_FS_NAMED_SEMAPHORES)
  { "sem_close", (FAR const void *)sem_close },
#endif
  { "sem_destroy", (FAR const void *)sem_destroy },
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
#if defined(CONFIG_NET)
  { "send", (FAR const void *)send },
#endif
#if defined(CONFIG_NET_SENDFILE)
  { "sendfile", (FAR const void *)sendfile },
#endif
#if defined(CONFIG_NET)
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
#if defined(CONFIG_NET)
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
#if defined(CONFIG_NET)
  { "socket", (FAR const void *)socket },
#endif
  { "stat", (FAR const void *)stat },
  { "statfs", (FAR const void *)statfs },
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
  { "telldir", (FAR const void *)telldir },
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
#if !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "umount2", (FAR const void *)umount2 },
#endif
  { "uname", (FAR const void *)uname },
#if !defined(CONFIG_DISABLE_MOUNTPOINT)
  { "unlink", (FAR const void *)unlink },
#endif
#if !defined(CONFIG_DISABLE_ENVIRON)
  { "unsetenv", (FAR const void *)unsetenv },
#endif
  { "up_assert", (FAR const void *)up_assert },
#if defined(CONFIG_ARCH_HAVE_VFORK)
  { "vfork", (FAR const void *)vfork },
#endif
#if defined(CONFIG_SCHED_WAITPID) && defined(CONFIG_SCHED_HAVE_PARENT)
  { "wait", (FAR const void *)wait },
#endif
#if defined(CONFIG_SCHED_WAITPID) && defined(CONFIG_SCHED_HAVE_PARENT)
  { "waitid", (FAR const void *)waitid },
#endif
#if defined(CONFIG_SCHED_WAITPID)
  { "waitpid", (FAR const void *)waitpid },
#endif
  { "write", (FAR const void *)write }
};

#define NSYMBOLS (sizeof(g_symtab) / sizeof (struct symtab_s))
