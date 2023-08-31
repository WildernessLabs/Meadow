/****************************************************************************
 * /configs/stm32f777zit6-meadow/src/ethernet/meadow_ethnet_start.c
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

// Uncomment the #define below to turn on debug help macros.
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This is called from hcom_nx_startup_mgr.c
int meadow_eth_mngr_startup()
{
  int ret;

  // Verify that this is a LAN9355 chip
  ret = meadow_eth_utils_verify_lan9355();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Installed LAN chip is not a LAN9355. ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Initialize link status monitoring via the LAN9355's IRQ pin. As soon as
  // configured the IRQ pin will indicate an interrupt if the link status is up
  // for either PHY.
  ret = meadow_eth_mon_config_lan9355_irq();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_mon_config_lan9355_irq() failed. ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  // This call will do the initialization needed to make connections and report
  // link status changes.
  ret = meadow_eth_conn_startup();
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_conn_startup() failed. ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
    return ret;
  }

  return OK;
}

#endif    // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
