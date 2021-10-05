/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/hcom_nx/ethernet/hcom_nx_ethernet_manager.c
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

// This module is common to all related ethernet modules

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <ctype.h>
#include <stdint.h>
#include <nuttx/kthread.h>

#include "../hcom_nx_common.h"

#include "hcom_nx_ethnet_local.h"
#include <meadow/meadow_ethnet_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
#if defined(CONFIG_HCOM_INCLUDE_ETHNET_IN_BUILD)

static char *thisFile = __FILE__;
static struct dhcp_info_s *dhcp_info;
static int _enet_kthread_pid;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Function Implementations
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

struct dhcp_info_s* hcom_nx_eth_mgr_get_dhcp_info()
{
  return dhcp_info;
}

//==============================================================
// This is the main entry point.
int hcom_nx_start_up_ethernet(void)
{
  dhcp_info = malloc(sizeof(struct dhcp_info_s));
  if(dhcp_info == NULL)
  {
    syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Create a thread to do the ethernet startup
  _enet_kthread_pid = kthread_create(HCOM_THREAD_NAME_ETHNET_START,
                                  HCOM_THREAD_PRIORITY_ETHNET_START,
                                  HCOM_THREAD_STACKSIZE_ETHNET_START,
                                  (main_t) start_ethnet_kthread,
                                  (char *const *) NULL);
  if (_enet_kthread_pid <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of Ethernet kthread FAILED\n", thisFile, __LINE__);
    return -ENOEXEC;
  }
  return OK;
}

#else

int hcom_nx_start_up_ethernet(void)
{
  return OK;
}

#endif    // #if defined(CONFIG_HCOM_INCLUDE_ETHNET_IN_BUILD)


