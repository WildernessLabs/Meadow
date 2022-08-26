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
#include <nuttx/mm/mm.h>
#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>	//hostent
#include <arpa/inet.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/****************************************************************************
 * Local defines.
 ****************************************************************************/

//
//  Macros to help with the task of getting memory snapshots.
//
#define ALLOCATE_HEAP_STRUCTURES      struct mallinfo start, end, kstart, kend;
#define GET_INITIAL_HEAP_INFORMATION  espcp_test_get_mallinfo(&start, &kstart);
#define GET_FINAL_HEAP_INFORMATION    espcp_test_get_mallinfo(&end, &kend);
#define COPY_FINAL_TO_START           memcpy(&start, &end , sizeof(struct mallinfo)); memcpy(&kstart, &kend, sizeof(struct mallinfo));
#define HEAP_USAGE_PASS_OR_FAIL       espcp_test_check_heap_usage(&start, &end, &kstart, &kend, __func__);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void espcp_test_check_heap_usage(const struct mallinfo *before, const struct mallinfo *after, 
                                 const struct mallinfo *kbefore, const struct mallinfo *kafter, const char *test_name);
void espcp_test_get_mallinfo(struct mallinfo *mem, struct mallinfo *kmem);
void espcp_test_output_memory_info(const struct mallinfo *before, const struct mallinfo *after, 
                                   const struct mallinfo *kbefore, const struct mallinfo *kafter, const char *test_name);
