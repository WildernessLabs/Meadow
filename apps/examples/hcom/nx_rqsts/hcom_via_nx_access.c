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
#include <meadow/meadow_hw_version.h>

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
    return -errno;      // ioctl puts returned int into errno
  }

  hcom_logging_syslog(LOG_INFO, "%s@%d-SUCCESS %s opened\n",
          thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME);

  return nx_access_fd;
}

//=============================================================
int hcom_via_nx_set_bbr(uint32_t value)
{
  int ret;
  struct hcom_nx_upd_bbr_value bbr_value;

  bbr_value.value = value;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_SET_BBR_VALUE, (unsigned long)&bbr_value);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s)@%d-%s Failed to set reg, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno;      // ioctl puts returned int into errno
  }
  return OK;
}

//=============================================================
int hcom_via_nx_get_bbr(uint32_t *value)
{
  int ret;
  struct hcom_nx_upd_bbr_value bbr_value;
  
  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_GET_BBR_VALUE, (unsigned long)&bbr_value);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to get reg, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno;      // ioctl puts returned int into errno
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
  
  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_UPDATE_BBR_VALUE, (unsigned long)&bbr_update);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to update battery backed register, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno;      // ioctl puts returned int into errno
  }
  return OK;
}

//=============================================================
// Special version that is called from mono thread
int hcom_via_nx_update_bbr_alt(int alt_access_fd, uint32_t clearBits, uint32_t setBits)
{
  int ret;
  struct hcom_nx_upd_bbr_update bbr_update;

  bbr_update.clearBits = clearBits;
  bbr_update.setBits = setBits;
  
  ret = ioctl(alt_access_fd, HCOM_NX_UPD_UPDATE_BBR_VALUE, (unsigned long)&bbr_update);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to update battery backed register, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno;      // ioctl puts returned int into errno
  }
  return OK;
}

//=============================================================
// This returns the mcu unique id as a 12 byte array
int hcom_via_nx_get_mcu_id(uint8_t uniqueId[12])
{
  int ret;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_GET_MCU_ID, (unsigned long)uniqueId);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to get mcu id, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return -errno;      // ioctl puts returned int into errno
  }

  return OK;
}

//=============================================================
// This will return the mcu serial number as a null terminated char array
int hcom_via_nx_get_mcu_ser_numb(char mcuSerNumb[16])
{
  int ret;
  struct hcom_nx_upd_mcu_ser_numb_s mcuSn;
  mcuSn.ser_numb = mcuSerNumb;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_GET_MCU_SER_NUMB,
            (unsigned long) &mcuSn);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to get mcu id, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return -errno;      // ioctl puts returned int into errno
  }
  return OK;
}

//=============================================================
// Provides the nuttx side with a way to pass syslog messages back
// to userland so it can be sent to the CLI
size_t hcom_via_nx_provide_host_text_transport(uint16_t *requestType,
          char *buff, size_t bufLen)
{
  int ret;
  hcom_nx_upd_host_text_transport_t text_transport;

  text_transport.requestType = requestType;
  text_transport.transport_buf = buff;
  text_transport.buf_length = bufLen;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_HOST_TEXT_TRANSPORT,
            (unsigned long) &text_transport);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed text transport, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return -errno;      // ioctl puts returned int into errno
  }

  return text_transport.msg_length;
}

//=============================================================
// Provides the nuttx side with a way to pass syslog messages back
// to userland so it can be sent to the CLI
size_t hcom_via_nx_provide_cli_trace_transport(char *buff, size_t bufLen)
{
  int ret;
  hcom_nx_upd_cli_trace_transport_t trace_transport;

  trace_transport.transport_buf = buff;
  trace_transport.buf_length = bufLen;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_CLI_TRACE_TRANSPORT, (unsigned long) &trace_transport);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed trace transport, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return -errno;      // ioctl puts returned int into errno
  }

  return trace_transport.msg_length;
}

//=============================================================
// Is this partition mounted in the file system?
bool hcom_via_nx_is_mounted(uint32_t partitionId)
{
  int ret;

  struct hcom_nx_upd_is_part_mounted is_mounted;
  is_mounted.partitionId = partitionId;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_IS_PART_MOUNTED, (unsigned long) &is_mounted);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed nx mount, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return -errno;      // ioctl puts returned int into errno
  }

  return is_mounted.isMounted;
}

//=============================================================
// This is a stub for restarting meadow
int hcom_via_nx_host_restart_meadow()
{
  int ret;
  // Clear the Mono lockup flag, this is not a crash but a willful restart
  hcom_bbreg_clear_bbr_bits(HCOM_BBREG_MONO_LAST_RUN_LOCKUP_BIT);

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_HOST_RESTART_MEADOW_MCU, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed host restart meadow, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno;      // ioctl puts returned int into errno
  }
  return OK;
}

//=============================================================
// This is a stub for restarting meadow
int hcom_via_nx_only_restart_meadow()
{
  int ret;
  // Clear the Mono lockup flag, this is not a crash but a willful restart
  hcom_bbreg_clear_bbr_bits(HCOM_BBREG_MONO_LAST_RUN_LOCKUP_BIT);

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_ONLY_RESTART_MEADOW_MCU, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed only restart meadow, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno;      // ioctl puts returned int into errno
  }
  return OK;
}

//=============================================================
// This is a stub for entering DFU mode after restart
int hcom_via_nx_put_meadow_into_dfu_mode()
{
  int ret;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_ENTER_INTO_DFU_MODE, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to enter dfu mode, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno;      // ioctl puts returned int into errno
  }
  return OK;
}

//=============================================================
// This is a stub for starting ESPCP
int hcom_via_nx_start_espcp_running()
{
  int ret;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_START_ESPCP_RUNNING, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to start ESPCP, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return errno;      // ioctl puts returned int into errno, they are already negative
  }
  return OK;
}

//=============================================================
// The code restart the esp32 is on the os side
int hcom_via_nx_esp32_restart_esp32()
{
  int ret;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_ESP32_RESTART_ESP32, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s ESP32 restart, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return -errno;      // ioctl puts returned int into errno
  }

  return ret;
}

//=============================================================
// The code enter the programming mode on the esp32 is on the os side
int hcom_via_nx_esp32_enter_prog_mode()
{
  int ret;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_ESP32_ENTER_PROG_MODE, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s ESP32 enter prog mode ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return -errno;      // ioctl puts returned int into errno
  }

  return ret;
}

//=============================================================
// Mono has started running let kernelland know
void hcom_via_nx_mono_has_started()
{
  int ret;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_MONO_HAS_STARTED, (unsigned long) NULL);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Mono has started ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
  }
}

//=============================================================
// Mono (and espcp) can reconfigure the pins used by uarts needed
// for debugging. This function restores the tx and rx pins to be
// reconfigured as uart pins.
// Note: With the Meadow F7v1 this means uart 1, 4, 5 and 6 are valid
void hcom_via_nx_restore_uart_reconfig(uint32_t uartId)
{
  int ret;
  struct hcom_nx_upd_uart_reconfig_s uartReconfig;
  uartReconfig.uart_id = uartId;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_RESTORE_UART_CONFIG, (unsigned long) &uartReconfig);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s reconfig uart%d, ret:%d, errno:%d\n",
            thisFile, __LINE__, uartId, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
  }
}

//=============================================================
// This will return the numeric version number e.g. MEADOW_F7_HW_VERSION_NUMB_F7V2
uint32_t hcom_via_nx_get_hw_version()
{
  int ret;
  hcom_nx_upd_get_hw_ver_t hardwareVer;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_GET_HW_VERSION, (unsigned long) &hardwareVer);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
    return MEADOW_F7_HW_VERSION_NUMB_UNKNOWN;
  }

  return hardwareVer.hwVer;
}

uint32_t hcom_via_nx_get_hw_version_alt(int alt_access_fd)
{
  int ret;
  hcom_nx_upd_get_hw_ver_t hardwareVer;

  ret = ioctl(alt_access_fd, HCOM_NX_UPD_GET_HW_VERSION, (unsigned long) &hardwareVer);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
    return MEADOW_F7_HW_VERSION_NUMB_UNKNOWN;
  }

  return hardwareVer.hwVer;
}

//=============================================================
// Configures non-diag gpio via nx
int hcom_via_nx_gpio_config(uint32_t gpioPinDefn)
{
  int ret;
  struct hcom_nx_upd_gpio_config_s gpioConfig;

  gpioConfig.gpioPinDefn = gpioPinDefn;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_GPIO_CONFIG, (unsigned long) &gpioConfig);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio config, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return gpioConfig.result;
}

//=============================================================
// Configures non-diagnostic gpio via nx from mono
int hcom_via_nx_gpio_config_alt(int alt_access_fd, uint32_t gpioPinDefn)
{
  int ret;
  struct hcom_nx_upd_gpio_config_s gpioConfig;

  gpioConfig.gpioPinDefn = gpioPinDefn;

  ret = ioctl(alt_access_fd, HCOM_NX_UPD_GPIO_CONFIG, (unsigned long) &gpioConfig);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio config, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return ret;
  }

  return gpioConfig.result;
}

//=============================================================
// Writes to non-diag digital output gpio via nx
int hcom_via_nx_gpio_write(uint32_t gpioPinDefn, bool cmdValue)
{
  int ret;
  struct hcom_nx_upd_gpio_write_s gpioCommand;

  gpioCommand.gpioPinDefn = gpioPinDefn;
  gpioCommand.cmdValue = cmdValue;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_GPIO_COMMAND, (unsigned long) &gpioCommand);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio write, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return -errno;      // ioctl puts returned int into errno
  }

  return OK;
}

//=============================================================
// Writes to digital output gpio via nx using an alternate nx file descriptor
int hcom_via_nx_gpio_write_alt(int alt_access_fd, uint32_t gpioPinDefn, bool cmdValue)
{
  int ret;
  struct hcom_nx_upd_gpio_write_s gpioCommand;

  gpioCommand.gpioPinDefn = gpioPinDefn;
  gpioCommand.cmdValue = cmdValue;

  ret = ioctl(alt_access_fd, HCOM_NX_UPD_GPIO_COMMAND, (unsigned long) &gpioCommand);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed gpio write, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
    return -errno;      // ioctl puts returned int into errno
  }

  return OK;
}

//=============================================================
// Routes a command to execute a diagnostic event
void hcom_via_nx_exec_diag_app_cmd(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize)
{
  int ret;
  hcom_nx_upd_diag_app_command_t diagAppCmd;

  diagAppCmd.hdrMsg = hdrMsg;
  diagAppCmd.msgLen = packetSize;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_DIAG_APP_CMD, (unsigned long) &diagAppCmd);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed diag app cmd, ret:%d, errno:%d\n",
            thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, ret, errno);
  }
}

//--------------------------------------------------------------
// Diagnostic code
// Determines if a file descriptor exists in calling task by calling
// the calling thread's task inode list
void hcom_via_nx_diag_fd_inode(int fd)
{
  hcom_via_nx_diag_fd_inode_read(fd, NULL);
}

//--------------------------------------------------------------
// Diagnostic code
void hcom_via_nx_diag_fd_inode_read(int fd, struct inode **inodeOut)
{
  int ret;
  struct hcom_nx_upd_diag_fd_inode_s diag_fd_inode;

  diag_fd_inode.fileDescriptor = fd;

  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_DIAG_FD_INODE, (unsigned long) &diag_fd_inode);
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

//=============================================================
// CLI requests that can be directly to be executed on the OS side are
// routed through here
void hcom_via_nx_forward_cli_cmd_to_nx(uint16_t hcomCmd, uint32_t userData)
{
  int ret;
  struct hcom_nx_cmd_data cmdData;

  cmdData.hcomCmd = hcomCmd;
  cmdData.userData = userData;

  cmdData.logLevel = LOG_NONE;
  cmdData.logLen = 0;

  // Provides nx upd with the ability to send messages to CLI
  cmdData.send_host_msg = hcom_host_send_simple_string_msg;

  // Forward to hcom_nx to complete command
  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_CLI_COMMAND, (unsigned long)&cmdData);
  if (ret < 0)
  {
    // Call resulted in a log request
    if(cmdData.logLen > 0)
    {
      if(cmdData.logLevel < LOG_EMERG || cmdData.logLevel > LOG_DEBUG)
      {
        hcom_logging_syslog(LOG_WARNING, "Unknown log level:%d. Line? next log.\n",
                  thisFile, __LINE__, cmdData.logLevel);
      }
      
      // We check the returned log length against the known buffer size to
      // determine if snprintf in the called function truncated the message
      if(cmdData.logLen >= HCOM_NX_CMD_LOG_MSG_SIZE)
      {
        hcom_logging_syslog(LOG_WARNING, "snprintf buf too small need:%d. Line? next log.\n",
                  cmdData.logLen + 1);
      }

      hcom_logging_syslog(cmdData.logLevel, "%s [cli cmd:0x%04x]", cmdData.logMsg, hcomCmd);
    }
    else
    {
      hcom_logging_syslog(LOG_ERR, "%s:%s()@%d-%s Error detected, errno:%d, hcomCmd:0x%04x\n",
              thisFile, __func__, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno, hcomCmd);
      return;
    }
  }
}

/****************************************************************************
 * Name: hcom_via_nx_copy_config
 *
 * Description:
 *  Ask NuttX for a copy of the device configuration for use in user land.
 *
 * Input Parameters:
 *  config - Pointer to a memory block to hold the copy of the configuration.
 *
 * Returned Value:
 *  Result of the ioctl call.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int hcom_via_nx_copy_config(uint8_t *buffer)
{
  int ret = ioctl(_nx_access_fd, HCOM_NX_UPD_GET_CONFIG, (unsigned long) buffer);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s:%s()@%d Failed to copy the configuration.\n",
            thisFile, __func__, __LINE__);
  }
  return ret;
}

/****************************************************************************
 * Name: hcom_via_nx_execute_espcp_tests
 *
 * Description:
 *  Ask NuttX to execute the ESP Coprocessor (espcp) tests in kernel space.
 *
 * Input Parameters:
 *  userData - Value passed the CLI to HCOM ready for the kernel tests to use.
 *
 * Returned Value:
 *  Result of the ioctl call.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int hcom_via_nx_execute_espcp_tests(uint32_t userData)
{
  int ret = ioctl(_nx_access_fd, HCOM_NX_UPD_EXECUTE_ESPCP_TESTS, (unsigned long) userData);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s:%s()@%d Failed to execute network tests.\n",
            thisFile, __func__, __LINE__);
  }
  return ret;
}

//=========================================================================
// Set the RTC time
int hcom_via_nx_execute_rtc_set_clock(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize)
{
  hcom_nx_upd_rtc_set_time_t rtcSetTime;

  rtcSetTime.hdrMsg = hdrMsg;
  rtcSetTime.msgLen = packetSize;

  int ret = ioctl(_nx_access_fd, HCOM_NX_UPD_RTC_SET_TIME, (unsigned long) &rtcSetTime);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d Failed to set RTC time, ret:%d\n",
            thisFile, __LINE__, ret);
  }
  return ret;
}

//=========================================================================
// Set the RTC time wakeup time (i.e. RTC hardware alarm)
int hcom_via_nx_execute_rtc_set_wakeup_time(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t packetSize)
{
  hcom_nx_upd_rtc_wakeup_time_t rtcWakeupTime;

  rtcWakeupTime.hdrMsg = hdrMsg;
  rtcWakeupTime.msgLen = packetSize;

  int ret = ioctl(_nx_access_fd, HCOM_NX_UPD_RTC_WAKEUP_TIME, (unsigned long) &rtcWakeupTime);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d Failed to set RTC wakeup time, ret:%d\n",
            thisFile, __LINE__, ret);
  }
  return ret;
}

//=========================================================================
// Flash OS update part 1
int hcom_via_nx_update_OS1()
{
  int ret;
  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_UPDATE_OS1, 0);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to update OS (part 1), errno:%d\n",
                        thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno; // ioctl puts returned int into errno
  }
  return OK;
}

//=========================================================================
// Flash OS update part 2
int hcom_via_nx_update_OS2()
{
  int ret;
  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_UPDATE_OS2, 0);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to update OS (part 2), errno:%d\n",
                        thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno; // ioctl puts returned int into errno
  }
  return OK;
}

//=========================================================================
// Get OS update state
int hcom_via_nx_get_update_state(uint8_t flag)
{
  int ret;
  struct hcom_nx_upd_update_flag update = {.offset = flag};
  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_GET_UPDATE_FLAG, &update);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to get update state, errno:%d\n",
                        thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno; // ioctl puts returned int into errno
  }
  return OK;
}

//=========================================================================
// Set OS update state
int hcom_via_nx_set_update_state(uint8_t flag, uint8_t state)
{
  int ret;
  struct hcom_nx_upd_update_flag update = {.offset = flag, .value = state};
  ret = ioctl(_nx_access_fd, HCOM_NX_UPD_SET_UPDATE_FLAG, &update);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s Failed to set update state, errno:%d\n",
                        thisFile, __LINE__, HCOM_NX_UPD_DRIVER_NAME, errno);
    return -errno; // ioctl puts returned int into errno
  }
  return OK;
}