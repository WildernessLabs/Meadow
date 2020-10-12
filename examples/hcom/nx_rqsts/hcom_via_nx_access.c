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
static int _nx_access_fd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

//===========================================================================
// This call is made by all HCOM threads that need to access the nx_upd driver
// Other threads must save there own fd to get to the nuttx side.
int hcom_via_nx_get_fd()
{
  // Returns -1 of not opened
  return _nx_access_fd;
}

//===========================================================================
int hcom_via_nx_upd_setup()
{
  _nx_access_fd = hcom_via_nx_upd_driver_open();
  if (_nx_access_fd < 0)
  {
    syslog(LOG_ERR, "%s@%d-setup hcom nx access open:%d\n", thisFile, __LINE__, _nx_access_fd);
    _nx_access_fd = -1;
    return _nx_access_fd;
  }
  return OK;
}

//===========================================================================
// Returns the file descriptor for this open driver
int hcom_via_nx_upd_driver_open()
{
  int nx_access_fd;
  // Open the nx upd driver
  nx_access_fd = open(HCOM_NX_UPD_DRIVER_NAME, O_RDONLY);
  if(nx_access_fd == -1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to open, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno;
  }

  hcom_logging_syslog(LOG_INFO, "%s@%d-SUCCESS %s opened\n",
          thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME);

  return nx_access_fd;
}

//=============================================================
// Called from hcom_mono_control based on call from mono_main
void hcom_via_nx_upd_driver_close(int nx_access_fd)
{
  close(nx_access_fd);
  nx_access_fd = -1;
}

//=============================================================
int hcom_via_nx_set_bbr(int nx_access_fd, uint32_t value)
{
  int ret;
  struct hcom_nx_upd_bbr_value bbr_value;

  bbr_value.value = value;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_SET_BBR_VALUE, (unsigned long)&bbr_value);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s)@%d-%s Failed to set reg, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return ret;
  }
  return OK;
}

//=============================================================
int hcom_via_nx_get_bbr(int nx_access_fd, uint32_t *value)
{
  int ret;
  struct hcom_nx_upd_bbr_value bbr_value;
  
  ret = ioctl(nx_access_fd, HCOM_NX_UPD_GET_BBR_VALUE, (unsigned long)&bbr_value);
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
int hcom_via_nx_update_bbr(int nx_access_fd, uint32_t clearBits, uint32_t setBits)
{
  int ret;
  struct hcom_nx_upd_bbr_update bbr_update;

  bbr_update.clearBits = clearBits;
  bbr_update.setBits = setBits;
  
  ret = ioctl(nx_access_fd, HCOM_NX_UPD_UPDATE_BBR_VALUE, (unsigned long)&bbr_update);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to update battery backed register, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return ret;
  }
  return OK;
}

//=============================================================
// This is a stub for getting the mcu unique id
int hcom_via_nx_get_mcu_id(int nx_access_fd, uint8_t uniqueId[12])
{
  int ret;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_GET_MCU_ID, (unsigned long)uniqueId);
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
bool hcom_via_nx_is_mounted(int nx_access_fd, uint32_t partitionId)
{
  int ret;

  struct hcom_nx_upd_is_part_mounted is_mounted;
  is_mounted.partitionId = partitionId;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_IS_PART_MOUNTED, (unsigned long) &is_mounted);
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
int hcom_via_nx_restart_meadow(int nx_access_fd)
{
  int ret;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_RESTART_MEADOW_MCU, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to restart meadow, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return ret;
  }
  return OK;
}

//=============================================================
// The code restart the esp32 is on the os side
int hcom_via_nx_esp32_restart_esp32(int nx_access_fd)
{
  int ret;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_ESP32_RESTART_ESP32, (unsigned long) NULL);
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
int hcom_via_nx_esp32_enter_prog_mode(int nx_access_fd)
{
  int ret;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_ESP32_ENTER_PROG_MODE, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s ESP32 enter prog mode ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return ret;
}

//=============================================================
// Mono (and espcp) can reconfigure the pins used by uarts needed
// for debugging. This function restores the tx and rx pins to be
// reconfigured as uart pins.
// Note: With the Meadow F7 this means uart 1, 4 and 5 are valid
void hcom_via_nx_restore_uart_reconfig(int nx_access_fd, uint32_t uartId)
{
  int ret;
  struct hcom_nx_upd_uart_reconfig_s uartReconfig;

  uartReconfig.uart_id = uartId;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_RESTORE_UART_CONFIG, (unsigned long) &uartReconfig);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s reconfig uart%d, ret:%d, errno:%d\n",
            thisFile, __LINE__, uartId, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
  }
}

//=============================================================
// Configures gpio via nx
int hcom_via_nx_gpio_config(int nx_access_fd, int gpioHcomId, uint8_t configValue)
{
  int ret;
  struct hcom_nx_upd_gpio_config_s gpioConfig;

  gpioConfig.gpioHcomId = gpioHcomId;
  gpioConfig.configValue = configValue;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_GPIO_CONFIG, (unsigned long) &gpioConfig);
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
int hcom_via_nx_gpio_write(int nx_access_fd, int gpioHcomId, uint8_t cmdValue)
{
  int ret;
  struct hcom_nx_upd_gpio_write_s gpioCommand;

  gpioCommand.gpioHcomId = gpioHcomId;
  gpioCommand.cmdValue = cmdValue;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_GPIO_COMMAND, (unsigned long) &gpioCommand);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio write, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return OK;
}

//=============================================================
// Diagnostic code
// Determines if a file descriptor exists in calling task by calling
// the calling thread's task inode list
void hcom_via_nx_diag_fd_inode(int nx_access_fd, int fd)
{
  hcom_via_nx_diag_fd_inode_read(nx_access_fd, fd, NULL);
}

//--------------------------------------------------------------
// Diagnostic code
void hcom_via_nx_diag_fd_inode_read(int nx_access_fd, int fd, struct inode **inodeOut)
{
  int ret;
  struct hcom_nx_upd_diag_fd_inode_s diag_fd_inode;

  diag_fd_inode.fileDescriptor = fd;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_DIAG_FD_INODE, (unsigned long) &diag_fd_inode);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
    return;
  }

  if(inodeOut != NULL)
    *inodeOut = diag_fd_inode.inodeAddr;

  return;
}

#if HCOM_INCLUDE_IN_BUILD_DIAGNOSTIC_GPIO_CODE > 0
//=============================================================
// Configures diagnostic gpio via nx
int hcom_via_nx_diag_gpio_config(int nx_access_fd, int gpioHcomId, uint8_t configValue)
{
  int ret;
  struct hcom_nx_upd_gpio_config_s gpioConfig;

  gpioConfig.gpioHcomId = gpioHcomId;
  gpioConfig.configValue = configValue;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_DIAG_GPIO_CONFIG, (unsigned long) &gpioConfig);
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
int hcom_via_nx_diag_gpio_write(int nx_access_fd, int gpioHcomId, uint8_t cmdValue)
{
  int ret;
  struct hcom_nx_upd_gpio_write_s gpioCommand;

  gpioCommand.gpioHcomId = gpioHcomId;
  gpioCommand.cmdValue = cmdValue;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_DIAG_GPIO_COMMAND, (unsigned long) &gpioCommand);
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
int hcom_via_nx_diag_gpio_write_byte(int nx_access_fd, uint8_t byteValue, uint8_t rangeId)
{
  int ret;
  struct hcom_nx_upd_gpio_diag_set_byte_s gpioCommand;

  gpioCommand.byteValue = byteValue;
  gpioCommand.rangeId = rangeId;

  ret = ioctl(nx_access_fd, HCOM_NX_UPD_DIAG_GPIO_SET_BYTE, (unsigned long) &gpioCommand);
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
void hcom_via_nx_forward_cli_cmd_to_nx(int nx_access_fd, uint16_t hcomCmd, uint32_t userData)
{
  int ret;
  struct hcom_nx_cmd_data cmdData;

  cmdData.hcomCmd = hcomCmd;
  cmdData.userData = userData;

  cmdData.logLevel = LOG_NONE;
  cmdData.logLen = 0;

  // Provides nx upd with the ability to send messages to CLI
  // void hcom_host_send_simple_string_msg(uint16_t requestType, uint32_t userData, char *shortText,
  //       char *sourceFileName, int sourceLineNumber);
  cmdData.send_host_msg = hcom_host_send_simple_string_msg;

  // Forward to hcom_nx to complete command
  ret = ioctl(nx_access_fd, HCOM_NX_UPD_CLI_COMMAND, (unsigned long)&cmdData);
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
// int hcom_via_nx_set_register(int nx_access_fd, uint32_t address, uint32_t value)
// {
//   int ret;
//   struct hcom_nx_upd_register_value reg_value;

//   reg_value.address = address;
//   reg_value.value = value;

//   ret = ioctl(nx_access_fd, HCOM_NX_UPD_SET_REGISTER, (unsigned long)&reg_value);
//   if (ret < 0)
//   {
//     hcom_logging_syslog(LOG_ERR, "%s:%s()@%d-%s Failed to set reg, errno:%d\n",
//             thisFile, __func__, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
//     return ret;
//   }
//   return OK;
// }

// //=============================================================
// int hcom_via_nx_get_register(int nx_access_fd, uint32_t address, uint32_t *value)
// {
//   int ret;
//   struct hcom_nx_upd_register_value reg_value;
  
//   reg_value.address = address;

//   ret = ioctl(nx_access_fd, HCOM_NX_UPD_GET_REGISTER, (unsigned long)&reg_value);
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
// int hcom_via_nx_update_register(int nx_access_fd, uint32_t address, uint32_t clearBits, uint32_t setBits)
// {
//   int ret;
//   struct hcom_nx_upd_register_update reg_update;

//   reg_update.address = address;
//   reg_update.clearBits = clearBits;
//   reg_update.setBits = setBits;
  
//   ret = ioctl(nx_access_fd, HCOM_NX_UPD_UPDATE_REGISTER, (unsigned long)&reg_update);
//   if (ret < 0)
//   {
//     hcom_logging_syslog(LOG_ERR, "%s:%s()@%d-%s Failed to update reg, errno:%d\n",
//             thisFile, __func__, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
//     return ret;
//   }
//   return OK;
// }

