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

// This module contains code to monitor the state of the Ethernet connections.

// The LAN9355 is the only ethernet chip currently (June2023) used with Meadow
// for ethernet connectivity.
// The code in this module is LAN9355 specific and when another lan chip is
// supported, probably an new module will be need to support the new chip.
// Currently, this implementation checks if the LAN chip is the LAN9355 and if
// not will not run.

// At this time Nuttx has no specific support for the LAN9355. Howerver, it 
// was found that LAN8742A configuration would also support the LAN9355 for
// basic operation.

// The LAN9355 contains a switch with 3 ports. One is used by the OS and the
// other 2 ports for the ethernet connections. This difference needs to be kept
// in mind while working with this chip.

// Parts of this module orginally taken from nuttx 7.x
// /apps/nshlib/nsh_netinit.c.

#warning "(--) Peter is here"

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
#include <nuttx/wqueue.h>

#include "stm32_ethernet.h"
#include "meadow_ethnet_local.h"
#include <meadow/meadow_ethnet_common.h>
#include "../hcom_nx/hcom_nx_config_manager.h"
#include "../ntpclient/ntpclient.h"
#include "../espcp/espcp_common.h"

#ifndef CONFIG_SCHED_HPWORK
#error "meadow_ethnet_monitor requires CONFIG_SCHED_HPWORK"
#endif

// Uncomment the #define below to turn on debug help macros.
#define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

// Is Ethernet included?
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
// (--) FIX NAME TO REFLECT H/W SUPOPORTED?
// PH14 is wired to LAN9355's ETH_IRQ LINE. The LAN9355's ETH_IRQ_LINE is
// configured for push-pull operation
#define MEADOW_ETH_LAN9355_IRQ_PIN (GPIO_INPUT | GPIO_FLOAT | GPIO_PORTH | GPIO_PIN14)

// R/O register that indicate the interrupt source of the PHY interrupts.
// A read will clear the bits in this register (9.2.20.20)
// Needed in ISR to deterine if the interrupt was link up, link down or
// neither.
#define LAN9355_PHY_INTERRUPT_SOURCE (29)   // 16-bit register

// R/W register used to enable or mask the PHY interrupts (9.2.20.21 -
// PHY_INTERRUPT_MASK_x)
// Needed to enable link up (bit 9) & link down (bit 4) interrupts.
// Used in conjunction with 'INT_EN' to enable PHY A & B
#define LAN9355_PHY_INTERRUPT_MASK (30)     // 16-bit register

// IRQ configuration register (8.3.1 - IRQ_CFG)
// Needed to enable and set the IRQ line's behavior
#define LAN9355_PHY_INTERRUPT_IRQ_CFG (0x54)      // 32-bit register
// IRQ status register (8.3.2 - INT_STS)
// Needed to determine the source PHY A (bit 26) or B (bit 27)
#define LAN9355_PHY_INTERRUPT_INT_STS (0x58)      // 32-bit register
// IRQ enable/mask register (8.3.3 - INT_EN)
// Needed to enable interrupts for PHY A (bit 26) & B (bit 27)
#define LAN9355_PHY_INTERRUPT_INT_EN  (0x5c)      // 32-bit register

// A single 32-bit value contains the current status information.
#define WORKER_HAVE_CURRENT_LINK_STATUS (0xffff0000)
#define WORKER_STATUS_PHY_1_CURR_MASK   (0x00000001)
#define WORKER_STATUS_PHY_2_CURR_MASK   (0x00000002)

#define MEADOW_ETHNET_DHCP_RETRY_COUNT (3)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _prevLinkStatus;
static struct dhcp_info_s *_dhcp_info;
static bool _configUseDhcp;
static uint32_t _configStaticIpAddr;
static uint32_t _configStaticIpMask;
static uint32_t _configStaticGateWay;

// Note: Each work_s struct can support one queued worker. If, while one worker
// is waiting to be run another call to work_queue is made with the same work_s
// the first invocation is over-written by the second.
static struct work_s _eth_mon_work_q_struct;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int meadow_eth_monitor_link_status_isr(int irq, void *context, void *arg);
static void meadow_eth_monitor_worker(void *arg);
static int meadow_eth_mon_report_link_status_change(bool isLinkUp);
static int meadow_eth_mon_establish_connection(struct dhcp_info_s *dhcp_info,
          uint8_t *macAddr);
static int meadow_eth_mon_initiate_connection(void);

/****************************************************************************
 * Function Implementations
 ****************************************************************************/
// This ISR is called by LAN9355 via it's IRQ pin, for changes in PHY status.
// One interesting thing is that when the LAN9355, soon after it is initialized
// will generate the IRQ interrupt. This "feature" is used to make the initial
// connection if on Meadow startup the link status is up.
int meadow_eth_monitor_link_status_isr(int irq, void *context, void *arg)
{
  uint16_t temp16;
  bool currStatusPhy1;
  bool currStatusPhy2;
 
  syslog(1, "%s@%d--------------------------------------------------\n", thisFile, __LINE__);

  // Nuttx has already acknowledged the GPIO interrupt that generated this
  // call. But, the LAN9355 requiries 2 register reads for each PHY.
  // In this register PHY A bit 9 link up, bit 4 link down.
  (void) meadow_lan9355_phyread_16(1, LAN9355_PHY_INTERRUPT_SOURCE, &temp16);
  (void) meadow_lan9355_phyread_16(2, LAN9355_PHY_INTERRUPT_SOURCE, &temp16);

  // Now clear the link status registers
  (void) meadow_lan9355_phyread_16(1, MII_MSR, &temp16);
  currStatusPhy1 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  (void) meadow_lan9355_phyread_16(2, MII_MSR, &temp16);
  currStatusPhy2 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  // Combine status values for worker thread to use.
  uint32_t phyStatus = WORKER_HAVE_CURRENT_LINK_STATUS;
  if(currStatusPhy1) phyStatus |= WORKER_STATUS_PHY_1_CURR_MASK;
  if(currStatusPhy2) phyStatus |= WORKER_STATUS_PHY_2_CURR_MASK;

  // TESTING
  syslog(1, "%s@%d-mon-ISR-Link status PREV Link Status:%s, CURR PHY A:%s, CURR PHY B:%s\n",
          thisFile, __LINE__,
          _prevLinkStatus == 0 ? "Down" : "Up",
          currStatusPhy1  == 0 ? "Down" : "Up",
          currStatusPhy2  == 0 ? "Down" : "Up");
  // TESTING

  // Cannot requeue if there is currently an active worker. If we do the work
  // queue system gets confused and all future work_queue calls don't work.
  //
  // Is this queue currently in use? If it is we must ignore the interrupt
  // otherwise there will be the likelihood of corrupting the queue. The code
  // in the current work queue (Nuttx version 7.3) the worker element is set
  // to NULL as soon as the work has been selected to run.
  // (--) IS THERE A BETTER WAY TO DETECT THIS?????
  if(_eth_mon_work_q_struct.worker != NULL)
  {
    syslog(1, "%s@%d-mon-ISR-IGNORING interrupt because already queued.\n", thisFile, __LINE__);
    return OK;
  }

  syslog(1, "%s@%d-mon-ISR-Queuing worker thread to finish\n", thisFile, __LINE__);

  // Queue the worker and report the current status
  memset(&_eth_mon_work_q_struct, 0, sizeof (struct work_s));
  work_queue(HPWORK, &_eth_mon_work_q_struct, meadow_eth_monitor_worker,
            (void *)phyStatus, 0);

  return OK;
}

//=============================================================
// This function is called via the work queue, and only from the ISR.
// It assumes that the global _prevLinkStatus is set correctly in the arg
// argument passed in.
static void meadow_eth_monitor_worker(void *arg)
{
  int ret;
  bool currStatusPhy1;
  bool currStatusPhy2;
  bool linkNowUp = false;
  uint32_t providedStatus;

  syslog(1, "%s@%d-MonWorker-entry\n", thisFile, __LINE__);

  if(arg == NULL)
  {
    syslog(LOG_ERR, "%s@%d-MonWorker-called with arg == NULL\n", thisFile, __LINE__);
    return;
  }

  providedStatus = (uint32_t)arg;
  DEBUGASSERT((providedStatus & WORKER_HAVE_CURRENT_LINK_STATUS) != 0);

  // Get the current link status which must be provided by caller
  currStatusPhy1 = (providedStatus & WORKER_STATUS_PHY_1_CURR_MASK) != 0;
  currStatusPhy2 = (providedStatus & WORKER_STATUS_PHY_2_CURR_MASK) != 0;

  // Note: We combine the 2 PHY Link Status values into 1 combined link status.
  // This is because, Nuttx and Meadow only support 1 link status.
  if(currStatusPhy1 || currStatusPhy2)
    linkNowUp = true;

  // Did the combined link status change?
  if(linkNowUp == _prevLinkStatus)
  {
    syslog(1, "%s@%d-MonWorker-Combined link status, no change(it's:%d)\n", thisFile, __LINE__, linkNowUp);
    return; // No
  }

  // Save link status for next time
  _prevLinkStatus = linkNowUp;

  syslog(1, "%s@%d-MonWorker-calling meadow_eth_mon_report_link_status_change()\n", thisFile, __LINE__);

  // Report new link status to Nuttx and Meadow
  ret = meadow_eth_mon_report_link_status_change(linkNowUp);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_mon_report_link_status_change, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return;
  }
  // syslog(1, "%s@%d-MonWorker-meadow_eth_mon_report_link_status_change() returned\n", thisFile, __LINE__);

  if(linkNowUp)
  {
    syslog(1, "%s@%d-MonWorker-Processing LinkStatus now up\n", thisFile, __LINE__);
    syslog(1, "%s@%d-MonWorker-Calling meadow_eth_mon_initiate_connection()\n", thisFile, __LINE__);

    // Link status has transitioned from down to up, lets attempt to make a connection
    ret = meadow_eth_mon_initiate_connection();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Re-creating connection failed, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      return;
    }
    // syslog(1, "%s@%d-MonWorker-meadow_eth_mon_initiate_connection() returned\n", thisFile, __LINE__);

    // Determine if we should get the NTP time
    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    uint32_t refreshPeriod = config->ntp_refresh_period_seconds;
    bool timeAtStart = config->get_network_time_at_startup;
    hcom_nx_config_unlock();

    // Get the time if appropriate
    if(refreshPeriod > 0 || timeAtStart)
    {
      ntpc_start();
    }
  }
  else
  {
    // syslog(1, "%s@%d-MonWorker-Link Down processing\n", thisFile, __LINE__);
    // We need to cancel the lease renewal
    ret = meadow_eth_dhcp_cancel_lease_renewal();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Cancelling lease renewal failed, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
    }

    // Stop the reoccurring NTP time request
    ntpc_stop();
  }

  syslog(1, "%s@%d-MonWorker-EXITING\n", thisFile, __LINE__);
}

//====================================================================
// Report to Nuttx and meadow the change in link status
int meadow_eth_mon_report_link_status_change(bool isLinkUp)
{
  int ret;
  int sockDescp;
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, MEADOW_ETHMAC_DEVICENAME, IFNAMSIZ);

  // Past new link status to Meadow
  espcp_queue_ethernet_connection_changed_event(isLinkUp);

  // Keep Nuttx informed too
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

  return OK;
}

//=============================================================
// Called at startup. This function will do one time setup and configure the
// LAN9355 to generate an interrupt and route it to the LAN9355's IRQ pin.
int meadow_eth_mon_config_lan9355_irq()
{
  int ret;
  uint32_t meadow_eth_interrupt_status_reg = 0;
  uint16_t regVal16 = 0;

  _dhcp_info = malloc(sizeof(struct dhcp_info_s));
  if(_dhcp_info == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Get the configuration information
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

  // This is the full set of our items
  // Bit 0 - 1 IRQ buffer type 1 = push-pull (0 = open drain)
  // Bit 4 - 1 set the polarity high on interrupt
  // Bit 8 - 1 IRQ Pin Enable 1 = enable
  meadow_eth_interrupt_status_reg = 0x00000111;

  ret = meadow_lan9355_phywrite_32(LAN9355_PHY_INTERRUPT_IRQ_CFG, meadow_eth_interrupt_status_reg);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Writing IRQ_CFG failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Configure the interrupts sources. We want to monitor PHY A and PHY B.
  // This is what enables the specified interrupt to output on the IRQ pin of
  // the LAN9355 chip.
  // Bits 26 (PHY A) and 27 (PHY B) need to be set
  meadow_eth_interrupt_status_reg = 0x0c000000;

  ret = meadow_lan9355_phywrite_32(LAN9355_PHY_INTERRUPT_INT_EN, meadow_eth_interrupt_status_reg);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Writing INT_EN failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Need to configure the 16-bit LAN9355_PHY_INTERRUPT_MASK (30) to allow
  // link up and link down occurrences to trigger the interrupt for both PHYs.
  // Note: If only link up or link down enabled then no IRQ interrupt is
  // generated when the other happens (e.g. if only link up is enabled then no
  // IRQ interrupt is generated when the link goes down).
  // Bit 9 (link up) and Bit 4 (link down).
  regVal16 = 0x0210;

  // PHY 1
  ret = meadow_lan9355_phywrite_16(1, LAN9355_PHY_INTERRUPT_MASK, regVal16);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Writing INT_EN failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }
  
  // PHY 2
  ret = meadow_lan9355_phywrite_16(2, LAN9355_PHY_INTERRUPT_MASK, regVal16);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Writing INT_EN failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Clear any pending interrupts
  (void) meadow_lan9355_phyread_16(1, LAN9355_PHY_INTERRUPT_SOURCE, &regVal16);
  (void) meadow_lan9355_phyread_16(2, LAN9355_PHY_INTERRUPT_SOURCE, &regVal16);

  // Reading will clear any unexpected interrupts sources.
  (void) meadow_lan9355_phyread_16(1, MII_MSR, &regVal16);
  bool statusPhy1 = (regVal16 & MII_MSR_LINKSTATUS) != 0;
  (void) meadow_lan9355_phyread_16(2, MII_MSR, &regVal16);
  bool statusPhy2 = (regVal16 & MII_MSR_LINKSTATUS) != 0;

  // Save the initial link status state
  _prevLinkStatus = (statusPhy1 || statusPhy2);

  // Configure the GPIO connected to the LAN9355
  stm32_configgpio(MEADOW_ETH_LAN9355_IRQ_PIN);

  // This call makes irq_attach() and up_enable_irq() calls internally.
  // In our case rising edge means link status up and falling edge link
  // status down
  ret = stm32_gpiosetevent(
  MEADOW_ETH_LAN9355_IRQ_PIN,         // Nuttx cfgset
  1,                                  // Rising Edge,
  0,                                  // Falling Edge,
  0,                                  // Event
  meadow_eth_monitor_link_status_isr, // ISR
  NULL);

  return ret;
}

//========================================================================
// This function is called to initialize and connect the ethernet. This
// function can take some time to complete.
int meadow_eth_mon_establish_connection(struct dhcp_info_s *dhcp_info,
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
      // This call will populate the dhcp_info structure with: IP, netmask, DNS
      // server address, default router address and the lease expiration time,
      // from information from the DHCP server.
      ret = meadow_eth_dhcp_get_device_ip_info(dhcp_info, MEADOW_ETHMAC_DEVICENAME, macAddr);
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

//===========================================================================
// This function called a connection needs to be established
int meadow_eth_mon_initiate_connection()
{
  int ret;
  uint8_t macAddr[IFHWADDRLEN];
  
  memset(_dhcp_info, 0, sizeof(struct dhcp_info_s));

  ret = meadow_eth_utils_get_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_mac ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return -errno;
  }

  // (--) THIS FUNCTION IS ONLY CALLED ONCE, FROM HERE SHOULD IT BE A SEPARATE FUNCTION?
  // This function does all the heavy lifting of establishing a connection.
  // It also gets and saves all the needed network information in dhcp_info.
  ret = meadow_eth_mon_establish_connection(_dhcp_info, macAddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "Attempting to establish ethernet connection failed. ret:%d, errno:%d\n",
              ret, errno);
    return ret;
  }

  // Report via syslog user that ethernet is up etc.
  meadow_eth_utils_syslog_ip_mac();

  // If using DHCP for our ip address then initialize lease renewal process
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


#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
