/****************************************************************************
 * examples/mono/mono_main.c
 *
 *   Copyright (C) 2018-2019 Wilderness Labs. All rights reserved.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <syscall.h>
#include "nuttx-functions.h"
#include "../../../nuttx/configs/stm32f777zit6-meadow/src/hcom/hcom_common.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

static uint32_t _startupAction;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

static bool _shutting_down = false;
static int _pipe_fd = -1;

// Used in testing
bool _stop_now = false;

// Since this function is called by a (or maybe 'the') mono thread
// it cannot hold on to the thread waiting for connections etc.
// because this would block the caller.
static void TestPipeSendMessage(char *message, int msgLength)
{
  if(_shutting_down)
    return;

syslog(0, "==>%s() Entered\n", __func__);

  // Because we cannot hold on to this thread. We'll try to connect once
  // and if this fails throw the message away
  if(!_shutting_down && _pipe_fd < 0)
  {
    syslog(0, "==>%s() Opening pipe\n", __func__);

    // Note: normally open blocks if no reader has opened the read end,
    // that's why O_NONBLOCK is used
    _pipe_fd = open(HCOM_MONO_STDOUT_REDIRECT_PIPE, O_WRONLY|O_NONBLOCK);
    if (_pipe_fd < 0)
    {
      syslog(LOG_ERR, "==>%s() Error: open() of %s failed with errno=%d\n",
        __func__, HCOM_MONO_STDOUT_REDIRECT_PIPE, errno);
      return;
    }
  }

  syslog(0, "==>%s() - Writing %d bytes to pipe.\n",
          __func__, msgLength);

  ssize_t writeReturn = write(_pipe_fd, message, msgLength);
  if(writeReturn >= 0)
  {
    syslog(0, "==>%s() - Successfully wrote %d bytes to pipe.\n",
          __func__, writeReturn);
    return;   // Success
  }

  if(errno == EPIPE)
  {
    syslog(LOG_INFO, "==>%s() - Reader closed pipe (errno = EPIPE and errno = %d\n",
      __func__, errno);
  }
  else
  {
    syslog(LOG_ERR, "==>%s() - ERROR: Write to pipe error %d, errno = %d\n",
        __func__, writeReturn, errno);
  }
  
  close(_pipe_fd);
  _pipe_fd = -1;
}

//-----------------------------------------------------------
// Temporary garbage - For testing
//-----------------------------------------------------------
static int TestPipeCreatePayloadLoop(int argc, char *argv[])
{
#define HCOM_TEMP_PIPE_MAX_MSG_SIZE 2048
  bool _is_buffer_allocated = false;
  char *_buffer;
  int payloadSize;
  int sendDelay;

  syslog(0, "==>%s() - Entry. argc = %d, argv = %s\n", __func__, argc, argv[0]);

  _stop_now = false;

  switch(argc)
  {
    case 10:
      payloadSize = 10;
      sendDelay = 1 * 1000 * 1000;
      break;
    case 11:
      payloadSize = 10;
      sendDelay = 500 * 1000;
      break;
    case 12:
      payloadSize = 10;
      sendDelay = 200 * 1000;
      break;
    case 13:
      payloadSize = 10;
      sendDelay = 100 * 1000;
      break;
    case 14:
      payloadSize = 10;
      sendDelay = 50 * 1000;
      break;
    case 15:
      payloadSize = 10;
      sendDelay = 20 * 1000;
      break;

    case 20:
      payloadSize = 100;
      sendDelay = 1 * 1000 * 1000;
      break;
    case 21:
      payloadSize = 100;
      sendDelay = 500 * 1000;
      break;
    case 22:
      payloadSize = 100;
      sendDelay = 200 * 1000;
      break;
    case 23:
      payloadSize = 100;
      sendDelay = 100 * 1000;
      break;
    case 24:
      payloadSize = 100;
      sendDelay = 50 * 1000;
      break;
    case 25:
      payloadSize = 100;
      sendDelay = 20 * 1000;
      break;

    case 30:
      payloadSize = 1000;
      sendDelay = 10 * 1000 * 1000;
      break;
    case 31:
      payloadSize = 1000;
      sendDelay = 500 * 1000;
      break;
    case 32:
      payloadSize = 1000;
      sendDelay = 200 * 1000;
      break;
    case 33:
      payloadSize = 1000;
      sendDelay = 100 * 1000;
      break;
    case 34:
      payloadSize = 1000;
      sendDelay = 50 * 1000;
      break;
    case 35:
      payloadSize = 1000;
      sendDelay = 20 * 1000;
      break;

    // Stop thread and return
    case 9999:
      syslog(0, "==>%s() - case 9999: - Terminating current test\n", __func__);
      _stop_now = true;
      return 1234;

    default:
      break;  // Just run with default values
  }

  // Since we have a test job allocated a buffer
  if(!_is_buffer_allocated)
  {
    _buffer = malloc(HCOM_TEMP_PIPE_MAX_MSG_SIZE);
    _is_buffer_allocated = true;
  }

  // Test pipe write
  while(!_stop_now)
  {
    int i;  
    // Fill buffer with characters
    for(i = 0; i < payloadSize; i++)
    {
      _buffer[i] = (i % 94) + 33;     // 33 - 126 printable ascii
    }
    _buffer[i] = '\0';
    syslog(0, "==>%s() ----------Write to pipe %d bytes----------------\n", __func__, payloadSize);

    TestPipeSendMessage(_buffer, payloadSize);

    // Wait
    usleep(sendDelay);
  }

  free(_buffer);
  return 4321;
}

//==================================================================
static int RedirectStdout(void)
{
  int ret;
  int errcode = 0;

  if(!_shutting_down /*&& _pipe_fd < 0*/)
  {
    syslog(0, "==>%s() Entering\n", __func__);
    printf(" ");
    fflush(stdout);

    // It's normal for NuttX to start this before all the fcom initialization
    // has created this pipe.
    set_errno(0);
    do
    {
      // Note: normally open blocks if no reader has opened the read end,
      // that's why O_NONBLOCK is used
      _pipe_fd = open(HCOM_MONO_STDOUT_REDIRECT_PIPE, O_WRONLY|O_NONBLOCK);
      if(_pipe_fd > 0)
        break;

      errcode = errno;
      if(errcode != ENOENT)   // ENOENT = Error No Entity -> No such file or directory
      {
        syslog(LOG_ERR, "==>%s() Error: open() of %s failed with errno=%d\n",
          __func__, HCOM_MONO_STDOUT_REDIRECT_PIPE, errcode);
        _pipe_fd = -1;
        return 1;
      }
      
      syslog(0, "==>%s() **** Will wait and attempt to open pipe\n", __func__);
      usleep(500 * 1000);
    } while (errcode == ENOENT);

    syslog(0, "==>%s() Successfully opened pipe\n", __func__);

    ret = dup2(_pipe_fd, STDOUT_FILENO);
    if (ret != 0)
    {
      syslog(LOG_ERR, "redirect_writer: dup2 failed: %d\n", errno);
      return 2;
    }
    syslog(0, "==>%s() Successfully dup2'd pipe\n", __func__);

    /* Close the original file descriptor */
    ret = close(_pipe_fd);
    _pipe_fd = -1;    
    if (ret != 0)
    {
      syslog(LOG_ERR, "redirect_reader: failed to close fdout=%d\n", _pipe_fd);
      return 3;
    }
  }    
  syslog(0, "==>%s() Successfully closed original pipe fd\n", __func__);
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
  // there's 1 argument it's argv[0] = "init" which is the name of the NuttX
  // user defined the task.
  if(strcmp(argv[0], "init") != 0)
  {
    // This is not a normal nuttx startup via 'init' task
    // Maybe there's a work request to be acted upon for the next MCU reset.
    if(argc == (int)HCOM_MONO_MAIN_ACCESS_KEY)
    {
      // Save the action value until NuttX is restarted and re-starts mono.
      _startupAction = atoi(argv[0]);   // This is a number
      return OK;
    }
#ifdef CONFIG_SYSTEM_NSH
    // Note: there's always 1 argument, it's the name of the task. For the one
    // defined by CONFIG_USER_ENTRYPOINT it's "init". So using a different task
    // name (e.g. "nshTask") this entry point to be reused to launch NuttShell.
    if(argc == 1 && strcmp(argv[0], "nshTask") == 0)
    {
      nsh_main(argc, argv);
      return 0;
    }
#endif
    if(argc == 1 && strcmp(argv[0], "RedirectStdout") == 0)
    {
      int ret = RedirectStdout();
      return ret;
    }
    if(argc == 1 && strcmp(argv[0], "TestStdoutAfter") == 0)
    {
      printf("A printf() call AFTER Stdout has been redirected.");
      fflush(stdout);
      return OK;
    }
    if(argc == 1 && strcmp(argv[0], "TestStdoutBefore") == 0)
    {
      printf("A printf() call BEFORE Stdout has been redirected.");
      fflush(stdout);
      return OK;
    }
    if(argc == 1 && strcmp(argv[0], "TestPipe") == 0)
    {
      int ret = TestPipeCreatePayloadLoop(argc, argv);  // Won't return until commanded to stop
      return ret;
    }

    return OK;    // No special work so just exit
  }

  // Check before starting mono if the startup code set an action
  if(_startupAction == HCOM_MONO_ACTION_ENABLE_DISABLE_KEY)
  {
    // Disable mono by returning this thread
    return 0;
  }

  //----------------------------------------------------------
  // Start up mono

syslog(0, "==>%s() Mono starting up\n", __func__);
  
  TestPipeSendMessage(" ", 1);
  //RedirectStdout();

  for(;;) sleep(1);

  usleep(300 * 1000);   // NOT NEEDED IF THE PIPE STUFF IF THE IN PLACE

  // Normal mono startup follows
  symtab_initialize();

  const char app_path[] = "/meadow0/App.exe";

  if (access(app_path, F_OK) == -1) {
    syslog(LOG_ERR, "Mono managed app was not found in %s\nSkipping Mono...",
      app_path);
    return 0;
  }

  int ret;
  const int mono_argc = 4;
  const char *mono_argv[] = {"mono", "--trace", "--interp", app_path};

  setenv("MONO_LOG_LEVEL", "debug", 1);
  mono_set_assemblies_path("/meadow0");

  mono_dl_register_library("nuttx", meadow_os_mappings);
  ret = mono_main_driver (mono_argc, mono_argv);

  return ret;
}
