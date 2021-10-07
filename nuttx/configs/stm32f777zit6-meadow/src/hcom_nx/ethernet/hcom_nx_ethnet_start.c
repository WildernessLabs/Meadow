/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/hcom_nx/ethernet/hcom_nx_ethnet_start.c
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

#include "hcom_nx_ethnet_local.h"
#include <meadow/meadow_ethnet_common.h>

#include <meadow/hcom_shared_common.h>

//------------------------------------------------------------
// Temporary items that will ultimately come from the configuration.
static bool useDhcpForIPAddr = true;
static uint32_t staticIpAddr = 0xc0a802c9;   // 192.168.2.201  // Just some address
//------------------------------------------------------------

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HCOM_NX_ETHNET_DHCP_RETRY_COUNT (3)

/****************************************************************************
 * Private Data
 ****************************************************************************/
#if defined(CONFIG_HCOM_INCLUDE_ETHNET_IN_BUILD)

static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Function Implementations
 ****************************************************************************/
// This function is called to initialize and start the ethernet
static int hcom_nx_start_ethernet_function(struct dhcp_info_s *dhcp_info)
{
  int ret;
  uint8_t macAddr[IFHWADDRLEN];

  // Activates a network interface making it useable
  ret = ethnet_utils_exec_ifup(MEADOW_ETHMAC_DEVICENAME);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ethnet_utils_exec_ifup err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return -errno;
  }

  // Get the interfaces MAC address from the hardware. This is done by taking
  // The F7's unique ID and doing a CRC64 checksum. The result of the CRC64
  // Checksum is used to create the MAC Address.
  ret = ethnet_utils_get_hw_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ethnet_utils_get_hw_mac err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    ethnet_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
    return -errno;
  }

  ninfo("H/W MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
        ((uint8_t*)macAddr)[0], ((uint8_t*)macAddr)[1], ((uint8_t*)macAddr)[2],
        ((uint8_t*)macAddr)[3], ((uint8_t*)macAddr)[4], ((uint8_t*)macAddr)[5]);
  
  // Set the MAC address
  ret = ethnet_utils_set_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);  
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ethnet_utils_set_mac err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    ethnet_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
    return -errno;
  }

  if(useDhcpForIPAddr)
  {
    int count;

    // Try x times to get a DHCP to reponds.
    for(count = 0; count < HCOM_NX_ETHNET_DHCP_RETRY_COUNT; count++)
    {
      // Use dhcpc to set our IP address
      ret = ethnet_get_ip_addr_via_dhcp(dhcp_info, MEADOW_ETHMAC_DEVICENAME, macAddr);
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
          ethnet_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
          return -errno;
        }
      }

      break;    // Got IP Address
    }

    // Did we exit due to count?
    if(count == HCOM_NX_ETHNET_DHCP_RETRY_COUNT)
    {
      // Why try forever?
      syslog(LOG_ERR, "%s@%d-After %d attempts failed to get IP address via DHCP, ret:%d, errno:%d\n",
                thisFile, __LINE__, HCOM_NX_ETHNET_DHCP_RETRY_COUNT, ret, errno);
      ethnet_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
      return -errno;
    }
  }
  else
  {
    // Use a static IP address
    struct in_addr addr;
    addr.s_addr = staticIpAddr;

    ret = ethnet_utils_set_ipv4(MEADOW_ETHMAC_DEVICENAME, &addr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_utils_set_ipv4() err:0x%08x, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      ethnet_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
      return -errno;
    }
  }

  return OK;
}

// /****************************************************************************
//  * Public Functions
//  ****************************************************************************/
// New kthread enters here to startup ethernet
void *start_ethnet_kthread(int argc, char *argv[])
{
  struct dhcp_info_s *dhcp_info = hcom_nx_eth_mgr_get_dhcp_info();

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), HCOM_THREAD_NAME_ETHNET_START);
#endif

  // For reasons I have not investigated, the network cannot be brought up
  // immediately. A delay of 2 seconds allows it to start without errors.
  // Without the delay the first dhcp Discovery broadcast to a DHCP server
  // will fail. Therefore, receive will never happen. After 10 seconds the
  // receive will timeout and the Discovery will be sent again, this time it
  // will be sent successfully and everything works. Seems to be something
  // within Nuttx that needs to be initialized.
  sleep(2);   // See comment for reason for delay.
  int ret = hcom_nx_start_ethernet_function(dhcp_info);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Attempting to start ethernet failed. ret:%d, errno:%d\n",
              ret, errno);
    usleep(1 * 1000);  // Make sure this is written
    return NULL;
  }
  else
  {
    // Report to user that ethernet is up
    ethnet_utils_display_ip_mac();
  }

  // If not using DHCP for our address then don't need to renew the lease
  if(!useDhcpForIPAddr)
    return NULL;

  // Never return from this call
  ret = hcom_eth_renew_lease_loop(dhcp_info);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Attempting to enter renew lease failed:%d, errno:%d\n",
              ret, errno);
    usleep(1 * 1000);  // Make sure this is written
  }

  // Thread exists after starting ethernet
  return NULL;
}

#endif    // #if defined(CONFIG_HCOM_INCLUDE_ETHNET_IN_BUILD)
