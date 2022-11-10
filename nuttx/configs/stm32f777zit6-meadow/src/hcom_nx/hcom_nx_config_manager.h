/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\hcom_nx_config_manager.h
 *
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
#ifndef __CONFIGS_MEADOW_SRC_HCOM_NX_CONFIG_MANAGER__H
#define __CONFIGS_MEADOW_SRC_HCOM_NX_CONFIG_MANAGER__H

#include <meadow/hcom_shared_common.h>
#include "../libcyaml/cyaml.h"
#include "../espcp/espcp_encoders.h"

/****************************************************************************
 * Definitions.
 ****************************************************************************/

//
//  These defintions are determined by the ESP32.
//
#define MAXIMUM_SSID_LENGTH 32
#define MAXIMUM_PASSWORD_LENGTH 64

/****************************************************************************
 * Enums.
 ****************************************************************************/

/*
 *  Config values that can be read or written.
 *
 *  Important: These values must match those in the file IPlatformOS.Configuration.cs.
 */
enum configuration_values
{ 
    cv_device_name = 0, cv_product, cv_model, cv_os_version, cv_build_date, cv_processor_type, cv_unique_id, cv_serial_number, 
    cv_coprocessor_type, cv_coprocessor_firmware_version, cv_mono_version,
    cv_automatically_start_network, cv_automatically_reconnect, cv_maximum_network_retry_count, cv_get_time_at_startup,
    cv_mac_address, cv_soft_ap_mac_address, cv_default_access_point, cv_reset_reason,
    cv_reboot_on_unhandled_exception, cv_initialization_timeout, cv_sd_card_present, cv_selected_network,
    cv_static_ip_ddress, cv_subnet_mask, cv_default_gateway

};
typedef enum configuration_values configuration_values_t;

/****************************************************************************
 * Public methods.
 ****************************************************************************/

void hcom_nx_config_init(void);
void hcom_nx_config_lock(void);
void hcom_nx_config_unlock(void);
meadow_configuration_t *hcom_nx_config_get_pointer(void);
int hcom_nx_config_get_set_config_value(int, uint8_t, uint8_t *, int);
void hcom_nx_config_process_esp_configuration(espcp_system_configuration_t *);
void hcom_nx_config_process_wifi_credentials_file(void);
void hcom_nx_config_refresh_mono_version(meadow_configuration_t *);
int hcom_nx_config_set_esp_integer_value(espcp_configuration_items_t, uint32_t);
void hcom_nx_config_set_time_to_os_build_time(void);

#endif // __CONFIGS_MEADOW_SRC_HCOM_NX_CONFIG_MANAGER__H