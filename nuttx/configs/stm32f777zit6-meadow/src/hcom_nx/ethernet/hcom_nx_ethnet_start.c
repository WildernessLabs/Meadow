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
#include <nuttx/kthread.h>

#include "hcom_nx_ethnet_local.h"

#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
#if HCOM_INCLUDE_ETHERNET_IN_HCOM_IN_BUILD > 0

static char *thisFile = __FILE__;
int _enet_kthread_pid;

//------------------------------------------------------------
// Temporary items that will ultimately be from the configuration.
bool useDhcpForIPAddr = true;
uint32_t dhcpAddress = 0xc0a80201;    // 192.168.2.01
uint32_t staticIpAddr = 0xc0a802c9;   // 192.168.2.201
bool useDnsForAddrResolution = true;
uint32_t dnsAddress = 0xc0a80201;     // 192.168.2.01
uint32_t NetMaskIPv4 = 0xffffff00;
bool useEthnetStartupThread = true;
//------------------------------------------------------------

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void *start_ethnet_kthread(int argc, char *argv[]);
static int hcom_nx_ethnet_start_function(void);

/****************************************************************************
 * Private Function Implementations
 ****************************************************************************/

// Use dhp to get and set several ip addresses
static int ethnet_start_get_ip_w_dhcp(const char *interfaceName,
          const uint8_t *macAddr)
{
  int ret;
  FAR void *handle;
  struct dhcpc_state ds;

  /* Set up the DHCPC modules */

  handle = dhcpc_open(interfaceName, macAddr, IFHWADDRLEN);
  if (handle == NULL)
  {
    syslog(LOG_ERR, "%s@%d-dhcpc_open() failed, handle == NULL, errno:%d\n",
              thisFile, __LINE__,  errno);
    return -errno;
  }

  /* Get an IP address.  Note that there is no logic for renewing the IP address in this
   * example.  The address should be renewed in ds.lease_time/2 seconds.
   */

  ret = dhcpc_request(handle, &ds);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-dhcpc_request() failed:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    dhcpc_close(handle);
    return -errno;
  }

  // Save our IP address
  ret = ethnet_utils_set_ipv4(interfaceName, &ds.ipaddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-dhcpc_request() failed:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    dhcpc_close(handle);
    return -errno;
  }

  if (ds.netmask.s_addr != 0)
  {
    // netlib_set_ipv4netmask
    ret = ethnet_utils_set_ipv4_mask(interfaceName, &ds.netmask);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_utils_set_ipv4_mask() failed:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      dhcpc_close(handle);
      return -errno;
    }
  }

  if (ds.default_router.s_addr != 0)
  {
    // netlib_set_dripv4addr
    ret = ethnet_utils_set_router(interfaceName, &ds.default_router);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_utils_set_router() failed:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      dhcpc_close(handle);
      return -errno;
    }
  }

  if (ds.dnsaddr.s_addr != 0)
  {
    // netlib_set_ipv4dnsaddr
    ret = ethnet_utils_set_dns(&ds.dnsaddr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_utils_set_dns() failed:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      dhcpc_close(handle);
      return -errno;
    }
  }

  dhcpc_close(handle);
  return OK;
}

//==========================================================================
static void hcom_nx_start_log_net_up(void)
{
  uint8_t macAddr[IFHWADDRLEN];
  struct in_addr ipaddr;
  ipaddr.s_addr = 0;

  ethnet_utils_get_ipv4(MEADOW_ETHMAC_DEVICENAME, &ipaddr);
  ethnet_utils_get_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);

  syslog(LOG_NOTICE, "Ethernet up using MAC:%02x:%02x:%02x:%02x:%02x:%02x, IP:%d.%d.%d.%d\n",
            ((uint8_t*)macAddr)[0], ((uint8_t*)macAddr)[1], ((uint8_t*)macAddr)[2],
            ((uint8_t*)macAddr)[3], ((uint8_t*)macAddr)[4], ((uint8_t*)macAddr)[5],
            (ipaddr.s_addr       ) & 0xff,
            (ipaddr.s_addr >> 8  ) & 0xff,
            (ipaddr.s_addr >> 16 ) & 0xff,
            (ipaddr.s_addr >> 24 ) & 0xff);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
#define HCOM_THREAD_NAME_ETHNET_START "EthStart"
#define HCOM_THREAD_PRIORITY_ETHNET_START 60    // Low
#define HCOM_THREAD_STACKSIZE_ETHNET_START 2048 // 1024 too small

// This is the main entry point.
int hcom_nx_start_up_ethernet(void)
{
  if(useEthnetStartupThread)
  {
    // Create a thread to do the startup
    _enet_kthread_pid = kthread_create(HCOM_THREAD_NAME_ETHNET_START,
                                    HCOM_THREAD_PRIORITY_ETHNET_START,
                                    HCOM_THREAD_STACKSIZE_ETHNET_START,
                                    (main_t) start_ethnet_kthread,
                                    (char *const *) NULL);
    if (_enet_kthread_pid <= 0)
    {
      syslog(LOG_ERR, "%s@%d-Creation of ETH kthread FAILED\n", thisFile, __LINE__);
      return -ENOEXEC;
    }
  }
  else
  {
    int ret = hcom_nx_ethnet_start_function();
    if(ret < 0)
    {
      syslog(LOG_ERR, "Attempting to start ethernet failed:%d\n", ret);
      return ret;
    }
    else
    {
      // Report to user that ethernet is up
      hcom_nx_start_log_net_up();
    }
  }
  return OK;
}

//=============================================================================
// Net kthread enters here
void *start_ethnet_kthread(int argc, char *argv[])
{
  // For reasons I have not investigated, the network cannot be brought up
  // immediately. A short delay of 2 seconds allows it to start without errors.
  // Without the delay the first Discovery transmission to the DHCP server will
  // fail. Therefore, receive will never happen. After 10 seconds the receive
  // will timeout and the Discovery will be sent again, this time it will be
  // sent successfully and everything works. Seems to be something within
  // Nuttx that is needed.

  sleep(2);   // See comment above for why delay.

  int ret = hcom_nx_ethnet_start_function();

  if(ret < 0)
  {
    syslog(LOG_ERR, "Attempting to start ethernet failed:%d\n", ret);
  }
  else
  {
    // Report to user that ethernet is up
    hcom_nx_start_log_net_up();
  }
  return NULL;
}

//=============================================================================
int hcom_nx_ethnet_start_function(void)
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

  // Get the interfaces MAC address from the hardware
  ret = ethnet_utils_get_hw_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ethnet_utils_get_hw_mac err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return -errno;
  }

  ninfo("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
        ((uint8_t*)macAddr)[0], ((uint8_t*)macAddr)[1], ((uint8_t*)macAddr)[2],
        ((uint8_t*)macAddr)[3], ((uint8_t*)macAddr)[4], ((uint8_t*)macAddr)[5]);
  
  // Set the MAC address
  ret = ethnet_utils_set_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);  
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ethnet_utils_set_mac err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return -errno;
  }

  if(useDhcpForIPAddr)
  {
    // Use dhcpc to set our IP address
    ret = ethnet_start_get_ip_w_dhcp(MEADOW_ETHMAC_DEVICENAME, macAddr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_start_get_ip_w_dhcp() err:0x%08x, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }
  }
  else
  {
    struct in_addr addr;
    addr.s_addr = staticIpAddr;
    ret = ethnet_utils_set_ipv4(MEADOW_ETHMAC_DEVICENAME, &addr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_utils_set_ipv4() err:0x%08x, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }
  }

  return OK;
}

#else

int hcom_nx_start_up_ethernet(void)
{
  return OK;
}

#endif    // #if HCOM_INCLUDE_ETHERNET_IN_HCOM_IN_BUILD > 0
