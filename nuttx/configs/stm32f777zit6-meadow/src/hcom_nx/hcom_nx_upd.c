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
#include <meadow/meadow_hw_version.h>

#include "../espcp/espcp_coprocessor.h"
#include "../espcp/espcp_usrsock.h"
#include "../espcp/espcp_tests.h"
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
  int length;
  struct hcom_nx_upd_register_value *register_val;
  struct hcom_nx_upd_register_update *register_update;
  struct hcom_nx_upd_bbr_value *bbr_val;
  struct hcom_nx_upd_bbr_update *bbr_update;
  struct hcom_nx_cmd_data *cmdData;
  struct hcom_nx_upd_is_part_mounted *is_mounted;
  struct hcom_nx_upd_gpio_write_s *gpio_write;
  struct hcom_nx_upd_gpio_config_s *gpio_config;
  hcom_nx_upd_cli_trace_transport_t *trace_transport;
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
  hcom_nx_upd_host_text_transport_t *text_transport;
#endif
  hcom_nx_upd_get_hw_ver_t *hardwareVer;
  hcom_nx_upd_rtc_set_time_t *rtcSetTime;
  hcom_nx_upd_rtc_wakeup_time_t *rtcWakeupTime;

// At present (Sept 2021) The only use for this feature is with ethernet
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
  hcom_nx_upd_diag_app_command_t *diagAppCmd;
#endif

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

  case HCOM_NX_UPD_MONO_HAS_STARTED:
#if defined (CONFIG_RAMLOG_SYSLOG)
    return hcom_nx_trace_msg_mono_started();
#else
    return OK;
#endif

  case HCOM_NX_UPD_CLI_TRACE_TRANSPORT:
#if defined (CONFIG_RAMLOG_SYSLOG)
    trace_transport = (hcom_nx_upd_cli_trace_transport_t *)arg;
    trace_transport->msg_length = hcom_nx_trace_cli_trace_transport(
              trace_transport->transport_buf, trace_transport->buf_length);
#endif
    return OK;

// At present (Sept 2021) The only use for this feature is with ethernet
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
  case HCOM_NX_UPD_HOST_TEXT_TRANSPORT:
    text_transport = (hcom_nx_upd_host_text_transport_t *)arg;
    text_transport->msg_length = hcom_nx_text_to_host_transport(
              text_transport->requestType,
              text_transport->transport_buf,
              text_transport->buf_length);
    return OK;
#endif

  case HCOM_NX_UPD_EXECUTE_ESPCP_TESTS:
    espcp_execute_tests(arg);
    return(OK);

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
    ret = hcom_nx_config_copy_for_user_mode((uint8_t *) arg, length);
    return ret;

  case HCOM_NX_UPD_GET_HW_VERSION:
    hardwareVer = (hcom_nx_upd_get_hw_ver_t*)arg;
    hardwareVer->hwVer = meadow_hw_version_get();
    return OK;

  case HCOM_NX_UPD_ENTER_INTO_DFU_MODE:
    *((unsigned long *)MEADOW_ENTER_DFU_MODE_MEMORY_ADDR) = MEADOW_ENTER_DFU_MODE_MAGIC_NUMB;
    return OK;

  case HCOM_NX_UPD_GPIO_COMMAND:
    // Execute a gpio digital write to output gpio 
    gpio_write = (struct hcom_nx_upd_gpio_write_s*)arg;
    stm32_gpiowrite(gpio_write->gpioPinDefn, gpio_write->cmdValue);
    return OK;

  case HCOM_NX_UPD_GPIO_CONFIG:
    gpio_config = (struct hcom_nx_upd_gpio_config_s*)arg;
    ret = stm32_configgpio(gpio_config->gpioPinDefn);
    gpio_config->result = errno;
    return ret;

  case HCOM_NX_UPD_RTC_SET_TIME:
    // Set the time in the RTC hardware
    rtcSetTime = (hcom_nx_upd_rtc_set_time_t*)arg;
    ret = meadow_time_set_clock(rtcSetTime->hdrMsg,
              rtcSetTime->msgLen);
    return ret;

  case HCOM_NX_UPD_UPDATE_OS1:
    // Stage a updated OS bin
    ret = hcom_nx_exec_ex_flash_OS_update_flash1();
    return ret;
  
  case HCOM_NX_UPD_UPDATE_OS2:
    // Stage a updated external flash OS bin
    ret = hcom_nx_exec_ex_flash_OS_update_flash2();
    return ret;

// At present (Sept 2021) The only use for this feature is with ethernet
#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
  case HCOM_NX_UPD_DIAG_APP_CMD:
    diagAppCmd = (hcom_nx_upd_diag_app_command_t*)arg;
    ret = hcom_nx_diagnostic_app_execute(diagAppCmd->hdrMsg, diagAppCmd->msgLen);
    return ret;
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
    // F7v2 and CCMv2 use the same user GPIO pins
    if(meadow_hw_version_get() == MEADOW_F7_HW_VERSION_NUMB_F7V1)
      stm32_configgpio(GPIO_UART5_TX_V1); // PB13
    else
      stm32_configgpio(GPIO_UART5_TX_V2); // PC12
    stm32_configgpio(GPIO_UART5_RX); // PD2
    break;

  case MEADOW_RECONFIG_MISCONFIGURED_UART6:
    stm32_configgpio(GPIO_USART6_TX); // PC6
    stm32_configgpio(GPIO_USART6_RX); // PC7
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
