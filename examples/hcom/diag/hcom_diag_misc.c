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

#if HCOM_INCLUDE_DIAG_DECODE_MESSAGE_CODE > 0
//=======================================================================================
// UNTESTED! out of time Maybe next PR
// Takes an encoded message and decodes it, and outputs a string contining it's header
void hcom_diag_misc_build_info_from_recvd_msg(uint8_t buffer[], const int bufLen, bool isEncoded)
{
  uint8_t *decodedBuf;
  size_t decodedMsgLen;
  struct HcomProtocolHeader_s *msgHeader;

  char *MajorRqstType[] = 
  {
    "UNDEFINED",
    "SIMPLE",
    "FILE_START",
    "SIMPLE_TEXT",
    "SIMPLE_BINARY"
  };

  char *MinorSimple00e0RqstType[] = 
  {
    "UNDEFINED_REQUEST",       // 0x00
    "CREATE_ENTIRE_FLASH_FS",  // 0x01
    "CHANGE_TRACE_LEVEL",      // 0x02
    "FORMAT_FLASH_FILE_SYS",   // 0x03
    "END_FILE_TRANSFER",       // 0x04
    "RESTART_PRIMARY_MCU",     // 0x05
    "VERIFY_ERASED_FLASH",     // 0x06
    "PARTITION_FLASH_FS",      // 0x07
    "MOUNT_FLASH_FS",          // 0x08
    "INITIALIZE_FLASH_FS",     // 0x09
    "BULK_FLASH_ERASE",        // 0x0a
    "ENTER_DFU_MODE",          // 0x0b
    "ENABLE_DISABLE_NSH",      // 0x0c
    "LIST_PARTITION_FILES",    // 0x0d
    "LIST_PART_FILES_AND_CRC", // 0x0e
    "MONO_DISABLE",            // 0x0f
    "MONO_ENABLE",             // 0x10
    "MONO_RUN_STATE",          // 0x11
    "GET_DEVICE_INFORMATION",  // 0x12
    "PART_RENEW_FILE_SYS",     // 0x13
    "NO_TRACE_TO_HOST",        // 0x14
    "SEND_TRACE_TO_HOST",      // 0x15
    "END_ESP_FILE_TRANSFER",   // 0x16
    "READ_ESP_MAC_ADDRESS",    // 0x17
    "RESTART_ESP32",           // 0x18
    "MONO_FLASH",              // 0x19
    "SEND_TRACE_TO_UART",      // 0x1a
    "NO_TRACE_TO_UART",        // 0x1b
    "MONO_UPDATE_RUNTIME",     // 0x1c
    "MONO_UPDATE_FILE_END",    // 0x1d
  };

  char *MinorSimple0fffRqstType[] = 
  {
    "DEVELOPER_1",             // 0xf0
    "DEVELOPER_2",             // 0xf1
    "DEVELOPER_3",             // 0xf2
    "DEVELOPER_4",             // 0xf3
    "S25FL_QSPI_INIT",         // 0xf4
    "S25FL_QSPI_WRITE",        // 0xf5
    "S25FL_QSPI_READ",         // 0xf6
  };

  char *MinorFileStartRqstType[] = 
  {
    "Undefined",
    "START_FILE_TRANSFER",     // 0x01
    "DELETE_FILE_BY_NAME",     // 0x02
    "START_ESP_FILE_TRANSFER", // 0x03
  };

  char *MinorBinaryRqstType[] = 
  {
    "Undefined",
    "DEBUGGER_MSG",            // 0x01
  };

  if(isEncoded)
  {
  syslog(1, "===>>> print msg after received 1e\n"); usleep(50 * 1000);
    decodedBuf = malloc(bufLen);
  syslog(1, "===>>> print msg after received 2e\n"); usleep(50 * 1000);

    // Ignore the leading and trailing delimiters
    decodedMsgLen = hcom_host_cobs_decoder(buffer + 1, bufLen - 1, decodedBuf);
  syslog(1, "===>>> print msg after received 3e\n"); usleep(50 * 1000);

    // Decoded should always be smaller than encoded
    DEBUGASSERT(bufLen > decodedMsgLen);

  syslog(1, "===>>> print msg after received 4e\n"); usleep(50 * 1000);
    msgHeader = (struct HcomProtocolHeader_s *)decodedBuf;
  syslog(1, "===>>> print msg after received 5e\n"); usleep(50 * 1000);
  }
  else
  {
  syslog(1, "===>>> print msg after received 1n\n"); usleep(50 * 1000);
    msgHeader = (struct HcomProtocolHeader_s *)buffer;
  syslog(1, "===>>> print msg after received 2n\n"); usleep(50 * 1000);
  }
  
  uint8_t majorRqstType = (msgHeader->rqstType & HCOM_PROTOCOL_HEADER_MAJOR_TYPE_MASK) >> 8;
  uint8_t minorRqstType = msgHeader->rqstType & HCOM_PROTOCOL_HEADER_MINOR_TYPE_MASK;
  
  char *strMajorRqstType = MajorRqstType[majorRqstType];
  char *strMinorRqstType;
syslog(1, "===>>> print msg after received 6\n"); usleep(50 * 1000);
  
  switch(majorRqstType)
  {
    case 0:   // HCOM_PROTOCOL_HEADER_TYPE_UNDEFINED
    case 1:   // HCOM_PROTOCOL_HEADER_TYPE_SIMPLE
    case 2:   // HCOM_PROTOCOL_HEADER_TYPE_FILE_START
      if(minorRqstType < 0xf0)
        strMinorRqstType = MinorSimple00e0RqstType[minorRqstType];
      else
        strMinorRqstType = MinorSimple0fffRqstType[minorRqstType - 0xf0];
      break;
    case 3:   // HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT
      strMinorRqstType = MinorFileStartRqstType[minorRqstType];
      break;
    case 4:   // HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY (CLI -> HCOM)
      strMinorRqstType = MinorBinaryRqstType[minorRqstType];
      break;
    default:
      strMinorRqstType = "Unknown";
  }
syslog(1, "===>>> print msg after received 7\n"); usleep(50 * 1000);
hcom_diag_misc_print_buffer(msgHeader, bufLen, 1);

  syslog(1, "Message-Length:%d, SeqNumb:0x%04x, Version:0x%04x, rqstType:0x%04x, userData:0x%08x\n",
            bufLen, msgHeader->seqNumber, msgHeader->version, msgHeader->rqstType, msgHeader->userData);
syslog(1, "===>>> print msg after received 8\n"); usleep(50 * 1000);

  // Build a string for the user
  syslog(1, "Message-Encoded:%s, SeqNumb:%d, Version:0x%04x, Length:%d, Type:%s:%s\n",
            isEncoded ? "Yes" : "No", msgHeader->seqNumber, msgHeader->version,
            bufLen, strMajorRqstType, strMinorRqstType);
syslog(1, "===>>> print msg after received 9\n"); usleep(50 * 1000);
}

//=======================================================================================
// UNTESTED! out of time Maybe next PR
// Takes an encoded message and decodes it, and outputs a string contining it's header
void hcom_diag_misc_build_info_from_send_msg(uint8_t buffer[], const int bufLen, bool isEncoded)
{
  uint8_t *decodedBuf;
  size_t decodedMsgLen;
  struct HcomProtocolHeader_s *msgHeader;

  char *MajorRqstType[] = 
  {
    "UNDEFINED",
    "SIMPLE",
    "FILE_START",
    "SIMPLE_TEXT",
    "SIMPLE_BINARY"
  };

  char *MinorSimpleRqstType[] = 
  {
    "Unknown",
    "HEADER_MESSAGE"
  };

  char *MinorSimpleTextRqstType[] = 
  {
    "Unknown",            // 0x00
    "REJECTED",           // 0x01
    "ACCEPTED",           // 0x02
    "CONCLUDED",          // 0x03
    "ERROR",              // 0x04
    "INFORMATION",        // 0x05
    "LIST_HEADER",        // 0x06
    "LIST_MEMBER",        // 0x07
    "CRC_MEMBER",         // 0x08
    "MONO_STDOUT",        // 0x09
    "DEVICE_INFO",        // 0x0A
    "TRACE_MSG",          // 0x0B
    "RECONNECT",          // 0x0C
    "MONO_STDERR"         // 0x0D
  };

  char *MinorBinaryRqstType[] = 
  {
    "Undefined",
    "DEBUGGER_MSG",            // 0x01
  };

  if(isEncoded)
  {
  syslog(1, "===>>> print msg before sending 1e\n"); usleep(50 * 1000);
    decodedBuf = malloc(bufLen);
  syslog(1, "===>>> print msg before sending 2e\n"); usleep(50 * 1000);

    // Ignore the leading and trailing delimiters
    decodedMsgLen = hcom_host_cobs_decoder(buffer + 1, bufLen - 1, decodedBuf);
  syslog(1, "===>>> print msg before sending 3e\n"); usleep(50 * 1000);

    // Decoded should always be smaller than encoded
    DEBUGASSERT(bufLen > decodedMsgLen);

  syslog(1, "===>>> print msg before sending 4e\n"); usleep(50 * 1000);
    msgHeader = (struct HcomProtocolHeader_s *)decodedBuf;
  syslog(1, "===>>> print msg before sending 5e\n"); usleep(50 * 1000);
  }
  else
  {
  syslog(1, "===>>> print msg before sending 1n\n"); usleep(50 * 1000);
    msgHeader = (struct HcomProtocolHeader_s *)buffer;
  syslog(1, "===>>> print msg before sending 2n\n"); usleep(50 * 1000);
  }
  
  uint8_t majorRqstType = (msgHeader->rqstType & HCOM_PROTOCOL_HEADER_MAJOR_TYPE_MASK) >> 8;
  uint8_t minorRqstType = msgHeader->rqstType & HCOM_PROTOCOL_HEADER_MINOR_TYPE_MASK;
  
  char *strMajorRqstType = MajorRqstType[majorRqstType];
  char *strMinorRqstType;
syslog(1, "===>>> print msg before sending 6\n"); usleep(50 * 1000);
  
  switch(majorRqstType)
  {
    case 0:   // HCOM_PROTOCOL_HEADER_TYPE_UNDEFINED
    case 1:   // HCOM_PROTOCOL_HEADER_TYPE_SIMPLE
      strMinorRqstType = MinorSimpleRqstType[minorRqstType];
      break;
    case 2:   // HCOM_PROTOCOL_HEADER_TYPE_FILE_START
      strMinorRqstType = "Never used";
      break;
    case 3:   // HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT
      strMinorRqstType = MinorSimpleTextRqstType[minorRqstType];
      break;
    case 4:   // HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY (CLI -> HCOM)
      strMinorRqstType = MinorBinaryRqstType[minorRqstType];
      break;
    default:
      strMinorRqstType = "Unknown";
  }
syslog(1, "===>>> print msg before sending 7\n"); usleep(50 * 1000);
hcom_diag_misc_print_buffer(msgHeader, bufLen, 1);

  syslog(1, "Message-Length:%d, SeqNumb:0x%04x, Version:0x%04x, rqstType:0x%04x, userData:0x%08x\n",
            bufLen, msgHeader->seqNumber, msgHeader->version, msgHeader->rqstType, msgHeader->userData);
syslog(1, "===>>> print msg before sending 8\n"); usleep(50 * 1000);

  // Build a string for the user
  syslog(1, "Message-Encoded:%s, SeqNumb:%d, Version:0x%04x, Length:%d, Type:%s:%s\n",
            isEncoded ? "Yes" : "No", msgHeader->seqNumber, msgHeader->version,
            bufLen, strMajorRqstType, strMinorRqstType);
syslog(1, "===>>> print msg before sending 9\n"); usleep(50 * 1000);
}

#else
void hcom_diag_misc_build_info_from_recvd_msg(uint8_t buffer[], const int bufLen, bool isEncoded)
{
}
void hcom_diag_misc_build_info_from_send_msg(uint8_t buffer[], const int bufLen, bool isEncoded)
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
    hcom_via_nx_restore_uart_reconfig(hcom_via_nx_get_fd(), 4);

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
