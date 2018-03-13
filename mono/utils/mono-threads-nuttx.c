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
	g_assert_not_reached();
}

#endif
