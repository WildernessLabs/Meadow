/**
 * \file
 */

#include <config.h>

#if defined(__NuttX__)

#include <mono/utils/mono-threads.h>
#include <pthread.h>

void
mono_threads_platform_get_stack_bounds (guint8 **staddr, size_t *stsize)
{
	*staddr = (guint8*)pthread_get_stackaddr_np (pthread_self());
	*stsize = pthread_get_stacksize_np (pthread_self());

	printf ("staddr: %p\n", *staddr);
	printf ("stsize: %d\n", *stsize);

	/* staddr points to the start of the stack, not the end */
	*staddr -= *stsize;
}

#endif
