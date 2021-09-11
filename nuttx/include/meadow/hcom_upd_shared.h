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
#include <meadow/hcom_protocol.h>

/****************************************************************************
 * Private defines
 ****************************************************************************/
// This header file contains those items that must be shared by apps/hcom
// #include <string.h>

#define HCOM_NX_UPD_DRIVER_NAME "/dev/nxupd"

// Defining buffer sizes
#define HCOM_NX_CMD_HOST_MSG_SIZE     128
#define HCOM_NX_CMD_LOG_MSG_SIZE      128

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
  uint32_t gpioPinDefn;
  int result;
};

struct hcom_nx_upd_gpio_write_s
{
  uint32_t gpioPinDefn;
  bool cmdValue;   // 1 = high, 0 = low
};

struct hcom_nx_upd_uart_reconfig_s
{
  uint32_t uart_id;
};

struct hcom_nx_upd_diag_fd_inode_s
{
  uint32_t fileDescriptor;
  struct inode *inodeAddr;
};

struct hcom_nx_upd_mcu_ser_numb_s
{
  char *ser_numb;
};

struct hcom_nx_upd_ini_cfg_get_value_s
{
  char *file_name;
  char *section_name;
  char *key_name;
  char *return_value;
  int return_size;
};

struct hcom_nx_upd_ini_cfg_get_match_s
{
  char *file_name;
  char *section_name;
  char *key_name;
  char *return_error;
  char *match_value;
  bool return_bool;
};

struct hcom_nx_upd_ini_cfg_get_int_defval_s
{
  char *file_name;
  char *section_name;
  char *key_name;
  char *return_error;
  int default_value;
  int return_int;
};

typedef struct hcom_nx_upd_cli_trace_transport_s
{
  char * transport_buf;
  size_t buf_length;
  size_t msg_length;
} hcom_nx_upd_cli_trace_transport_t;

typedef struct hcom_nx_upd_host_text_transport_s
{
  char *transport_buf;
  uint16_t *requestType;
  size_t buf_length;
  size_t msg_length;    // Returned
} hcom_nx_upd_host_text_transport_t;

typedef struct hcom_nx_upd_get_hw_ver_s
{
  uint32_t hwVer;

} hcom_nx_upd_get_hw_ver_t;

typedef struct hcom_nx_upd_diag_app_command_s
{
  const HcomProtoHdrMsg_t *hdrMsg;
  size_t msgLen;

} hcom_nx_upd_diag_app_command_t;

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
#define HCOM_NX_UPD_DIAG_FD_INODE               15
#define HCOM_NX_UPD_GET_MCU_SER_NUMB            16
#define HCOM_NX_UPD_START_ESPCP_RUNNING         17
#define HCOM_NX_UPD_EXECUTE_ESPCP_TESTS         18
#define HCOM_NX_UPD_ENTER_INTO_DFU_MODE         19
#define HCOM_NX_UPD_HOST_RESTART_MEADOW_MCU     20
#define HCOM_NX_UPD_ONLY_RESTART_MEADOW_MCU     21
#define HCOM_NX_UPD_GET_CONFIG                  22
#define HCOM_NX_UPD_GET_STRING                  23
#define HCOM_NX_UPD_MONO_HAS_STARTED            24
#define HCOM_NX_UPD_CLI_TRACE_TRANSPORT         25
#define HCOM_NX_UPD_HOST_TEXT_TRANSPORT         26
#define HCOM_NX_UPD_GET_HW_VERSION              27
#define HCOM_NX_UPD_FLASH_OS_UPDATE             28
#define HCOM_NX_UPD_DIAG_APP_CMD                29

#endif  // __INCLUDE_MEADOW_HCOM_NX_SHARED__H
