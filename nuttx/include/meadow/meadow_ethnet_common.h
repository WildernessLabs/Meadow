/****************************************************************************
 * /nuttx/include/meadow/meadow_ethnet_common.h
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
#ifndef __CONFIGS_MEADOW_SRC_HCOM_NX_ETHNET_COMMON__H
#define __CONFIGS_MEADOW_SRC_HCOM_NX_ETHNET_COMMON__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <unistd.h>   // getopt() - parses command line args
#include <stdlib.h>
#include <time.h>
#include <poll.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdio.h>

#include <arpa/inet.h>
#include <nuttx/clock.h>
#include <nuttx/net/icmp.h>
#include <nuttx/net/ioctl.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <meadow/hcom_shared_common.h>

#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

// #include "../configs/stm32f777zit6-meadow/src/ethernet/meadow_ethnet_local.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define MEADOW_ETHMAC_DEVICENAME "eth0"

/****************************************************************************
 * Public Types
 ****************************************************************************/

#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************************************
 * Public Function Prototypes
 ****************************************************************************************************/

// Starts ethernet
int meadow_eth_mngr_startup(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

#endif // __CONFIGS_MEADOW_SRC_HCOM_NX_ETHNET_COMMON__H
