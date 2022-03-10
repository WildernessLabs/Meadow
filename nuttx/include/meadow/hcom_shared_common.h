/*************************************************************************
 * \include\meadow\hcom_shared_common.h
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
#ifndef __INCLUDE_MEADOW_HCOM_SHARED_COMMON__H
#define __INCLUDE_MEADOW_HCOM_SHARED_COMMON__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <unistd.h>
#include <nuttx/semaphore.h>

/****************************************************************************
 * Shared enums.
 ****************************************************************************/

/****************************************************************************
 * External definitions.
 ****************************************************************************/

/****************************************************************************
 * Private defines
 ****************************************************************************/
// This header file contains those items that must be shared between apps and
// nuttx sides

#ifndef OK
  #define OK 0
#endif

#ifndef MIN
#  define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#  define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

// Partition Id may be postpended to /meadow (i.e /meadow0)
// Note:The following string must fit into 1/2 of the buffer whose size is
// defined by HCOM_NX_MAX_PATH_AND_FILE_BUFF_LENGTH
#define HCOM_FILE_MOUNT_POINT_TARGET "/meadow"

// Partitioning changes will effect the following
#ifdef CONFIG_MTD_PARTITION
#define MONO_MEADOW_EXECUTABLE_PARTITION_NAME "/meadow0"
#define MONO_MEADOW_EXECUTABLE_APP_EXE "/meadow0/App.exe"
#else
#define MONO_MEADOW_EXECUTABLE_PARTITION_NAME "/meadow"
#define MONO_MEADOW_EXECUTABLE_APP_EXE "/meadow/App.exe"
#endif

//==================================================
// Host text message buffer sizes for text messages
#define HCOM_SHORT_HOST_STRING_BUFF_LENGTH 128                  // automatic variable
#define HCOM_MAX_HOST_STRING_BUFF_LENGTH 2048                   // allocate
// PATH_MAX is defined by Nuttx in limits.h. It's 256 or less
#define HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH ((PATH_MAX * 2) + 2) // allocate

//==================================================
// Default name of meadow configuration file
// Only the default file name is case sensitive.
// All other INI CFG items are case insensitive
#define MEADOW_CONFIG_DEFAULT_FILE_NAME "/meadow0/meadow.config.yaml"
#define MEADOW_WIFI_CREDENTIALS_DEFAULT_FILE_NAME "/meadow0/wifi.config.yaml"
#define MEADOW_CONFIG_DEFAULT_DEVICE_NAME "MeadowF7"

//==================================================
//  Structure to hold the configuration of the Meadow board.
struct meadow_configuration_s
{
  /**
   *  @brief Using default configuration because the configuration file cannot be found
   *         or there was a problem reading the configuration file.
   */
  int using_default_configuration;
  
  /*
   *  Should mono be run at startup?
   */
  int disable_mono;

  /**
   *  @brief Options to be passed to the Mono runtime system when the
   *         applications is started.
   */
  char *mono_options;

  /*
   *  Should the ESP32 be reset at startup.  This is used by developers to prevent
   *  STM32 code from resetting the ESP32 and disconnecting the debugger.
   */
  int reset_esp32_at_startup;

  /*
   *  Reason for the last ESP32 restart.
   */
  uint8_t esp32_reset_reason;

  /*
   *  Level of trace output to generate.
   */
  int trace_level;

  /*
   *  Should trace output be diverted to UART1?
   */
  uint8_t use_uart1_for_trace;

  /*
   *  Clock speed of the SPI interface between the STM32 and the ESP32.
   */
  uint32_t esp_spi_speed;

  /*
   *  Name of the board.
   */
  char *device_name;

  /*
   *  Version of the software running on the ESP32.
   */
  char *esp_software_version;

  /*
   *  Mono version.
   */
  uint32_t mono_version;

  /*
   *  Version of the software running on the STM32.
   */
  char *meadow_software_version;

  /*
   *  Meadow hardware version software is executing on.
   *
   *  Note that this is normally NULL except when passing the version
   *  information from kernel space to HCOM in user space.
   */
  char *meadow_hardware_version;

  /**
   *  @brief Hardware version number.
   */
  int hardware_version;

  /*
   *  Serial number of the STM32 microcontroller.
   */
  uint8_t serial_number[16];

  /*
   *  ID of the STM32 microprocessor.
   */
  uint8_t chip_id[12];

  /**
   *  @brief Deault access point (used with the automatically_start_network property).
   */
  char *default_access_point;

  /**
   *  @brief Get network time at startup?
   */
  uint8_t get_network_time_at_startup;

  /**
   *  @brief Network time servers and the number of servers in the list.
   */
  char **ntp_servers;
  uint32_t ntp_servers_count;

  /**
   *  @brief Number of seconds between time updates from the NTP server.
   */
  uint32_t ntp_refresh_period;

  /**
   *  @brief Automatically start the network?
   */
  uint8_t automatically_start_network;

  /**
   *  @brief Automatically reconnect to the preconfigured access point?
   */
  uint8_t automatically_reconnect;

  /**
   * @brief MAC address of the board. 
   */
  uint8_t board_mac_address[6];

  /**
   * @brief MAC address of the soft access point. 
   */  
  uint8_t soft_ap_mac_address[6];

  /**
   *  @brief Number of retries for connects etc before the system gives up
   *         and returns an error code.
   */  
  uint32_t maximum_retry_count;
};
typedef struct meadow_configuration_s meadow_configuration_t;

//
//  Default NTP server to be used if none is specified.
//
#define NTP_DEFAULT_SERVER0 "0.pool.ntp.org"
#define NTP_DEFAULT_SERVER1 "1.pool.ntp.org"
#define NTP_DEFAULT_SERVER2 "2.pool.ntp.org"
#define NTP_DEFAULT_SERVER3 "3.pool.ntp.org"

//
//  Default DNS server.
//
#define DNS_DEFAULT_SERVER "1.1.1.1"

//
//  Default period (seconds) between time freshes from the NTP server.
//
#define NTP_DEFAULT_REFRESH_PERIOD 3600

//
//  Minimum number of seconds that can be used for the time refresh period.
//
#define NTP_MINIMUM_REFRESH_PERIOD 60

//
//  Number of seconds between retry attempts if the time could not be read
//  from the time server.
//
#define NTP_DEFAULT_ERROR_RETRY_PERIOD 10

//==================================================
// These identify the 3 stm32f7 uarts used by meadow
#define MEADOW_RECONFIG_MISCONFIGURED_UART1 1
#define MEADOW_RECONFIG_MISCONFIGURED_UART4 4
#define MEADOW_RECONFIG_MISCONFIGURED_UART5 5
#define MEADOW_RECONFIG_MISCONFIGURED_UART6 6

//==================================================
// hcom nx upd ioctl commands
// Augments the normal Nuttx LOG_XXXX list
#define LOG_NONE                         0xff

//--------------------------------------------------------------------
// These needed Meadow features can be excluded from a build by
// using the make menuconfig 'Board Selection' option.
// To enable/disable remote debugging use CONFIG_HCOM_MONO_REMOTE_DEBUGGING 
// To enable/disable stdout and stder use CONFIG_HCOM_MONO_STDERR_STDOUT

//--------------------------------------------------------------------
// The following control diagnostics that can be added to the built
//
// When set to 1 the syslog mask is set for all tracing except for
// debug. At startup syslog messages are routed to UART1 without
// the need for configuration or the CLI Uart Trace command.
#define HCOM_FORCE_SYSLOG_MASK_AND_OUTPUT_TO_UART1    0

// Cause the build to include the ability to print a buffer
// full of data, showing hex and ascii. Duplicate code is created
// on both the apps and nuttx side of hcom
#define HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE           0

// Outputs to syslog the PID of each new thread
#define HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS    0

// Ease the understanding of a startup that never finishes
#define HCOM_DIAG_INCLUDE_STARTUP_SYSLOG              0

// Should mono be prevented from running?
#define HCOM_DIAG_PREVENT_MONO_FROM_RUNNING           0

// Adds code that takes the HCOM messages from CLI and outputs
// a decoded version to syslog
#define HCOM_DIAG_INCLUDE_MESSAGE_DECODING_IN_BUILD   0

// LOG_DEBUG syslog message are almost never used. Set this to 1
// if you wish to have them compiled into Meadow
#define HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD          0

//-------------------------------------------------------------------
// Include test code
#define HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD      0

#define HCOM_INCLUDE_BATTERY_BACKED_REG_TEST          0

// Include the network tests in the build ?
#define HCOM_INCLUDE_ESPCP_TESTS                      0

#define HCOM_INCLUDE_QSPI_FLASH_TESTS_IN_BUILD        0

// snprintf behavior is platform dependent. These tests reveal the Nuttx
// behavior. HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE is needed, see above.
#define HCOM_INCLUDE_SNPRINTF_ON_NUTTX_TESTS_IN_BUILD 0

// Include tests for SDCard operation
#define HCOM_INCLUDE_SD_CARD_TESTS_IN_BUILD           0

// Include some simple gpio tests
#define HCOM_INCLUDE_GPIO_DIAG_TESTS_IN_BUILD         0

// Include a test that allows the MCU to be overloaded
#define HCOM_INCLUDE_OVERLOAD_MCU_TESTS_IN_BUILD      0

// Configured within a menuconfig Kconfig file
#if defined (CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)

// Include a test that allows the F7 to provide an echo
// chat TCP/IP server.  This code is on the Apps side of
// Nuttx.
#define MEADOW_ETHERNET_INCLUDE_CHAT_TEST_IN_BUILD    0
#else
#define MEADOW_ETHERNET_INCLUDE_CHAT_TEST_IN_BUILD    0 // Always 0
#endif

#endif  // __INCLUDE_MEADOW_HCOM_SHARED_COMMON__H
