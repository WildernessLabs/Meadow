/****************************************************************************
 * \apps\examples\hcom\diag\hcom_diag_misc.c
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

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/userspace.h>

#include <arch/board/boardctl.h>
#include <sys/boardctl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#if HCOM_NUTT_SHELL_LAUNCHER_INCLUDE_IN_BUILD > 0 &&  !defined (CONFIG_SYSTEM_NSH)
#warning"HCOM_NUTT_SHELL_LAUNCHER_INCLUDE_IN_BUILD is > 0 but CONFIG_SYSTEM_NSH is not defined"
#endif

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

#if HCOM_NUTT_SHELL_LAUNCHER_INCLUDE_IN_BUILD > 0
int nsh_main(int argc, char *argv[]);
static bool _nsh_enabled;
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_diag_misc_setup()
{
#if HCOM_NUTT_SHELL_LAUNCHER_INCLUDE_IN_BUILD > 0
  _nsh_enabled = false;
#endif
  return OK;
}

#if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
//============================================================================
// For diagnostic use only
void hcom_diag_misc_print_buffer(const uint8_t buffer[], const int bufLen, uint8_t msgPriority)
{
#define HCOM_UTIL_BYTES_PER_LINE 16
#define HCOM_UTIL_LEADING_SPACES 2
#define HCOM_UTIL_HEXADECIMAL_OFFSET (8 + HCOM_UTIL_LEADING_SPACES)
#define HCOM_UTIL_ASCII_OFFSET (57 + HCOM_UTIL_LEADING_SPACES)
#define HCOM_UTIL_DISPLAY_LENGTH (HCOM_UTIL_ASCII_OFFSET + HCOM_UTIL_BYTES_PER_LINE + 3)

  if ((hcom_diag_logging_get_syslog_mask() & LOG_MASK(msgPriority)) == 0)
    return;

  if(bufLen <= 0)
  {
    hcom_logging_syslog(msgPriority, "%s@%d-hcom_diag_misc_print_buffer() but 'bufLen:%d'\n",
              thisFile, __LINE__, bufLen);
    return;
  }

  if(buffer == NULL)
  {
    hcom_logging_syslog(msgPriority, "%s@%d-hcom_diag_misc_print_buffer() but 'buffer == NULL'\n",
              thisFile, __LINE__);
    return;
  }

  int rowStartOffset, rowByteOffset;
  char lineBuff[HCOM_UTIL_DISPLAY_LENGTH];
  int hexOffset;
  int asciiOffset;

  // One line at a time
  for (rowStartOffset = 0; rowStartOffset < bufLen; rowStartOffset += HCOM_UTIL_BYTES_PER_LINE)
  {
    memset(lineBuff, 0x20, HCOM_UTIL_DISPLAY_LENGTH);

    // Buffer offset address
    snprintf(&lineBuff[HCOM_UTIL_LEADING_SPACES], HCOM_UTIL_DISPLAY_LENGTH, "%08x ", rowStartOffset);

    hexOffset = HCOM_UTIL_HEXADECIMAL_OFFSET;
    asciiOffset = HCOM_UTIL_ASCII_OFFSET;

    for (rowByteOffset = 0; rowByteOffset < HCOM_UTIL_BYTES_PER_LINE; rowByteOffset++)
    {
      off_t buffOffset = rowStartOffset + rowByteOffset;
      if (buffOffset >= bufLen)
        break;        // Reached the end of the buffer's data

      // Grab the next byte to output
      uint8_t nextByte = buffer[buffOffset];

      // Save the hex value (add '.' half way)
      if(rowByteOffset == HCOM_UTIL_BYTES_PER_LINE / 2)
        snprintf(&lineBuff[hexOffset], HCOM_UTIL_DISPLAY_LENGTH - hexOffset, ".%02x", nextByte);
      else
        snprintf(&lineBuff[hexOffset], HCOM_UTIL_DISPLAY_LENGTH - hexOffset, " %02x", nextByte);
      hexOffset += 3;

      // Save the ascii value
      if (nextByte == 0) // Make it easy to spot '0'
        snprintf(&lineBuff[asciiOffset], HCOM_UTIL_DISPLAY_LENGTH - hexOffset, "-");
      else if (nextByte == 0xff)
        snprintf(&lineBuff[asciiOffset], HCOM_UTIL_DISPLAY_LENGTH - hexOffset, "*");
      else if (nextByte < 0x20 || nextByte > 0x7e)  //isprint()
        snprintf(&lineBuff[asciiOffset], HCOM_UTIL_DISPLAY_LENGTH - hexOffset, ".");
      else
        snprintf(&lineBuff[asciiOffset], HCOM_UTIL_DISPLAY_LENGTH - hexOffset, "%c", nextByte);

      asciiOffset++;
      DEBUGASSERT(asciiOffset < HCOM_UTIL_DISPLAY_LENGTH - 1);
    }

    // This row is ready
    lineBuff[hexOffset] = 0x20;   // Replace last hex null with a space
    lineBuff[asciiOffset++] = 0x0a; // line feed
    lineBuff[asciiOffset] = 0x00; // null terminator

  // Output one row at a time
  hcom_logging_syslog(msgPriority, lineBuff);
  }
}
#else
void hcom_diag_misc_print_buffer(const uint8_t buffer[], const int bufLen, uint8_t msgPriority)
{
}
#endif

//=======================================================================================
// I'm leaving this code here because it could become useful. The reason this
// doesn't work is that when it was moved to /apps it now shares stdout with
// all other /apps applications including mono. Mono routes all Console.Write()
// calls to stdout. To get Console.Write() calls to output on CLI, stdout was
// redirected, and with it the NSH output was redirected too.
// If NSH is desired either disable mono from running via CLI command or prevent
// the stdout redirection code from running (via a code hack). Then hook up a
// terminal to D0 & D1 (UART 4).
// userData = 1 enables all other values are ignored. The CLI command --NSHEnable
// will automatically set userData to 1.
void hcom_diag_misc_launch_nsh(uint32_t userData)
{
#if HCOM_NUTT_SHELL_LAUNCHER_INCLUDE_IN_BUILD > 0

  // Currently, NSH can only be enabled once from the CLI. After this the _nsh_enable flag
  // will be set true and prevents nsh from being re-started.
  // A nice but not needed feature would be to find a different method to determine
  // if nsh is running (e.g. pthread_join, waitpid(),  waitid() or atexit()).
  // This is not a big problem especially as this code no longer works, as discribed
  // above.
  int _nsh_pid;

  if(_nsh_enabled)
  {
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, "NSH already enabled",
            thisFile, __LINE__);
    return;
  }

  if(userData == 1)
  {
    // When mono starts it reconfigures all the GPIOs. Thie call
    // will restore the Tx and Rx configuration to UART4.
    hcom_via_nx_restore_uart_reconfig(4);

    // Create a unique task for NSH
    _nsh_pid = task_create("nsh", CONFIG_SYSTEM_NSH_PRIORITY,
                        CONFIG_SYSTEM_NSH_STACKSIZE,
                        (main_t)nsh_main,
                        (FAR char * const *) NULL);
    if(_nsh_pid > 0)
    {
      _nsh_enabled = true;
      hcom_logging_syslog(LOG_INFO, "%s@%d-NSH now enabled [pid:%d, pri:%d, stack:%d]\n",
              thisFile, __LINE__, _nsh_pid, CONFIG_SYSTEM_NSH_PRIORITY, CONFIG_SYSTEM_NSH_STACKSIZE);
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
              "NSH now enabled until Meadow restart", thisFile, __LINE__);
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
            "Not possible to disable NSH", thisFile, __LINE__);
    return;
  }
#else
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, 
          "NuttShell not available", thisFile, __LINE__);
#endif
}
