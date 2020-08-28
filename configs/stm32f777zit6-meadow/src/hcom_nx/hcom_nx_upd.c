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
#include "stm32_uid.h"          // stm32_get_uniqueid()

#include <nuttx/board.h>
#include <arch/board/board.h>
#include "stm32_gpio.h"
#include "stm32f777zit6-meadow.h"

#include <meadow/hcom_upd_shared.h>
#include "hcom_nx_common.h"
#include <meadow/hcom_bbreg_defn.h>
#include "diag/hcom_nx_diag.h"

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

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static const struct file_operations g_hcom_nx_operations =
{
  .open  = hcom_upd_nx_open,
  .close = hcom_upd_nx_close,
  .read  = hcom_upd_nx_read,
  .ioctl = hcom_upd_nx_ioctl
};

// The next 2 tables eliminate switch statements by providing
// a lookup table. The order of the entries matches their shared
// values 0 - n
static struct hcom_nx_upd_gpio_output_map_s gpioOutputDefnArray[] = 
{
  // Defined in board.h                     // Defined in hcom_shared_common.h
  // Provide the GPIO definition            // Provide the relative offset
  {MEADOW_ESP32_ONBOARD_RESET_PIN_OUTPUT},  // 0 HCOM_GPIO_DIG_NX_ID_ESP_RESET
  {MEADOW_ESP32_ONBOARD_BOOT_PIN_OUTPUT},   // 1 HCOM_GPIO_DIG_NX_ID_ESP_BOOT
  // Defined in stm32f777zit6-meadow.h
  {GPIO_LED_BLUE},                          // 2 HCOM_GPIO_DIG_NX_ID_BLUE_LED
};

#define HCOM_NUMBER_OF_GPIO_OUTPUT_MAP_ELEMENTS (sizeof(gpioOutputDefnArray)/sizeof(struct hcom_nx_upd_gpio_output_map_s))

static struct hcom_nx_upd_gpio_input_map_s gpioInputDefnArray[] =
{
  // Defined in board.h
  {MEADOW_ESP32_ONBOARD_RESET_PIN_INPUT},
  {MEADOW_ESP32_ONBOARD_BOOT_PIN_INPUT}
};

#define HCOM_NUMBER_OF_GPIO_INPUT_MAP_ELEMENTS (sizeof(gpioInputDefnArray)/sizeof(struct hcom_nx_upd_gpio_input_map_s))

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
static int hcom_upd_nx_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  uint32_t ret;
  struct hcom_nx_upd_register_value *register_val;
  struct hcom_nx_upd_register_update *register_update;
  struct hcom_nx_upd_bbr_value *bbr_val;
  struct hcom_nx_upd_bbr_update *bbr_update;
  struct hcom_nx_cmd_data *cmdData;
  struct hcom_nx_upd_is_part_mounted *is_mounted;

  switch(cmd)
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

    case HCOM_NX_UPD_IS_PART_MOUNTED:
      is_mounted = (struct hcom_nx_upd_is_part_mounted *)arg;
      is_mounted->isMounted = hcom_nx_fs_is_mounted(is_mounted->partitionId);
      return OK;

    case HCOM_NX_UPD_GPIO_COMMAND:
      return hcom_nx_upd_execute_gpio_write(arg);

    case HCOM_NX_UPD_GPIO_CONFIG:
      return hcom_nx_upd_execute_gpio_config(arg);

#if HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE > 0
    case HCOM_NX_UPD_DIAG_GPIO_COMMAND:
      return hcom_nx_upd_diag_gpio_write(arg);

    case HCOM_NX_UPD_DIAG_GPIO_CONFIG:
      return hcom_nx_upd_diag_gpio_config(arg);
      
    case HCOM_NX_UPD_DIAG_GPIO_SET_BYTE:
      return hcom_nx_upd_diag_gpio_write_byte(arg);
#endif
    default:
      syslog(LOG_ERR, "%s@%d-unknown hcom nx upd command:%d\n", thisFile, __LINE__, cmd);
  }

  return ERROR;
}

// ====================================================================
// Execute a gpio digital write to output gpio 
int hcom_nx_upd_execute_gpio_write(unsigned long arg)
{
  struct hcom_nx_upd_gpio_write_s *gpio_write;

  gpio_write = (struct hcom_nx_upd_gpio_write_s*)arg;

  if(gpio_write->gpioHcomId > HCOM_NUMBER_OF_GPIO_OUTPUT_MAP_ELEMENTS)
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

  gpio_config = (struct hcom_nx_upd_gpio_config_s*)arg;

  if(gpio_config->configValue == HCOM_GPIO_DIGITAL_CONFIG_OUTPUT)
  {
    if(gpio_config->gpioHcomId > HCOM_NUMBER_OF_GPIO_OUTPUT_MAP_ELEMENTS)
    {
      syslog(LOG_ERR, "%s@%d-GPIO output defn:%d out of range\n", thisFile, __LINE__, gpio_config->gpioHcomId);
      return -1;
    }
    gpioIODefn = gpioOutputDefnArray[gpio_config->gpioHcomId].gpio_output_defn;
  }
  else if(gpio_config->configValue == HCOM_GPIO_DIGITAL_CONFIG_INPUT)
  {
    if(gpio_config->gpioHcomId > HCOM_NUMBER_OF_GPIO_INPUT_MAP_ELEMENTS)
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

