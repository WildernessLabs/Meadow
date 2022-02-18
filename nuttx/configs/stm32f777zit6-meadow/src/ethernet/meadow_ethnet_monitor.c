/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/ethernet/meadow_ethnet_monitor.c
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

// This module is contains code to monitor the state of the Ethernet
// Parts of this module orginally were taken from nuttx 7.x at
// /apps/nshlib/nsh_netinit.c. In nuttx 10 this was found at 
// /apps/netutils/netinit/netinit.c. A single change was included here

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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MEADOW_ETH_MONITOR_SIGNAL_NO (18)
#define MEADOW_ETH_MONITOR_LONG_TIME_SEC    (60*60) /* One hour in seconds */
#define MEADOW_ETH_MONITOR_SHORT_TIME_SEC   (2)     /* 2 seconds */
#define MEADOW_ETH_MONITOR_RETRY_MS (2000)

// Nuttx config provides CONFIG_STM32F7_PHYADDR for a single PHY but I have
// chosen to ignore this Nuttx config value.
#define MEADOW_ETH_MONITOR_PHY_0  (0)
#define MEADOW_ETH_MONITOR_PHY_1  (1)
#define MEADOW_ETH_MONITOR_PHY_2  (2)

/****************************************************************************
 * Private Data
 ****************************************************************************/
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

static char *thisFile = __FILE__;
static int _meadow_eth_monitor_kthrd;
static sem_t _notifySem;
static int _sockDescp;
static bool _prevLnkStat1;
static bool _prevLnkStat2;
static bool _wasLinkUp;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static void *meadow_eth_monitor_kthread(int argc, char *argv[]);
static int meadow_eth_monitor_check(void);
static void meadow_eth_monitor_signal(int signo, FAR siginfo_t *siginfo,
                               FAR void *context);
static int meadow_eth_monitor_link_status(struct ifreq *ifr, uint16_t phyNumb,
          bool *currentLnkStat, bool prevLnkStat, struct timespec *delaytime);

/****************************************************************************
 * Function Implementations
 ****************************************************************************/

int meadow_eth_monitor_startup(void)
{
  _sockDescp = -1;

  // Create a thread to do the ethernet monitoring
  _meadow_eth_monitor_kthrd = kthread_create(MEADOW_THREAD_NAME_ETHNET_MONITOR,
                                  MEADOW_THREAD_PRIORITY_ETHNET_MONITOR,
                                  MEADOW_THREAD_STACKSIZE_ETHNET_MONITOR,
                                  (main_t) meadow_eth_monitor_kthread,
                                  (char *const *) NULL);
  if (_meadow_eth_monitor_kthrd <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
                thisFile, __LINE__,  MEADOW_THREAD_NAME_ETHNET_MONITOR);
    return -ENOEXEC;
  }

  return OK;
}

//=============================================================
// This thread is used to monitor the link status of the Ethernet connection(s)
// Orginal from .../apps/nshlib/nsh_netinit.c
void *meadow_eth_monitor_kthread(int argc, char *argv[])
{
  int ret;
  struct ifreq ifr;
  struct sigaction act;

#if HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS > 0
  syslog(2, "New kthread [PID:%d],'%s'\n", getpid(), MEADOW_THREAD_NAME_ETHNET_MONITOR);
#endif

  _wasLinkUp = false;

  sleep(1);

  /* Initialize the notification semaphore */

  ret = sem_init(&_notifySem, 0, 0);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-sem_init failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return NULL;
  }

  /* Get a socket descriptor that we can use to communicate with the network
   * interface driver.
   */
  // Note: this call must be by the thread that will use the sd
  _sockDescp = socket(AF_INET, SOCK_DGRAM, 0);
  if (_sockDescp < 0)
  {
    syslog(LOG_ERR, "%s@%d-socket open failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return NULL;
  }

  // We could read the link status from the OS but if we assume it's down we
  // can be sure Meadow will report the link up.
  _prevLnkStat1 = false;
  _prevLnkStat2 = false;

  // Attach a signal handler so that we do not lose PHY events
  act.sa_sigaction = meadow_eth_monitor_signal;
  act.sa_flags = SA_SIGINFO;

  ret = sigaction(MEADOW_ETH_MONITOR_SIGNAL_NO, &act, NULL);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-socket failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    close(_sockDescp);
    return NULL;
  }

  // Enter a forever loop that periodically calls the monitor function
  for(;;)
  {
    // struct timespec waketime;

    ret = meadow_eth_monitor_check();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow Ethernet Monitor failed, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      break;
    }

    // // Wait for timeout
    // syslog(1, "==-->Waiting for timeout\n");
    // (void)sem_timedwait(&_notifySem, &waketime);
    // syslog(1, "==-->Back from timeout\n");
    
    // // Note: sched_xxxx are non-standard functions implemented by Nuttx.
    // // By design pre-emption is enabled whenever a thread blocks itself.
    // // Therefore, the sem_timedwait allowed the OS to do pre-emption
    // // and the sched_unlock call simply cleans up the count.
    // sched_unlock();
  }

  // PeterM - May need to do more cleanup here?
  if(_sockDescp > -1)
    close(_sockDescp);

  return NULL;
}

// //=============================================================
// // Temporary for showing register contents
// static uint16_t read_mii_register(struct ifreq *ifr, unsigned long mii_reg_addr)
// {
//   ifr->ifr_mii_reg_num = mii_reg_addr;
//   ioctl(_sockDescp, SIOCGMIIREG, (unsigned long)ifr);
//   return ifr->ifr_mii_val_out;
// }

//=============================================================
int meadow_eth_monitor_check()
{
  int ret;
  struct ifreq ifr;
  struct timespec waketime;
  struct timespec delaytime;
  bool currentLnkStat;
  bool isLnkStatUp;

  memset(&ifr, 0, sizeof(struct ifreq));
  // Need the name because it is the key to locating the desired device
  strncpy(ifr.ifr_name, MEADOW_ETHMAC_DEVICENAME, IFNAMSIZ);

  /* Configure to receive a signal on changes in link status */

// PeterM - Will monitor Ethernet the PHY's IRQ being connected to a GPIO pin that
// handles the PHY's interrupts? Probably Not.

// PeterM-Need to verify if fully supported in the STM32F7 Ethernet driver.
// This needs to be revisited!!! See stm32_ethernet.c @3967
// Would need to set CONFIG_ARCH_PHY_INTERRUPT and may be related to
// the configuration option:
// Network Support -> Network Device Operation [ ]CONFIG_NEWDOWN_NOTIFIER

  // ifr.ifr_mii_notify_event.sigev_notify = SIGEV_SIGNAL;
  // ifr.ifr_mii_notify_event.sigev_signo = MEADOW_ETH_MONITOR_SIGNAL_NO;

  // ret = ioctl(_sockDescp, SIOCMIINOTIFY, (unsigned long)&ifr);
  // if (ret < 0)
  // {
  //   syslog(LOG_ERR, "%s@%d-ioctl(SIOCMIINOTIFY) failed, ret:%d, errno:%d\n",
  //               thisFile, __LINE__, ret, errno);
  //   return ret;
  // }

  /* Does the driver think that the link is up or down? */
// This reads the previous link status. We have 2 link statuses to monitor

  /* Get the current PHY address in use.  This probably does not change,
      * but just in case...
      *
      * NOTE: We are assuming that the network device name is preserved in
      * the ifr structure.
      */
  // Calling ioctl with SIOCGMIIPHY just returns the value in
  // CONFIG_STM32F7_PHYADDR. We know that both PHY 1 & 2 are needed
  // ret = ioctl(_sockDescp, SIOCGMIIPHY, (unsigned long)&ifr);
  // if (ret < 0)
  // {
  //   syslog(LOG_ERR, "%s@%d-ioctl(SIOCGMIIPHY) failed, ret:%d, errno:%d\n",
  //               thisFile, __LINE__, ret, errno);
  //   return ret;
  // }

  if(ethUseLAN9355notLAN8742A)
  {
    // There are 3 PHY's within the LAN9355 used on the Meadow CCM
    // breakout board, numbered 0, 1 & 2. The single value specified in the
    // configuration option: System Type -> Ethernet MAC configuration
    // [0] PHY address, we ignore.
    // The other 2 PHYs are connected to the 2 RJ45 connectors. We need to
    // monitor both of these.
    uint16_t phyNumb;
    bool *prevLnkStat;
    struct timespec delaytime1;
    struct timespec delaytime2;
    struct timespec *ts_delay;

    isLnkStatUp = false;

    for(phyNumb = MEADOW_ETH_MONITOR_PHY_1;
        phyNumb <= MEADOW_ETH_MONITOR_PHY_2;
        phyNumb++)
    {
      if(phyNumb == MEADOW_ETH_MONITOR_PHY_1)
      {
        prevLnkStat = &_prevLnkStat1;
        ts_delay = &delaytime1;
      }
      else  // if(phyNumb == MEADOW_ETH_MONITOR_PHY_2)
      {
        prevLnkStat = &_prevLnkStat2;
        ts_delay = &delaytime2;
      }

      // Note prevLnkStat and ts_delay are just a scheme to transport pointers
      // to the real variable that may be updated. This call will set the
      // currentLnkStat but not prevLnkStat.
      ret = meadow_eth_monitor_link_status(&ifr, phyNumb,
                &currentLnkStat, *prevLnkStat, ts_delay);
      if (ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-link status check failed, ret:%d, errno:%d\n",
                    thisFile, __LINE__, ret, errno);
        return ret;
      }

      // If either or both PHYs are up tell Nuttx it's up. Otherwise, it's down.
      if(currentLnkStat)
        isLnkStatUp = true;

      // Set the appropriate status, _prevLnkStat1 or _prevLnkStat2
      *prevLnkStat = currentLnkStat;
    }

    // Now find the shortest delay
    if(delaytime1.tv_sec == delaytime2.tv_sec)
    {
      if(delaytime1.tv_nsec < delaytime2.tv_nsec)
      {
        delaytime.tv_sec = delaytime1.tv_sec;
        delaytime.tv_nsec = delaytime1.tv_nsec;
      }
      else
      {
        delaytime.tv_sec = delaytime2.tv_sec;
        delaytime.tv_nsec = delaytime2.tv_nsec;
      }
    }
    else
    {
      if(delaytime1.tv_sec < delaytime2.tv_sec)
      {
        delaytime.tv_sec = delaytime1.tv_sec;
        delaytime.tv_nsec = delaytime1.tv_nsec;
      }
      else
      {
        delaytime.tv_sec = delaytime2.tv_sec;
        delaytime.tv_nsec = delaytime2.tv_nsec;
      }
    }
  }
  else
  {
    // With a single PHY it's pretty simple. Use _prevLnkStat1 for the
    // previous link status when there's just a single PHY to worry about.
    // Note: _prevLnkStat1 is not updated by the call.
    ret = meadow_eth_monitor_link_status(&ifr, MEADOW_ETH_MONITOR_PHY_0,
              &currentLnkStat, _prevLnkStat1, &delaytime);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-link status check failed, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      return ret;
    }

    _prevLnkStat1 = currentLnkStat;   // For next call
    isLnkStatUp = currentLnkStat;     // For updating Nuttx
  }

  // Nuttx needs to know if the status has changed
  if(isLnkStatUp != _wasLinkUp)
  {
    if(isLnkStatUp)
    {
      ifr.ifr_flags = IFF_UP;
    }
    else
    {
      ifr.ifr_flags = IFF_DOWN;
    }

    ret = ioctl(_sockDescp, SIOCSIFFLAGS, (unsigned long)&ifr);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-ioctl(SIOCSIFFLAGS) failed, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      return ret;
    }

    _wasLinkUp = isLnkStatUp;
  }

  // Now wait for either the semaphore to be posted or a timed-out to occur
  sched_lock();
  ret = clock_gettime(CLOCK_REALTIME, &waketime);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-clock_gettime, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  waketime.tv_sec += delaytime.tv_sec;
  waketime.tv_nsec += delaytime.tv_nsec;
  if (waketime.tv_nsec >= 1000000000)
  {
    waketime.tv_sec++;
    waketime.tv_nsec -= 1000000000;
  }

  // Wait for timeout
  // syslog(LOG_INFO, "Waiting for timeout, 10 seconds\n");
  sleep(10);

  // (void)sem_timedwait(&_notifySem, &waketime);

  // syslog(LOG_INFO, "Back from timeout\n");

  sched_unlock();

  return OK;
}

//====================================================================
// Finds the Link Status for the specifiec PHY
int meadow_eth_monitor_link_status(struct ifreq *ifr, uint16_t phyNumb,
          bool *currentLnkStat, bool prevLnkStat, struct timespec *delaytime)
{
  int ret;

  ifr->ifr_mii_reg_num = MII_MSR;    // MII management status
  ifr->ifr_ifru.ifru_mii_data.phy_id = phyNumb;

  // Read MII Status Register
  ret = ioctl(_sockDescp, SIOCGMIIREG, (unsigned long)ifr);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ioctl(SIOCGMIIREG) failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Get the current status and set the callers copy
  *currentLnkStat = (ifr->ifr_mii_val_out & MII_MSR_LINKSTATUS) != 0;

  // Link Status change?
  if (*currentLnkStat == prevLnkStat)
  {
    // No change to link status
    if(*currentLnkStat)
    {
      // The link is still up
      delaytime->tv_sec = MEADOW_ETH_MONITOR_LONG_TIME_SEC;
      delaytime->tv_nsec = 0;
    }
    else
    {
      // Link still down (Seems like a short wait for still being down)
      delaytime->tv_sec = MEADOW_ETH_MONITOR_RETRY_MS / 1000;
      delaytime->tv_nsec = (MEADOW_ETH_MONITOR_RETRY_MS % 1000) * 1000000;
    }

    return OK;
  }

  // syslog(1, "Link Status of PHY %d has changed. It is now:%s\n",
  //           phyNumb, *currentLnkStat ? "Up" : "Down");

  if(*currentLnkStat)
  {
    // Was down now up
    /* And wait for a short delay.  We will want to recheck the
    * link status again soon.    */
    delaytime->tv_sec = MEADOW_ETH_MONITOR_SHORT_TIME_SEC;
    delaytime->tv_nsec = 0;
  }
  else
  {
    // Was up now down
    delaytime->tv_sec = MEADOW_ETH_MONITOR_RETRY_MS / 1000;
    delaytime->tv_nsec = (MEADOW_ETH_MONITOR_RETRY_MS % 1000) * 1000000;
  }
  
  return OK;
}


//=============================================================
// This function receives signals
void meadow_eth_monitor_signal(int signo, FAR siginfo_t *siginfo,
                               FAR void *context)
{
  int semcount;
  int ret;

  /* What is the count on the semaphore?  Don't over-post */

  ret = sem_getvalue(&_notifySem, &semcount);
  syslog(LOG_INFO, "Entry: semcount=%d\n", semcount);

  if (ret == OK && semcount <= 0)
  {
    sem_post(&_notifySem);
  }

  // syslog(LOG_INFO, "Exit\n");
}

#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
