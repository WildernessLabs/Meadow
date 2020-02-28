/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_esp32_proc_util.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include "hcom_esp32_comms.h"

#include <nuttx/board.h>
#include <arch/board/board.h>
#include "stm32_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _shutting_down;
static bool _is_comms_initialized;
static char _lineBuff[8];
static uint8_t hcom_esp_sync_msg[] =
{ 
  0x07, 0x07, 0x12, 0x20, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 
  0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 
  0x55, 0x55, 0x55, 0x55
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
 static char *thisFile = __FILE__;

 static void hcom_esp32_util_gpio_enter_prog_mode(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_esp32_util_setup_lazy()
{
  _shutting_down = false;
  _is_comms_initialized = false;
  return OK;
}

//====================================================================
void hcom_esp32_util_shutdown()
{
  _shutting_down = true;
}

//====================================================================
// Reboot needed after programming to enter run mode
void hcom_esp32_utils_hardware_reboot(void)
{
  // Initiate a reset
  stm32_gpiowrite(MEADOW_ESP32_ONBOARD_RESET_PIN, DIGITAL_OUTPUT_STATE_LOW);
  usleep(10 * 1000);
  stm32_gpiowrite(MEADOW_ESP32_ONBOARD_RESET_PIN, DIGITAL_OUTPUT_STATE_HIGH);
  usleep(30 * 1000);   // allow chip to recover
}

//====================================================================
// The following sequence puts the ESP32 into programming mode
void hcom_esp32_util_gpio_enter_prog_mode(void)
{
  // Pull boot pin low
  // When reset pin is released it reads the boot pin (ESP32-GPIO0), if low
  // the ESP32 enters bootloader.
  stm32_gpiowrite(MEADOW_ESP32_ONBOARD_BOOT_PIN, DIGITAL_OUTPUT_STATE_LOW);
  stm32_gpiowrite(MEADOW_ESP32_ONBOARD_RESET_PIN, DIGITAL_OUTPUT_STATE_LOW);
  usleep(10 * 1000);
  stm32_gpiowrite(MEADOW_ESP32_ONBOARD_RESET_PIN, DIGITAL_OUTPUT_STATE_HIGH);
  usleep(20 * 1000);
  // Boot pin's been read by now
  stm32_gpiowrite(MEADOW_ESP32_ONBOARD_BOOT_PIN, DIGITAL_OUTPUT_STATE_HIGH);
}

//====================================================================
// Takes care of the GPIO and sending synchronization messages.
// To prevent getting locked into this loop the number of attempts
// can be specified.
int hcom_esp32_util_initialize_communications()
{
  int ret;
  int maxNumbAttempts = 40;
  struct HcomEsp32UserRecvdData_s recvdData[1];

  // Has communications already been initialized?
  if(_is_comms_initialized)
    return OK;
  
  // Make sure everything has been initialized. Note: this call will
  // in turn call all the setup_exp32_xxx_xxx_lazy functions
  ret = hcom_esp32_uart_lazy_initialization();
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Lazy init:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  hcom_esp32_recv_expect_command_type(Esp32CommandUndefined);

  // Place in known state and delay
  hcom_esp32_utils_hardware_reboot();

  // Put the esp into programming mode
  hcom_esp32_util_gpio_enter_prog_mode();

  // The esp will send text for about 1.1 seconds so we'll just wait
  usleep(1250 * 1000);

  // From here out we should only receive binary information
  // Send sync commands until esp32 responds by echoing the sync command.
  do
  {
    maxNumbAttempts--;  // Attemts vary from 1 to 10+.

    // 100 ms seems to be a reasonable compromise. Much faster and ESP32 never responses
    // (it's probably too busy handling these commands) and slower just takes longer to
    // sync
    ret = hcom_esp32_xmit_build_and_send_msg(hcom_esp_sync_msg, sizeof(hcom_esp_sync_msg),
            Esp32CommandSynchronise, HCOM_ESP_XMIT_CONNECT_DELAY_MS, recvdData);
    if(ret == -ETIMEDOUT)
    {
      f7syslog(LOG_DEBUG, "%s@%d-send sync Timed out will try again. ret:%d, %d tries left\n",
          thisFile, __LINE__, ret, maxNumbAttempts);
      continue;     // try again
    }
    else if(ret < 0)
    {
      f7syslog(LOG_ERR, "%s@%d-Error:Sending hcom_esp_sync_msg:%d\n", thisFile, __LINE__, ret);
      return ret;
    }

    _is_comms_initialized = true;
  } while (maxNumbAttempts > 0 && !_is_comms_initialized);

  maxNumbAttempts--;   // if no attempts left return -1
  if(_is_comms_initialized)
  {
    // Give time for the other 7 responses to be received before
    // allowing the initiating command to be processed
    usleep(100 * 1000);
  }
  else
  {
    f7syslog(LOG_ERR, "%s@%d-No connection, %d attempts\n", thisFile, __LINE__, maxNumbAttempts);
  }
  
  return maxNumbAttempts;
}

//====================================================================
// Returns register value in the regValue
int hcom_esp32_util_read_register(uint32_t regAddr, uint32_t *regValue)
{
  int ret;
  uint8_t regAddrBody[4];

  ret = hcom_esp32_util_initialize_communications();
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Init Comms:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  regAddrBody[0] =  regAddr & 0x000000ff;
  regAddrBody[1] = (regAddr & 0x0000ff00) >> 8;
  regAddrBody[2] = (regAddr & 0x00ff0000) >> 16;
  regAddrBody[3] = (regAddr & 0xff000000) >> 24;

  f7syslog(LOG_DEBUG, "%s@%d-Register read at:%p\n", thisFile, __LINE__, regAddr);

  struct HcomEsp32UserRecvdData_s recvdData[1];

  ret = hcom_esp32_xmit_build_and_send_msg(regAddrBody, sizeof(regAddrBody),
            Esp32CommandReadRegister, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:send reg read:%d\n", thisFile, __LINE__, ret);
    return ret;
  }
  
  // Value was calculated in receiver
  *regValue = recvdData->espHdr.value;
  return OK;
}

//====================================================================
// Writes value to register
int hcom_esp32_util_write_register(uint32_t regAddr, uint32_t regValue)
{
  int ret = OK;

  // p-m not implemented
  // ret = hcom_esp32_util_initialize_communications();
  // if(ret < 0)
  // {
  //   f7syslog(LOG_ERR, "%s@%d-Error:Init Comms:%d\n", thisFile, __LINE__, ret);
  //   return ret;
  // }

  return ret;
}

//====================================================================
// Restart the ESP32
void hcom_esp32_exec_restart_esp32(uint32_t userData)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

  ret = hcom_esp32_util_initialize_communications();
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Init Comms:%d\n", thisFile, __LINE__, ret);
    return;
  }

  hcom_esp32_utils_hardware_reboot();
  
  int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "ESP32 restarted");
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s@%d-Host xmit err:%d\n", thisFile, __LINE__, ret);
}

//====================================================================
// Get the ESP32's Mac address
void hcom_esp32_exec_read_esp32_mac(uint32_t userData)
{
  int ret;
  int stringLen;  
  uint32_t chipIdInfo;
  uint32_t chipMac1;
  uint32_t chipMac2;

  ret = hcom_esp32_util_initialize_communications();
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:init comms:%d\n", thisFile, __LINE__, ret);
    return;
  }
  
  // Step #1 make sure ESP32
  ret = hcom_esp32_util_read_register(Esp32RegAddrUART_DATE_REG_ADDR, &chipIdInfo);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Read Reg 0x%08x err:%d\n", thisFile, __LINE__, Esp32RegAddrUART_DATE_REG_ADDR, ret);
    return;
  }
  
  // Verify this is an ESP32
  DEBUGASSERT(chipIdInfo == Esp32RegValueDATE_REG_VALUE_ESP32);

  // Step #2 read the 2 registers containing the Mac address
  ret = hcom_esp32_util_read_register(Esp32RegAddrEFUSE_REG_BASE + 4, &chipMac1);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Read Reg 0x%08x err:%d\n", thisFile, __LINE__, Esp32RegAddrEFUSE_REG_BASE + 4, ret);
    return;
  }

  ret = hcom_esp32_util_read_register(Esp32RegAddrEFUSE_REG_BASE + 8, &chipMac2);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Read Reg 0x%08x err:%d\n", thisFile, __LINE__, Esp32RegAddrEFUSE_REG_BASE + 8, ret);
    return;
  }

  // Build the MAC string
  char macAddr[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  stringLen = snprintf(macAddr, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
      "%02x:%02x:%02x:%02x:%02x:%02x",
      (chipMac2 & 0x0000ff00) >> 8, chipMac2 & 0x000000ff, (chipMac1 & 0xff000000) >> 24,
      (chipMac1 & 0x00ff0000) >> 16, (chipMac1 & 0x0000ff00) >> 8, chipMac1 & 0x000000ff);
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);

  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, macAddr);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s@%d-Host xmit err:%d\n", thisFile, __LINE__, ret);
}

//====================================================================
char *hcom_esp_command_hex_to_string(uint8_t cmd)
{
  switch(cmd)
  {
    case 0x02:
    return "FLASH_BEGIN";
    case 0x03:
    return "FLASH_DATA";
    case 0x04:
    return "FLASH_END";
    case 0x05:
    return "MEM_BEGIN";
    case 0x06:
    return "MEM_DATA";
    case 0x07:
    return "MEM_END";
    case 0x08:
    return "SYNC";
    case 0x09:
    return "WRITE_REG";
    case 0x0a:
    return "READ_REG";
    case 0x0b:
    return "SPI_SET_PARAMS";
    case 0x0d:
    return "SPI_ATTACH";
    case 0x13:
    return "SPI_FLASH_MD5";
    default:
    {
      snprintf(_lineBuff, 8, "?-0x%02x", cmd);
      return _lineBuff;
    }
  }
}
//====================================================================
// p-m ??
void hcom_esp32_util_developer_4(uint32_t userData)
{
  // For testing
  int ret;

  switch(userData)
  {
    case 1:
    hcom_esp32_uart_lazy_initialization();
    break;

    case 2:
    hcom_esp32_util_gpio_enter_prog_mode();
    break;

    case 3:
    {
      uint32_t regValue = 0;
      ret = hcom_esp32_util_read_register(0x40008658, &regValue);
      if(ret < 0)
      {
        f7syslog(LOG_ERR, "%s@%d-Error:Read Reg 0x%08x err:%d\n", thisFile, __LINE__, Esp32RegAddrUART_DATE_REG_ADDR, ret);
        return;
      }
      syslog(0, "=== From address 0x40008658 read:0x%08x\n", regValue);
    }
    break;

    case 4:
    {
      uint32_t regValue = 0;
      ret = hcom_esp32_util_read_register(Esp32RegAddrUART_DATE_REG_ADDR, &regValue);
      if(ret < 0)
      {
        f7syslog(LOG_ERR, "%s@%d-Error:Read Reg 0x%08x err:%d\n", thisFile, __LINE__, Esp32RegAddrUART_DATE_REG_ADDR, ret);
        return;
      }
      syslog(0, "=== From address 0x%08x read:0x%08x\n", Esp32RegAddrUART_DATE_REG_ADDR, regValue);
    }
    break;
    default:
      syslog(0, "Dev 4 userData:%d meaningless\n", userData);

  }
}

