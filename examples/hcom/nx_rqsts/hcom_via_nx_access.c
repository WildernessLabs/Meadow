/****************************************************************************
 * \apps\examples\hcom\os_rqsts\hcom_via_nx_access.c
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

// All of the low-level calls that must be executed on the Nuttx OS side
// go though here

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <sys/ioctl.h>

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_upd_shared.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_bbreg_defn.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static int _hcom_via_nx_fd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_via_nx_access_setup()
{
  // Open the nx
  _hcom_via_nx_fd = open(HCOM_NX_UPD_DRIVER_NAME, O_RDONLY);
  if(_hcom_via_nx_fd == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to open, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return _hcom_via_nx_fd;
  }

  hcom_logging_syslog(LOG_INFO, "%s@%d-SUCCESS %s opened\n",
          thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME);
  return OK;
}

//=============================================================
int hcom_via_nx_set_bbr(uint32_t value)
{
  int ret;
  struct hcom_nx_upd_bbr_value bbr_value;

  bbr_value.value = value;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_SET_BBR_VALUE, (unsigned long)&bbr_value);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s)@%d-%s Failed to set reg, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return ret;
  }
  return OK;
}

//=============================================================
int hcom_via_nx_get_bbr(uint32_t *value)
{
  int ret;
  struct hcom_nx_upd_bbr_value bbr_value;
  
  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_GET_BBR_VALUE, (unsigned long)&bbr_value);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to get reg, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return ret;
  }

  *value = bbr_value.value;
  return OK;
}

//=============================================================
int hcom_via_nx_update_bbr(uint32_t clearBits, uint32_t setBits)
{
  int ret;
  struct hcom_nx_upd_bbr_update bbr_update;

  bbr_update.clearBits = clearBits;
  bbr_update.setBits = setBits;
  
  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_UPDATE_BBR_VALUE, (unsigned long)&bbr_update);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to update reg, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return ret;
  }
  return OK;
}

//=============================================================
// This is a stub for getting the mcu unique id
int hcom_via_nx_get_mcu_id(uint8_t uniqueId[12])
{
  int ret;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_GET_MCU_ID, (unsigned long)uniqueId);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to get mcu id, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return OK;
}

//=============================================================
// Is this partition mounted in the file system?
bool hcom_via_nx_is_mounted(uint32_t partitionId)
{
  int ret;

  struct hcom_nx_upd_is_part_mounted is_mounted;
  is_mounted.partitionId = partitionId;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_IS_PART_MOUNTED, (unsigned long) &is_mounted);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed nx mount, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return is_mounted.isMounted;
}

//=============================================================
// This is a stub for restarting meadow
int hcom_via_nx_restart_meadow(void)
{
  hcom_via_nx_forward_cli_cmd_to_nx(HCOM_MDOW_REQUEST_RESTART_PRIMARY_MCU, 0);
  return OK;
}

//=============================================================
// The code enter the programming mode on the esp32 is on the os side
int hcom_via_nx_esp32_restart_esp32()
{
  int ret;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_ESP32_RESTART_ESP32, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s ESP32 restart, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return ret;
}

//=============================================================
// The code enter the programming mode on the esp32 is on the os side
int hcom_via_nx_esp32_enter_prog_mode()
{
  int ret;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_ESP32_ENTER_PROG_MODE, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s ESP32 enter prog mode ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return ret;
}

//=============================================================
// Configures gpio via nx
int hcom_via_nx_gpio_config(int gpioHcomId, uint8_t configValue)
{
  int ret;
  struct hcom_nx_upd_gpio_config_s gpioConfig;

  gpioConfig.gpioHcomId = gpioHcomId;
  gpioConfig.configValue = configValue;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_GPIO_CONFIG, (unsigned long) &gpioConfig);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio config, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret * 100;
  }

  return gpioConfig.result;
}

//=============================================================
// Writes to gpio via nx
int hcom_via_nx_gpio_write(int gpioHcomId, uint8_t cmdValue)
{
  int ret;
  struct hcom_nx_upd_gpio_write_s gpioCommand;

  gpioCommand.gpioHcomId = gpioHcomId;
  gpioCommand.cmdValue = cmdValue;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_GPIO_COMMAND, (unsigned long) &gpioCommand);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio write, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return OK;
}

#if HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE > 0
//=============================================================
// Configures diagnostic gpio via nx
int hcom_via_nx_diag_gpio_config(int gpioHcomId, uint8_t configValue)
{
  int ret;
  struct hcom_nx_upd_gpio_config_s gpioConfig;

  gpioConfig.gpioHcomId = gpioHcomId;
  gpioConfig.configValue = configValue;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_DIAG_GPIO_CONFIG, (unsigned long) &gpioConfig);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio config, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret * 100;
  }

  return gpioConfig.result;
}

//=============================================================
// Writes to diagnostic gpio via nx
int hcom_via_nx_diag_gpio_write(int gpioHcomId, uint8_t cmdValue)
{
  int ret;
  struct hcom_nx_upd_gpio_write_s gpioCommand;

  gpioCommand.gpioHcomId = gpioHcomId;
  gpioCommand.cmdValue = cmdValue;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_DIAG_GPIO_COMMAND, (unsigned long) &gpioCommand);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio write, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return OK;
}

//=============================================================
// Writes to diagnostic gpio via nx
int hcom_via_nx_diag_gpio_write_byte(uint8_t byteValue, uint8_t rangeId)
{
  int ret;
  struct hcom_nx_upd_gpio_diag_set_byte_s gpioCommand;

  gpioCommand.byteValue = byteValue;
  gpioCommand.rangeId = rangeId;

  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_DIAG_GPIO_SET_BYTE, (unsigned long) &gpioCommand);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio write 8, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return OK;
}
#endif

//=============================================================
// Those CLI requests that need to be executed on the OS side are
// routed through here
void hcom_via_nx_forward_cli_cmd_to_nx(uint16_t hcomCmd, uint32_t userData)
{
  int ret;
  struct hcom_nx_cmd_data cmdData;

  cmdData.hcomCmd = hcomCmd;
  cmdData.userData = userData;

  cmdData.logLevel = LOG_NONE;
  cmdData.logLen = 0;

  // void hcom_host_send_simple_string_msg(uint16_t requestType, uint32_t userData, char *shortText,
  //       char *sourceFileName, int sourceLineNumber);
  cmdData.send_host_msg = hcom_host_send_simple_string_msg;

  // Forward to hcom_nx to complete command
  ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_CLI_COMMAND, (unsigned long)&cmdData);
  if (ret < 0)
  {
    // Call resulted in a log request
    if(cmdData.logLen > 0)
    {
      DEBUGASSERT(cmdData.logLevel != LOG_NONE);
      DEBUGASSERT(cmdData.logLen <= HCOM_NX_CMD_LOG_MSG_SIZE);
      hcom_logging_syslog(cmdData.logLevel, "%s [cli cmd:0x%04x]", cmdData.logMsg, hcomCmd);
    }
    else
    {
      hcom_logging_syslog(LOG_ERR, "%s:%s()@%d-%s Failed to update reg, errno:%hcomCmd cmd:0x%04x\n",
              thisFile, __func__, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno, hcomCmd);
      return;   // Returned data may not be reliable
    }
  }
}

// No direct register access seems to be possible. The following functions
// provide access. However, the caller needs to know the correct register
// address. Since this is difficult on the apps side of nuttx other means
// have been implemented.
// BUT, KEEPING THE CODE IN THE CASE SOME FUTURE NEED ARISES
// //=============================================================
// int hcom_via_nx_set_register(uint32_t address, uint32_t value)
// {
//   int ret;
//   struct hcom_nx_upd_register_value reg_value;

//   reg_value.address = address;
//   reg_value.value = value;

//   ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_SET_REGISTER, (unsigned long)&reg_value);
//   if (ret < 0)
//   {
//     hcom_logging_syslog(LOG_ERR, "%s:%s()@%d-%s Failed to set reg, errno:%d\n",
//             thisFile, __func__, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
//     return ret;
//   }
//   return OK;
// }

// //=============================================================
// int hcom_via_nx_get_register(uint32_t address, uint32_t *value)
// {
//   int ret;
//   struct hcom_nx_upd_register_value reg_value;
  
//   reg_value.address = address;

//   ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_GET_REGISTER, (unsigned long)&reg_value);
//   if (ret < 0)
//   {
//     hcom_logging_syslog(LOG_ERR, "%s:%s()@%d-%s Failed to get reg, errno:%d\n",
//             thisFile, __func__, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
//     return ret;
//   }

//   *value = reg_value.value;
//   return OK;
// }

// //=============================================================
// int hcom_via_nx_update_register(uint32_t address, uint32_t clearBits, uint32_t setBits)
// {
//   int ret;
//   struct hcom_nx_upd_register_update reg_update;

//   reg_update.address = address;
//   reg_update.clearBits = clearBits;
//   reg_update.setBits = setBits;
  
//   ret = ioctl(_hcom_via_nx_fd, HCOM_NX_UPD_UPDATE_REGISTER, (unsigned long)&reg_update);
//   if (ret < 0)
//   {
//     hcom_logging_syslog(LOG_ERR, "%s:%s()@%d-%s Failed to update reg, errno:%d\n",
//             thisFile, __func__, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
//     return ret;
//   }
//   return OK;
// }

