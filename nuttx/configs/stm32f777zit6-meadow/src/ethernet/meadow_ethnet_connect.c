/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/ethernet/meadow_ethnet_connect.c
 * 
 *   Copyright (C) 2021-2023 Wilderness Labs. All rights reserved.
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

// This module contains code to connect to Ethernet

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <ctype.h>
#include <stdint.h>
#include <nuttx/kthread.h>
#include <nuttx/clock.h>
#include <nuttx/net/mii.h>
#include <nuttx/syslog/syslog.h>

#include "meadow_ethnet_local.h"
#include <meadow/meadow_ethnet_common.h>
#include "../hcom_nx/hcom_nx_config_manager.h"
#include "../ntpclient/ntpclient.h"
#include "../espcp/espcp_common.h"

// Uncomment the #define below to turn on debug help macros.
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Is Ethernet included in build?
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

#define MEADOW_ETHNET_DHCP_CONNECTION_RETRY_COUNT (5)

// The default Lease time is only used when using DHCP addressing and there is
// no connection.
// #define MEADOW_ETHNET_LEASE_TIME_DEFAULT_SEC (120)  // For testing
#define MEADOW_ETHNET_LEASE_TIME_DEFAULT_SEC (3600)   // Once an hour

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;
static struct dhcp_info_s *_dhcp_info;

static int _meadow_ethnet_connect_kthrd;
static sem_t _connectSem;
static bool _linkStatusUp;
static bool _isConnectionValid;
static bool _configUseDhcp;
static uint32_t _configStaticIpAddr;
static uint32_t _configStaticIpMask;
static uint32_t _configStaticGateWay;

static enum 
{
  MEADOW_ETH_CONN_UNDEFINED_STATE,
  MEADOW_ETH_CONN_LINK_STATUS_CHANGED,
  MEADOW_ETH_CONN_LEASE_RENEWAL_TIME
} _meadow_eth_conn_action;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void *meadow_ethnet_connect_kthread(int argc, char *argv[]);
static int meadow_eth_conn_establish_connection(struct dhcp_info_s *dhcp_info,
          uint8_t *macAddr);
static int meadow_eth_conn_renew_lease_periodically(struct dhcp_info_s *dhcp_info);
static int meadow_eth_conn_thread_do_work(void);
static int meadow_eth_conn_process_link_status_change(bool linkStatusUp);
static int meadow_eth_conn_report_link_status_change(bool isLinkUp);

/****************************************************************************
 * Function Implementations
 ****************************************************************************/
int meadow_eth_conn_startup()
{
  _linkStatusUp = false;
  _isConnectionValid = false;

  // Create a thread to establish ethernet connections and handle lease renewal
  _meadow_ethnet_connect_kthrd = kthread_create(MEADOW_THREAD_NAME_ETHNET_CONNECTION,
                                  MEADOW_THREAD_PRIORITY_ETHNET_CONNECTION,
                                  MEADOW_THREAD_STACKSIZE_ETHNET_CONNECTION,
                                  (main_t) meadow_ethnet_connect_kthread,
                                  (char *const *) NULL);
  if (_meadow_ethnet_connect_kthrd <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
                thisFile, __LINE__,  MEADOW_THREAD_NAME_ETHNET_CONNECTION);
    return -ENOEXEC;
  }

  return OK;
}

//=====================================================================
// This thread is used to make Ethernet connections, renew the DHCP lease and
// manage reading the NTP source.
void *meadow_ethnet_connect_kthread(int argc, char *argv[])
{
  int ret;
  time_t leaseTimeSec = 0;
  _meadow_eth_conn_action = MEADOW_ETH_CONN_UNDEFINED_STATE;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_THREAD_NAME_ETHNET_CONNECTION);
#endif

  _dhcp_info = malloc(sizeof(struct dhcp_info_s));
  if(_dhcp_info == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return NULL;   // -ENOMEM;
  }
  memset(_dhcp_info, 0, sizeof(struct dhcp_info_s));

  // Initialize the connection semaphore
  ret = sem_init(&_connectSem, 0, 0);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-sem_init failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return NULL;
  }

  // If not using DHCP the connection information is still required.
  // Note: DNS is setup automatically by meadow configuration via the file
  // dns.conf. Nuttx uses the information in this file so nothing else is
  // needs to be done.
  hcom_nx_config_lock();
  meadow_configuration_t *config = hcom_nx_config_get_pointer();
  _configUseDhcp = config->default_interface->use_dhcp == 1 ? true : false;
  if(!_configUseDhcp)
  {
    _configStaticIpAddr  = NTOHL(config->default_interface->ip_address);
    _configStaticIpMask  = NTOHL(config->default_interface->netmask);
    _configStaticGateWay = NTOHL(config->default_interface->gateway);
  }
  hcom_nx_config_unlock();

  //----------------------------------------------------------------
  // Enter a forever loop that waits for the connect semaphore.
  for(;;)
  {
    struct timespec waketime;

    // Wait for either the semaphore to be posted or a time-out to occur if
    // using DHCP
    if(_configUseDhcp)
    {
      sched_lock();
      ret = clock_gettime(CLOCK_REALTIME, &waketime);
      if(ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-clock_gettime, ret:%d, errno:%d\n",
                    thisFile, __LINE__, ret, errno);
        sched_unlock();
        return NULL;
      }

      // Use the DHCP Lease Renewal time to wakeup the semaphore if connected
      // otherwise use a default
      if(_isConnectionValid)
      {
        leaseTimeSec = _dhcp_info->lease_time/2;
      }
      else
      {
        leaseTimeSec = MEADOW_ETHNET_LEASE_TIME_DEFAULT_SEC;
      }
      waketime.tv_sec += leaseTimeSec;
      
      // Wait for lease time to expire or the link status to change
      MEADOW_TRACE_INFORMATION("%s@%d-DHCP addressing. Waiting on status or lease timeout\n",
                thisFile, __LINE__);
      ret = sem_timedwait(&_connectSem, &waketime);
      sched_unlock();
    }
    else
    {
      // No, least time to worry about so just wait for interrupt from LAN Chip
      MEADOW_TRACE_INFORMATION("%s@%d-Static addressing. Waiting on status change\n",
                thisFile, __LINE__);
      ret = sem_wait(&_connectSem);
    }

    if(ret < 0) 
    {
      if (errno == ETIMEDOUT)
      {
        if(! _configUseDhcp)
          continue;       // Nothing to do

        MEADOW_TRACE_INFORMATION("%s@%d-Time to Renew Lease\n", thisFile, __LINE__);
        _meadow_eth_conn_action = MEADOW_ETH_CONN_LEASE_RENEWAL_TIME;
      }
      else
      {
        syslog(LOG_ERR, "%s@%d-sem_timedwait() Failed, ret:%d, errno:%d\n",
                    thisFile, __LINE__, ret, errno);
      }
    }

    // Do the work, then return and continue to wait for the next event.
    ret = meadow_eth_conn_thread_do_work();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow Ethernet Monitor failed, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      break;
    }
  }

  return NULL;
}

//===========================================================================
// This function is called for lease renewal and link status changes
int meadow_eth_conn_thread_do_work(void)
{
  int ret;
  
  switch(_meadow_eth_conn_action)
  {
    case MEADOW_ETH_CONN_LINK_STATUS_CHANGED:
    ret = meadow_eth_conn_process_link_status_change(_linkStatusUp);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-switch default value:%d\n",
                  thisFile, __LINE__, _meadow_eth_conn_action);
    }
    break;

    case MEADOW_ETH_CONN_LEASE_RENEWAL_TIME:
    if(_isConnectionValid)
    {
      ret = meadow_eth_conn_renew_lease_periodically(_dhcp_info);
      if(ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-switch default value:%d\n",
                    thisFile, __LINE__, _meadow_eth_conn_action);
      }
    }
    break;

    case MEADOW_ETH_CONN_UNDEFINED_STATE:
    break;

    default:
    syslog(LOG_ERR, "%s@%d-Unknown switch value:%d\n",
                thisFile, __LINE__, _meadow_eth_conn_action);
  }

  _meadow_eth_conn_action = MEADOW_ETH_CONN_UNDEFINED_STATE;

  return OK;
}

//===========================================================================
// This function is called when the connections link status has changed.
int meadow_eth_conn_process_link_status_change(bool linkStatusUp)
{
  int ret;
  uint8_t macAddr[IFHWADDRLEN];

  hcom_nx_config_lock();
  meadow_configuration_t *config = hcom_nx_config_get_pointer();
  uint32_t refreshPeriod = config->ntp_refresh_period_seconds;
  bool timeAtStart = config->get_network_time_at_startup;
  hcom_nx_config_unlock();

  MEADOW_TRACE_INFORMATION("%s@%d-Processing LinkStatus change. Now %s\n",
            thisFile, __LINE__, _linkStatusUp ? "Up" : "Down");

  // Did the link status transition from down to up?
  if(linkStatusUp)
  {
    ret = meadow_eth_utils_get_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_mac ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }

    // This function does all the heavy lifting of establishing a connection.
    // If we are using dhcp for our IP address this call will populate the
    // dhcp_info with network information.
    ret = meadow_eth_conn_establish_connection(_dhcp_info, macAddr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "Attempting to establish ethernet connection failed. ret:%d, errno:%d\n",
                ret, errno);
      return ret;
    }

    // Show via syslog user that ethernet is up etc.
    meadow_eth_utils_syslog_ip_mac();

    // Now that everything is ready, report that the status is up
    ret = meadow_eth_conn_report_link_status_change(true);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_conn_report_link_status_up, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      return ret;
    }

    // Determine if we should get the NTP time now or ever,.
    if(refreshPeriod > 0 || timeAtStart)
    {
      // This call will cause the ntpclient.c code to periodically refresh the
      // NTP time without additional intervention.
      MEADOW_TRACE_INFORMATION("Sending NTP start message to NTP queue\n");
      espcp_send_message_to_ntp_queue(NTPC_START);
    }

    // We now have an ethernet connection.
    MEADOW_TRACE_INFORMATION("%s@%d-We now have an ethernet connection\n", thisFile, __LINE__);
    _isConnectionValid = true;
  }
  else
  {
    // Stop the periodic NTP time request
    if (timeAtStart)
    {
      MEADOW_TRACE_INFORMATION("Sending NTP stop message to NTP queue\n");

      espcp_send_message_to_ntp_queue(NTPC_STOP);
    }

    // Link status transitioned to down
    ret = meadow_eth_conn_report_link_status_change(false);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_conn_report_link_status_up, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      return ret;
    }
  }

  return ret;
}

//========================================================================
// This function is called to initialize and connect the ethernet. This
// function can take some time to complete.
int meadow_eth_conn_establish_connection(struct dhcp_info_s *dhcp_info,
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

  // Get the interfaces MAC address from the F7 hardware. This is done by
  // taking the F7's unique ID and doing a CRC64 checksum. The result of the
  // CRC64 Checksum is used to create the MAC Address.
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
    for(count = 0; count < MEADOW_ETHNET_DHCP_CONNECTION_RETRY_COUNT; count++)
    {
      // This call will populate the dhcp_info structure with: IP, netmask, DNS
      // server address, default router address and the lease expiration time,
      // using information from the DHCP server.
      ret = meadow_eth_dhcp_get_dhcp_info(dhcp_info, MEADOW_ETHMAC_DEVICENAME, macAddr);
      if(ret < 0)
      {
        if (errno == EAGAIN)
        {
          continue;   // Try again since socket timed out
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
    if(count == MEADOW_ETHNET_DHCP_CONNECTION_RETRY_COUNT)
    {
      // Why try forever?
      syslog(LOG_ERR, "%s@%d-After %d attempts failed to get IP address via DHCP, ret:%d, errno:%d\n",
                thisFile, __LINE__, MEADOW_ETHNET_DHCP_CONNECTION_RETRY_COUNT, ret, errno);
      meadow_eth_utils_exec_ifdown(MEADOW_ETHMAC_DEVICENAME);
      return -errno;
    }
  }
  else
  {
    // Using static IP addressing
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
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_ipv4_mask() failed, ret:%d, errno:%d\n",
             thisFile, __LINE__, ret, errno);
      return -errno;
    }

    // netlib_set_dripv4addr
    addr.s_addr = HTONL(_configStaticGateWay);
    ret = meadow_eth_utils_set_router(MEADOW_ETHMAC_DEVICENAME, &addr);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_router() failed, ret:%d, errno:%d\n",
             thisFile, __LINE__, ret, errno);
      return -errno;
    }
  }

  return OK;
}

//====================================================================
// Report to Nuttx and meadow the change in link status
int meadow_eth_conn_report_link_status_change(bool isLinkUp)
{
  int ret;
  int sockDescp;
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, MEADOW_ETHMAC_DEVICENAME, IFNAMSIZ);

  // Keep Nuttx informed
  ifr.ifr_flags = isLinkUp ? IFF_UP : IFF_DOWN;
  
  // Need a socket descriptor to communicate with the network interface
  sockDescp = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockDescp < 0)
  {
    syslog(LOG_ERR, "%s@%d-socket open failed, sockDescp:%d, errno:%d\n",
                thisFile, __LINE__, sockDescp, errno);
    return -errno;
  }

  // Set the Nuttx link status value
  ret = ioctl(sockDescp, SIOCSIFFLAGS, (unsigned long)&ifr);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ioctl(SIOCSIFFLAGS) failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  close(sockDescp);

  // Past the new link status to Meadow
  espcp_queue_ethernet_connection_changed_event(isLinkUp);

  return OK;
}

//==============================================================================
// This will renew the lease periodically. It is only called from this module.
// Note: the contents of dhcp_info can change, including the IP address and the
// lease timeout. These are also re-evaluated here.
int meadow_eth_conn_renew_lease_periodically(struct dhcp_info_s *dhcp_info)
{
  int ret;
  uint8_t macAddr[IFHWADDRLEN];

  DEBUGASSERT(dhcp_info != NULL);

  MEADOW_TRACE_INFORMATION("%s@%d-Renewing lease\n", thisFile, __LINE__);

  ret = meadow_eth_utils_get_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_mac ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Reconnect to DHCP server to renew lease and update dhcp_info as needed
  ret = meadow_eth_dhcp_get_dhcp_info(dhcp_info, MEADOW_ETHMAC_DEVICENAME, macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_dhcp_get_dhcp_info() ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This function is called by the ISR in the monitoring code when the LAN chip
// has detected a link status change.
// Note:This function needs execute as quickly as possible as it is part of the
// ISR execution time.
int meadow_eth_conn_link_status_changed(bool linkStatusUp)
{
  _linkStatusUp = linkStatusUp;

  // Set the action and then wakeup our thread
  _meadow_eth_conn_action = MEADOW_ETH_CONN_LINK_STATUS_CHANGED;

  (void)sem_post(&_connectSem);

  return OK;
}

#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
