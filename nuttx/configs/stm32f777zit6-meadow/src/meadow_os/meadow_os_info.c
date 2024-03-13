/****************************************************************************
 * meadow_os_info.c
 *
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
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

#include <nuttx/config.h>

#include <stdlib.h>
#include <nuttx/kmalloc.h>

#include <meadow/meadow_os.h>
#include <meadow/meadow_hw_version.h>

#include "../hcom_nx/hcom_nx_config_manager.h"

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_os_power_cycle_count
 *
 * Description:
 *  Get the number of times the board has been through a power cycle
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Number of times the board has been power cycled.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t meadow_os_power_cycle_count(void)
{
    return(0);
}

/****************************************************************************
 * Name: meadow_os_reset_cycle_count
 *
 * Description:
 *  Get the number of times the board has been through a reset (including
 *  power cycles).
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Number of times the board has been reset.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t meadow_os_reset_cycle_count(void)
{
    return(0);
}

/****************************************************************************
 * Name: meadow_os_reset_reason
 *
 * Description:
 *  What was the cause of the last reset?
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Cause of the last reset?
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t meadow_os_reset_reason(void)
{
    return(0);
}

/****************************************************************************
 * Name: meadow_os_hardware_version
 *
 * Description:
 *  Hardware version / type that the OS is running on.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Hardware version / type
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t meadow_os_hardware_version(void)
{
    return(meadow_hw_version_get());
}

/****************************************************************************
 * Name: meadow_os_native_protocol_version
 *
 * Description:
 *  Get the native protocol version.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Protocol version
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint32_t meadow_os_native_protocol_version(void)
{
    return(1);
}
