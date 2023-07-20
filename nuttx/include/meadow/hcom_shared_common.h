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
#include <nuttx/net/net.h>
#include <nuttx/semaphore.h>
#include <meadow/hcom_protocol.h>

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
#define MONO_MEADOW_EXECUTABLE_APP_EXE "/meadow0/Meadow.dll"
#else
#define MONO_MEADOW_EXECUTABLE_PARTITION_NAME "/meadow"
#define MONO_MEADOW_EXECUTABLE_APP_EXE "/meadow/Meadow.dll"
#endif

#define HCOM_NX_FS_MONO_RAW_PARTITION_SIZE 0x300000 // 3MB
#define HCOM_NX_FS_OTA_RESERVED_SPACE 0x200000 // 2MB reserved space for updates
#define HCOM_NX_FS_NUTTX_UPDATE_SIZE 0x1C0000   // (2MB - 256KB)

//==================================================
// Host text message buffer sizes for text messages
#define HCOM_TINY_HOST_STRING_BUFF_LENGTH 64        // automatic variable
#define HCOM_SHORT_HOST_STRING_BUFF_LENGTH 128      // automatic variable
#define HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH 144  // automatic variable
#define HCOM_MAX_HOST_STRING_BUFF_LENGTH 2048       // allocate
// PATH_MAX is defined by Nuttx in limits.h. It's 256 or less
#define HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH ((PATH_MAX * 2) + 2) // allocate

//==================================================
// Default name of meadow configuration file
// Only the default file name is case sensitive.
// All other INI CFG items are case insensitive
#define MEADOW_CONFIG_DEFAULT_FILE_NAME "/meadow0/meadow.config.yaml"
#define MEADOW_WIFI_CREDENTIALS_DEFAULT_FILE_NAME "/meadow0/wifi.config.yaml"
#define MEADOW_CELL_CONFIG_DEFAULT_FILE_NAME "/meadow0/cell.config.yaml"
#define MEADOW_CONFIG_DEFAULT_DEVICE_NAME "MeadowF7"

//==================================================
// Meadow file logging defintions.
#define MEADOW_LOGGING_OS_FILE_NAME    "/meadow0/meadow.log"

#define HCOM_NX_FS_NUTTX_UPDATE_FILENAME "Meadow.OS.Update.bin"
#define HCOM_NX_FS_MONO_RUNTIME_FILENAME "Meadow.OS.Runtime.bin"
#define UPDATE_DIR "/meadow0/update/"
#define UPDATE_APP_DIR UPDATE_DIR "app/"
#define UPDATE_OS_DIR UPDATE_DIR "os/"
#define ROLLBACK_DIR "/meadow0/rollback"

//==================================================
//  Network interface types.
//
//  These values are flag values.
#define MEADOW_IFT_UNKNOWN          0xffffffff
#define MEADOW_IFT_UNKNOWN_NAME     "Unknown"
#define MEADOW_IFT_ESP32            0x00000000
#define MEADOW_IFT_ESP32_NAME       "WiFi"
#define MEADOW_IFT_ETHERNET         0x00000001
#define MEADOW_IFT_ETHERNET_NAME    "Ethernet"
#define MEADOW_IFT_CELL             0x00000002
#define MEADOW_IFT_CELL_NAME        "Cell"

//==================================================
//  Cell module models.
//
//  These values are flag values.
#define CELL_UNKNOWN_MODULE          0xffffffff
#define CELL_UNKNOWN_MODULE_NAME     "Unknown"
#define CELL_BG770A_MODULE           0x00000000
#define CELL_BG770A_MODULE_NAME      "BG770A"
#define CELL_M95_MODULE              0x00000001
#define CELL_M95_MODULE_NAME         "M95"
#define CELL_BG95M3_MODULE           0x00000002
#define CELL_BG95M3_MODULE_NAME      "BG95M3"

//  Cell network operation modes.
//
//  These values are flag values.
#define CELL_UNKNOWN_MODE           0xffffffff
#define CELL_UNKNOWN_MODE_NAME      "Unknown"
#define CELL_CATM1_MODE             0x00000000
#define CELL_CATM1_MODE_NAME        "CATM1"
#define CELL_NBIOT_MODE             0x00000001
#define CELL_NBIOT_MODE_NAME        "NBIOT"
#define CELL_GSM_MODE               0x00000002
#define CELL_GSM_MODE_NAME          "GSM"

//  Meadow F7FeatherV2 pin names
#define F7_MICRO_V2_A00_PIN_NAME "A00"
#define F7_MICRO_V2_A01_PIN_NAME "A01"
#define F7_MICRO_V2_A02_PIN_NAME "A02"
#define F7_MICRO_V2_A03_PIN_NAME "A03"
#define F7_MICRO_V2_A04_PIN_NAME "A04"
#define F7_MICRO_V2_A05_PIN_NAME "A05"
#define F7_MICRO_V2_D00_PIN_NAME "D00"
#define F7_MICRO_V2_D01_PIN_NAME "D01"
#define F7_MICRO_V2_D02_PIN_NAME "D02"
#define F7_MICRO_V2_D03_PIN_NAME "D03"
#define F7_MICRO_V2_D04_PIN_NAME "D04"
#define F7_MICRO_V2_D05_PIN_NAME "D05"
#define F7_MICRO_V2_D06_PIN_NAME "D06"
#define F7_MICRO_V2_D07_PIN_NAME "D07"
#define F7_MICRO_V2_D08_PIN_NAME "D08"
#define F7_MICRO_V2_D09_PIN_NAME "D09"
#define F7_MICRO_V2_D10_PIN_NAME "D10"
#define F7_MICRO_V2_D11_PIN_NAME "D11"
#define F7_MICRO_V2_D12_PIN_NAME "D12"
#define F7_MICRO_V2_D13_PIN_NAME "D13"
#define F7_MICRO_V2_D14_PIN_NAME "D14"
#define F7_MICRO_V2_D15_PIN_NAME "D15"

//  Correspondent MCU pin names for F7FeatherV2
#define F7_MICRO_V2_A00_PIN GPIO_PORTA | GPIO_PIN4
#define F7_MICRO_V2_A01_PIN GPIO_PORTA | GPIO_PIN5
#define F7_MICRO_V2_A02_PIN GPIO_PORTA | GPIO_PIN3
#define F7_MICRO_V2_A03_PIN GPIO_PORTB | GPIO_PIN0
#define F7_MICRO_V2_A04_PIN GPIO_PORTB | GPIO_PIN1
#define F7_MICRO_V2_A05_PIN GPIO_PORTC | GPIO_PIN0
#define F7_MICRO_V2_D00_PIN GPIO_PORTI | GPIO_PIN9
#define F7_MICRO_V2_D01_PIN GPIO_PORTH | GPIO_PIN13
#define F7_MICRO_V2_D02_PIN GPIO_PORTH | GPIO_PIN10
#define F7_MICRO_V2_D03_PIN GPIO_PORTB | GPIO_PIN8
#define F7_MICRO_V2_D04_PIN GPIO_PORTB | GPIO_PIN9
#define F7_MICRO_V2_D05_PIN GPIO_PORTB | GPIO_PIN4
#define F7_MICRO_V2_D06_PIN GPIO_PORTB | GPIO_PIN13
#define F7_MICRO_V2_D07_PIN GPIO_PORTB | GPIO_PIN7
#define F7_MICRO_V2_D08_PIN GPIO_PORTB | GPIO_PIN6
#define F7_MICRO_V2_D09_PIN GPIO_PORTC | GPIO_PIN6
#define F7_MICRO_V2_D10_PIN GPIO_PORTC | GPIO_PIN7
#define F7_MICRO_V2_D11_PIN GPIO_PORTC | GPIO_PIN9
#define F7_MICRO_V2_D12_PIN GPIO_PORTB | GPIO_PIN14
#define F7_MICRO_V2_D13_PIN GPIO_PORTB | GPIO_PIN15
#define F7_MICRO_V2_D14_PIN GPIO_PORTB | GPIO_PIN12
#define F7_MICRO_V2_D15_PIN GPIO_PORTG | GPIO_PIN12

//==================================================
//  Structure to hold cell network interface information
struct cell_settings_s
{
  /**
   *  @brief Default name module (i.e BG770A, M95, BG95M3).
   */
  char* module;
  
  /**
   *  @brief Default module id.
   */
  uint32_t module_id;

  /**
   *  @brief Default cell access point name (APN).
   */
  char* apn;

  /**
   *  @brief Default cell numeric operator code (i.e. 72410).
   */
  char* operator;

  /**
   *  @brief Default IoT operation mode (e.g, NBIoT, CatM1, GSM)
   */
  char* mode;

  /**
   *  @brief Default IoT operation mode id
   */
  uint32_t mode_id;

  /**
   *  @brief Default interface name used in the communication 
   *  with the cell module.
   */
  char* ttyname;

  /**
   *  @brief Default Meadow device pin name used to turn on the
   *  cell modules (e.g, C7)
   */
  char* turn_on_pin_name;

  /**
   *  @brief Default Meadow device pin used to turn on the
   *  cell modules
   */
  uint32_t* turn_on_pin;

  /**
   *  @brief Default chat app timeout in seconds, used to 
   * define how long to wait for the modem response.
   */
  char* timeout;

  /**
  *  @brief Default cell PAP authentication user.
  */
  char* pap_user;

  /**
   *  @brief Default cell PAP authentication password.
   */
  char* pap_password;
};
typedef struct cell_settings_s cell_settings_t;

//==================================================
//  Structure to hold network interface information
struct meadow_network_interface_s
{
  /**
   *  @brief Network interface type (see MEADOW_IFT_* constants).
   */
  uint32_t interface_type;

  /**
   * @brief Name used to identify this interface.
   */
  char *name;

  /**
   *  @brief Use a DHCP server?
   */
  int32_t use_dhcp;

  /**
   *  @brief IP address.
   */
  uint32_t ip_address;

  /**
   *  @brief Subnet mask.
   */
  uint32_t netmask;

  /**
   *  @brief Default gateway.
   */
  uint32_t gateway;

  /**
   * @brief Pointer to the psock methods
   */
  const struct sock_intf_s *psock_methods;
};
typedef struct meadow_network_interface_s meadow_network_interface_t;

//==================================================
//  Structure to hold a version number.

/**
 * @brief Structure hold a version number as component parts.
 * 
 *  The version number is assumed to be of the format:
 * 
 *    major.minor.patch.build
 */
struct meadow_version_number_s
{
  /**
   * @brief Major part of the version number. 
   */
  uint32_t major;

  /**
   * @brief Minor part of the version number.
   */
  uint32_t minor;

  /**
   * @brief Patch part of the version number.
   */
  uint32_t revision;

  /**
   * @brief Build part of the version number.
   */
  uint32_t build;

  /**
   * @brief Day of the build.
   */
  uint8_t day;

  /**
   * @brief Month of the build.
   */
  uint8_t month;

  /**
   * @brief Three day month text of the build.
   */
  char month_text[4];

  /**
   * @brief Year of the build.
   */
  uint8_t year;

  /**
   * @brief Hour of the build.
   */
  uint8_t hour;

  /**
   * @brief Minute of the build.
   */
  uint8_t minute;

  /**
   * @brief Second of the build.
   */
  uint8_t second;

  /**
   * @brief Git hash at the time of the build.
   */
  uint32_t hash;

  /**
   * @brief Name of the branch used in this build.
   */
  char *branch_name;

  /**
   * @brief Short version string (%d.%d.%d.%d)
   */
  char *short_string;

  /**
   * @brief Long version string (%d.%d.%d.%d, built %02d %s 20%02d %02d:%02d:%02d UTC (%08x/%s))
   */
  char *long_string;
};
typedef struct meadow_version_number_s meadow_version_number_t;

//  Structure to hold the configuration of the Meadow board.
struct meadow_configuration_s
{
  /**
   *  @brief Using default configuration because the configuration file cannot be found
   *         or there was a problem reading the configuration file.
   */
  int using_default_configuration;
  
  /**
   *  @brief Options to be passed to the Mono runtime system when the
   *         applications is started.
   */
  char *mono_options;

  /**
   *  @brief Should the ESP32 be reset at startup.  This is used by developers to prevent
   *         STM32 code from resetting the ESP32 and disconnecting the debugger.
   */
  int reset_esp32_at_startup;

  /**
   *  @brief Reason for the last ESP32 restart.
   */
  uint8_t esp32_reset_reason;

  /**
   *  @brief Level of trace output to generate.
   */
  int trace_level;

  /**
   *  @brief Should trace output be diverted to UART1?
   */
  uint8_t use_uart1_for_trace;

  /**
   *  @brief Clock speed of the SPI interface between the STM32 and the ESP32.
   */
  uint32_t esp_spi_speed_hz;

  /**
   *  @brief Name of the board.
   */
  char *device_name;

  /**
   *  @brief Should the system reboot if the .NET application encounter an unhandled exception?
   */
  uint8_t reboot_on_unhandled_exceptions;

  /**
   *  @brief Maximum amount of time the initialisation method in the .NET application can run
   *         before it is assumed to have failed.
   */
  uint32_t initialisation_timeout_seconds;

  /**
   * @brief Does the system have SD card hardware installed (CCM).
   */
  uint8_t sd_storage_supported;

  /**
   * @brief Names of any reserved pins.
   */
  char *reserved_pins;

  /**
   * @brief Operating system version information.
   */
  meadow_version_number_t os_version;

  /**
   *  @brief Mono version.
   */
  meadow_version_number_t mono_version;

  /**
   *  @brief ESP32 firmware version.
   */
  meadow_version_number_t esp_version;

  /**
   *  @brief Meadow hardware version software is executing on.
   *
   *  Note that this is normally NULL except when passing the version
   *  information from kernel space to HCOM in user space.
   */
  char *hardware_version_text;

  /**
   *  @brief Hardware version number.
   */
  int hardware_version;

  /**
   *  @brief Serial number of the STM32 microcontroller.
   */
  uint8_t serial_number[16];

  /**
   *  @brief ID of the STM32 microprocessor.
   */
  uint8_t chip_id[12];

  /**
   *  @brief Point to the structure holding the default network interface information.
   */
  meadow_network_interface_t *default_interface;

  /**
   *  @brief Default access point (used with the automatically_start_network property).
   */
  char *default_access_point;

  /**
   *  @brief Default cell network interface settings
   */
  cell_settings_t *default_cell_settings;
  
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
  uint32_t ntp_refresh_period_seconds;

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

/**
 * @brief Mono signature held in the runtime (see user-space.ld).
 */
struct mono_signature_s
{
  /**
   * @brief Signature to verify that the this is Mono runtime.
   */
  uint32_t signature;

  /**
   * @brief Build number.
   */
  uint32_t build;

  /**
   * @brief Build revision.
   */
  uint32_t revision;

  /**
   * @brief Build minor number.
   */
  uint32_t minor;

  /**
   * @brief Build major number.
   */
  uint32_t major;

  /**
   * @brief Day of the build.
   */
  uint8_t day;

  /**
   * @brief Month of the build.
   */
  uint8_t month;

  /**
   * @brief Year of the build.
   */
  uint8_t year;

  /**
   * @brief Hour of the build.
   */
  uint8_t hour;

  /**
   * @brief Minute of the build.
   */
  uint8_t minute;

  /**
   * @brief Second of the build.
   */
  uint8_t second;

  /**
   * @brief Git has of this build.
   */
  uint32_t hash;

  /**
   * @brief First character of the branch used for this build.
   * 
   * This is actually a byte array (null terminated string).
   */
  char start_of_branch_string;
} __attribute__((packed));
typedef struct mono_signature_s mono_signature_t;

//
//  The three options below define the possible Mono options that can be used
//  to control the run mode of the application.
//
#define MONO_OPTION_JIT       "--jit"
#define MONO_OPTION_AOT       "--aot"
#define MONO_OPTION_INTERP    "--interp"
#define MONO_OPTION_SDB	      "--soft-breakpoints"

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

//
//  Default speed (in Hz) for the SPI bus connecting the STM and ESP chips.
//
#define DEFAULT_STM_ESP_SPI_SPEED 8000000UL

//
//  How long should the runtime allow the initialisation method to execute before
//  system should restart (i.e. assume the initialisation has stalled).
//
#define DEFAULT_INITIALISATION_TIMEOUT_SECONDS 60

//
//  How long should be the chat script timeout (in seconds), which is used in the
/// PPPD app to communicate to the modem, before restarting the chat script.
//
#define DEFAULT_CELL_PPPD_TIMEOUT "30"

//
//  Default interface name used to communicate with the cell module.
//
#define DEFAULT_CELL_INTERFACE "/dev/ttyS1"

//
//  Default turn on pin used to activate the cell module.
//
#define DEFAULT_CELL_TURN_ON_PIN "D10"

//
//  Default cell network operation mode
//
#define DEFAULT_CELL_MODE ""

//
//  Default cell operator numeric code
//
#define DEFAULT_CELL_OPERATOR ""

//
//  Default Cell PAP authentication user
//
#define DEFAULT_CELL_PAP_USER ""

//
//  Default Cell PAP authentication password
//
#define DEFAULT_CELL_PAP_PASSWORD ""

//==================================================
// These identify the stm32f7 uarts used by meadow
#define MEADOW_RECONFIG_MISCONFIGURED_UART1 1
#define MEADOW_RECONFIG_MISCONFIGURED_UART4 4
#define MEADOW_RECONFIG_MISCONFIGURED_UART5 5
#define MEADOW_RECONFIG_MISCONFIGURED_UART6 6

//==================================================
// hcom nx upd ioctl commands
// Augments the normal Nuttx LOG_XXXX list
#define LOG_NONE                         0xff

// typedef for sending messages to host (e.g. CLI) from nuttx side
typedef int (* send_host_std_msg_data)(HcomProtoHdrMsg_t *hdrMsg,
          size_t totalMsgLen, char *sourceFileName, int sourceLineNumber);

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
 // To output non-null terminated string. This won't work if binary in buffer
 // syslog(2, "%.*s\n", textLen, buffer);

// Outputs to syslog the PID of each new thread
#define HCOM_DIAG_OUTPUT_SYSLOG_PID_OF_NEW_THREADS    0

// Ease the understanding of a startup that never finishes
#define HCOM_DIAG_INCLUDE_STARTUP_SYSLOG              0

// Should mono be prevented from running?
#define HCOM_DIAG_PREVENT_MONO_FROM_RUNNING           0

// Adds code that takes the HCOM messages from CLI and outputs
// a decoded version to syslog enable
// HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE to add hex dump of HCOM messages
#define HCOM_DIAG_INCLUDE_MESSAGE_DECODING_IN_BUILD   0

// LOG_DEBUG syslog message are almost never used. Set this to 1
// if you wish to have them compiled into Meadow
#define HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD          0

//-------------------------------------------------------------------
// Include test code
#define HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD      0

#define HCOM_INCLUDE_QSPI_FLASH_TESTS_IN_BUILD        0

// Include tests related to F7 timers
#define MEADOW_INCLUDE_TIMER_HARDWARE_TESTS_IN_BUILD  0

#define MEADOW_INCLUDE_ETHERNET_CHAT_TESTS_IN_BUILD   0

#endif  // __INCLUDE_MEADOW_HCOM_SHARED_COMMON__H
