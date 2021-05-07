/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\hcom_nx_upd.c
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

// This file was inspired by (and copied from) Chris Tacke's
// 'Universal Platform Driver' (Meadow-upd.c). Thanks Chris!
// This allows the apps side to call the nuttx side safely.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>

#include <nuttx/config.h>

#include <nuttx/fs/fs.h>
#include <nuttx/kmalloc.h>
#include <arch/board/board.h>

#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include "chip.h"
#include "fcntl.h"

#include <dirent.h>
#include <sys/ioctl.h>
#include "stm32_uid.h" // stm32_get_uniqueid()

#include <nuttx/board.h>
#include <arch/board/board.h>
#include "stm32_gpio.h"
#include "stm32f777zit6-meadow.h"

#include <sched.h>
#include "sched/sched.h"

#include <meadow/hcom_upd_shared.h>
#include "hcom_nx_common.h"
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/hcom_nuttx_shared.h>
#include <meadow/hcom_gpio_defn_diag.h>
#include "../inicfg/meadow_inicfg.h"

#include "diag/hcom_nx_upd_diag.h"
#include "../espcp/espcp_coprocessor.h"
#include "../espcp/espcp_usrsock.h"
#include "hcom_nx_config_manager.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_upd_nx_open(struct file *filep);
static int hcom_upd_nx_close(struct file *filep);
static int hcom_upd_nx_ioctl(FAR struct file *filep, int cmd, unsigned long arg);

// Added read functionality because open failed without it. The open flag O_RDONLY == 0.
static int hcom_upd_nx_read(FAR struct file *filep, FAR char *buffer, size_t buflen);
static int hcom_nx_upd_execute_gpio_config(unsigned long arg);
static int hcom_nx_upd_execute_gpio_write(unsigned long arg);
static int hcom_nx_restore_uart_reconfig(unsigned long arg);
static int hcom_nx_upd_diag_fd_inode(unsigned long arg);
static int hcom_nx_get_mcu_ser_numb(unsigned long arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static const struct file_operations g_hcom_nx_operations =
{
    .open = hcom_upd_nx_open,
    .close = hcom_upd_nx_close,
    .read = hcom_upd_nx_read,
    .ioctl = hcom_upd_nx_ioctl
};

// The next 2 tables eliminate switch statements by providing
// a lookup table. The order of the entries matches their shared
// values 0 - n
static struct hcom_nx_upd_gpio_output_map_s gpioOutputDefnArray[] =
{
    // Defined in board.h                     // Defined in hcom_shared_common.h
    // Provide the GPIO definition            // Provide the relative offset
    {MEADOW_ESP32_ONBOARD_RESET_PIN_OUTPUT},  // 0 HCOM_NX_GPIO_DIG_ID_ESP_RESET
    {MEADOW_ESP32_ONBOARD_BOOT_PIN_OUTPUT},   // 1 HCOM_NX_GPIO_DIG_ID_ESP_BOOT
    // Defined in stm32f777zit6-meadow.h
    {GPIO_LED_BLUE},                          // 2 HCOM_NX_GPIO_DIG_ID_BLUE_LED
};

#define HCOM_NUMBER_OF_GPIO_OUTPUT_MAP_ELEMENTS (sizeof(gpioOutputDefnArray) / sizeof(struct hcom_nx_upd_gpio_output_map_s))

static struct hcom_nx_upd_gpio_input_map_s gpioInputDefnArray[] =
{
    // Defined in board.h
    {MEADOW_ESP32_ONBOARD_RESET_PIN_INPUT},
    {MEADOW_ESP32_ONBOARD_BOOT_PIN_INPUT}
};

#define HCOM_NUMBER_OF_GPIO_INPUT_MAP_ELEMENTS (sizeof(gpioInputDefnArray) / sizeof(struct hcom_nx_upd_gpio_input_map_s))

// ====================================================================
// Called when hcom nuttx upd driver is opened
int hcom_upd_nx_open(struct file *filep)
{
  return OK;
}

// ====================================================================
// Called when hcom nuttx upd driver is closed
int hcom_upd_nx_close(struct file *filep)
{
  return OK;
}

// ====================================================================
int hcom_upd_nx_read(FAR struct file *filep, FAR char *buffer, size_t buflen)
{
  return OK;
}

// ====================================================================
// Note ioctl calls put any returned value into errno and the returned int
// is set to -1
static int hcom_upd_nx_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  int ret;
  bool retBool;
  int  retInt;
  int length;
  struct hcom_nx_upd_register_value *register_val;
  struct hcom_nx_upd_register_update *register_update;
  struct hcom_nx_upd_bbr_value *bbr_val;
  struct hcom_nx_upd_bbr_update *bbr_update;
  struct hcom_nx_cmd_data *cmdData;
  struct hcom_nx_upd_is_part_mounted *is_mounted;
  struct hcom_nx_upd_ini_cfg_get_value_s *get_cfg_value;
  struct hcom_nx_upd_ini_cfg_get_match_s *is_cfg_match;
  struct hcom_nx_upd_ini_cfg_get_int_defval_s *get_cfg_int;

  switch (cmd)
  {
  // This work with any register
  case HCOM_NX_UPD_SET_REGISTER:
    register_val = (struct hcom_nx_upd_register_value *)arg;
    putreg32(register_val->value, register_val->address);
    return OK;
  case HCOM_NX_UPD_GET_REGISTER:
    register_val = (struct hcom_nx_upd_register_value *)arg;
    register_val->value = getreg32(register_val->address);
    return OK;
  case HCOM_NX_UPD_UPDATE_REGISTER:
    // this does an atomic read/set/write of a register
    register_update = (struct hcom_nx_upd_register_update *)arg;
    modifyreg32(register_update->address, register_update->clearBits, register_update->setBits);
    return OK;

    // This only works with Battery Backed Registers via 0-31 id value
  case HCOM_NX_UPD_SET_BBR_VALUE:
    bbr_val = (struct hcom_nx_upd_bbr_value *)arg;
    putreg32(bbr_val->value, HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER);
    return OK;
  case HCOM_NX_UPD_GET_BBR_VALUE:
    bbr_val = (struct hcom_nx_upd_bbr_value *)arg;
    bbr_val->value = getreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER);
    return OK;
  case HCOM_NX_UPD_UPDATE_BBR_VALUE:
    bbr_update = (struct hcom_nx_upd_bbr_update *)arg;
    modifyreg32(HCOM_NX_MEADOW_BATTERY_BACKED_REGISTER, bbr_update->clearBits, bbr_update->setBits);
    return OK;

  case HCOM_NX_UPD_CLI_COMMAND:
    cmdData = (struct hcom_nx_cmd_data *)arg;
    ret = hcom_nx_route_cli_command(cmdData);
    return ret;

  case HCOM_NX_UPD_GET_MCU_ID:
    stm32_get_uniqueid((uint8_t *)arg);
    return OK;

  case HCOM_NX_UPD_GET_MCU_SER_NUMB:
    return hcom_nx_get_mcu_ser_numb(arg);
    
  case HCOM_NX_UPD_IS_PART_MOUNTED:
    is_mounted = (struct hcom_nx_upd_is_part_mounted *)arg;
    is_mounted->isMounted = hcom_nx_fs_is_mounted(is_mounted->partitionId);
    return OK;

#if defined(CONFIG_MEADOW_ESPCP_MANAGER)
  case HCOM_NX_UPD_ESP32_ENTER_PROG_MODE:
    espcp_enter_programming_mode();
    return OK;

  case HCOM_NX_UPD_ESP32_RESTART_ESP32:
    espcp_reset();
    return OK;

  case HCOM_NX_UPD_START_ESPCP_RUNNING:
  {
    // Start the ESP32 coprocessor.
    ret = espcp_init();
    if(ret != OK)
    {
      syslog(LOG_EMERG, "ERROR: ESP32 initialization failed:%d\n", ret);
      return ret;
    }

    ret = espcp_enter_run_mode();
    if(ret != OK)
    {
      syslog(LOG_EMERG, "ERROR: ESP32 enter run mode failed:%d\n", ret);
      return ret;
    }

    usrsock_register_sockif(&g_usrsock_sockif_esp32);
    return OK;
  }
#endif

  case HCOM_NX_UPD_RESTORE_UART_CONFIG:
    return hcom_nx_restore_uart_reconfig(arg);

  case HCOM_NX_UPD_DIAG_FD_INODE:
    return hcom_nx_upd_diag_fd_inode(arg);
    
  case HCOM_NX_UPD_HOST_RESTART_MEADOW_MCU:
    hcom_nx_common_utils_host_restart_meadow();
    return OK;

  case HCOM_NX_UPD_ONLY_RESTART_MEADOW_MCU:
    hcom_nx_common_utils_only_restart_meadow();
    return OK;

  case HCOM_NX_UPD_GET_CONFIG:
    length = *((int *) arg);
    ret = hcom_nx_copy_config_for_user_mode((uint8_t *) arg, length);
    return ret;

  // Note: Two classes of GPIO. One operational and the other diagnostic
  case HCOM_NX_UPD_GPIO_COMMAND:
    return hcom_nx_upd_execute_gpio_write(arg);

  case HCOM_NX_UPD_GPIO_CONFIG:
    return hcom_nx_upd_execute_gpio_config(arg);

  case HCOM_NX_UPD_ENTER_INTO_DEF_MODE:
    *((unsigned long *)MEADOW_ENTER_DFU_MODE_MEMORY_ADDR) = MEADOW_ENTER_DFU_MODE_MAGIC_NUMB;
    return OK;

#if HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE > 0
  case HCOM_NX_UPD_DIAG_GPIO_COMMAND:
    return hcom_nx_upd_diag_gpio_write(arg);

  case HCOM_NX_UPD_DIAG_GPIO_CONFIG:
    return hcom_nx_upd_diag_gpio_config(arg);

  case HCOM_NX_UPD_DIAG_GPIO_SET_BYTE:
    return hcom_nx_upd_diag_gpio_write_byte(arg);

  case HCOM_NX_UPD_DIAG_GPIO_MAKE_DEFNS:
    return hcom_nx_upd_diag_gpio_make_defines(arg);
#endif

  default:
    syslog(LOG_ERR, "%s@%d-unknown hcom nx upd command:%d\n", thisFile, __LINE__, cmd);
  }

  return ERROR;
}

// ====================================================================
// This is use to determine for diagnostics to see the information
// related to file descriptors
int hcom_nx_upd_diag_fd_inode(unsigned long arg)
{
  struct hcom_nx_upd_diag_fd_inode_s *is_fd_valid;
  is_fd_valid = (struct hcom_nx_upd_diag_fd_inode_s *)arg;

  // Note the caller's task group may be different
  FAR struct tcb_s *thisTcb = this_task();
  FAR struct file *fileList;

  // Get pointer to the task's file list
  fileList = thisTcb->group->tg_filelist.fl_files;

  is_fd_valid->inodeAddr = fileList[is_fd_valid->fileDescriptor].f_inode;

  syslog(LOG_NOTICE, "Task:'%s', fd:%d, inode addr:0x%08x [%d]\n",
          thisTcb->name,
          is_fd_valid->fileDescriptor,
          fileList[is_fd_valid->fileDescriptor].f_inode,
          is_fd_valid->fileDescriptor);

  return OK;
}

// ====================================================================
// Execute a gpio digital write to output gpio
int hcom_nx_upd_execute_gpio_write(unsigned long arg)
{
  struct hcom_nx_upd_gpio_write_s *gpio_write;

  gpio_write = (struct hcom_nx_upd_gpio_write_s *)arg;

  if (gpio_write->gpioHcomId > HCOM_NUMBER_OF_GPIO_OUTPUT_MAP_ELEMENTS)
  {
    syslog(LOG_ERR, "%s@%d-GPIO output defn:%d out of range\n", thisFile, __LINE__, gpio_write->gpioHcomId);
    return -1;
  }
  uint32_t gpioOutputDefn = gpioOutputDefnArray[gpio_write->gpioHcomId].gpio_output_defn;
  stm32_gpiowrite(gpioOutputDefn, gpio_write->cmdValue);
  return OK;
}

// ====================================================================
// Configure a gpio
int hcom_nx_upd_execute_gpio_config(unsigned long arg)
{
  int ret;
  uint32_t gpioIODefn;
  struct hcom_nx_upd_gpio_config_s *gpio_config;

  gpio_config = (struct hcom_nx_upd_gpio_config_s *)arg;

  if (gpio_config->configValue == HCOM_NX_GPIO_DIGITAL_CONFIG_OUTPUT)
  {
    if (gpio_config->gpioHcomId > HCOM_NUMBER_OF_GPIO_OUTPUT_MAP_ELEMENTS)
    {
      syslog(LOG_ERR, "%s@%d-GPIO output defn:%d out of range\n", thisFile, __LINE__, gpio_config->gpioHcomId);
      return -1;
    }
    gpioIODefn = gpioOutputDefnArray[gpio_config->gpioHcomId].gpio_output_defn;
  }
  else if (gpio_config->configValue == HCOM_NX_GPIO_DIGITAL_CONFIG_INPUT)
  {
    if (gpio_config->gpioHcomId > HCOM_NUMBER_OF_GPIO_INPUT_MAP_ELEMENTS)
    {
      syslog(LOG_ERR, "%s@%d-GPIO input defn:%d out of range\n", thisFile, __LINE__, gpio_config->gpioHcomId);
      return -1;
    }
    gpioIODefn = gpioInputDefnArray[gpio_config->gpioHcomId].gpio_input_defn;
  }
  else
  {
    syslog(LOG_ERR, "%s@%d-GPIO configuration only supports digital I/O, invalid value:%d\n",
           thisFile, __LINE__, gpio_config->gpioHcomId);
    return -1;
  }

  ret = stm32_configgpio(gpioIODefn);
  gpio_config->result = ret;
  return ret;
}

// ====================================================================
// This is also called to add serial number to USB
int hcom_nx_get_mcu_ser_numb(unsigned long arg)
{
  int ret;
  char strMcuSn[16];
  struct hcom_nx_upd_mcu_ser_numb_s *mcu_ser;
  mcu_ser = (struct hcom_nx_upd_mcu_ser_numb_s *)arg;

  ret = hcom_nx_common_utils_calculate_serial_numb(NULL, strMcuSn);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Calc of serial numb failed:ret:%d\n",
           thisFile, __LINE__, ret);
    return ret;
  }

  strcpy(mcu_ser->ser_numb, strMcuSn);
  return OK;
}

// ====================================================================
// For a UART that has been configured to output etc. reconfig to be
// Tx or Rx as needed for the following UARTs
int hcom_nx_restore_uart_reconfig(unsigned long arg)
{
  struct hcom_nx_upd_uart_reconfig_s *uartReconfig;
  uartReconfig = (struct hcom_nx_upd_uart_reconfig_s *)arg;

  switch (uartReconfig->uart_id)
  {
  case MEADOW_RECONFIG_MISCONFIGURED_UART1:
    stm32_configgpio(GPIO_USART1_TX); // PB14
    stm32_configgpio(GPIO_USART1_RX); // PH13
    break;

  case MEADOW_RECONFIG_MISCONFIGURED_UART4:
    stm32_configgpio(GPIO_UART4_TX); // PH13
    stm32_configgpio(GPIO_UART4_RX); // PI9
    break;

  case MEADOW_RECONFIG_MISCONFIGURED_UART5:
    stm32_configgpio(GPIO_UART5_TX); // PB13
    stm32_configgpio(GPIO_UART5_RX); // PD2
    break;

  case MEADOW_RECONFIG_MISCONFIGURED_UART6:
    stm32_configgpio(GPIO_UART6_TX); // PC6
    stm32_configgpio(GPIO_UART6_RX); // PC7
    break;

  default:
    syslog(LOG_ERR, "%s@%d-restore uart reconfig, invalid uart:%d\n",
           thisFile, __LINE__, uartReconfig->uart_id);
    return -1;
  }
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_nx_upd_initialize
 *
 * Description:
 *   Initialize hcom nuttx upd
 *
 ****************************************************************************/

int hcom_nx_upd_initialize(void)
{
  syslog(LOG_DEBUG, "+hcom_nx_upd_initialize\n");

  // register the driver, passing in our entry points
  int ret = register_driver(HCOM_NX_UPD_DRIVER_NAME, &g_hcom_nx_operations, 0666, NULL);
  if (ret)
  {
    return ERROR;
  }

  return OK;
}
