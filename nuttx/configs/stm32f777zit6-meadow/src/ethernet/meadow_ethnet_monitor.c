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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <ctype.h>
#include <stdint.h>
#include <nuttx/kthread.h>

#include "meadow_ethnet_local.h"
#include <meadow/meadow_ethnet_common.h>
#include <nuttx/net/mii.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MEADOW_ETH_MONITOR_SIGNAL_NO (18)
#define MEADOW_ETH_MONITOR_LONG_TIME_SEC    (60*60) /* One hour in seconds */
#define MEADOW_ETH_MONITOR_SHORT_TIME_SEC   (2)     /* 2 seconds */
#define MEADOW_ETH_MONITOR_RETRY_MS 2000

/****************************************************************************
 * Private Data
 ****************************************************************************/
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

static char *thisFile = __FILE__;
static sem_t _notifySem;
static int _sockDescp;
static int _meadow_eth_monitor_kthrd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static void nsh_netinit_signal(int signo, FAR siginfo_t *siginfo,
                               FAR void *context);

static void *meadow_eth_monitor_kthread(int argc, char *argv[]);
static int nsh_netinit_monitor(void);

/****************************************************************************
 * Private Function Implementations
 ****************************************************************************/
int meadow_eth_monitor_startup(void)
{
  _sockDescp = -1;

  // Create a thread to do the ethernet startup
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
// This is temporary until a generalized programmable sleep timer is implemented
//
void *meadow_eth_monitor_kthread(int argc, char *argv[])
{
  int ret;
  struct sigaction act;

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
    syslog(LOG_ERR, "%s@%d-socket failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return NULL;
  }

  /* Attach a signal handler so that we do not lose PHY events */

  act.sa_sigaction = nsh_netinit_signal;
  act.sa_flags = SA_SIGINFO;

  ret = sigaction(MEADOW_ETH_MONITOR_SIGNAL_NO, &act, NULL);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-socket failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    close(_sockDescp);
    return NULL;
  }

  // Enter a forever loop that periodically calls the monitor functions
  for(;;)
  {
    // NICE TO MOVE TIMEOUT CODE HERE
    ret = nsh_netinit_monitor();
    if(ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-Meadow Ethernet Monitor failed, ret:%d, errno:%d\n",
                  thisFile, __LINE__, ret, errno);
      break;
    }
  }

  // NEED TO CLEAN UP MORE
  // Stop the PHY notifications and remove the signal handler too
  if(_sockDescp > -1)
    close(_sockDescp);

  return NULL;
}

//=============================================================
// This function receives signals
static void nsh_netinit_signal(int signo, FAR siginfo_t *siginfo,
                               FAR void *context)
{
  int semcount;
  int ret;

  /* What is the count on the semaphore?  Don't over-post */

  ret = sem_getvalue(&_notifySem, &semcount);
  ninfo("Entry: semcount=%d\n", semcount);

  if (ret == OK && semcount <= 0)
  {
    sem_post(&_notifySem);
  }

  ninfo("Exit\n");
}

//=============================================================
int nsh_netinit_monitor(void)
{
  int ret;
  bool devup;
  struct ifreq ifr;
  struct timespec waketime;
  struct timespec waittime;

  // for (;;)
  // {

  /* Configure to receive a signal on changes in link status */

  memset(&ifr, 0, sizeof(struct ifreq));
  strncpy(ifr.ifr_name, MEADOW_ETHMAC_DEVICENAME, IFNAMSIZ);

  ifr.ifr_mii_notify_event.sigev_notify = SIGEV_SIGNAL;
  ifr.ifr_mii_notify_event.sigev_signo = MEADOW_ETH_MONITOR_SIGNAL_NO;

  ret = ioctl(_sockDescp, SIOCMIINOTIFY, (unsigned long)&ifr);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ioctl(SIOCMIINOTIFY) failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  /* Does the driver think that the link is up or down? */

  ret = ioctl(_sockDescp, SIOCGIFFLAGS, (unsigned long)&ifr);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ioctl(SIOCGIFFLAGS) failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  devup = ((ifr.ifr_flags & IFF_UP) != 0);

  /* Get the current PHY address in use.  This probably does not change,
      * but just in case...
      *
      * NOTE: We are assuming that the network device name is preserved in
      * the ifr structure.
      */

  ret = ioctl(_sockDescp, SIOCGMIIPHY, (unsigned long)&ifr);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ioctl(SIOCGMIIPHY) failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  /* Read the PHY status register */

  ifr.ifr_mii_reg_num = MII_MSR;

  ret = ioctl(_sockDescp, SIOCGMIIREG, (unsigned long)&ifr);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-ioctl(SIOCGMIIREG) failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  ninfo("%s: devup=%d PHY address=%02x MSR=%04x\n",
        ifr.ifr_name, devup, ifr.ifr_mii_phy_id, ifr.ifr_mii_val_out);

  /* Check for link up or down */

  if ((ifr.ifr_mii_val_out & MII_MSR_LINKSTATUS) != 0)
  {
    /* Link up... does the drive think that the link is up? */

    if (!devup)
    {
      /* No... We just transitioned from link down to link up.
              * Bring the link up.
              */

      ninfo("Bringing the link up\n");

      ifr.ifr_flags = IFF_UP;
      ret = ioctl(_sockDescp, SIOCSIFFLAGS, (unsigned long)&ifr);
      if (ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-ioctl(SIOCSIFFLAGS) failed, ret:%d, errno:%d\n",
                    thisFile, __LINE__, ret, errno);
        return ret;
      }

      /* And wait for a short delay.  We will want to recheck the
       * link status again soon.
       */

      waittime.tv_sec = MEADOW_ETH_MONITOR_SHORT_TIME_SEC;
      waittime.tv_nsec = 0;
    }
    else
    {
      /* The link is still up.  Take a long, well-deserved rest */

      waittime.tv_sec = MEADOW_ETH_MONITOR_LONG_TIME_SEC;
      waittime.tv_nsec = 0;
    }
  }
  else
  {
    /* Link down... Was the driver link state already down? */

    if (devup)
    {
      /* No... we just transitioned from link up to link down.  Take
              * the link down.
              */

      ninfo("Taking the link down\n");

      ifr.ifr_flags = IFF_DOWN;
      ret = ioctl(_sockDescp, SIOCSIFFLAGS, (unsigned long)&ifr);
      if (ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-ioctl(SIOCSIFFLAGS) failed, ret:%d, errno:%d\n",
                    thisFile, __LINE__, ret, errno);
        return ret;
      }
    }

    /* In either case, wait for the short, configurable delay */

    waittime.tv_sec = MEADOW_ETH_MONITOR_RETRY_MS / 1000;
    waittime.tv_nsec = (MEADOW_ETH_MONITOR_RETRY_MS % 1000) * 1000000;
  }

  /* Now wait for either the semaphore to be posted for a timed-out to
      * occur.
      */

  sched_lock();
  ret = clock_gettime(CLOCK_REALTIME, &waketime);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-clock_gettime, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  waketime.tv_sec += waittime.tv_sec;
  waketime.tv_nsec += waittime.tv_nsec;
  if (waketime.tv_nsec >= 1000000000)
  {
    waketime.tv_sec++;
    waketime.tv_nsec -= 1000000000;
  }

  // Wait for timeout
  (void)sem_timedwait(&_notifySem, &waketime);
  sched_unlock();
  // }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
