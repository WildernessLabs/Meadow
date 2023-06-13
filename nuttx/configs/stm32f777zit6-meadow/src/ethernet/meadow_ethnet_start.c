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
#include <nuttx/kthread.h>

#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
#include "meadow_ethnet_local.h"
#include <meadow/meadow_ethnet_common.h>
#include <meadow/hcom_shared_common.h>
#include "../hcom_nx/hcom_nx_config_manager.h"

#ifndef CONFIG_SCHED_LPWORK
#error "meadow_ethnet_monitor requires CONFIG_SCHED_LPWORK"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MEADOW_ETHNET_DHCP_RETRY_COUNT (3)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *thisFile = __FILE__;

static bool _configUseDhcp;
static uint32_t _configStaticIpAddr;
static uint32_t _configStaticIpMask;
static uint32_t _configStaticGateWay;

static int _meadow_eth_start_kthrd;

static struct dhcp_info_s *_dhcp_info;
static uint8_t *_macAddr;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int meadow_eth_start_establish_connection(struct dhcp_info_s *dhcp_info,
          uint8_t *macAddr);
static void *meadow_eth_start_kthread(int argc, char *argv[]);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This is called from hcom_nx_startup_mgr.c
int meadow_eth_mgr_startup(void)
{
  _dhcp_info = malloc(sizeof(struct dhcp_info_s));
  if(_dhcp_info == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  _macAddr = malloc(sizeof(IFHWADDRLEN));
  if(_macAddr == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  hcom_nx_config_lock();
  meadow_configuration_t *config = hcom_nx_config_get_pointer();
  _configUseDhcp = config->default_interface->use_dhcp == TRUE ? true : false;

  // If not using DHCP other information is needed.
  // Note: DNS is setup automatically by meadow configuration via the file
  // dns.conf. Nuttx uses the information in this file so nothing else is
  // needs to be done.
  if(!_configUseDhcp)
  {
    _configStaticIpAddr  = NTOHL(config->default_interface->ip_address);
    _configStaticIpMask  = NTOHL(config->default_interface->netmask);
    _configStaticGateWay = NTOHL(config->default_interface->gateway);
  }
  hcom_nx_config_unlock();

  // Create a thread to do the ethernet startup
  _meadow_eth_start_kthrd = kthread_create(MEADOW_THREAD_NAME_ETHNET_START,
                                  MEADOW_THREAD_PRIORITY_ETHNET_START,
                                  MEADOW_THREAD_STACKSIZE_ETHNET_START,
                                  (main_t) meadow_eth_start_kthread,
                                  (char *const *) NULL);
  if (_meadow_eth_start_kthrd <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
              thisFile, __LINE__, MEADOW_THREAD_NAME_ETHNET_START);
    return -ENOEXEC;
  }

  return OK;
}

//=========================================================================
// This short lived thread allows the rest of Nuttx initialization to
// continue while it completes the Ethernet startup, which can take some
// time. Mostly, in getting the DHCP address.
void *meadow_eth_start_kthread(int argc, char *argv[])
{
  int ret;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_THREAD_NAME_ETHNET_START);
#endif

  // Without the following delay the first dhcp Discovery broadcast to a DHCP
  // server will fail. Therefore, receive will never happen. After 10 seconds
  // the receive will timeout and the Discovery will be sent again. This second
  // time it will be sent successfully and everything works. Seems to be
  // something within Nuttx that needs time to be fully initialized.
  sleep(2);   // See comment for reason for delay.

  // Verify that this is a LAN9355 and initialize link status monitoring via
  // the LAN9355's IRQ pin
  ret = meadow_eth_monitor_startup();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-calling meadow_eth_monitor_startup() failed. ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Allow Ethernet Monitor to set the initial link status values before the
  // intial attempt to make a connection.
  bool startLinkStatus = meadow_eth_mon_startup_set_status();

  // Establish a connection, if we have link status up
  if(startLinkStatus)
    (void) meadow_eth_start_re_establish_connection();

  return NULL;
}

/****************************************************************************
 * Private Function Implementations
 ****************************************************************************/
// This function is called to initialize and start the ethernet. This is not a
// fast operation so we run it on our temporary thread.
int meadow_eth_start_establish_connection(struct dhcp_info_s *dhcp_info,
          uint8_t *macAddr)
{
  int ret;

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
  // Checksum is used to create the MAC Address. And, yes, duplicates are
  // possible but very unlikely on the same subnet.
  ret = meadow_eth_utils_get_hw_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_utils_get_hw_mac err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    meadow_eth_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
    return -errno;
  }

  // Set the MAC address
  ret = meadow_eth_utils_set_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);  
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_mac err:0x%08x, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    meadow_eth_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
    return -errno;
  }

  if(_configUseDhcp)
  {
    int count;

    // Try x times to get a DHCP to response.
    for(count = 0; count < MEADOW_ETHNET_DHCP_RETRY_COUNT; count++)
    {
      // Use dhcpc to get and set our IP address
      ret = meadow_eth_dhcp_get_device_ip_info(dhcp_info, MEADOW_ETHMAC_DEVICENAME, macAddr);
      if(ret < 0)
      {
        if (errno == EAGAIN)
        {
          continue;   // Try again since socket timeout this time
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
    addr.s_addr = HTONL(_configStaticIpAddr);

    ret = meadow_eth_utils_set_ipv4(MEADOW_ETHMAC_DEVICENAME, &addr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_ipv4() err:0x%08x, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      meadow_eth_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
      return -errno;
    }

    // netlib_set_ipv4netmask
    addr.s_addr = HTONL(_configStaticIpMask);
    ret = meadow_eth_utils_set_ipv4_mask(MEADOW_ETHMAC_DEVICENAME, &addr);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_ipv4_mask() failed:%d, errno:%d\n",
             thisFile, __LINE__, ret, errno);
      return -errno;
    }

    // netlib_set_dripv4addr
    addr.s_addr = HTONL(_configStaticGateWay);
    ret = meadow_eth_utils_set_router(MEADOW_ETHMAC_DEVICENAME, &addr);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_router() failed:%d, errno:%d\n",
             thisFile, __LINE__, ret, errno);
      return -errno;
    }
  }

  return OK;
}

/****************************************************************************
 * Public Function Implementations
 ****************************************************************************/
// This function called during the startup and for reconnections.
int meadow_eth_start_re_establish_connection()
{
  int ret;

  // This function does all the heavy lifting of establishing a connection.
  ret = meadow_eth_start_establish_connection(_dhcp_info, _macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Attempting to start ethernet failed. ret:%d, errno:%d\n",
              ret, errno);
    return ret;
  }

  // Report to syslog user that ethernet is up
  meadow_eth_utils_syslog_ip_mac();

  // If using DHCP for our ip address then initialize lease renewal
  if(_configUseDhcp)
  {
    ret = meadow_eth_init_dhcp_lease_renewal(_dhcp_info);
    if(ret < 0)
    {
      syslog(LOG_ERR, "Init dhcp lease failed. ret:%d, errno:%d\n",
                ret, errno);
    }
  }
  return ret;
}

#endif    // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
