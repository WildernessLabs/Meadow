/****************************************************************************
 * \include\meadow\hcom_upd_shared.h
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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
#ifndef __INCLUDE_MEADOW_HCOM_NX_SHARED__H
#define __INCLUDE_MEADOW_HCOM_NX_SHARED__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Private defines
 ****************************************************************************/
// This header file contains those items that must be shared by apps/hcom
// #include <string.h>

#define HCOM_NX_UPD_DRIVER_NAME "/dev/nxupd"

// Battery Backed Register
struct hcom_nx_upd_bbr_value
{
  uint32_t value;
};

struct hcom_nx_upd_bbr_update
{
  uint32_t clearBits;
  uint32_t setBits;
};

struct hcom_nx_upd_register_value
{
  uint32_t address;
  uint32_t value;
};

struct hcom_nx_upd_register_update
{
  uint32_t address;
  uint32_t clearBits;
  uint32_t setBits;
};

struct hcom_nx_upd_is_part_mounted
{
  uint32_t partitionId;
  bool isMounted;
};

struct hcom_nx_cmd_data
{
  uint16_t hcomCmd;   // The orginal host command
  uint32_t userData;

  uint8_t logLevel;
  uint8_t logLen;
  char logMsg[HCOM_NX_CMD_LOG_MSG_SIZE + 1];
  void (* send_host_msg)(uint16_t, uint32_t, char *, char *, int);
};

struct hcom_nx_upd_gpio_config_s
{
  uint8_t gpioHcomId;     // 1- n
  uint8_t configValue;  // 1 = input, 0 = output
  int result;
};

struct hcom_nx_upd_gpio_write_s
{
  uint8_t gpioHcomId;   // 1- n
  uint8_t cmdValue;   // 1 = high, 0 = low
};

struct hcom_nx_upd_gpio_output_map_s
{
  uint32_t gpio_output_defn;
};

struct hcom_nx_upd_gpio_input_map_s
{
  uint32_t gpio_input_defn;
};

struct hcom_nx_upd_gpio_diag_set_byte_s
{
  uint8_t rangeId;    // 0, 1 etc.
  uint8_t byteValue;    // The byte to output
};

struct hcom_nx_upd_uart_reconfig_s
{
  uint32_t uart_id;
};

//==================================================
// hcom nx upd ioctl commands
#define HCOM_NX_UPD_SET_REGISTER                1
#define HCOM_NX_UPD_GET_REGISTER                2
#define HCOM_NX_UPD_UPDATE_REGISTER             3
#define HCOM_NX_UPD_SET_BBR_VALUE               4
#define HCOM_NX_UPD_GET_BBR_VALUE               5
#define HCOM_NX_UPD_UPDATE_BBR_VALUE            6
#define HCOM_NX_UPD_CLI_COMMAND                 7
#define HCOM_NX_UPD_GET_MCU_ID                  8
#define HCOM_NX_UPD_IS_PART_MOUNTED             9
#define HCOM_NX_UPD_ESP32_ENTER_PROG_MODE       10
#define HCOM_NX_UPD_RESTORE_UART_CONFIG         11
#define HCOM_NX_UPD_ESP32_RESTART_ESP32         12
#define HCOM_NX_UPD_GPIO_COMMAND                13
#define HCOM_NX_UPD_GPIO_CONFIG                 14
#define HCOM_NX_UPD_DIAG_GPIO_COMMAND           15
#define HCOM_NX_UPD_DIAG_GPIO_CONFIG            16
#define HCOM_NX_UPD_DIAG_GPIO_SET_BYTE          17

// These define GPIOs that cannot be access from
// the apps side
#define HCOM_GPIO_DIGITAL_CONFIG_INPUT        1
#define HCOM_GPIO_DIGITAL_CONFIG_OUTPUT       0
#define HCOM_GPIO_DIGITAL_CMD_VALUE_HIGH      1
#define HCOM_GPIO_DIGITAL_CMD_VALUE_LOW       0

#endif  // __INCLUDE_MEADOW_HCOM_NX_SHARED__H