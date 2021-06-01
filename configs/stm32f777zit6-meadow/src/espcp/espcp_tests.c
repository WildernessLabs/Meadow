/****************************************************************************
 * espcp_tests.c
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <debug.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// #include "secrets.h"

#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: output_memory_info
 *
 * Description:
 *  Execute any network tests.
 *
 * Input Parameters:
 *   mem - Pointer to structure holding memory information.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void output_memory_info(const struct mallinfo *mem, const char *title, uint32_t header)
{
    if (header > 0)
    {
        syslog(LOG_INFO, "             %11s%11s%11s%11s\n", "Total", "Used", "Free", "Largest");
    }
    syslog(LOG_INFO, "%-12s %11d%11d%11d%11d\n", title, mem->arena, mem->uordblks, mem->fordblks, mem->mxordblk);
}

/****************************************************************************
 * Name: get_mallinfo
 *
 * Description:
 *  Get memory information.
 *
 * Input Parameters:
 *   mem - Pointer to structure in which to put the memory information.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void get_mallinfo(struct mallinfo *mem)
{
#ifdef CONFIG_CAN_PASS_STRUCTS
  *mem = mallinfo();
#else
  (void)mallinfo(mem);
#endif
}


/****************************************************************************
 * Name: espcp_execute_network_tests
 *
 * Description:
 *  Execute any network tests.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
void espcp_execute_network_tests(void)
{
    struct mallinfo start, end, difference;

    syslog(LOG_INFO, "Executing network tests.\n");
    get_mallinfo(&start);

    get_mallinfo(&end);
    difference.arena = start.arena - end.arena;
    difference.uordblks = start.uordblks - end.uordblks;
    difference.fordblks = start.fordblks - end.fordblks;
    difference.mxordblk = start.mxordblk - end.mxordblk;
    output_memory_info(&start, "Start", 1);
    output_memory_info(&end, "End", 0);
    output_memory_info(&difference, "Difference", 0);

    syslog(LOG_INFO, "Network tests completed.\n");
}