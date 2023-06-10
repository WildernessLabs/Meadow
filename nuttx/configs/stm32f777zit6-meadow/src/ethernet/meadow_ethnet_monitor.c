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

// This module contains code to monitor the state of the Ethernet.
// It is very LAN9355 specific and if another lan chip is supported this module
// might well be renamed and a new module created just for the new lan chip

// Parts of this module orginally were taken from nuttx 7.x at
// /apps/nshlib/nsh_netinit.c. Plus a single line change from Nuttx 10 was
// made.

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
#include <nuttx/kmalloc.h>

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

// Nuttx signals are defined in /nuttx/include/signal.h
// #define MEADOW_ETH_MONITOR_SIGNAL_NO        (18)

// (--) RETHINK THIS ENTIRE TIMING THING. IS IT NEEDED AT ALL???
// The first 2 defines are arbitrary and the third was the a Nuttx config
// option CONFIG_NSH_NETINIT_RETRYMSEC
#define MEADOW_ETH_MONITOR_LONG_RECHECK_SEC     (2) // FOR TESTING ALL VALUES ARE SET TO 2 SECONDS
// #define MEADOW_ETH_MONITOR_LONG_RECHECK_SEC     (60) // One minute
// #define MEADOW_ETH_MONITOR_LONG_RECHECK_SEC     (60*60) // One hour in seconds
#define MEADOW_ETH_MONITOR_SHORT_RECHECK_SEC    (2)     // 2 seconds
#define MEADOW_ETH_MONITOR_RETRY_RECHECK_SEC    (2)     // 2 seconds

// Nuttx config provides CONFIG_STM32F7_PHYADDR for a single PHY but I have
// chosen to ignore this Nuttx config value because we need more options.
#define MEADOW_ETH_MONITOR_PHY_0  (0)
#define MEADOW_ETH_MONITOR_PHY_1  (1)
#define MEADOW_ETH_MONITOR_PHY_2  (2)

// Is Ethernet included?
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

// (--) FIX NAME TO REFLECT H/W SUPOPORTED?
// PH14 is wired to LAN9355's ETH_IRQ LINE. The LAN9355's ETH_IRQ_LINE is
// configured for push-pull operation
#define MEADOW_ETH_PHY_IRQ_INPUT_PH14 (GPIO_INPUT | GPIO_FLOAT | GPIO_PORTH | GPIO_PIN14)

// (--) IS THIS NEEDED OR IS THIS ENTIRE FILE DEDICATED TO LAN9355?
#define MEADOW_ETHERNET_BUILD_FOR_USE_LAN9355         1
// The LAN9355 is the only ethernet switch currently be used with any Meadow
// that has  ethernet connectivity.
// In the future this may need a configuration option or query the chip to
// determine the proper chip at run-time. There is code in this module that
// detects the chip type and will not run if it is not a LAN9355.
// At this time Nuttx has no specific support for the LAN9355, therefore it 
// was found that LAN8742A configuration would also support the LAN9355. for
// basic operation.

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

// Needed to verify that the connected lan chip is the supported LAN9355
#define LAN9355_CHIP_ID_REVISION_REGISTER (0x50)  // 32-bit register

// IRQ configuration register (8.3.1 - IRQ_CFG)
// Needed to enable and set the IRQ line's behavior
#define LAN9355_PHY_INTERRUPT_IRQ_CFG (0x54)      // 32-bit register
// IRQ status register (8.3.2 - INT_STS)
// Needed to determine the source PHY A (bit 26) or B (bit 27)
#define LAN9355_PHY_INTERRUPT_INT_STS (0x58)      // 32-bit register
// IRQ enable/mask register (8.3.3 - INT_EN)
// Needed to enable interrupts for PHY A (bit 26) & B (bit 27)
#define LAN9355_PHY_INTERRUPT_INT_EN  (0x5c)      // 32-bit register

// For timeout of certain read/write operation that need to be delayed
#define LAN9355_PHY_READ_TIMEOUT  (0x0004ffff)
#define LAN9355_PHY_WRITE_TIMEOUT (0x0004ffff)

// Allocating memory for the worker thread to use is problematic because if
// another function in this module calls a queued worker invocation the
// pointer to the allocated memory will be lost when the OS reschedules the
// new invocation.
// Therefore, a single 32-bit value contains the current status (only
// available from the ISR) and the previous status (not know by startup).
// The following allow this information to be determined by the ISR. The LS
// bits store the status info.
#define WORKER_HAVE_CURRENT_LINK_STATUS (0xf0000000)
#define WORKER_STATUS_PHY_1_CURR_MASK   (0x00000001)
#define WORKER_STATUS_PHY_1_CURR_SHIFT  (0)
#define WORKER_STATUS_PHY_2_CURR_MASK   (0x00000002)
#define WORKER_STATUS_PHY_2_CURR_SHIFT  (1)
// #define WORKER_STATUS_PHY_1_PREV_MASK   (0x00000004)
// #define WORKER_STATUS_PHY_1_PREV_SHIFT  (2)
// #define WORKER_STATUS_PHY_2_PREV_MASK   (0x00000008)
// #define WORKER_STATUS_PHY_2_PREV_SHIFT  (3)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;
static bool _prevStatusPhy1;    // Last know status for Ethernet link #1
static bool _prevStatusPhy2;    // Last know status for Ethernet link #2
static bool _prevLinkUp;         // If ether one is up this will be true.

// Each work_s struct can support one queued worker. If, while one worker is
// waiting to be run another call to work_queue is made with the same work_s
// the first invocation will be over-written by the second.
static struct work_s _eth_mon_work_q_struct;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
// static void *meadow_eth_monitor_kthread(int argc, char *argv[]);
// static int meadow_eth_mon_check_phy_link_status(void);
static int meadow_eth_monitor_link_status_isr(int irq, void *context, void *arg);
static void meadow_eth_monitor_worker(void *arg);
static int meadow_eth_config_lan9355_irq(void);
static int meadow_eth_mon_report_link_status_change(bool isLinkUp);
static int meadow_eth_mon_notify_link_up(bool isLinkUp);

static int meadow_eth_phywrite_16(uint16_t phyAddr, uint16_t regAddr, uint16_t value);
static int meadow_eth_phyread_16(uint16_t phyAddr, uint16_t regAddr, uint16_t *value);
static int meadow_eth_phywrite_32(uint16_t csrAddr, uint32_t value);
static int meadow_eth_phyread_32(uint16_t csrAddr, uint32_t *value);

/****************************************************************************
 * Function Implementations
 ****************************************************************************/

//=============================================================
// Called at startup after attempting to make ethernet connection
int meadow_eth_monitor_startup(void)
{
  int ret;
  uint32_t lanChipId;

  _prevLinkUp = false;
  // We could read the link status from the OS but if we assume it's down we
  // can be sure Meadow will report the link up.
  _prevStatusPhy1 = false;
  _prevStatusPhy2 = false;

  // Verify this is a LAN9355 chip
  ret = meadow_eth_phyread_32(LAN9355_CHIP_ID_REVISION_REGISTER,
            &lanChipId);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_phyread_32 failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // (--) TEMPORARY
  syslog(1, "--> Chip Id:0x%08x\n", lanChipId);
  // (--) TEMPORARY

  if((lanChipId & 0xffff0000) != 0x93550000)
  {
    syslog(LOG_ERR, "%s@%d-Ethernet chip must be LAN9355\n",
                thisFile, __LINE__);
    return -ENOTSUP;
  }

  // Configure the LAN9355 to generate an interrupts when there's a
  // change in either PHY link status.
  ret = meadow_eth_config_lan9355_irq();
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_config_lan9355_irq failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
  }

  // There's no need to call the worker thread here because the caller of this
  // function has already checked the status and set the _prevStatusPhy1 and
  // _prevStatusPhy2 values. Now that the LAN9355 interrupts are configured
  // everthing should be automatic.

  return OK;
}

//=============================================================
// This ISR is called for changes in PHY status.
int meadow_eth_monitor_link_status_isr(int irq, void *context, void *arg)
{
  uint16_t temp16;
  bool newStatusPhy1;
  bool newStatusPhy2;
 
  syslog(1, "+++ isr-Eth monitor's ISR received interrupt\n");

  // Nuttx has already acknowledged the GPIO interrupt that generated this
  // call. But, the LAN9355 requiries 2 register reads for each PHY.
  // In this register PHY A bit 9 link up, bit 4 link down.
  (void) meadow_eth_phyread_16(1, LAN9355_PHY_INTERRUPT_SOURCE, &temp16);
  (void) meadow_eth_phyread_16(2, LAN9355_PHY_INTERRUPT_SOURCE, &temp16);

  // This register contains a single bit field, 1 = Link up and 0 = Link down
  // Combine the status from both PHYs into a single value
  (void) meadow_eth_phyread_16(1, MII_MSR, &temp16);
  newStatusPhy1 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  (void) meadow_eth_phyread_16(2, MII_MSR, &temp16);
  newStatusPhy2 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  // Combine current status into a uint32_t value for worker thread use.
  // Allocating a struct has risks because if another work item is queued
  // and uses the same struct work_s, the memory will be leaked.
  uint32_t phyStatus = WORKER_HAVE_CURRENT_LINK_STATUS;
  if(newStatusPhy1) phyStatus |= WORKER_STATUS_PHY_1_CURR_MASK;
  if(newStatusPhy2) phyStatus |= WORKER_STATUS_PHY_2_CURR_MASK;

  // Don't waste time if nothing to do
  if(_prevStatusPhy1 == newStatusPhy1 && _prevStatusPhy2 == newStatusPhy2)
  {
    syslog(1, "+++ isr-No status changes so no extra work via work queue\n");
    return OK;
  }

  work_queue(HPWORK, &_eth_mon_work_q_struct, meadow_eth_monitor_worker,
            (void *)phyStatus, 0);

  return OK;
}

//=============================================================
// This function is called only during startup to set the initial status values
int meadow_eth_mon_startup_set_status()
{
  uint16_t temp16;

  // These registers contains a single bit field, 1 = Link up and
  // 0 = Link down
  (void) meadow_eth_phyread_16(1, MII_MSR, &temp16);
  _prevStatusPhy1 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  (void) meadow_eth_phyread_16(2, MII_MSR, &temp16);
  _prevStatusPhy2 = (temp16 & MII_MSR_LINKSTATUS) != 0;

  return OK;
}

//=============================================================
// This function is called via the Nuttx work queue. It assumes that the global
// _prevStatusPhy1 and _prevStatusPhy2 are correctly set.
void meadow_eth_monitor_worker(void *arg)
{
  int ret;
  bool newStatusPhy1;
  bool newStatusPhy2;
  bool linkNowUp = false;
  bool linkWasUp = false;

  if(arg == NULL)
  {
    syslog(LOG_ERR, "Eth monitor worker called with NULL");
    return;
  }

  uint32_t providedStatus = (uint32_t)arg;

  // Get the current link status if not provided by caller
  if((providedStatus & WORKER_HAVE_CURRENT_LINK_STATUS) != 0)
  {
    // Caller provided status
    newStatusPhy1 = (providedStatus & WORKER_STATUS_PHY_1_CURR_MASK) != 0;
    newStatusPhy2 = (providedStatus & WORKER_STATUS_PHY_2_CURR_MASK) != 0;
  }
  else
  {
    uint16_t temp16;

    // These registers contains a single bit field containing, 1 = Link up and
    // 0 = Link down
    (void) meadow_eth_phyread_16(1, MII_MSR, &temp16);
    newStatusPhy1 = (temp16 & MII_MSR_LINKSTATUS) != 0;

    (void) meadow_eth_phyread_16(2, MII_MSR, &temp16);
    newStatusPhy2 = (temp16 & MII_MSR_LINKSTATUS) != 0;
  }

  // TESTING
  syslog(1, "+++ wq-Split Link status PREVIOUS PHY A:%s, PHY B:%s, CURRENT PHY A:%s, PHY B:%s\n",
              _prevStatusPhy1 == 0 ? "Down" : "Up",
              _prevStatusPhy2 == 0 ? "Down" : "Up",
              newStatusPhy1   == 0 ? "Down" : "Up",
              newStatusPhy2   == 0 ? "Down" : "Up");
  // TESTING

  // Note: We combine the 2 PHY Link Status into 1 combined link status. This
  // is because, at least at this time, Nuttx and Meadow only support 1 link
  // status value.
  // Either PHY now link up?
  if(newStatusPhy1 || newStatusPhy2)
    linkNowUp = true;

  // Were either link up before?
  if(_prevStatusPhy1 || _prevStatusPhy2)
    linkWasUp = true;

  // Was there a transition?
  if(linkNowUp == linkWasUp)
    return; // No

  // Report new link status to Nuttx and Meadow
  ret = meadow_eth_mon_notify_link_up(linkNowUp);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Re-creating connection failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return;
  }

  if(linkNowUp)
  {
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

  _prevStatusPhy1 = newStatusPhy1;
  _prevStatusPhy2 = newStatusPhy2;
}

//=============================================================
// This function will notify all concerned entities 
int meadow_eth_mon_notify_link_up(bool isLinkUp)
{
  int ret;

  // Tell Nuttx and Meadow only if the combined status has changed
  if(isLinkUp != _prevLinkUp)
  {
    ret = meadow_eth_mon_report_link_status_change(isLinkUp);
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_mon_report_link_status_change, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      return ret;
    }

    // Is the link status is now up?
    if(isLinkUp)
    {
      // Determine if we should get the time now
      hcom_nx_config_lock();
      meadow_configuration_t *config = hcom_nx_config_get_pointer();
      uint32_t refreshPeriod = config->ntp_refresh_period_seconds;
      bool timeAtStart = config->get_network_time_at_startup;
      hcom_nx_config_unlock();

      // Get the time if necessary
      if(refreshPeriod > 0 || timeAtStart)
        ntpc_start();
    }
  }
  return OK;
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

  syslog(1, "cfg-Initial status, PHY1:0x%04x, PHY2:0x%04x\n", _prevStatusPhy1, _prevStatusPhy2);

  // Configure the GPIO connected to the LAN9355
  stm32_configgpio(MEADOW_ETH_PHY_IRQ_INPUT_PH14);

  // This call makes irq_attach() and up_enable_irq() calls internally.
  // In our case rising edge means link status up and falling edge link
  // status down
  ret = stm32_gpiosetevent(
  MEADOW_ETH_PHY_IRQ_INPUT_PH14,    // Nuttx cfgset
  1,                                // Rising Edge,
  0,                                // Falling Edge,
  0,                                // Event
  meadow_eth_monitor_link_status_isr,    // ISR
  NULL);

  return ret;
}

//=============================================================
// It took me a bit to understand from the LAN9355 data sheet how to configure
// its 32-bit registers. Once understood I created the following to hide the
// complexity. Since the MII protocol only deals with 16-bit values. When
// accessing the 32-bit registers of the LAN9355 two 2 16-bit reads or writes
// are necessary. So, a 32-bit versions of read and write are also provided.
//
// Note: The following don't use Nuttx ioctl (SIOCGMIIREG and SIOCSMIIREG)
// because this requires a socket descriptor (sd). Why? So they can be used
// in the ISR, which uses a different thread.
int meadow_eth_phyread_16(uint16_t phyAddr, uint16_t regAddr, uint16_t *value)
{
  int regval;
  volatile uint32_t timeout;

  // Preserve CSR Clock Range CR[2:0] bits
  regval  = getreg32(STM32_ETH_MACMIIAR);
  regval &= ETH_MACMIIAR_CR_MASK;

  // Set the PHY device address, PHY register address, and set the busy bit.
  regval |= (((uint32_t)phyAddr << ETH_MACMIIAR_PA_SHIFT) & ETH_MACMIIAR_PA_MASK);
  regval |= (((uint32_t)regAddr << ETH_MACMIIAR_MR_SHIFT) & ETH_MACMIIAR_MR_MASK);
  regval |= ETH_MACMIIAR_MB;

  putreg32(regval, STM32_ETH_MACMIIAR);

  // Wait for the transfer to complete
  for (timeout = 0; timeout < LAN9355_PHY_READ_TIMEOUT; timeout++)
  {
    // When the ETH_MACMIIAR_MW is clear, the read has completed.
    if ((getreg32(STM32_ETH_MACMIIAR) & ETH_MACMIIAR_MB) == 0)
    {
      // Read the register value from data register
      *value = (uint16_t)(getreg32(STM32_ETH_MACMIIDR) & 0xffff);
      return OK;
    }
  }

  syslog(1, "MII transfer timed out: phyAddr: %04x regAddr: %04x\n",
        phyAddr, regAddr);

  return -ETIMEDOUT;
}

//=============================================================
// This is a simple wrapper to hide the complexity of writing to a 16-bit register
int meadow_eth_phywrite_16(uint16_t phyAddr, uint16_t regAddr, uint16_t value)
{
  volatile uint32_t timeout;
  uint32_t regval;

  // Preserve CSR Clock Range CR[2:0] bits
  regval = getreg32(STM32_ETH_MACMIIAR);
  regval &= ETH_MACMIIAR_CR_MASK;

  // To conform to the MII protocol assemble the phyAddr and regAddr bits in
  // the proper field locations. Also, set the busy bit and the bit indicating
  // a write operation.
  regval |= (((uint32_t)phyAddr << ETH_MACMIIAR_PA_SHIFT) & ETH_MACMIIAR_PA_MASK);
  regval |= (((uint32_t)regAddr << ETH_MACMIIAR_MR_SHIFT) & ETH_MACMIIAR_MR_MASK);
  regval |= (ETH_MACMIIAR_MB | ETH_MACMIIAR_MW);

  // Write the value to the data register
  putreg32((uint32_t)value, STM32_ETH_MACMIIDR);

  // Write the destination register address in the MACIIDR register
  putreg32(regval, STM32_ETH_MACMIIAR);

  // When the ETH_MACMIIAR_MW is clear, the write has completed.
  for (timeout = 0; timeout < LAN9355_PHY_WRITE_TIMEOUT; timeout++)
  {
    if ((getreg32(STM32_ETH_MACMIIAR) & ETH_MACMIIAR_MB) == 0)
    {
      return OK;
    }
  }

  syslog(1, "Transfer timed out: phyAddr: %04x regAddr: %04x value: %04x\n",
        regAddr, phyAddr, value);

  return -ETIMEDOUT;
}

//=============================================================
// This is a special 32-bit write that is needed by the LAN9355 to access its
// Control and Status Registers (CSRs)
int meadow_eth_phywrite_32(uint16_t csrAddr, uint32_t value)
{
  int ret;
  uint8_t phyAddr;
  uint8_t regAddr;

  // Break the csrAddr down into it's component parts so it can ride on the
  // MII protocol. See LAN9355 data sheet section 14.2.
  // phyAddr is bit 4 set plus bits 9:6 of csrAddr as bits 3:0 of phyAddr.
  phyAddr = 0x10 | ((csrAddr >> 6) & 0x0f);

  // regAddr is bits 5:1 of csrAddr. Note: bit 0 of csrAddr is ignored.
  // However, bit 0 (which was csrAddr bit 1) determines if the upper or
  // lower 16-bits of the CSR register is being accessed
  regAddr = (csrAddr >> 1) & 0x1f;

  // Write the lower 16-bits
  ret = meadow_eth_phywrite_16(phyAddr, regAddr, value & 0xffff);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_phywrite_16-1 failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Write the upper 16-bits by setting regAddr bit 0 to 1 and writing the
  // upper 16-bits
  ret = meadow_eth_phywrite_16(phyAddr, regAddr + 1, value >> 16);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_phywrite_16-2 failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  return OK;
}

//=============================================================
// This is a special 32-bit read that is needed by the LAN9355 to access its
// Control and Status Registers (CSRs). See LAN9355 section 5.1.
int meadow_eth_phyread_32(uint16_t csrAddr, uint32_t *value)
{
  int ret;
  uint8_t phyAddr;
  uint8_t regAddr;
  uint16_t temp16;

  // Break the csrAddr down into it's component parts. See LAN9355 data sheet
  // section 14.2.
  // phyAddr is bit 4 set plus bits 9:6 of csrAddr.
  phyAddr = 0x10 | ((csrAddr >> 6) & 0x0f);

  // regAddr is bits 5:1 of csrAddr without. Note: bit 0 of csrAddr is ignored.
  // However, bit 0 (which was csrAddr bit 1) determines if the upper or
  // lower 16-bits of the CSR register is being accessed
  regAddr = (csrAddr >> 1) & 0x1f;

  // Read the lower 16-bits
  ret = meadow_eth_phyread_16(phyAddr, regAddr, &temp16);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_phyread_16-1 failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Lower 16-bits
  *value = temp16;

  // Now read the upper 16-bits by setting regAddr bit 0 to 1
  ret = meadow_eth_phyread_16(phyAddr, regAddr + 1, &temp16);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_phyread_16-2, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Add the upper 16-bits
  *value |= temp16 << 16;

  return OK;
}


#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
