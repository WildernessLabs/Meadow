/****************************************************************************
 * \apps\examples\hcom\esp32\hcom_esp32_proc_util.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include "hcom_esp32_comms.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

static bool _shutting_down;
static char _cmdStrBuff[8];   // Only for converting command to string

// This is the command to sync the ESP32
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
 static bool _connectionActive;
 static void hcom_esp32_util_gpio_enter_prog_mode(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_esp32_util_setup_lazy()
{
  _shutting_down = false;
  _connectionActive = false;
  return OK;
}

//====================================================================
void hcom_esp32_util_shutdown()
{
  _shutting_down = true;
}

//====================================================================
// Reboot needed after programming to enter run mode
int hcom_esp32_util_hardware_restart(void)
{
  int ret;
  
  _connectionActive = false;
  
  // Make sure the boot pin is high then drop the reset pin and raise it (toggle it).
#if HCOM_ESP32_ALLOW_BOOT_PIN_TO_BE_INPUT > 0
  // The boot pin is used for input too so need to config
  ret = hcom_via_nx_gpio_config(HCOM_GPIO_DIG_NX_ID_ESP_BOOT, HCOM_GPIO_DIGITAL_CONFIG_OUTPUT);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio config 1:%d\n", thisFile, __LINE__, ret);
    return ret;
  }
  usleep(10 * 1000);
#endif

  // Insure boot pin is high, if it's low ESP32 will enter Boot Loader Mode
  ret = hcom_via_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_ESP_BOOT, HCOM_GPIO_DIGITAL_CMD_VALUE_HIGH);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio write 1:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_via_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_ESP_RESET, HCOM_GPIO_DIGITAL_CMD_VALUE_LOW);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio write 2:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  ret = hcom_via_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_ESP_RESET, HCOM_GPIO_DIGITAL_CMD_VALUE_HIGH);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio write 3:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#if HCOM_ESP32_ALLOW_BOOT_PIN_TO_BE_INPUT > 0
  ret = hcom_via_nx_gpio_config(HCOM_GPIO_DIG_NX_ID_ESP_BOOT, HCOM_GPIO_DIGITAL_CONFIG_INPUT);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio config 2:%d\n", thisFile, __LINE__, ret);
    return ret;
  }
#endif
  return ret;
}

//====================================================================
// The following sequence puts the ESP32 into programming mode
// This mode is called Boot Loader mode in ESP32 documents
void hcom_esp32_util_gpio_enter_prog_mode(void)
{
  int ret;

#if HCOM_ESP32_ALLOW_BOOT_PIN_TO_BE_INPUT > 0
  // The boot pin is only used for output when needed
  ret = hcom_via_nx_gpio_config(HCOM_GPIO_DIG_NX_ID_ESP_BOOT, HCOM_GPIO_DIGITAL_CONFIG_OUTPUT);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio config 1:%d\n", thisFile, __LINE__, ret);
    return;
  }
  usleep(20 * 1000);
#endif

  // Pull boot pin low. Then pull reset low and release reset pin. At this
  // moment the boot pin is read by the ESP32. If low the ESP32 enters bootloader.
  ret = hcom_via_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_ESP_BOOT, HCOM_GPIO_DIGITAL_CMD_VALUE_LOW);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio write 1:%d\n", thisFile, __LINE__, ret);
    return;
  }

  ret = hcom_via_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_ESP_RESET, HCOM_GPIO_DIGITAL_CMD_VALUE_LOW);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio write 2:%d\n", thisFile, __LINE__, ret);
    return;
  }
  usleep(10 * 1000);
  
  ret = hcom_via_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_ESP_RESET, HCOM_GPIO_DIGITAL_CMD_VALUE_HIGH);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio write 3:%d\n", thisFile, __LINE__, ret);
    return;
  }
  usleep(20 * 1000);

  // Boot pin's been read by now
  ret = hcom_via_nx_gpio_write(HCOM_GPIO_DIG_NX_ID_ESP_BOOT, HCOM_GPIO_DIGITAL_CMD_VALUE_HIGH);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio write 4:%d\n", thisFile, __LINE__, ret);
    return;
  }

#if HCOM_ESP32_ALLOW_BOOT_PIN_TO_BE_INPUT > 0
  ret = hcom_via_nx_gpio_config(HCOM_GPIO_DIG_NX_ID_ESP_BOOT, HCOM_GPIO_DIGITAL_CONFIG_INPUT);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-gpio config 2:%d\n", thisFile, __LINE__, ret);
    return;
  }
#endif
}

//====================================================================
// Takes care of the GPIO and sending synchronization messages.
// To prevent getting locked into this loop the number of attempts
// can be specified.
int hcom_esp32_util_init_comms_enter_boot_mode()
{
#define HCOM_ESP32_MAX_NUMB_CONNECT_ATTEMPTS 40
  int ret;
  struct HcomEsp32UserRecvdData_s recvdData[1];
  int currentNumbAttempts = HCOM_ESP32_MAX_NUMB_CONNECT_ATTEMPTS;

  if(_connectionActive)
    return OK;

  // Make sure everything has been initialized. Note: this call will
  // in turn call all the setup_exp32_xxx_xxx_lazy functions. It will
  // only do this once.
  ret = hcom_esp32_uart_lazy_initialization();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Lazy init:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  hcom_esp32_recv_expect_command_type(Esp32CommandUndefined);

  // Restart the esp32 hardware
  ret = hcom_esp32_util_hardware_restart();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-restart failed:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Put the esp into programming mode
  hcom_esp32_util_gpio_enter_prog_mode();

  // The esp will send text for about 1.1 seconds so we'll just wait
  usleep(1250 * 1000);

  // The ESP32 should be done sending text. From here out we should only
  // receive binary information. Send sync commands until esp32 responds by
  // echoing the sync command.
  do
  {
    currentNumbAttempts--;  // Attempts vary from 1 to n.

    // 100 ms seems to be a reasonable compromise. Much faster and ESP32 never responses
    // (it's probably too busy handling these commands) and slower just takes longer to
    // sync
    ret = hcom_esp32_xmit_build_and_send_msg(hcom_esp_sync_msg, sizeof(hcom_esp_sync_msg),
            Esp32CommandSynchronise, HCOM_ESP_XMIT_CONNECT_DELAY_MS, recvdData);
    if(ret >= 0)
    {
      break;    // Success
    }
    else if(ret == -ETIMEDOUT)
    {
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-send sync Timed out will try again. ret:%d, %d tries left\n",
              thisFile, __LINE__, ret, currentNumbAttempts);
      continue;     // try again
    }
    else
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Sending hcom_esp_sync_msg:%d\n", thisFile, __LINE__, ret);
      return ret;
    }

  } while (currentNumbAttempts > 0);

  currentNumbAttempts--;   // if no attempts left return -1
  if(currentNumbAttempts == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-No connection, %d attempts\n", thisFile, __LINE__,
              HCOM_ESP32_MAX_NUMB_CONNECT_ATTEMPTS);
  }
  else
  {
    // Give time for the other 7 responses to be received before
    // allowing the command to be processed
    _connectionActive = true;    
    usleep(100 * 1000);
  }

  return currentNumbAttempts;
}

//====================================================================
// Returns register value in the regValue
int hcom_esp32_util_read_register(uint32_t regAddr, uint32_t *regValue)
{
  int ret;
  uint8_t regAddrBody[4];

  regAddrBody[0] =  regAddr & 0x000000ff;
  regAddrBody[1] = (regAddr & 0x0000ff00) >> 8;
  regAddrBody[2] = (regAddr & 0x00ff0000) >> 16;
  regAddrBody[3] = (regAddr & 0xff000000) >> 24;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Register read at:%p\n", thisFile, __LINE__, regAddr);

  struct HcomEsp32UserRecvdData_s recvdData[1];
  ret = hcom_esp32_xmit_build_and_send_msg(regAddrBody, sizeof(regAddrBody),
            Esp32CommandReadRegister, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, recvdData);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-send reg read:%d\n", thisFile, __LINE__, ret);
    return ret;
  }
  
  // Value was calculated in receiver
  *regValue = recvdData->espHdr.value;
  return OK;
}

//====================================================================
// Write value to register
int hcom_esp32_util_write_register(uint32_t regAddr, uint32_t regValue)
{
  // not implemented
  int ret = OK;
  hcom_logging_syslog(LOG_ERR, "%s@%d-write reg not implemented\n", thisFile, __LINE__);
  return ret;
}

//====================================================================
// Host command to restart the ESP32 arrives here
void hcom_esp32_util_restart_esp32(uint32_t userData)
{
  int ret;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];

  // Restart the ESP32 via GPIO lines
  ret = hcom_esp32_util_hardware_restart();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-restart failed:%d\n", thisFile, __LINE__, ret);
    return;
  }

  int stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "ESP32 restarted");
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
          thisFile, __LINE__);
}

//====================================================================
// Host command to read the ESP32's Mac address arrives here
void hcom_esp32_util_read_esp32_mac(uint32_t userData)
{
  int ret;
  int stringLen;  
  uint32_t chipIdInfo;
  uint32_t chipMac1;
  uint32_t chipMac2;

  ret = hcom_esp32_util_init_comms_enter_boot_mode();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Init comms for mac:%d\n", thisFile, __LINE__, ret);
    return;
  }

  // Step #1 make sure ESP32
  ret = hcom_esp32_util_read_register(Esp32RegAddrUART_DATE_REG_ADDR, &chipIdInfo);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Read Reg 0x%08x err:%d\n",
              thisFile, __LINE__, Esp32RegAddrUART_DATE_REG_ADDR, ret);
    return;
  }

  // Verify this is an ESP32
  DEBUGASSERT(chipIdInfo == Esp32RegValueDATE_REG_VALUE_ESP32);

  // Step #2 read the 2 registers containing the Mac address
  ret = hcom_esp32_util_read_register(Esp32RegAddrEFUSE_REG_BASE + 4, &chipMac1);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Read Reg 0x%08x err:%d\n",
              thisFile, __LINE__, Esp32RegAddrEFUSE_REG_BASE + 4, ret);
    return;
  }

  ret = hcom_esp32_util_read_register(Esp32RegAddrEFUSE_REG_BASE + 8, &chipMac2);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Read Reg 0x%08x err:%d\n",
              thisFile, __LINE__, Esp32RegAddrEFUSE_REG_BASE + 8, ret);
    return;
  }

  // Build the MAC string
  char macAddr[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  stringLen = snprintf(macAddr, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
      "%02x:%02x:%02x:%02x:%02x:%02x",
      (chipMac2 & 0x0000ff00) >> 8, chipMac2 & 0x000000ff, (chipMac1 & 0xff000000) >> 24,
      (chipMac1 & 0x00ff0000) >> 16, (chipMac1 & 0x0000ff00) >> 8, chipMac1 & 0x000000ff);
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);

  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, macAddr,
          thisFile, __LINE__);

  // Restore to original state
  ret = hcom_esp32_util_hardware_restart();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-restart failed:%d\n", thisFile, __LINE__, ret);
    return;
  }
}

//====================================================================
char *hcom_esp32_util_convert_cmd_to_string(uint8_t cmd)
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
      snprintf(_cmdStrBuff, 8, "?-0x%02x", cmd);
      return _cmdStrBuff;
    }
  }
}

