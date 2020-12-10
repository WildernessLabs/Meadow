/****************************************************************************
 * \apps\examples\hcom\mono\hcom_mono_control.c
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

// This module controls the exection of mono

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_upd_shared.h>

#if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
#include <sys/socket.h>
#include <sys/un.h>
#endif

#define HCOM_MONO_RUNTIME_TASK_STACKSIZE 32768

// Note:
// CONFIG_USERMAIN_PRIORITY defined via make menuconfig at RTOS Features >
// Tasks and Scheduling > init thread priority. It's used to set the priority
// of the nuttx launch user app which in our case is hcom.
// SCHED_PRIORITY_DEFAULT defined in ...\Meadow\Meadow.OS\nuttx\include\sys\types.h
// It's a hardcoded nuttx value of 100
#define HCOM_MONO_RUNTIME_TASK_PRIORITY SCHED_PRIORITY_DEFAULT

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

int mono_main(int argc, char *argv[]);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *thisFile = __FILE__;
static int _stdout_fd;
static int _stderr_fd;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static bool hcom_mono_ctrl_are_needed_files_here(void);
static bool hcom_mono_ctrl_should_mono_run(void);
static bool hcom_mono_ctrl_did_mono_run_last_time(void);
static int redirect_stdout_stderr(void);
#if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
static int hcom_mono_remote_dbg_open_mono_sock(void);
static int mono_main_proxy(int argcX, char *argvX[]);
#endif
/****************************************************************************
 * Public Functions
 ****************************************************************************/

//====================================================================
int hcom_mono_ctrl_mono_main_setup()
{
  int ret;
  
  _stdout_fd = -1;
  _stderr_fd = -1;

  // Configure Blue LED as output
  ret = hcom_via_nx_gpio_config(HCOM_NX_GPIO_DIG_ID_BLUE_LED, HCOM_NX_GPIO_DIGITAL_CONFIG_OUTPUT);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_via_nx_gpio_config:%d\n",
              thisFile, __LINE__, ret);
    return -1;
  }
  return OK;
}

//====================================================================
// This function is called by the startup manager and is thus the bringup thread
// It is responsible to start mono if it is desired and enabled
// Note: This thread is from a different task that hcom
int hcom_mono_ctrl_start_mono_main()
{
  int ret;
  int mono_pid;

  // Config blue LED.
  ret = hcom_via_nx_gpio_config(HCOM_NX_GPIO_DIG_ID_BLUE_LED, HCOM_NX_GPIO_DIGITAL_CONFIG_OUTPUT);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_via_nx_gpio_config:%d\n",
              thisFile, __LINE__, ret);
    return -1;
  }
 
  // Blue LED will stay on of mono doesn't start
  ret = hcom_via_nx_gpio_write(HCOM_NX_GPIO_DIG_ID_BLUE_LED, HCOM_NX_GPIO_DIGITAL_CMD_VALUE_LOW);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_via_nx_gpio_write, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
  }

  // Don't start if there's a reason
  if(!hcom_mono_ctrl_should_mono_run())
  {
    // Reason has been reported already, exit here
    return OK;
  }

  //------------------------------------------------------------
  // Start espcp running
#if defined(CONFIG_MEADOW_ESPCP_MANAGER)
  ret = hcom_via_nx_start_espcp_running();
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_via_nx_start_espcp_running, ret:%d, errno:%d\n",
              thisFile, __LINE__, ret, errno);
  }
#endif

  //------------------------------------------------------------
  // Set the flag that can identify if mono locks up. It will be
  // cleared by mono once mono is running correctly.
  hcom_bbreg_set_bbr_bits(HCOM_BBREG_MONO_LAST_RUN_LOCKUP_BIT);

  hcom_logging_syslog(LOG_NOTICE, "%s@%d-Attempting to start mono\n", thisFile, __LINE__);

  //------------------------------------------------------------
  // Read some configuration options from the config file.  These
  // values will influence the flags / arguments that will be
  // passed to Mono.
  int argc = 0;
  char *argv[] = { NULL, NULL, NULL };
  if (hcom_via_nx_ini_cfg_get_int_default(NULL, "startup", "MonoDebug", 0) == 1)
  {
    argv[argc] = "--debug";
    argc++;
  }
  char traceInformation[64];
  if (hcom_via_nx_ini_cfg_get_value(NULL, "startup", "MonoTrace", traceInformation, 64) == OK)
  {
    argv[argc] = (char *) malloc(72);   // Need space to add the "--trace=" part of the command line.
    snprintf(argv[argc], 72, "--trace=%s", traceInformation);
    argc++;
  }

  // Create a task to execute mono
  mono_pid = task_create("mono", HCOM_MONO_RUNTIME_TASK_PRIORITY,
                      CONFIG_PTHREAD_STACK_DEFAULT,
#if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
                      (main_t)mono_main_proxy,
#else
                      (main_t)mono_main,
#endif                      
                      (FAR char * const *) argv);
  if(mono_pid > 0)
  {
    hcom_logging_syslog(LOG_INFO, "%s@%d-MONO launched [pid:%d, pri:%d, stack size:%d]\n",
            thisFile, __LINE__, mono_pid, HCOM_MONO_RUNTIME_TASK_PRIORITY,
            CONFIG_PTHREAD_STACK_DEFAULT);

    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
            "Meadow successfully started MONO", thisFile, __LINE__);
    return OK;
  }

  hcom_logging_syslog(LOG_ERR, "%s@%d-The task to run mono failed in create\n",
            thisFile, __LINE__);

  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          "Meadow could not start Mono task", thisFile, __LINE__);
  return -1;
}

//====================================================================
// This function will test all the reasons to start and not start mono
bool hcom_mono_ctrl_should_mono_run()
{
  // Is mono enabled?
  if(!hcom_mono_ctrl_is_mono_enabled())
  {
    char *noStartReason = "Meadow will not start MONO because it is not enabled";
    hcom_logging_syslog(LOG_WARNING, "%s@%d-%s\n", thisFile, __LINE__, noStartReason);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
            noStartReason, thisFile, __LINE__);
    return false;
  }

  // Are the necessary files in place?
  if(!hcom_mono_ctrl_are_needed_files_here())
  {
    return false;
  }

  // Did mono run currectly the last time?
  bool run_mono = hcom_mono_ctrl_did_mono_run_last_time();
  if(!run_mono)
  {
    char *noStartReason = "Meadow will not start MONO because it didn't run correctly last time";
    hcom_logging_syslog(LOG_WARNING, "%s@%d-%s\n", thisFile, __LINE__, noStartReason);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
            noStartReason, thisFile, __LINE__);
    return false;
  }

  return true;
}

//===================================================================
// This bit is set everytime we attempt to start mono. Mono then clears
// it once it's running. If this bit is set when hcom starts, then
// we don't start mono because something is wrong. This solves the 
// condition can prevent all host communications.
bool hcom_mono_ctrl_did_mono_run_last_time()
{
  // hcom_bbreg_is_bbr_bits_set_n_clear could be used
  if(hcom_bbreg_is_bbr_bit_set(HCOM_BBREG_MONO_LAST_RUN_LOCKUP_BIT))
  {
    // It should not be set unless mono locked up
    hcom_bbreg_clear_bbr_bits(HCOM_BBREG_MONO_LAST_RUN_LOCKUP_BIT);
    return false;
  }

  return true;
}

//===================================================================
// Check if the necessary files exist in the files system
bool hcom_mono_ctrl_are_needed_files_here()
{
  char missingFiles[128];
  int listOff = 0;
  int offset = 0;
  int listCount = 0;
  char *neededApps[] = 
  {
    "mscorlib.dll",
    "System.Core.dll",
    "System.dll",
    "Meadow.dll",
    "App.exe",
    NULL
  };

  memset(missingFiles, 0, 128);

  while(neededApps[listOff] != NULL)
  {
    char appPath[64];
    snprintf(appPath, 64, "%s/%s", MONO_MEADOW_EXECUTABLE_PARTITION_NAME, neededApps[listOff]);

    int fd = open(appPath, O_RDONLY);
    if (fd == -1)
    {
      listCount++;
      if(offset > 0)
      {
        missingFiles[offset++] = ',';
        missingFiles[offset++] = ' ';
      }

      strcpy(missingFiles + offset, neededApps[listOff]);
      offset += strlen(neededApps[listOff]);
    }
    else
    {
      close(fd);
    }
    listOff++;
  }
  
  if(offset == 0)
    return true;

  // Some file(s) is missing
  char errReason[HCOM_LARGE_HOST_STRING_BUFF_LENGTH];

  int stringLen = snprintf(errReason, HCOM_LARGE_HOST_STRING_BUFF_LENGTH,
            "Meadow will not start MONO because the following file%s %s missing:%s",
            listCount == 1 ? "" : "s", listCount == 1 ? "is" : "are",
            missingFiles);
  hcom_logging_syslog(LOG_WARNING, "%s@%d-%s\n", thisFile, __LINE__, errReason);
  
  DEBUGASSERT(stringLen < HCOM_LARGE_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
        errReason, thisFile, __LINE__);
  
  return false;
}

//===================================================================
// Determine the state of the mono run flag.
// The bit is set when mono is disabled
// Note: at startup this gets call several times to optimze could
// cache value on first call. However, this would mean a restart
// would be necessary if meadow.cfg changed
bool hcom_mono_ctrl_is_mono_enabled()
{
  // Check ini config file to override the user request
  if(hcom_via_nx_ini_cfg_get_match(NULL, "startup", "monorun", "no"))
  {
    // config file contains 'monorun=no'
    return false;
  }

  return !hcom_bbreg_is_bbr_bit_set(HCOM_BBREG_USER_RQST_MONO_ENABLE_BIT);
}

//=======================================================================================
// The following are called from host 
//=======================================================================================
// Called from host to disable Mono from running on next MCU reset
void hcom_mono_ctrl_disable_mono(uint32_t userData)
{
  hcom_bbreg_set_bbr_bits(HCOM_BBREG_USER_RQST_MONO_ENABLE_BIT);

  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          "Mono has been disabled. Restarting Meadow", thisFile, __LINE__);
}

//=======================================================================================
// Called from host to enable Mono to run on next MCU reset
void hcom_mono_ctrl_enable_mono(uint32_t userData)
{
  hcom_bbreg_clear_bbr_bits(HCOM_BBREG_USER_RQST_MONO_ENABLE_BIT);

  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          "Mono has been enabled. Restarting F7 Micro", thisFile, __LINE__);
}

//======================================================================================
// The host has ask for the mono startup state
void hcom_mono_ctrl_report_mono_enabled_state(uint32_t userData)
{
  char *monoStartupMsg;

  if(hcom_mono_ctrl_is_mono_enabled())
    monoStartupMsg = "On reset, Meadow will start MONO and run app.exe";
  else
    monoStartupMsg = "On reset, Meadow will not start MONO, therefore app.exe will not run";

  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          monoStartupMsg, thisFile, __LINE__);
}

//==================================================================
// Below called from mono_main after it has initialized but before it
// actually starts mono running.
// The main thread of the task that will run Mono calls this function.
// An error here will prevent mono from starting
//==================================================================
int hcom_mono_ctrl_mono_appears_to_be_running()
{
  int ret;
  int nx_access_fd;

  // For Mono apps to forward Console.WriteLine text, we must redirect
  // the Mono tasks stdout fd to a fifo which will route this text
  // to the host PC if CLI or equal is running.
  ret = redirect_stdout_stderr();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-stdout/stderr redirect:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // For this Mono main thread to access the nuttx side it needs to
  // open, use and close the nx upd driver.
  nx_access_fd = hcom_via_nx_upd_driver_open();
  if (nx_access_fd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-setup hcom nx access:%d\n", thisFile, __LINE__, nx_access_fd);
    return nx_access_fd;
  }

#if defined (CONFIG_RAMLOG_SYSLOG)
  // Sets flag so ramlog can restore UART1's proper configuration since
  // mono's internal initialization reconfigured as digital output
  hcom_diag_trace_ramlog_mono_started();
#endif

  // Turn off blue LED. Must reconfigure because mono may have changed the
  // configuration during startup
  ret = hcom_via_nx_gpio_config_alt(nx_access_fd, 
            HCOM_NX_GPIO_DIG_ID_BLUE_LED, HCOM_NX_GPIO_DIGITAL_CONFIG_OUTPUT);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_via_nx_gpio_config:%d\n",
              thisFile, __LINE__, ret);
    return -1;
  }

  ret = hcom_via_nx_gpio_write_alt(nx_access_fd,
          HCOM_NX_GPIO_DIG_ID_BLUE_LED, HCOM_NX_GPIO_DIGITAL_CMD_VALUE_HIGH);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_via_nx_gpio_write:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Clear the flag so mono will start next time.
  hcom_bbreg_clear_bbr_bits_alt(nx_access_fd, HCOM_BBREG_MONO_LAST_RUN_LOCKUP_BIT);

  // Finished interacting with nuttx side
  close(nx_access_fd);

  hcom_logging_syslog(LOG_NOTICE, "%s@%d-Mono has succesfully started\n", thisFile, __LINE__);
  return OK;
}

//==================================================================
// Since the mono_main task's main thread called this function it will
// cause it's stdout and stderr calls to be routed to the correct fifo
int redirect_stdout_stderr(void)
{
  int ret;

  if(_stdout_fd < 0)
  {
    // Open stdout fifo
    do
    {
      // Opening with O_NONBLOCK seems like the right thing to do but
      // it is NOT. It causes the mono app to halt.
      _stdout_fd = open(HCOM_MONO_STDOUT_REDIRECT_FIFO, O_WRONLY);
      if(_stdout_fd >= 0)
        break;            // Success

      if(errno != ENOENT)   // ENOENT = Error No Entity -> No such file or directory
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Open of %s failed errno:%d\n",
          thisFile, __LINE__, HCOM_MONO_STDOUT_REDIRECT_FIFO, errno);
        _stdout_fd = -1;
        return 1;
      }
      
      // All errors sleep and try again
      usleep(100 * 1000);
    } while (errno == ENOENT);

    // Assign the fifo's write end to the stdout fd.
    // Note: ret should be 1 the stdout fd
    ret = dup2(_stdout_fd, STDOUT_FILENO);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-redirect_writer: dup2 failed ret:%d errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }
  }

  // stderr
  if(_stderr_fd < 0)
  {
    // Open stderr fifo
    do
    {
      // Opening with O_NONBLOCK seems like the right thing to do but
      // it is NOT. It causes the mono app to halt.
      _stderr_fd = open(HCOM_MONO_STDERR_REDIRECT_FIFO, O_WRONLY);
      if(_stderr_fd >= 0)
        break;            // Success

      if(errno != ENOENT)   // ENOENT = Error No Entity -> No such file or directory
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Open of %s failed errno:%d\n",
          thisFile, __LINE__, HCOM_MONO_STDERR_REDIRECT_FIFO, errno);
        _stderr_fd = -1;
        return 1;
      }
      
      // All errors sleep and try again
      usleep(100 * 1000);
    } while (errno == ENOENT);

    // Assign the fifo's write end to the stderr fd.
    // Note: ret should be 1 the stderr fd
    ret = dup2(_stderr_fd, STDERR_FILENO);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-redirect_writer: dup2 failed ret:%d errno:%d\n",
                thisFile, __LINE__, ret, errno);
      return -errno;
    }
  }
  return OK;
}

#if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
//==================================================================
// Mono debugging requires a socket connection. To save mono from needing
// to open the socket we'll do it here. 
// Called from mono_main_proxy() which calls mono_main 
int hcom_mono_remote_dbg_open_mono_sock()
{
  struct sockaddr_un myaddr;
  socklen_t addrlen;
  int ret;
  int vsSockFd = -1;

  vsSockFd = socket(PF_LOCAL, SOCK_STREAM, 0);
  if (vsSockFd < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Error:Socket creation failed vsSockFd:%d errno:%d\n",
              thisFile, __LINE__, vsSockFd, errno);
    return vsSockFd;
  }

  // Connect the socket to the server
  addrlen = strlen(HCOM_MONO_REMOTE_DBG_SOCKET_NAME);
  if (addrlen > UNIX_PATH_MAX - 1)
    addrlen = UNIX_PATH_MAX - 1;

  myaddr.sun_family = AF_LOCAL;
  strncpy(myaddr.sun_path, HCOM_MONO_REMOTE_DBG_SOCKET_NAME, addrlen);
  myaddr.sun_path[addrlen] = '\0';
  addrlen += sizeof(sa_family_t) + 1;
 
  int attemptCnt = 0;
  do
  {
    attemptCnt++;
    if(attemptCnt > 5)
      break;

    ret = connect(vsSockFd, (struct sockaddr *)&myaddr, addrlen);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_INFO, "%s@%d-Connect attempt failed, will retry, ret: %d, errno:%d attempt:%d\n",
                thisFile, __LINE__, ret, errno, attemptCnt);
      usleep(100 * 1000);
    }
  } while(ret < 0);

  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Mono Debug connect attempt failed after %d attempts, ret: %d, errno:%d\n",
                thisFile, __LINE__, attemptCnt, ret, errno);
    return ret;
  }
  return vsSockFd;
}

//==================================================================
// In order to provide VS debugging with that proper socket port number
// the newly created mono task comes here so that the main thread of
// the mono task can open the port. 
int mono_main_proxy(int argcX, char *argvX[])
{
  int dbgSD;
  int argc;
  char **argv;

  if(hcom_mono_remote_dbg_is_active())
  {
    dbgSD = hcom_mono_remote_dbg_open_mono_sock();
    if(dbgSD < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Opening remote dbg socket failed\n", thisFile, __LINE__);
      return dbgSD;    // This will terminate this thread and task-> mono won't start
    }

    // Add command line argument for mono
    argc = 1;
    argv = (char **) malloc(sizeof(char *));
    argv[0] = (char *) malloc(16);
    snprintf(argv[0], 16, "%s=%d", HCOM_MONO_REMOTE_DBG_CMD_LINE_SD, dbgSD);
  }
  else
  {
    //
    //  Work out if there are any arguments to pass on to mono_main.
    //  Note that the first argument will be "mono" and should be ignored.
    //
    argc = 0;
    if (argcX > 1)
    {
      argv = &argvX[1];
      argc = argcX - 1;
    }
    else
    {
      argv = NULL;
    }
  }

  // Launch mono_main (or remote debugging test)
#if HCOM_VS_DEBUGGING_TESTS_INCLUDE_IN_BUILD > 0
  MonoVsRemoteDebugTestSetup(argc, argv);
#else
  mono_main(argc, argv);
#endif

  return OK;
}

#endif  // #if defined (CONFIG_HCOM_MONO_REMOTE_DEBUGGING) 
