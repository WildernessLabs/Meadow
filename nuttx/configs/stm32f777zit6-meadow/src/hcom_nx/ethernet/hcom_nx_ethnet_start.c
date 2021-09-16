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

#if defined (HCOM_INCLUDE_ETHERNET_IN_HCOM_IN_BUILD)

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

  syslog(1, "===>3-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);

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

  // Retrieve the needed information 
  syslog(1, "===>4-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
  ret = dhcpc_request(handle, &ds);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-dhcpc_request() failed:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    dhcpc_close(handle);
    return -errno;
  }

  // Save our IP address
  syslog(1, "===>5-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
  ret = ethnet_utils_set_ipv4_w_addr(interfaceName, &ds.ipaddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-dhcpc_request() failed:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    dhcpc_close(handle);
    return -errno;
  }

  syslog(1, "===>6-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
  if (ds.netmask.s_addr != 0)
  {
    // netlib_set_ipv4netmask
  syslog(1, "===>7-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
    ret = ethnet_utils_set_ipv4_mask_w_addr(interfaceName, &ds.netmask);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_utils_set_ipv4_mask_w_addr() failed:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      dhcpc_close(handle);
      return -errno;
    }
  }

  syslog(1, "===>8-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
  if (ds.default_router.s_addr != 0)
  {
    // netlib_set_dripv4addr
  syslog(1, "===>9-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
    ret = ethnet_utils_set_router_w_addr(interfaceName, &ds.default_router);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_utils_set_router_w_addr() failed:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      dhcpc_close(handle);
      return -errno;
    }
  }

  syslog(1, "===>10-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
  if (ds.dnsaddr.s_addr != 0)
  {
    // netlib_set_ipv4dnsaddr
    syslog(1, "===>11-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
    ret = ethnet_utils_set_dns_w_addr(&ds.dnsaddr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_utils_set_dns_w_addr() failed:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      dhcpc_close(handle);
      return -errno;
    }
  }

  dhcpc_close(handle);
  syslog(1, "===>12-%s@%d-ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
  return OK;
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
  syslog(1, "===>%s@%d-Entered from STARTUP MGR, hcom_nx_start_up_ethernet()\n",
            thisFile, __LINE__); //usleep(20 * 1000);
  if(useEthnetStartupThread)
  {
    syslog(1, "===>%s@%d-Creating ETH kthread for start_ethnet_kthread()\n",
              thisFile, __LINE__); //usleep(20 * 1000);
    // Create a thread to do the startup
    _enet_kthread_pid = kthread_create(HCOM_THREAD_NAME_ETHNET_START,
                                    HCOM_THREAD_PRIORITY_ETHNET_START,
                                    HCOM_THREAD_STACKSIZE_ETHNET_START,
                                    (main_t) start_ethnet_kthread,
                                    (char *const *) NULL);
    if (_enet_kthread_pid <= 0)
    {
      syslog(1, "===>%s@%d-Creation of ETH kthread FAILED\n", thisFile, __LINE__); //usleep(20 * 1000);
      return -ENOEXEC;
    }

    syslog(1, "===>%s@%d-ETH kthread SUCCESSFULLY created\n",
            thisFile, __LINE__); //usleep(20 * 1000);
  }
  else
  {
    syslog(1, "===>%s@%d-Calling hcom_nx_ethnet_start_function() (NO KTHREAD)\n",
             thisFile, __LINE__); //usleep(20 * 1000);
    return hcom_nx_ethnet_start_function();
    syslog(1, "===>%s@%d-hcom_nx_ethnet_start_function() Exiting (NO KTHREAD)\n",
            thisFile, __LINE__); //usleep(20 * 1000);
  }
  return OK;
}

//=============================================================================
// Net kthread enters here
void *start_ethnet_kthread(int argc, char *argv[])
{
  syslog(1, "===>%s@%d-KTHREAD IS RUNNING. Will sleep 2 seconds\n",
            thisFile, __LINE__); //usleep(20 * 1000);
  sleep(2);
  syslog(1, "===>%s@%d-KTHREAD IS RUNNING. Calling hcom_nx_ethnet_start_function()\n",
            thisFile, __LINE__); //usleep(20 * 1000);
  
  (void) hcom_nx_ethnet_start_function();

  syslog(1, "===>%s@%d-Enet startup completed. Kthread EXITING after 50 second delay.\n",
            thisFile, __LINE__); //usleep(20 * 1000);

  sleep(5);
  return NULL;
}

//=============================================================================
int hcom_nx_ethnet_start_function(void)
{
  int ret;
  uint8_t macAddr[IFHWADDRLEN];

  syslog(1, "===>1-%s@%d-Entered hcom_nx_start_up_ethernet() (Generic starup)\n",
          thisFile, __LINE__); //usleep(20 * 1000);

  // Activates a network interface making it useable
  ret = ethnet_utils_exec_ifup(MEADOW_ETHMAC_DEVICENAME);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ethnet_utils_exec_ifup err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return -errno;
  }

  // Get the interfaces MAC address from the hardware
  syslog(1, "===>2-%s@%d-hcom_nx_start_up_ethernet()\n", thisFile, __LINE__); //usleep(20 * 1000);
  ret = ethnet_utils_get_hw_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ethnet_utils_get_hw_mac err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return -errno;
  }

  // Debugging
  syslog(1, "===>MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
        ((uint8_t*)macAddr)[0], ((uint8_t*)macAddr)[1], ((uint8_t*)macAddr)[2],
        ((uint8_t*)macAddr)[3], ((uint8_t*)macAddr)[4], ((uint8_t*)macAddr)[5]);
  
  // // Set the MAC address
  // syslog(1, "===>3-%s@%d-hcom_nx_start_up_ethernet()\n", thisFile, __LINE__); //usleep(20 * 1000);
  // ret = ethnet_utils_set_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);  
  // if(ret < 0)
  // {
  //   syslog(LOG_ERR, "%s@%d-ethnet_utils_set_mac err:0x%08x, errno:%d\n",
  //             thisFile, __LINE__, ret, errno);
  //   return -errno;
  // }

  if(useDhcpForIPAddr)
  {
    // Use dhcpc to set our IP address
    // syslog(1, "===>4-%s@%d-Calling ethnet_start_get_ip_w_dhcp()\n", thisFile, __LINE__); //usleep(20 * 1000);
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
    syslog(1, "===>%s@%d-Calling ethnet_utils_set_ipv4()\n", thisFile, __LINE__); //usleep(20 * 1000);
    ret = ethnet_utils_set_ipv4(MEADOW_ETHMAC_DEVICENAME, staticIpAddr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ethnet_utils_set_ipv4() err:0x%08x, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }
  }

  syslog(1, "===>%s@%d-Finished with starting up ethernet\n", thisFile, __LINE__); //usleep(20 * 1000);

  return OK;
}

#else

int hcom_nx_start_up_ethernet(void)
{
  return OK;
}

#endif    // HCOM_INCLUDE_ETHERNET_IN_HCOM_IN_BUILD