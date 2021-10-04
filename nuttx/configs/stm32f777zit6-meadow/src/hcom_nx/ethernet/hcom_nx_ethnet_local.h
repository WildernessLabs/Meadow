/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/hcom_nx/ethernet/hcom_nx_ethnet_local.h
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
#ifndef __CONFIGS_MEADOW_SRC_HCOM_NX_ETHNET_LOCAL__H
#define __CONFIGS_MEADOW_SRC_HCOM_NX_ETHNET_LOCAL__H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdio.h>

#include <netinet/in.h>
#include <net/if.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MEADOW_ETHMAC_DEVICENAME "eth0"

#define HCOM_THREAD_NAME_ETHNET_START "EthInit"
// Need priority higher than mono or ethernet initialization will take a long time
#define HCOM_THREAD_PRIORITY_ETHNET_START 120
#define HCOM_THREAD_STACKSIZE_ETHNET_START 2048 // 1024 was small

/****************************************************************************
 * Private Data
 ****************************************************************************/

struct dhcpc_state
{
  struct in_addr serverid;
  struct in_addr ipaddr;
  struct in_addr netmask;
  struct in_addr dnsaddr;
  struct in_addr default_router;
  uint32_t       lease_time;      /* Lease expires in this number of seconds */
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

// Utilities
int ethnet_utils_get_hw_mac(const char *interfaceName, uint8_t *macAddr);
int ethnet_utils_exec_ifup(const char *interfaceName);
int ethnet_utils_set_mac(const char *interfaceName, const uint8_t *macAddr);
int ethnet_utils_set_ipv4(const char *interfaceName,
          const struct in_addr *addr);
int ethnet_utils_get_ipv4(const char *interfaceName,
          struct in_addr *addr);
int ethnet_utils_get_mac(const char *interfaceName,
          uint8_t *macAddr);
int ethnet_utils_set_ipv4_mask(const char *interfaceName,
          const struct in_addr *addr);
int ethnet_utils_set_dns(const struct in_addr *inaddr);
int ethnet_utils_set_router(const char *interfaceName,
          const struct in_addr *addr);

// From dhcpc.h
FAR void *dhcpc_open(FAR const char *interface,
                     FAR const void *mac_addr, int mac_len);
int  dhcpc_request(FAR void *handle, FAR struct dhcpc_state *presult);
void dhcpc_close(FAR void *handle);

#endif // __CONFIGS_MEADOW_SRC_HCOM_NX_ETHNET_LOCAL__H
