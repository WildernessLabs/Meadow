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

// Uncomment the #define below to turn on debug help macros.
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

// Is Ethernet included?
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
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
static bool _prevLinkStatus;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int meadow_eth_monitor_link_status_isr(int irq, void *context, void *arg);

/****************************************************************************
 * Function Implementations
 ****************************************************************************/
// This ISR is called by LAN9355 via it's IRQ pin, for changes in PHY status.
// One interesting thing is that when the LAN9355, soon after it is initialized
// will generate the IRQ interrupt. This "feature" is used to make the initial
// connection if on Meadow startup the link status is up.
int meadow_eth_monitor_link_status_isr(int irq, void *context, void *arg)
{
  int ret;
  uint16_t temp16;
  bool currStatusPhy1;
  bool currStatusPhy2;
  bool linkNowUp = false;
 
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

  // Use to verify that LAN9355 IRQ is being generated and getting this far
  // syslog(LOG_MDIAG,
  //   "%s@%d-mon-ISR-Link status PREV Link Status:%s, CURR PHY A:%s, CURR PHY B:%s\n",
  //   thisFile, __LINE__,
  //   _prevLinkStatus == 0 ? "Down" : "Up",
  //   currStatusPhy1  == 0 ? "Down" : "Up",
  //   currStatusPhy2  == 0 ? "Down" : "Up");

  // Note: We combine the 2 PHY Link Status values into single combined link
  // status. This is because, Nuttx and Meadow only support 1 link status.
  if(currStatusPhy1 || currStatusPhy2)
    linkNowUp = true;

  // Did the combined link status change?
  if(linkNowUp == _prevLinkStatus)
  {
    return OK; // No combined status change
  }

  // Save link status for next time
  _prevLinkStatus = linkNowUp;

  // Link status has transitioned. Let the thread in meadow_ethnet_connect.c
  // finish the work of possible making a connection attempt.
  // Note:This call only post to a semaphore.
  ret = meadow_eth_conn_link_status_changed(linkNowUp);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Re-creating connection failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

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

#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
