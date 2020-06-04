/****************************************************************************
 * examples/mono/mono_main.c
 *
 *   Copyright (C) 2018-2019 Wilderness Labs. All rights reserved.
 *
 ****************************************************************************/

// #define BUILD_MONO_DEBUGGING_TEST_CODE
#define MONO_DEBUG_TEST_RECV_BUFF_SIZE 500

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/net/net.h>

#include <arch/board/boardctl.h>
#include <sys/boardctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <syscall.h>

#include "../../../mono/config.h"

typedef struct {
  const char *name;
  void *addr;
} MonoDlMapping;

#include "mappings-meadow.h"
#include "mappings-system-native.h"

#include "../../../nuttx/configs/stm32f777zit6-meadow/src/hcom/hcom_mono_main.h"

#ifdef BUILD_MONO_DEBUGGING_TEST_CODE
// For TCP Testing
#include <sys/socket.h>
#include <sys/un.h>
#endif
/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t _startupAction;
static bool _shutting_down = false;

static int _pipe_fd = -1;

#ifdef BUILD_MONO_DEBUGGING_TEST_CODE
static int _sockfd = -1;
static bool _connected = false;
#endif
/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef BUILD_MONO_DEBUGGING_TEST_CODE
//====================================================
// Called mono_main
static int MonoDebugTestConnect(void)
{
  struct sockaddr_un myaddr;
  socklen_t addrlen;
  int ret;

  _sockfd = socket(PF_LOCAL, SOCK_STREAM, 0);
  if (_sockfd < 0)
  {
    syslog(LOG_ERR, "M->client:socket creation failed _sockfd:%d errno:%d\n", _sockfd, errno);
    goto errout_with_nothing;
  }

  /* Connect the socket to the server */
  addrlen = strlen(HCOM_REMOTE_DBG_SOCKET_NAME);
  if (addrlen > UNIX_PATH_MAX - 1)
    addrlen = UNIX_PATH_MAX - 1;

  myaddr.sun_family = AF_LOCAL;
  strncpy(myaddr.sun_path, HCOM_REMOTE_DBG_SOCKET_NAME, addrlen);
  myaddr.sun_path[addrlen] = '\0';
  addrlen += sizeof(sa_family_t) + 1;

  syslog(0, "M->client: Connect to %s...\n", HCOM_REMOTE_DBG_SOCKET_NAME);

  int attemptCnt = 0;
  do
  {
    attemptCnt++;
    ret = connect(_sockfd, (struct sockaddr *)&myaddr, addrlen);
    if (ret < 0)
    {
      syslog(LOG_INFO, "M->client:connect failed retry in 1 sec. ret: %d, errno:%d attempted %d\n",
       ret, errno, attemptCnt);
      sleep(1);
    }
  } while(ret < 0);

  syslog(LOG_INFO, "M->client:hcom-d connect attempted %d SUCCESSFUL\n", attemptCnt);
  usleep(50 * 1000);
  _connected = true;  
  return OK;

errout_with_nothing:
  return -1;
}

//==========================================================
static int MonoDebugTestSend(uint8_t *sendBuffer, int sendSize)
{
  int nbytessent;
  
  /* Then send one message */
  nbytessent = send(_sockfd, sendBuffer, sendSize, 0);
  if (nbytessent < 0)
  {
    syslog(0, "M->client:send failed: %d\n", errno);
    goto errout_with_socket;
  }
  else if (nbytessent != sendSize)
  {
    syslog(0, "M->client:Bad send length: %d Expected: %d\n", nbytessent, sendSize);
    goto errout_with_socket;
  }

  syslog(0, "M->client:Sent %d bytes to hcom-d\n", sendSize);
  return OK;

errout_with_socket:
  syslog(0, "M->client:Exit error\n");
  return 1;
}


//-------------------------------------------------------
// This call blocks
static int MonoDebugTestReceive(uint8_t *recvBuffer)
{
  int nbytesrecvd;  
  
  syslog(0, "M->client:Waiting to receiving from hcom-d\n");
  nbytesrecvd = recv(_sockfd, recvBuffer, MONO_DEBUG_TEST_RECV_BUFF_SIZE, 0);
  if (nbytesrecvd < 0)
  {
    syslog(0, "M->client:recv failed: %d\n", errno);
    goto errout_with_socket;
  }
  else if (nbytesrecvd == 0)
  {
    syslog(0, "M->client:The server closed the connection\n");
    goto errout_with_socket;
  }

  syslog(0, "M->client:Received %d bytes\n", nbytesrecvd);
  
  //close(_sockfd);
  return nbytesrecvd;

errout_with_socket:
  syslog(0, "M->client:RECEIVE Exit error\n");
  return -1;
}

//---------------------------------------------------
// Receives and echos back to host
static int MonoDebugTestExecute(void)
{
  int ret;
  FAR uint8_t *inbuf;
  FAR uint8_t *outbuf;

  outbuf = (uint8_t*)malloc(MONO_DEBUG_TEST_RECV_BUFF_SIZE);
  if (outbuf == NULL)
  {
    syslog(0, "M->client:failed to allocate outbuf %d long must exit\n", MONO_DEBUG_TEST_RECV_BUFF_SIZE);
    exit(1);
  }

  inbuf  = (uint8_t*)malloc(MONO_DEBUG_TEST_RECV_BUFF_SIZE);
  if (inbuf == NULL)
  {
    syslog(0, "M->client:failed to allocate inbuf %d long must exit\n", MONO_DEBUG_TEST_RECV_BUFF_SIZE);
    exit(1);
  }

  syslog(0, "M->client: inbuf and outbuf allocated\n");
  
  int xmitCount = 0;

  while(true)
  {
    ret = MonoDebugTestConnect();
    if(ret < 0)
    {
      syslog(0, "M->client:Connection could not be made\n");
      return -1;
    }
    
    // Loop forever
    do
    {
      xmitCount++;

      // Blocking call
      ret = MonoDebugTestReceive(inbuf);
      if(ret < 0)
      {
        syslog(0, "M->client:Receive failed\n");
        continue;
      }

      // This is an echo client so we set whatever we receive
      // Send
      ret = MonoDebugTestSend(inbuf, ret);
      if(ret < 0)
      {
        syslog(0, "M->client:Send failed\n");
      }
    } while(xmitCount % 5 != 0);
    
    syslog(0, "M->client:Closing socket. Will reconnect %d.\n", xmitCount);
    close(_sockfd);
  }

  exit(1);
}
#endif
//==================================================================
static int RedirectStdout(void)
{
  int ret;
  int errcode = 0;

  if(!_shutting_down && _pipe_fd < 0)
  {
    set_errno(0);

    // Open loop
    do
    {
      // Opening with O_NONBLOCK seems like the right thing to do but
      // it is NOT. It causes the mono app to halt.
      _pipe_fd = open(HCOM_MONO_MAIN_STDOUT_PIPE, O_WRONLY);
      if(_pipe_fd > 0)
        break;

      errcode = errno;
      if(errcode != ENOENT)   // ENOENT = Error No Entity -> No such file or directory
      {
        syslog(LOG_ERR, "%s() Error: open() of %s failed with errno=%d\n",
          __func__, HCOM_MONO_MAIN_STDOUT_PIPE, errcode);
        _pipe_fd = -1;
        return 1;
      }
      
      usleep(500 * 1000);
    } while (errcode == ENOENT);

    // Assign the pipe's input to what has been stdout
    // ret should be 1 the stdout fd
    ret = dup2(_pipe_fd, STDOUT_FILENO);
    if (ret < 0)
    {
      syslog(LOG_ERR, "redirect_writer: dup2 failed ret:%d errno:%d\n", ret, errno);
      return 2;
    }
  }
  return OK;
}

/****************************************************************************
 * mono_main
 ****************************************************************************/

extern int mono_main (int argc, char* argv[]);
extern void mono_dl_register_library(char *name, MonoDlMapping *mappings);

extern void symtab_initialize(void);

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int mono_main(int argc, char *argv[])
#endif
{
  // When nuttx launches the user defined entry point (CONFIG_USER_ENTRYPOINT) 
  // argc == 1 and argv[0] = "init" this is the name NuttX always gives the
  // task it internally launches.
  if(argc != 1 || strcmp(argv[0], "init") != 0)
  {
    // This is NOT THE NORMAL NUTTX STARTUP via 'init' task
    // Maybe there's a work request to be acted upon for the next MCU reset.
    if(argc == (int)HCOM_MONO_MAIN_ACCESS_KEY)
    {
      // Save the numberic value until NuttX re-starts mono. It will then be tested
      // and if a match is found take some special action.
      _startupAction = atoi(argv[0]);
      return OK;
    }

#ifdef CONFIG_SYSTEM_NSH
    // Note: there's always 1 argument, it's the name of the task. For the one
    // defined by CONFIG_USER_ENTRYPOINT it's "init". So using a different task
    // name (e.g. "nshTask") allows the mono_main entry point to launch NuttShell.
    if(argc == 1 && strcmp(argv[0], "nshTask") == 0)
    {
      nsh_main(argc, argv);
      return OK;
    }
#endif

    return OK;
    //-------------------------------------------------
  }

  // NuttX is attempting to start mono.
  // Check if it should be started
  if(_startupAction == HCOM_MONO_MAIN_ACTION_ENABLE_KEY)
    return OK;    // Disable mono by returning the thread that was to run it

  // Normal mono startup follows
  symtab_initialize();

  // Enable QSPI memory mapping mode.
  boardctl(BIOC_ENTER_MEMMAP, 0);

  // Check if Meadow.OS runtime is flashed at external flash.
  #define STM32_FMCBANK4_BASE  0x90000000     /* 0x90000000-0x9fffffff: FMC bank 4 */
  uint32_t signature = *((uint32_t*)STM32_FMCBANK4_BASE);
  if (signature != 0xDDCCBBAA)
  {
    syslog(LOG_ERR, "Mono runtime was not found flashed in external flash.\n");
    return 0;
  }

  // Copy the Meadow.OS runtime to SDRAM for execution.
  memcpy(CONFIG_HEAP2_BASE, STM32_FMCBANK4_BASE, 0x200000);

  boardctl(BIOC_EXIT_MEMMAP, 0);

#ifdef CONFIG_MTD_PARTITION
  const char app_path[] = "/meadow0/App.exe";
#else
  const char app_path[] = "/meadow/App.exe";
#endif

  int fd = open(app_path, O_RDONLY);
  if (fd == -1) {
    syslog(LOG_ERR, "Mono managed app was not found in %s, skipping Mono initialization...\n",
      app_path);
    return 0;
  }

#if defined(CONFIG_HCOM_MONO_OUTPUT_PIPE)
  RedirectStdout();
#endif

#ifdef BUILD_MONO_DEBUGGING_TEST_CODE
  MonoDebugTestExecute();
#endif

  usleep(300 * 1000);

  int ret;
  const char *mono_argv[] = {"mono", "--interp", app_path};
  const int mono_argc = sizeof(mono_argv) / sizeof(mono_argv[0]);

  setenv("MONO_LOG_LEVEL", "debug", 1);
  
#ifdef CONFIG_MTD_PARTITION
  mono_set_assemblies_path("/meadow0");
#else
  mono_set_assemblies_path("/meadow");
#endif

  mono_dl_register_library("System.Native", system_native_mappings);
  mono_dl_register_library("nuttx", meadow_mappings);

  ret = mono_main_driver (mono_argc, mono_argv);

  return ret;
}
