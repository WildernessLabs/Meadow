/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/ethernet/meadow_ethnet_monitor.c
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

// This module contains code to monitor the state of the Ethernet
// Parts of this module orginally were taken from nuttx 7.x at
// /apps/nshlib/nsh_netinit.c. In nuttx 10 this was found at 
// /apps/netutils/netinit/netinit.c. A single change was included from 10

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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Is Ethernet included?
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

#define MEADOW_ETHNET_DHCP_CONNECTION_RETRY_COUNT (3)

// Set some arbitrary large value since without a dhcp connection there
// won't be any work.
// (--) 120 for testing, should be much larger
#define MEADOW_ETHNET_LEASE_TIME_DEFAULT_SEC (120)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;
static struct dhcp_info_s *_dhcp_info;

static int _meadow_ethnet_connect_kthrd;
static sem_t _connectSem;
static bool _configUseDhcp;
static uint32_t _configStaticIpAddr;
static uint32_t _configStaticIpMask;
static uint32_t _configStaticGateWay;
static bool _linkStatusUp;
static bool _isConnectionValid;

static enum 
{
  MEADOW_ETH_CONN_LINK_STATUS_CHANGED,
  MEADOW_ETH_CONN_LEASE_RENEWAL_TIME
} _meadow_eth_conn_action;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void *meadow_ethnet_connect_kthread(int argc, char *argv[]);
static int meadow_eth_conn_establish_connection(struct dhcp_info_s *dhcp_info,
          uint8_t *macAddr);
static int meadow_eth_conn_cancel_lease_renewal(void);
static int meadow_eth_conn_init_lease_renewal(uint32_t leaseTime);
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

  // Create a thread to do ethernet establish connections and handle lease
  // renewal
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
// This thread is used to make Ethernet connections and renew the DHCP lease.
void *meadow_ethnet_connect_kthread(int argc, char *argv[])
{
  int ret;
  time_t leaseTimeSec = 0;

// (--)
// #if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_THREAD_NAME_ETHNET_CONNECTION);
// #endif

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

  // Get the configuration information
  hcom_nx_config_lock();
  meadow_configuration_t *config = hcom_nx_config_get_pointer();
  _configUseDhcp = config->default_interface->use_dhcp == TRUE ? true : false;

  // If not using DHCP the connection information is still required.
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

  // Enter a forever loop that waits for the connect semaphore.
  for(;;)
  {
    struct timespec waketime;

    // Do we have a connection that needs lease renewal?
    if(_isConnectionValid && _configUseDhcp)
    {
      // Possibly set or update the lease time
      leaseTimeSec = _dhcp_info->lease_time;
    }
    else
    {
      leaseTimeSec = MEADOW_ETHNET_LEASE_TIME_DEFAULT_SEC;
    }

    // Wait for either the semaphore to be posted or a time-out to occur
    sched_lock();
    ret = clock_gettime(CLOCK_REALTIME, &waketime);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-clock_gettime, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      return NULL;
    }

    waketime.tv_sec += leaseTimeSec;
    
    syslog(1, "%s@%d-Waiting for SEMAPHORE\n", thisFile, __LINE__);
    // Wait for lease time to expire or the link status to change
    ret = sem_timedwait(&_connectSem, &waketime);
    sched_unlock();
    
    syslog(1, "%s@%d-Semaphore released. Work to do.\n", thisFile, __LINE__);

    if(ret < 0) 
    {
      if (ret == -ETIMEDOUT)
      {
        if(! _configUseDhcp)
          continue;       // Nothing to do

        syslog(LOG_ERR, "%s@%d-Timeout of sem_timedwait. Time to Renew Lease\n");
        _meadow_eth_conn_action = MEADOW_ETH_CONN_LEASE_RENEWAL_TIME;
      }
      else
      {
        // Error
        syslog(LOG_ERR, "%s@%d-sem_timedwait() Failed, ret:%d, errno:%d\n",
                    thisFile, __LINE__, ret, errno);
      }
    }

    // Do the requested work, then return and continue to wait for the next event.
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
// This function is called when the LAN chips has detected a link status
// change. It is also called very early at startup it seems to be during just
// after the lan chip is reset. Maybe, as soon as the interrupts are
// configured.
int meadow_eth_conn_link_status_changed(bool linkStatusUp)
{
  int ret;
  int semcount;
  _linkStatusUp = linkStatusUp;

  _meadow_eth_conn_action = MEADOW_ETH_CONN_LINK_STATUS_CHANGED;

  syslog(1, "%s@%d-EARLEST Indication of Status Change, it is now %s\n",
            thisFile, __LINE__, linkStatusUp ? "Up" : "Down");

  // What is the count on the semaphore?
  ret = sem_getvalue(&_connectSem, &semcount);
  // syslog(LOG_INFO, "Entry: semcount=%d\n", semcount);

  // Don't over-post
  if (ret == OK && semcount <= 0)
  {
    syslog(1, "%s@%d-Posting to Semaphore because status change.\n", thisFile, __LINE__);
    ret = sem_post(&_connectSem);
  }
  else
  {
    syslog(1, "%s@%d-Posting to Semaphore FAILED for status change.\n", thisFile, __LINE__);
  }

  return OK;
}

//===========================================================================
// This function will be called when the lease renewal period has expired
int meadow_eth_conn_thread_do_work(void)
{
  int ret;
  
  switch(_meadow_eth_conn_action)
  {
    case MEADOW_ETH_CONN_LINK_STATUS_CHANGED:
    syslog(1, "%s@%d-LINK_STATUS_CHANGED\n", thisFile, __LINE__);
    ret = meadow_eth_conn_process_link_status_change(_linkStatusUp);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-switch default value:%d\n",
                  thisFile, __LINE__, _meadow_eth_conn_action);
    }
    break;

    case MEADOW_ETH_CONN_LEASE_RENEWAL_TIME:
    syslog(1, "%s@%d-MEADOW_ETH_CONN_LEASE_RENEWAL_TIME\n", thisFile, __LINE__);
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

    default:
    syslog(LOG_ERR, "%s@%d-Unknown switch value:%d\n",
                thisFile, __LINE__, _meadow_eth_conn_action);
  }

  return OK;
}

//===========================================================================
// This function is called when the connections link status has changed.
int meadow_eth_conn_process_link_status_change(bool linkStatusUp)
{
  int ret;
  uint8_t macAddr[IFHWADDRLEN];
  
  syslog(1, "%s@%d-Processing LinkStatus change. Now %s\n", thisFile, __LINE__, _linkStatusUp ? "Up" : "Down");

  // Did the link status transition from down to up?
  if(linkStatusUp)
  {
    syslog(1, "%s@%d-EthConn-Link Up processing\n", thisFile, __LINE__);

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
    // (--) Is this needed? Does it hurt anything?
    // memset(_dhcp_info, 0, sizeof(struct dhcp_info_s));
    ret = meadow_eth_conn_establish_connection(_dhcp_info, macAddr);
    if(ret < 0)
    {
      syslog(LOG_ERR, "Attempting to establish ethernet connection failed. ret:%d, errno:%d\n",
                ret, errno);
      return ret;
    }

    // We now have an ethernet connection.
    syslog(1, "%s@%d-We now have an ethernet connection\n", thisFile, __LINE__);
    _isConnectionValid = true;

    // Show via syslog user that ethernet is up etc.
    meadow_eth_utils_syslog_ip_mac();

// NTP
    // // Determine if we should get the NTP time.
    // hcom_nx_config_lock();
    // meadow_configuration_t *config = hcom_nx_config_get_pointer();
    // uint32_t refreshPeriod = config->ntp_refresh_period_seconds;
    // bool timeAtStart = config->get_network_time_at_startup;
    // hcom_nx_config_unlock();
    // if(refreshPeriod > 0 || timeAtStart)
    // {
    //   syslog(1, "%s@%d-Getting the NTP time\n", thisFile, __LINE__);
    //   // This call will cause the ntpclient.c code to periodically refresh the
    //   // NTP time without additional intervention.
    //   ntpc_start();
    // }

// LEASE RENEWAl
    // // If using DHCP for our ip address then initialize lease renewal process
    // if(_configUseDhcp)
    // {
    //   ret = meadow_eth_conn_init_lease_renewal(_dhcp_info->lease_time);
    //   if(ret < 0)
    //   {
    //     syslog(LOG_ERR, "Init lease renewal failed. ret:%d, errno:%d\n",
    //               ret, errno);
    //   }
    // }

    // Now that everything is ready, report that the status is up
    ret = meadow_eth_conn_report_link_status_change(true);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_conn_report_link_status_up, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      return ret;
    }
  }
  else
  {
    // Report status as down asap
    ret = meadow_eth_conn_report_link_status_change(false);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_conn_report_link_status_up, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      return ret;
    }

    // Link status transitioned to down
    syslog(1, "%s@%d-EthConn-Link Down processing\n", thisFile, __LINE__);

    // LEASE RENEWAL
    // We need to cancel the lease renewal
    ret = meadow_eth_conn_cancel_lease_renewal();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Cancelling lease renewal failed, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
    }

// NTP
    // Stop the reoccurring NTP time request
    // ntpc_stop();
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
      // from information from the DHCP server.
      ret = meadow_eth_dhcp_get_dhcp_info(dhcp_info, MEADOW_ETHMAC_DEVICENAME, macAddr);
      if(ret < 0)
      {
        if (errno == EAGAIN)
        {
          continue;   // Try again since socket timed out this time
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

// // (--) EXPERIMENT SET THE PREVIOUS ADDRESS TO 0.0.0.0
// // (--) THIS MADE NO DIFFERENCE
//   if(!isLinkUp)
//   {
//     memset((void *)&ifr.ifr_ifru.ifru_addr, 0, sizeof(struct sockaddr));

//     ret = ioctl(sockDescp, SIOCSIFADDR, (unsigned long)&ifr);
//     if (ret < 0)
//     {
//       syslog(LOG_ERR, "%s@%d-ioctl(SIOCSIFFLAGS) failed, ret:%d, errno:%d\n",
//                   thisFile, __LINE__, ret, errno);
//       return ret;
//     }
//   }
// // (--) END OF EXPERIMENT
  close(sockDescp);

  // Past new link status to Meadow
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

  syslog(1, "%s@%d-Renewing lease\n", thisFile, __LINE__);

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

//==============================================================================
// This function is called when the link status has been lost. It will remove
// the queued call to renew the lease.
// (--) MOVE THIS TO connect
int meadow_eth_conn_cancel_lease_renewal()
{
  // int ret;

  // Only need to cancel if there's a worker. If not tested then will get error
  // from work_cancel.
  // if(_dhcp_work_q_struct.worker == NULL)
  //   return OK;

  // ret = work_cancel(LPWORK, &_dhcp_work_q_struct);
  // if(ret < 0)
  // {
  //   syslog(LOG_ERR, "%s@%d-meadow_eth_cancel_dhcp_lease_renewal ret:%d, errno:%d\n",
  //             thisFile, __LINE__, ret, errno);
  //   return -errno;
  // }
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This function is called from ethernet monitor after a connection has been
// established. It will setup the low priority worker queue to renew the lease
// periodically after the correct amount of time.
int meadow_eth_conn_init_lease_renewal(uint32_t leaseTime)
{
  // int ret;

  syslog(1, "%s@%d-LEASE RENEWAL PERIOD IS:%d seconds\n", thisFile, __LINE__, leaseTime);

  // // Queue the dhcp lease renewal to start periodic execution
  // memset(&_dhcp_work_q_struct, 0, sizeof(struct work_s));
  // ret = work_queue(LPWORK, &_dhcp_work_q_struct,
  //           meadow_eth_conn_renew_lease_periodically, (void*)dhcp_info,
  //           ((leaseTime/2) * 1000)/MSEC_PER_TICK);
  // if(ret < 0)
  // {
  //   syslog(LOG_ERR, "%s@%d-meadow_eth_conn_renew_lease_periodically ret:%d, errno:%d\n",
  //             thisFile, __LINE__, ret, errno);
  //   return -errno;
  // }
  return OK;
}

// //=============================================================
// // This public function is called when:
// // 1. an ethernet connection needs to be made.
// // 2. the dhcp lease needs to be renewed.
// // A lease time of 0 means attempt a connection now, any other value specifies
// // a reoccurring lease time.
// void meadow_eth_connect_run(uint32_t leaseTime)
// {
//   int semcount;
//   int ret;

//   // What is the count on the semaphore?  Don't over-post
//   ret = sem_getvalue(&_connectSem, &semcount);
//   // syslog(LOG_INFO, "Entry: semcount=%d\n", semcount);

//   if (ret == OK && semcount <= 0)
//   {
//     sem_post(&_connectSem);
//   }
// }


#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
