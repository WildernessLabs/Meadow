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
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <nuttx/fs/fs.h>
#include <nuttx/kmalloc.h>

#include <arch/board/board.h>
#include "../hcom_nx/hcom_nx_common.h"

#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_os.h>
#include <meadow/meadow_hw_version.h>
#include <meadow/meadow_os_persistent_data.h>
#include <meadow/hcom_bbreg_defn.h>

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

/**
 * @brief Number of times the board has been reset due to power on / power loss
 */
static uint32_t _power_cycle_count = 1;

/**
 * @brief Number of times the board has been reset.
 */
static uint32_t _reset_cycle_count = 1;

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
    return(_power_cycle_count);
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
    return(_reset_cycle_count);
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
    return((getreg32(HCOM_NX_MEADOW_RESET_REASON_BBR) & 0xff000000) >> 24);
}

/****************************************************************************
 * Name: meadow_os_reset_update_counters
 *
 * Description:
 *  Use the reset reason to update the reset counters.
 * 
 *  The reset counter is normally always incremented.  The power cycle counter
 *  is only incremented if the reset reason is a power related.
 * 
 *  The counters are read from the OS permanent storage on flash, updated and
 *  then written back to flash.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK if successful, -ENOMEM if no memory, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int meadow_os_reset_update_counters(void)
{
    meadow_os_persistent_data_t *data = kmm_malloc(sizeof(meadow_os_persistent_data_t));
    if (data == NULL)
    {
        return(-ENOMEM);
    }

    int result;
    result = mkdir(MEADOW_SYSTEM_DIR, 0666);
    struct file f;
    result = file_open(&f, MEADOW_SYSTEM_OS_PERSISTENT_DATA_FILE, O_RDWR | O_CREAT);
    if (result < 0)
    {
        kmm_free(data);
        return(ERROR);
    }
    size_t bytes_read = file_read(&f, data, sizeof(meadow_os_persistent_data_t));
    if (bytes_read < 0)
    {
        //
        //  Error if less than 0.
        //
        file_close(&f);
        kmm_free(data);
        return(ERROR);
    }
    if (bytes_read != sizeof(meadow_os_persistent_data_t))
    {
        //
        //  Assume the file has just been created so there is no data in it.
        //
        memset(data, 0, sizeof(meadow_os_persistent_data_t));
        data->version = OS_PERSISTENT_DATA_VERSION;
    }

    data->reset_count++;
    uint8_t power_flags = MEADOW_OS_RESET_BROWNOUT
                        | MEADOW_OS_RESET_POWER_CYCLE
                        | MEADOW_OS_RESET_LOW_POWER;
    uint32_t reason = meadow_os_reset_reason();
    if ((reason & power_flags) || (reason == 0) || (data->power_cycle_count == 0))
    {
        data->power_cycle_count++;
    }
    _power_cycle_count = data->power_cycle_count;
    _reset_cycle_count = data->reset_count;

    int offset = file_seek(&f, 0, SEEK_SET);
    if (offset < 0)
    {
        file_close(&f);
        kmm_free(data);
        return(ERROR);
    }
    result = file_write(&f, data, sizeof(meadow_os_persistent_data_t));
    if (result != sizeof(meadow_os_persistent_data_t))
    {
        file_close(&f);
        kmm_free(data);
        return(ERROR);
    }

    kmm_free(data);
    file_close(&f);

    MEADOW_TRACE_INFORMATION("Reset cycle count: %d, Power cycle count: %d\n",
                            data.reset_count, data.power_cycle_count);
    return(OK);
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

/****************************************************************************
 * Name: meadow_os_get_gateway_address
 *
 * Description:
 *  Get the gateway address.
 *
 * Input Parameters:
 *  buffer - buffer to store the network information.
 *
 * Returned Value:
 *  Length of the buffer.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int meadow_os_get_gateway_address(char *buffer)
{
    int ret = -1;

    if (buffer == NULL)
    {
        return ret;
    }

    uint32_t gateway = 0;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    meadow_network_interface_t *iface = config->default_interface;
    if (iface != NULL)
    {
        gateway = iface->gateway;
    }
    hcom_nx_config_unlock();

    char address[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &gateway, address, INET_ADDRSTRLEN) != NULL)
    {
        ret = strlen(strcpy(buffer, address));
    }

    return ret;
}