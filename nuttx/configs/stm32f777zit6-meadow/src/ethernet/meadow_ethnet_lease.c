/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/ethernet/meadow_ethnet_lease.c
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

// This module is common to all related ethernet modules

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <ctype.h>
#include <stdint.h>

#include "meadow_ethnet_local.h"
#include <meadow/meadow_ethnet_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Function Implementations
 ****************************************************************************/
static int meadow_eth_lease_wait_till_time_to_renew(uint32_t timeoutSec)
{
  int ret;

  sigset_t waitset;
  struct timespec timeout;

  timeout.tv_sec = timeoutSec;    // Number of seconds to wait
  timeout.tv_nsec = 0;            // Number of nanoseconds to wait

  sigemptyset(&waitset);
  sigaddset(&waitset, SIGALRM);

  // Only wakeup on timeout
  ret = sigtimedwait(&waitset, NULL, &timeout);
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int meadow_eth_renew_lease_loop(struct dhcp_info_s *dhcp_info)
{
  int ret;
  uint8_t macAddr[IFHWADDRLEN];

  do
  {
    // Wait for the appropriate amount of time to renew the lease
    ret = meadow_eth_lease_wait_till_time_to_renew(dhcp_info->lease_time/2);

    // Need MAC and it won't change
    ret = meadow_eth_utils_get_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);  
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_mac err:0x%08x, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }

    // Renew the lease
    // Note: Everything in dhcp_info may change including our IP address and
    // lease timeout.
    ret = meadow_eth_get_ip_addr_via_dhcp(dhcp_info, MEADOW_ETHMAC_DEVICENAME, macAddr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_get_ip_addr_via_dhcp() err:0x%08x, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }

    // Displays at LOG_NOTICE
    meadow_eth_utils_display_ip_mac();

  } while(true);
}

#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
