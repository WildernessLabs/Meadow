/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/ethernet/meadow_ethnet_start.c
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

// This module contains code to start ethernet for the Embedded Meadow board
// At present (13Sept21) only IPv4 is supported

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <ctype.h>
#include <stdint.h>

#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
#include "meadow_ethnet_local.h"
#include <meadow/meadow_ethnet_common.h>
#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MEADOW_ETHNET_DHCP_RETRY_COUNT (3)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int meadow_ethernet_start_function(struct dhcp_info_s *dhcp_info);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// New kthread created by meadow_ethernet_manager, enters here to startup
// ethernet and allow other startup operations to continue
void *meadow_eth_start_kthread(int argc, char *argv[])
{
  struct dhcp_info_s *dhcp_info = meadow_eth_mgr_get_dhcp_info();

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_THREAD_NAME_ETHNET_START);
#endif

  // For reasons I have not investigated, the network cannot be brought up
  // immediately. A delay of 2 seconds allows it to start without errors.
  // Without the delay the first dhcp Discovery broadcast to a DHCP server
  // will fail. Therefore, receive will never happen. After 10 seconds the
  // receive will timeout and the Discovery will be sent again, this time it
  // will be sent successfully and everything works. Seems to be something
  // within Nuttx that needs to be initialized.
  sleep(2);   // See comment for reason for delay.
  int ret = meadow_ethernet_start_function(dhcp_info);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Attempting to start ethernet failed. ret:%d, errno:%d\n",
              ret, errno);
    return NULL;
  }
  else
  {
    // Report to user that ethernet is up
    meadow_eth_utils_display_ip_mac();

    // Start monitoring ethernet link status
    meadow_eth_monitor_startup();
  }

  // If not using DHCP for our address then don't need to renew the lease
  if(!ethUseDhcpForIpAddr)
    return NULL;

  //---------------------------------------------------------------
  // This call will never return as it periodically renews the DHCP lease.
  // Hoping this is temporary and can be replaced with a geneneralized
  // periodic timer.
  ret = meadow_eth_renew_lease_loop(dhcp_info);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Attempting to enter renew lease failed:%d, errno:%d\n",
              ret, errno);
  }

  // Thread exists here
  return NULL;
}


/****************************************************************************
 * Private Function Implementations
 ****************************************************************************/
// This function is called to initialize and start the ethernet
int meadow_ethernet_start_function(struct dhcp_info_s *dhcp_info)
{
  int ret;
  uint8_t macAddr[IFHWADDRLEN];

  // Activates a network interface making it useable
  ret = meadow_eth_utils_exec_ifup(MEADOW_ETHMAC_DEVICENAME);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_utils_exec_ifup err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return -errno;
  }

  // Get the interfaces MAC address from the hardware. This is done by taking
  // The F7's unique ID and doing a CRC64 checksum. The result of the CRC64
  // Checksum is used to create the MAC Address.
  ret = meadow_eth_utils_get_hw_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_utils_get_hw_mac err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    meadow_eth_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
    return -errno;
  }

  ninfo("H/W MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
        ((uint8_t*)macAddr)[0], ((uint8_t*)macAddr)[1], ((uint8_t*)macAddr)[2],
        ((uint8_t*)macAddr)[3], ((uint8_t*)macAddr)[4], ((uint8_t*)macAddr)[5]);
  
  // Set the MAC address
  ret = meadow_eth_utils_set_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);  
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_mac err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    meadow_eth_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
    return -errno;
  }

  if(ethUseDhcpForIpAddr)
  {
    int count;

    // Try x times to get a DHCP to reponds.
    for(count = 0; count < MEADOW_ETHNET_DHCP_RETRY_COUNT; count++)
    {
      // Use dhcpc to get and set our IP address
      ret = meadow_eth_get_ip_addr_via_dhcp(dhcp_info, MEADOW_ETHMAC_DEVICENAME, macAddr);
      if(ret < 0)
      {
        if (errno == EAGAIN)
        {
          continue;   // Try again since socket timeout
        }
        else
        {
          syslog(LOG_ERR, "%s@%d-failed to get IP address via DHCP, ret:%d, errno:%d\n",
                    thisFile, __LINE__, ret, errno);
          meadow_eth_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
          return -errno;
        }
      }

      break;    // Got IP Address
    }

    // Did we exit due to count?
    if(count == MEADOW_ETHNET_DHCP_RETRY_COUNT)
    {
      // Why try forever?
      syslog(LOG_ERR, "%s@%d-After %d attempts failed to get IP address via DHCP, ret:%d, errno:%d\n",
                thisFile, __LINE__, MEADOW_ETHNET_DHCP_RETRY_COUNT, ret, errno);
      meadow_eth_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
      return -errno;
    }
  }
  else
  {
    // Use a static IP address
    struct in_addr addr;
    addr.s_addr = ethUseAsStaticIpAddr;

    ret = meadow_eth_utils_set_ipv4(MEADOW_ETHMAC_DEVICENAME, &addr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_ipv4() err:0x%08x, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      meadow_eth_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
      return -errno;
    }
  }

  return OK;
}

#endif    // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
