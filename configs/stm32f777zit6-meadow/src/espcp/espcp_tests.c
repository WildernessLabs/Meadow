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

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "secrets.h"

#include "../meadow-upd.h"
#include <meadow/hcom_shared_common.h>
#include "espcp_usrsock.h"
#include "espcp_common.h"
#include "espcp_coprocessor.h"

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
static void espcp_test_output_memory_info(const struct mallinfo *before, const struct mallinfo *after, 
                                          const struct mallinfo *kbefore, const struct mallinfo *kafter, const char *title)
{
    struct mallinfo difference, kdifference;

    difference.arena = before->arena - after->arena;
    difference.uordblks = before->uordblks - after->uordblks;
    if (difference.uordblks < 0)
    {
        difference.uordblks = -difference.uordblks;
    }
    difference.fordblks = before->fordblks - after->fordblks;
    difference.mxordblk = before->mxordblk - after->mxordblk;

    kdifference.arena = kbefore->arena - kafter->arena;
    kdifference.uordblks = kbefore->uordblks - kafter->uordblks;
    if (kdifference.uordblks < 0)
    {
        kdifference.uordblks = -kdifference.uordblks;
    }
    kdifference.fordblks = kbefore->fordblks - kafter->fordblks;
    kdifference.mxordblk = kbefore->mxordblk - kafter->mxordblk;

    if (title != NULL)
    {
        syslog(1, "%s\n", title);
    }
    syslog(1, "                    %11s%11s%11s%11s\n", "Total", "Used", "Free", "Largest");
    syslog(1, "User Before:        %11d%11d%11d%11d\n", before->arena, before->uordblks, before->fordblks, before->mxordblk);
    syslog(1, "User After:         %11d%11d%11d%11d\n", after->arena, after->uordblks, after->fordblks, after->mxordblk);
    syslog(1, "User Difference:    %11d%11d%11d%11d\n", difference.arena, difference.uordblks, difference.fordblks, difference.mxordblk);
    syslog(1, "Kernel Before:      %11d%11d%11d%11d\n", kbefore->arena, kbefore->uordblks, kbefore->fordblks, kbefore->mxordblk);
    syslog(1, "Kernel After:       %11d%11d%11d%11d\n", kafter->arena, kafter->uordblks, kafter->fordblks, kafter->mxordblk);
    syslog(1, "Kernel Difference:  %11d%11d%11d%11d\n", kdifference.arena, kdifference.uordblks, kdifference.fordblks, kdifference.mxordblk);
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
static void espcp_test_get_mallinfo(struct mallinfo *mem, struct mallinfo *kmem)
{
#ifdef CONFIG_CAN_PASS_STRUCTS
  *mem = mallinfo();
  *kmem = kmm_mallinfo();
#else
  (void) mallinfo(mem);
  (void) kmm_mallinfo(kmem);
#endif
}

/****************************************************************************
 * Name: espcp_test_start_wifi
 *
 * Description:
 *  Connect to a WiFi access point.
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
static void espcp_test_start_wifi(void)
{
    espcp_wi_fi_credentials_t credentials;
    credentials.network_name = WIFI_NETWORK;
    credentials.password = WIFI_PASSWORD;

    struct upd_esp32_command message;
    memset(&message, 0, sizeof(struct upd_esp32_command));
    message.interface = espcp_esp32_interfaces_wi_fi;
    message.function = espcp_wi_fi_function_connect_to_access_point;
    message.payload_length = espcp_wi_fi_credentials_buffer_size(&credentials);
    message.payload = (uint8_t *) malloc(message.payload_length);
    espcp_encode_wi_fi_credentials(&credentials, message.payload);
    message.block = 1;

    //
    //  Using the UPD method as the .NET managed code passes messages
    //  through this route.
    //
    upd_handle_esp32_command(&message);
    //
    //  The returned message will have the result of the call in the payload
    //  so we need to release this memory.
    //
    free(message.payload);
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
void espcp_execute_tests(void)
{
    struct mallinfo start, end;
    struct mallinfo kstart, kend;

    syslog(LOG_CRIT, "Executing network tests.\n");

    bool waiting_for_esp32 = true;
    while (waiting_for_esp32)
    {
      espcp_config_lock();
      espcp_configuration_t *config = espcp_get_configuration();
      if (!config->esp_not_responding)
      {
        waiting_for_esp32 = false;
      }
      espcp_config_unlock();
      if (waiting_for_esp32)
      {
          usleep(500000);   // 500 ms
      }
    }


    espcp_test_get_mallinfo(&start, &kstart);

    espcp_test_start_wifi();

    espcp_test_get_mallinfo(&end, &kend);
    espcp_test_output_memory_info(&start, &end, &kstart, &kend, "Connecting to Access Point");

    syslog(LOG_CRIT, "Network tests completed.\n");
}