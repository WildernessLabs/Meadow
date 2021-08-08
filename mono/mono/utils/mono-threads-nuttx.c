/**
 * \file
 */

#include <config.h>

#if defined(__NuttX__)

#include <mono/utils/mono-threads.h>
#include <pthread.h>
#include <unistd.h>

void
mono_threads_platform_get_stack_bounds (guint8 **staddr, size_t *stsize)
{
	*staddr = (guint8*)pthread_get_stackaddr_np (pthread_self());
	*stsize = pthread_get_stacksize_np (pthread_self());

	/* staddr points to the start of the stack, not the end */
	*staddr -= *stsize;
}

guint64
mono_native_thread_os_id_get (void)
{
	// thrd_current() would probably be better semantically here,
	// but NuttX threads.h fails to be included. Its just a simple define
	// wrapper over getpid() anyway so just call it directly.
	return (guint64)getpid();
}

#endif
