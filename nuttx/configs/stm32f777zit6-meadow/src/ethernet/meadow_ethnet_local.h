/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/ethernet/meadow_ethnet_local.h
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
#ifndef __CONFIGS_MEADOW_SRC_MEADOW_ETHNET_LOCAL__H
#define __CONFIGS_MEADOW_SRC_MEADOW_ETHNET_LOCAL__H

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

#define MEADOW_THREAD_NAME_ETHNET_START "EthInit"
// Need priority higher than mono or ethernet initialization will take a long time
#define MEADOW_THREAD_PRIORITY_ETHNET_START 120
#define MEADOW_THREAD_STACKSIZE_ETHNET_START 2048 // 1024 was small

#define MEADOW_THREAD_NAME_ETHNET_MONITOR "EthMon"
#define MEADOW_THREAD_PRIORITY_ETHNET_MONITOR 120
#define MEADOW_THREAD_STACKSIZE_ETHNET_MONITOR 2048

//------------------------------------------------------------
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
// Temporary items that need to come from the Meadow configuration.
static bool     ethUseDhcpForAddr   = true;         // If false have the following
static uint32_t ethUseStaticIpAddr  = 0xc0a802c9;   // 192.168.2.201 - ip address
static uint32_t ethUseStaticIpMask  = 0xffffff00;   // 255.255.255.0 - address mask
static uint32_t ethUseStaticGateWay = 0xc0a80201;   // 192.168.2.1   - gateway address
static uint32_t ethUseStaticDNS     = 0x01010101;   // 1.1.1.1       - dns server address (cloud flare)
#endif
//------------------------------------------------------------

/****************************************************************************
 * Private Data
 ****************************************************************************/

// Note there where 2 structs one named 'dhcp_state_s' and one named 'dhcp_state'.
// 'dhcp_state' is now 'dhcp_info_s'.
struct dhcp_info_s
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
void meadow_eth_utils_display_ip_mac(void);
int meadow_eth_utils_get_hw_mac(const char *interfaceName, uint8_t *macAddr);
int meadow_eth_utils_exec_ifup(const char *interfaceName);
int meadow_eth_utils_exec_ifdown(const char *interfaceName);
int meadow_eth_utils_set_mac(const char *interfaceName, const uint8_t *macAddr);
int meadow_eth_utils_set_ipv4(const char *interfaceName,
          const struct in_addr *addr);
int meadow_eth_utils_get_ipv4(const char *interfaceName,
          struct in_addr *addr);
int meadow_eth_utils_get_mac(const char *interfaceName,
          uint8_t *macAddr);
int meadow_eth_utils_set_ipv4_mask(const char *interfaceName,
          const struct in_addr *addr);
int meadow_eth_utils_set_dns(const struct in_addr *inaddr);
int meadow_eth_utils_set_router(const char *interfaceName,
          const struct in_addr *addr);

void *meadow_eth_start_kthread(int argc, char *argv[]);

int meadow_eth_renew_lease_loop(struct dhcp_info_s *dhcp_info);
int meadow_eth_monitor_startup(void);

// From dhcpc.h
FAR void *meadow_eth_dhcp_open(FAR const char *interface,
                     FAR const void *mac_addr, int mac_len);
int  meadow_eth_dhcp_request(FAR void *handle, FAR struct dhcp_info_s *presult);
void meadow_eth_dhcp_close(FAR void *handle);

#endif // __CONFIGS_MEADOW_SRC_MEADOW_ETHNET_LOCAL__H
