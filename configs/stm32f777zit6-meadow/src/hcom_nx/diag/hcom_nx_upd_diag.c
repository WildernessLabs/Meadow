/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\diag\hcom_nx_upd_diag.c
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

// This file contains methods for using gpio for diagnostics

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
#include "../hcom_nx_common.h"
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/hcom_gpio_defn_diag.h>
#include "hcom_nx_upd_diag.h"

#if HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE > 0
/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

// The next 2 tables eliminate switch statements by providing
// a lookup table. The order of the entries matches their shared
// values 0 - n
static struct hcom_nx_upd_gpio_output_map_s gpioOutputDiagArray[] = 
{
  // Defined in board.h                     // Defined in hcom_shared_common.h
  // Provide the GPIO definition            // Order provides the relative offset 0 -24
  {MEADOW_DIAG_GPIO_A0___01_OUTPUT},        // HCOM_NX_DIAG_GPIO_A0
  {MEADOW_DIAG_GPIO_A1___02_OUTPUT},        // HCOM_NX_DIAG_GPIO_A1
  {MEADOW_DIAG_GPIO_A2___03_OUTPUT},        // HCOM_NX_DIAG_GPIO_A2
  {MEADOW_DIAG_GPIO_A3___04_OUTPUT},        // HCOM_NX_DIAG_GPIO_A3
  {MEADOW_DIAG_GPIO_A4___05_OUTPUT},        // HCOM_NX_DIAG_GPIO_A4
  {MEADOW_DIAG_GPIO_A5___06_OUTPUT},        // HCOM_NX_DIAG_GPIO_A5
  {MEADOW_DIAG_GPIO_SCK__07_OUTPUT},        // HCOM_NX_DIAG_GPIO_SCK
  {MEADOW_DIAG_GPIO_MOSI_08_OUTPUT},        // HCOM_NX_DIAG_GPIO_MOSI
  {MEADOW_DIAG_GPIO_MISO_09_OUTPUT},        // HCOM_NX_DIAG_GPIO_MISO
  // UART 4 D0 & D1
  {MEADOW_DIAG_GPIO_D00__10_OUTPUT},        // HCOM_NX_DIAG_GPIO_D00
  {MEADOW_DIAG_GPIO_D01__11_OUTPUT},        // HCOM_NX_DIAG_GPIO_D01
  {MEADOW_DIAG_GPIO_D02__12_OUTPUT},        // HCOM_NX_DIAG_GPIO_D02
  {MEADOW_DIAG_GPIO_D03__13_OUTPUT},        // HCOM_NX_DIAG_GPIO_D03
  {MEADOW_DIAG_GPIO_D04__14_OUTPUT},        // HCOM_NX_DIAG_GPIO_D04
  {MEADOW_DIAG_GPIO_D05__15_OUTPUT},        // HCOM_NX_DIAG_GPIO_D05
  {MEADOW_DIAG_GPIO_D06__16_OUTPUT},        // HCOM_NX_DIAG_GPIO_D06
  {MEADOW_DIAG_GPIO_D07__17_OUTPUT},        // HCOM_NX_DIAG_GPIO_D07
  {MEADOW_DIAG_GPIO_D08__18_OUTPUT},        // HCOM_NX_DIAG_GPIO_D08
  {MEADOW_DIAG_GPIO_D09__19_OUTPUT},        // HCOM_NX_DIAG_GPIO_D09
  {MEADOW_DIAG_GPIO_D10__20_OUTPUT},        // HCOM_NX_DIAG_GPIO_D10
  {MEADOW_DIAG_GPIO_D11__21_OUTPUT},        // HCOM_NX_DIAG_GPIO_D11
  // UART 1 D12 & D13
  {MEADOW_DIAG_GPIO_D12__22_OUTPUT},        // HCOM_NX_DIAG_GPIO_D12
  {MEADOW_DIAG_GPIO_D13__23_OUTPUT},        // HCOM_NX_DIAG_GPIO_D13
  {MEADOW_DIAG_GPIO_D14__24_OUTPUT},        // HCOM_NX_DIAG_GPIO_D14
  {MEADOW_DIAG_GPIO_D15__25_OUTPUT},        // HCOM_NX_DIAG_GPIO_D15
};

#define HCOM_NUMBER_OF_DIAG_OUTPUT_ELEMENTS (sizeof(gpioOutputDiagArray)/sizeof(struct hcom_nx_upd_gpio_output_map_s))

/****************************************************************************
 * Public Functions
 ****************************************************************************/

// Execute a gpio digital write to output gpio 
int hcom_nx_upd_diag_gpio_write(unsigned long arg)
{
  struct hcom_nx_upd_gpio_write_s *gpio_write;

  gpio_write = (struct hcom_nx_upd_gpio_write_s*)arg;

  if(gpio_write->gpioHcomId > HCOM_NUMBER_OF_DIAG_OUTPUT_ELEMENTS)
  {
    syslog(LOG_ERR, "%s@%d-GPIO output defn:%d out of range\n", thisFile, __LINE__, gpio_write->gpioHcomId);
    return -1;
  }
  uint32_t gpioOutputDefn = gpioOutputDiagArray[gpio_write->gpioHcomId].gpio_output_defn;
  stm32_gpiowrite(gpioOutputDefn, gpio_write->cmdValue);

  return OK;
}

// ====================================================================
// Configure a gpio
int hcom_nx_upd_diag_gpio_config(unsigned long arg)
{
  int ret;
  uint32_t gpioIODefn;
  struct hcom_nx_upd_gpio_config_s *gpio_config;

  gpio_config = (struct hcom_nx_upd_gpio_config_s*)arg;

  if(gpio_config->configValue == HCOM_NX_GPIO_DIGITAL_CONFIG_OUTPUT)
  {
    if(gpio_config->gpioHcomId > HCOM_NUMBER_OF_DIAG_OUTPUT_ELEMENTS)
    {
      syslog(LOG_ERR, "%s@%d-GPIO output defn:%d out of range\n", thisFile, __LINE__, gpio_config->gpioHcomId);
      return -1;
    }
    gpioIODefn = gpioOutputDiagArray[gpio_config->gpioHcomId].gpio_output_defn;
  }
  else
  {
    syslog(LOG_ERR, "%s@%d-GPIO configuration only supports digital Output, invalid value:%d\n",
              thisFile, __LINE__, gpio_config->gpioHcomId);
    return -1;
  }

  ret = stm32_configgpio(gpioIODefn);
  gpio_config->result = ret;

  return ret;
}

//==============================================================
int hcom_nx_upd_diag_gpio_write_byte(unsigned long arg)
{
  struct hcom_nx_upd_gpio_diag_set_byte_s *gpio_set_byte;

  gpio_set_byte = (struct hcom_nx_upd_gpio_diag_set_byte_s*)arg;

  uint8_t setBits = gpio_set_byte->byteValue;
  
  if(gpio_set_byte->rangeId == 0)
  {
    stm32_gpiowrite(MEADOW_DIAG_GPIO_A0___01_OUTPUT, (setBits & 0x01) == 0 ? 0 : 1);  // HCOM_NX_DIAG_GPIO_A0
    stm32_gpiowrite(MEADOW_DIAG_GPIO_A1___02_OUTPUT, (setBits & 0x02) == 0 ? 0 : 1);  // HCOM_NX_DIAG_GPIO_A1
    stm32_gpiowrite(MEADOW_DIAG_GPIO_A2___03_OUTPUT, (setBits & 0x04) == 0 ? 0 : 1);  // HCOM_NX_DIAG_GPIO_A2
    stm32_gpiowrite(MEADOW_DIAG_GPIO_A3___04_OUTPUT, (setBits & 0x08) == 0 ? 0 : 1);  // HCOM_NX_DIAG_GPIO_A3
    stm32_gpiowrite(MEADOW_DIAG_GPIO_A4___05_OUTPUT, (setBits & 0x10) == 0 ? 0 : 1);  // HCOM_NX_DIAG_GPIO_A4
    stm32_gpiowrite(MEADOW_DIAG_GPIO_A5___06_OUTPUT, (setBits & 0x20) == 0 ? 0 : 1);  // HCOM_NX_DIAG_GPIO_A5
    stm32_gpiowrite(MEADOW_DIAG_GPIO_SCK__07_OUTPUT, (setBits & 0x40) == 0 ? 0 : 1);  // HCOM_NX_DIAG_GPIO_SCK
    stm32_gpiowrite(MEADOW_DIAG_GPIO_MOSI_08_OUTPUT, (setBits & 0x80) == 0 ? 0 : 1);  // HCOM_NX_DIAG_GPIO_MOSI
  }
  else if(gpio_set_byte->rangeId == 1)
  {
    // Range skips D00 & D01 (UART4) and D02
    // 1 turns GPIO transistor on which is low
    stm32_gpiowrite(MEADOW_DIAG_GPIO_D03__13_OUTPUT, (setBits & 0x01) == 0 ? 1 : 0);
    stm32_gpiowrite(MEADOW_DIAG_GPIO_D04__14_OUTPUT, (setBits & 0x02) == 0 ? 1 : 0);
    stm32_gpiowrite(MEADOW_DIAG_GPIO_D05__15_OUTPUT, (setBits & 0x04) == 0 ? 1 : 0);
    stm32_gpiowrite(MEADOW_DIAG_GPIO_D06__16_OUTPUT, (setBits & 0x08) == 0 ? 1 : 0);
    stm32_gpiowrite(MEADOW_DIAG_GPIO_D07__17_OUTPUT, (setBits & 0x10) == 0 ? 1 : 0);
    stm32_gpiowrite(MEADOW_DIAG_GPIO_D08__18_OUTPUT, (setBits & 0x20) == 0 ? 1 : 0);
    stm32_gpiowrite(MEADOW_DIAG_GPIO_D09__19_OUTPUT, (setBits & 0x40) == 0 ? 1 : 0);
    stm32_gpiowrite(MEADOW_DIAG_GPIO_D10__20_OUTPUT, (setBits & 0x80) == 0 ? 1 : 0);
  }
  return OK;
}

int hcom_nx_upd_diag_gpio_make_defines(unsigned long arg)
{
  syslog(2, "#define MEADOW_NUMB_A00_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_A0___01_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_A01_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_A1___02_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_A02_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_A2___03_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_A03_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_A3___04_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_A04_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_A4___05_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_A05_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_A5___06_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_SCK_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_SCK__07_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_MOSI_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_MOSI_08_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_MISO_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_MISO_09_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D00_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D00__10_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D01_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D01__11_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D02_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D02__12_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D03_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D03__13_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D04_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D04__14_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D05_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D05__15_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D06_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D06__16_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D07_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D07__17_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D08_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D08__18_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D09_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D09__19_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D10_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D10__20_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D11_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D11__21_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D12_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D12__22_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D13_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D13__23_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D14_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D14__24_OUTPUT);
  syslog(2, "#define MEADOW_NUMB_D15_OUTPUT (0x%08x)\n", MEADOW_DIAG_GPIO_D15__25_OUTPUT);
  return OK;
}
#endif