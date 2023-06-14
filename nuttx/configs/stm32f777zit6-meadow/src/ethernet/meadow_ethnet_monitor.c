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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Is Ethernet included?
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

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

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;
static bool _prevStatusPhy1;    // Last know status for Ethernet link #1
static bool _prevStatusPhy2;    // Last know status for Ethernet link #2

// Note: Each work_s struct can support one queued worker. If, while one worker
// is waiting to be run another call to work_queue is made with the same work_s
// the first invocation is over-written by the second.
static struct work_s _eth_mon_work_q_struct;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int meadow_eth_monitor_link_status_isr(int irq, void *context, void *arg);
static void meadow_eth_monitor_worker(void *arg);
static int meadow_eth_config_lan9355_irq(void);
static int meadow_eth_mon_report_link_status_change(bool isLinkUp);

/****************************************************************************
 * Function Implementations
 ****************************************************************************/
// Called at startup after attempting to make ethernet connection
int meadow_eth_monitor_startup(void)
{
  int ret;

  _prevStatusPhy1 = false;
  _prevStatusPhy2 = false;

  // Configure the LAN9355 to generate an interrupts when there's a
  // change in either PHY link status.
  ret = meadow_eth_config_lan9355_irq();
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_config_lan9355_irq failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // There's no need to call the worker thread here because the caller of this
  // function has already checked the startup status and set _prevStatusPhy1 and
  // _prevStatusPhy2 values apropriately.
  // Now that the LAN9355 interrupts are configured everything should be automatic.
  return OK;
}

//=============================================================
// This ISR is called by LAN9355, via it's IRQ pin, for changes in PHY status.
int meadow_eth_monitor_link_status_isr(int irq, void *context, void *arg)
{
  uint16_t temp16;
  bool currStatusPhy1;
  bool currStatusPhy2;
 
  syslog(1, "--------------------------------------------------\n");
  syslog(1, "mon-isr-Eth monitor's ISR received interrupt\n");

  // Nuttx has already acknowledged the GPIO interrupt that generated this
  // call. But, the LAN9355 requiries 2 register reads for each PHY.
  // In this register PHY A bit 9 link up, bit 4 link down.
  (void) meadow_eth_phyread_16(1, LAN9355_PHY_INTERRUPT_SOURCE, &temp16);
  (void) meadow_eth_phyread_16(2, LAN9355_PHY_INTERRUPT_SOURCE, &temp16);

  // Now clear the link status registers
  (void) meadow_eth_phyread_16(1, MII_MSR, &temp16);
  currStatusPhy1 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  (void) meadow_eth_phyread_16(2, MII_MSR, &temp16);
  currStatusPhy2 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  // Combine status values just read into a value for worker thread to use.
  // Allocating a struct has risks because if another work item is
  // queued using the same 'struct work_s', the memory would be leaked.
  uint32_t phyStatus = WORKER_HAVE_CURRENT_LINK_STATUS;
  if(currStatusPhy1) phyStatus |= WORKER_STATUS_PHY_1_CURR_MASK;
  if(currStatusPhy2) phyStatus |= WORKER_STATUS_PHY_2_CURR_MASK;

  // TESTING
  syslog(1, "mon-isr-Link    status PREV PHY A:%s, CURR PHY A:%s, PREV PHY B:%s, CURR PHY B:%s\n",
          _prevStatusPhy1 == 0 ? "Down" : "Up",
          currStatusPhy1  == 0 ? "Down" : "Up",
          _prevStatusPhy2 == 0 ? "Down" : "Up",
          currStatusPhy2  == 0 ? "Down" : "Up");
  // TESTING

  // Don't waste time if nothing to do
  if((_prevStatusPhy1 == currStatusPhy1) && (_prevStatusPhy2 == currStatusPhy2))
  {
    syslog(1, "mon-isr-No status change. Early Exit (no work queue needed)\n");
    return OK;
  }

  // Queue the worker and report the current status
  work_queue(HPWORK, &_eth_mon_work_q_struct, meadow_eth_monitor_worker,
            (void *)phyStatus, 0);

  return OK;
}

//=============================================================
// This function is called via the work queue, and only from the ISR.
// It assumes that the global values '_prevStatusPhy1' and '_prevStatusPhy2'
// are correctly set.
// Note: On startup there is other functionality that set the initial status
// values (_prevStatusPhy1 and _prevStatusPhy2).
void meadow_eth_monitor_worker(void *arg)
{
  int ret;
  bool currStatusPhy1;
  bool currStatusPhy2;
  bool linkNowUp = false;
  bool linkWasUp = false;
  uint32_t providedStatus;

  syslog(1, "***Eth monitor worker entry\n");

  if(arg == NULL)
  {
    syslog(LOG_ERR, "meadow_eth_monitor_worker() called with NULL\n");
    return;
  }

  providedStatus = (uint32_t)arg;
  DEBUGASSERT((providedStatus & WORKER_HAVE_CURRENT_LINK_STATUS) != 0);

  // Get the current link status if not provided by caller
  currStatusPhy1 = (providedStatus & WORKER_STATUS_PHY_1_CURR_MASK) != 0;
  currStatusPhy2 = (providedStatus & WORKER_STATUS_PHY_2_CURR_MASK) != 0;

  // Note: We combine the 2 PHY Link Status values into 1 combined link status.
  // This is because, Nuttx and Meadow only support 1 link status.
  if(currStatusPhy1 || currStatusPhy2)
    linkNowUp = true;

  // Were either link up before?
  if(_prevStatusPhy1 || _prevStatusPhy2)
    linkWasUp = true;

  // Keep the individual status values for the next invocation.
  _prevStatusPhy1 = currStatusPhy1;
  _prevStatusPhy2 = currStatusPhy2;

  // Did the combined link status change?
  if(linkNowUp == linkWasUp)
  {
    syslog(1, "Combined link status did not change\n");
    return; // No
  }

  // Report new link status to Nuttx and Meadow
  ret = meadow_eth_mon_report_link_status_change(linkNowUp);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_mon_report_link_status_change, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return;
  }

  if(linkNowUp)
  {
    syslog(1, "***Eth monitor worker-combined link up processing\n");

    // Link status has transitioned from down to up
    ret = meadow_eth_start_re_establish_connection();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Re-creating connection failed, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);

      return;
    }

    // Determine if we should get the time
    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    uint32_t refreshPeriod = config->ntp_refresh_period_seconds;
    bool timeAtStart = config->get_network_time_at_startup;
    hcom_nx_config_unlock();

    // Get the time if appropriate
    if(refreshPeriod > 0 || timeAtStart)
      ntpc_start();
  }
  else
  {
    syslog(1, "***Eth monitor worker-combined link down processing\n");
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

  syslog(1, "***Eth monitor worker EOF EXIT\n");
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

  if(isLinkUp)
  {
    espcp_queue_ethernet_connection_changed_event(true);
    ifr.ifr_flags = IFF_UP;
  }
  else
  {
    espcp_queue_ethernet_connection_changed_event(false);
    ifr.ifr_flags = IFF_DOWN;
  }

  // Get a socket descriptor that we can use to communicate with the network
  // interface driver.
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

  if(sockDescp > -1)
    close(sockDescp);

  return OK;
}

//=============================================================
// This function will configure the LAN9355 to generate an interrutp and route
// it to the IRQ pin of the chip.
int meadow_eth_config_lan9355_irq()
{
  int ret;
  uint32_t meadow_eth_interrupt_status_reg = 0;
  uint16_t regVal16 = 0;

  // This is the full set of our items
  // Bit 0 - IRQ buffer type 1 = push-pull, 0 = open drain
  // Bit 4 - set the polarity can't be used with open drain
  // Bit 8 - IRQ Enable 1 = enable
  meadow_eth_interrupt_status_reg = 0x00000111;

  ret = meadow_eth_phywrite_32(LAN9355_PHY_INTERRUPT_IRQ_CFG, meadow_eth_interrupt_status_reg);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Writing IRQ_CFG failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Configure the interrupts sources. We want to monitor PHY A and PHY B.
  // This is what enables the specified interrupt to output on the IRQ pin of
  // the LAN9355 chip.
  meadow_eth_interrupt_status_reg = 0x0c000000;  // Bits 26 (PHY A) and 27 (PHY B) need to be set

  ret = meadow_eth_phywrite_32(LAN9355_PHY_INTERRUPT_INT_EN, meadow_eth_interrupt_status_reg);
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
  regVal16 = 0x0210;    // Bit 9 (link up) and Bit 4 (link down).

  // This must be done for both PHYs
  ret = meadow_eth_phywrite_16(1, LAN9355_PHY_INTERRUPT_MASK, regVal16);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Writing INT_EN failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }
  
  ret = meadow_eth_phywrite_16(2, LAN9355_PHY_INTERRUPT_MASK, regVal16);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Writing INT_EN failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Clear any pending interrupts
  (void) meadow_eth_phyread_16(1, LAN9355_PHY_INTERRUPT_SOURCE, &regVal16);
  (void) meadow_eth_phyread_16(2, LAN9355_PHY_INTERRUPT_SOURCE, &regVal16);

  // Reading will clear any unexpected interrupts sources and initialize the
  // previous state information
  (void) meadow_eth_phyread_16(1, MII_MSR, &regVal16);
  _prevStatusPhy1 = (regVal16 & MII_MSR_LINKSTATUS) != 0;
  (void) meadow_eth_phyread_16(2, MII_MSR, &regVal16);
  _prevStatusPhy2 = (regVal16 & MII_MSR_LINKSTATUS) != 0;

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

//=============================================================
// This function is called during startup to set the initial link status values
bool meadow_eth_mon_startup_set_status()
{
  uint16_t temp16;

  // These registers contains a single bit field, for PHY status, 1 = Link up
  // and 0 = Link down
  (void) meadow_eth_phyread_16(1, MII_MSR, &temp16);
  _prevStatusPhy1 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  (void) meadow_eth_phyread_16(2, MII_MSR, &temp16);
  _prevStatusPhy2 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  return(_prevStatusPhy1 || _prevStatusPhy2);
}

#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
