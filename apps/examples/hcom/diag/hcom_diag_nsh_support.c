/****************************************************************************
 * \apps\examples\hcom\diag\hcom_diag_nsh_support.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// This can be found by '$ make menuconfig' and navigating
// Application Configuration -> System Libraries and NSH Add-Ons [] NuttShell
#if defined (CONFIG_SYSTEM_NSH)
/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;
static bool _nsh_enabled;


/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

int nsh_main(int argc, char *argv[]);
static int nsh_main_proxy(int argcx, char *argvx[]);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_diag_nsh_support_setup()
{
  _nsh_enabled = false;
  return OK;
}

//=======================================================================================
// Below is the needed configuration changes to support NSH while running Meadow
// Background
// Mono routes all Console.Write() calls to stdout. To get Console.Write() calls
// to output on CLI, stdout need to be redirected. However, normally this would
// mean that all stdout from all sources on the apps side of nuttx would also be
// routed. The solution was to configure Nuttx to not copy the existing stdio to
// new tasks. This has the side effect of preventing NSH to function since it
// normally depends on the stdio for input and output. The code in this file as
// well as certain Nuttx configuration changes resolved this problem.
// Information
// Any of the Meadow UARTs can be used based on the #define HCOM_DIAG_NSH_SERIAL_DEVICE.
// The CLI command --NshEnable will automatically set userData to the needed value of 1.
// NSH can only be enabled once from the CLI. After this the _nsh_enable flag will be set
// true and prevents nsh from being re-started.
// A nice, but not needed, feature would be to find a different method to determine
// if nsh is running (e.g. pthread_join, waitpid(),  waitid() or atexit()).
// NSH Configuration
// 'Application Configuration'->'NSH Library'->'Console Configuration'->
//  '[*] Use Console', '[*] Alternate Console Device' and set all three
//  'Alternate console "stdxxx" device names to '/dev/ttyS0', or '/dev/ttyS2'
//  or '/dev/ttyS3'. Usually, /dev/ttyS0 = USART1, /dev/ttyS1 = UART4 and
//  /dev/ttyS3 = USART6.
// 'Application Configuration'->'System Libraries and NSH Add-Ons'->
//  '[*] NuttShell (NSH) example
// This is enough to get NSH working. Other features can be added as required.
void hcom_diag_misc_launch_nsh(uint32_t userData)
{
  int _nsh_pid;

  // When mono starts it reconfigures all the GPIOs. This call
  // will restore the Tx and Rx configuration to the desired UART.
  if(strcmp(HCOM_DIAG_NSH_SERIAL_DEVICE, "/dev/ttyS0") == 0)
  {
    hcom_via_nx_restore_uart_reconfig(MEADOW_RECONFIG_MISCONFIGURED_UART1);
  }
  else if(strcmp(HCOM_DIAG_NSH_SERIAL_DEVICE, "/dev/ttyS1") == 0)
  {
    hcom_via_nx_restore_uart_reconfig(MEADOW_RECONFIG_MISCONFIGURED_UART4);
  }
  else if (strcmp(HCOM_DIAG_NSH_SERIAL_DEVICE, "/dev/ttyS3") == 0)
  {
    hcom_via_nx_restore_uart_reconfig(MEADOW_RECONFIG_MISCONFIGURED_UART6);
  }
  else
  {
    hcom_logging_syslog(LOG_ERR, "NSH Serial device '%s' unknown\n",
              HCOM_DIAG_NSH_SERIAL_DEVICE);
    return;
  }

  if(_nsh_enabled)
  {
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
             "NSH already enabled",thisFile, __LINE__);
    return;
  }

  if(userData == 1)
  {
    // Create a unique task for NSH
    _nsh_pid = task_create("nsh", CONFIG_SYSTEM_NSH_PRIORITY,
                        CONFIG_SYSTEM_NSH_STACKSIZE,
                        (main_t)nsh_main_proxy,
                        (FAR char * const *) NULL);
    if(_nsh_pid > 0)
    {
      _nsh_enabled = true;
      hcom_logging_syslog(LOG_INFO, "%s@%d-NSH now enabled [pid:%d, pri:%d, stack:%d]\n",
              thisFile, __LINE__, _nsh_pid, CONFIG_SYSTEM_NSH_PRIORITY, CONFIG_SYSTEM_NSH_STACKSIZE);

      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
              "NSH enabled until Meadow is restarted", thisFile, __LINE__);
    }
    else
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-NSH could not be created\n", thisFile, __LINE__);

      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
              "NSH could not be created", thisFile, __LINE__);
    }
  }
  else
  {
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
            "It is not possible to disable NSH", thisFile, __LINE__);
    return;
  }
}

//====================================================================
// Because mono uses stdout for C# Console.WriteLine and stderr for exceptions
// the following is necessary. To allow mono to send Console.Write message
// to CLI, the nuttx configuration was changed to prevent new tasks from
// inheriting stdin, stdout and stderr from the parent task. This includes
// the task (HCOM) creating the mono task.
// By design Nuttx builds NSH expecting to using stdin, stdout and stderr for
// interactions with the serial port. This is resolved here in that the proper
// serial port is assigned to stdout and stderr by this new task that runs NSH.
// And the nuttx configuration assignes the alternate console for the proper port.
//
// Note: cannot use the hcom_via_nx_xxx() calls without calling
// hcom_via_nx_upd_driver_open() to obtain a proper handle. Because this is
// a different task and therefore doesn't have the same file descriptors
// as the hcom task's threads.
//
int nsh_main_proxy(int argcx, char *argvx[])
{
  int ret;
  int argc = 0;
  char **argv = NULL;

  static int stdout_fd = -1;
  static int stderr_fd = -1;

  if(stdout_fd < 0)
  {
    do
    {
      stdout_fd = open(HCOM_DIAG_NSH_SERIAL_DEVICE, O_WRONLY);
      if(stdout_fd >= 0)
        break;            // Success

      if(errno != ENOENT)   // ENOENT = Error No Entity -> No such file or directory
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Open of %s failed errno:%d\n",
          thisFile, __LINE__, HCOM_DIAG_NSH_SERIAL_DEVICE, errno);
        stdout_fd = -1;
        return 1;
      }
      
      // All errors sleep and try again
      usleep(100 * 1000);
    } while (errno == ENOENT);

    // Assign the serial port write to the stdout fd.
    ret = dup2(stdout_fd, STDOUT_FILENO);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-redirect_writer: dup2 failed ret:%d errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }
    close(stdout_fd);
  }

  // stderr
  if(stderr_fd < 0)
  {
    do
    {
      stderr_fd = open(HCOM_DIAG_NSH_SERIAL_DEVICE, O_WRONLY);
      if(stderr_fd >= 0)
        break;            // Success

      if(errno != ENOENT)   // ENOENT = Error No Entity -> No such file or directory
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Open of %s failed errno:%d\n",
          thisFile, __LINE__, HCOM_DIAG_NSH_SERIAL_DEVICE, errno);
        stderr_fd = -1;
        return 1;
      }
      
      // All errors sleep and try again
      usleep(100 * 1000);
    } while (errno == ENOENT);

    // Assign the serial port write to the stderr fd too
    ret = dup2(stderr_fd, STDERR_FILENO);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-redirect_writer: dup2 failed ret:%d errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }
    close(stderr_fd);
  }

  usleep(100 * 1000);

  // TEST CODE - Verify that the expected uart comm port is opened
  // int nsh_port_fd = open(HCOM_DIAG_NSH_SERIAL_DEVICE, O_RDWR);
  // if(nsh_port_fd < 0)
  // {
  //   syslog(LOG_MTEST, "Error attempting to open NSH Port, ret:%d errno:%d\n", nsh_port_fd, errno);
  //   return -1;
  // }

  // syslog(LOG_MTEST, "NSH Port opened success. Will echo.\n");
  // // ECHO for TESTING
  // char testBuf[16];
  // while(true)
  // {
  //   // Read the port
  //   ret = read(nsh_port_fd, testBuf, 1);
  //   if(ret < 0)
  //   {
  //     syslog(LOG_MTEST, "Error reading NSH Port, ret:%d errno:%d\n", ret, errno);
  //     continue;
  //   }

  //   syslog(LOG_MTEST, "NSH Port read character\n");
    
  //   ret =  write(nsh_port_fd, testBuf, 1);
  //   if(ret < 0)
  //   {
  //     syslog(LOG_MTEST, "Error writing NSH Port, ret:%d errno:%d\n", ret, errno);
  //   }
  // }

  // Use this thread to execute nsh
  nsh_main(argc, argv);

  // Only here if NSH terminated
  return OK;
}

#else // CONFIG_SYSTEM_NSH
// If NSH excluded from build use these functions
int hcom_diag_nsh_support_setup()
{
  return OK;
}

void hcom_diag_misc_launch_nsh(uint32_t userData)
{
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, 
          "NuttShell not available", __FILE__, __LINE__);
}
#endif // CONFIG_SYSTEM_NSH
