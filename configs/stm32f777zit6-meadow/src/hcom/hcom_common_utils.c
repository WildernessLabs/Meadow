/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_common_utils.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2017 Alan Carvalho de Assis. All rights reserved.
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
#include "hcom_common.h"

#include <nuttx/config.h>
#include "syslog.h"
#include <nuttx/userspace.h>

// FOR TESTING
// #include <nuttx/sched.h>
// #include <../sched/sched/sched.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

// The g_syslog_mask is external and set by NuttX. Don't make static
uint8_t g_syslog_mask;

static pid_t _hcom_pid = 0;
static sem_t _waitF7syslogSem;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static void vf7syslog(int priority, FAR const IPTR char *fmt, va_list args);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

//============================================================================
int hcom_common_utils_setup()
{
  _hcom_pid  = getpid();
  return nxsem_init(&_waitF7syslogSem, 0, 1);
}

//============================================================================
void hcom_common_utils_shutdown()
{
  nxsem_destroy(&_waitF7syslogSem);
}

//===================================================================================
// Wait for the thread writing to exit
static void hcom_common_utils_f7syslog_takesem(void)
{
  int ret;

  do
    {
      /* Take the semaphore (perhaps waiting) */
      ret = nxsem_wait(&_waitF7syslogSem);

      /* The only case that an error should occur here is if the wait was
       * awakened by a signal.
       */
      DEBUGASSERT(ret == OK || ret == -EINTR);
    }
  while (ret == -EINTR);
}

//============================================================================
void hcom_diag_print_buffer(const uint8_t buffer[], const int bufLen, uint8_t logPriority)
{
#if 1
#define HCOM_UTIL_BYTES_PER_LINE 16
#define HCOM_UTIL_LEADING_SPACES 2
#define HCOM_UTIL_HEXADECIMAL_OFFSET (8 + HCOM_UTIL_LEADING_SPACES)
#define HCOM_UTIL_ASCII_OFFSET (57 + HCOM_UTIL_LEADING_SPACES)
#define HCOM_UTIL_DISPLAY_LENGTH (HCOM_UTIL_ASCII_OFFSET + HCOM_UTIL_BYTES_PER_LINE + 2)

  if ((g_syslog_mask & LOG_MASK(logPriority)) == 0)
    return;

  // If task not the hcom pid then we can't grab the semaphore
  if(_hcom_pid != getpid())
    return;

  hcom_common_utils_f7syslog_takesem();

  int rowStartOffset, rowByteOffset;
  char lineBuff[HCOM_UTIL_DISPLAY_LENGTH];
  int hexOffset;
  int asciiOffset;

  // There are offsets used in lineBuffer
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

      // Save the hex value
      snprintf(&lineBuff[hexOffset], HCOM_UTIL_DISPLAY_LENGTH - hexOffset, " %02x", nextByte);
      hexOffset += 3;

      // Save the ascii value
      if (nextByte == 0) // Make it easy to spot '\0'
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
    lineBuff[asciiOffset] = 0x00; // Follow last character with null

    syslog(logPriority, "%s\n", lineBuff);
  }

  syslog(logPriority, "\n");
  nxsem_post(&_waitF7syslogSem);
#endif
}

//===================================================================
// Reads any of the 32 battery backed registers
uint32_t hcom_bbreg_read(uint32_t regNumber)
{
  return *((uint32_t *) regNumber);
}

//===================================================================
// Writes any of the 32 battery backed registers
void hcom_bbreg_write(uint32_t regNumber, uint32_t value)
{
  *((uint32_t *) regNumber) = value;
}

//===================================================================
// Reads bit(s) in any of the 32 battery backed registers
bool hcom_bbreg_bit_test_and_clear(uint32_t regNumber, uint32_t value)
{
  uint32_t reg = *((uint32_t *) regNumber);
  *((uint32_t *) regNumber) = reg & (~value);
  return (value & reg) != 0;
}

//===================================================================
// Reads bit(s) in any of the 32 battery backed registers
bool hcom_bbreg_bit_test(uint32_t regNumber, uint32_t value)
{
  uint32_t reg = *((uint32_t *) regNumber);
  return (value & reg) != 0;
}

//===================================================================
void hcom_bbreg_bit_clear(uint32_t regNumber, uint32_t value)
{
  uint32_t reg = *((uint32_t *) regNumber);
  *((uint32_t *) regNumber) = reg & (~value);
}

//===================================================================
// Set bit(s) in any of the 32 battery backed registers
void hcom_bbreg_bit_set(uint32_t regNumber, uint32_t value)
{
  uint32_t reg = *((uint32_t *) regNumber);
  *((uint32_t *) regNumber) = reg | value;
}

//===================================================================
// This is called during startup, before the hcom thread is created.
// Its purpose it to allow hcom a chance to call mono_main and configure
// it to either run or not run, before mono_main has a chance to run.
void hcom_boot_time_mono_check()
{
#ifdef CONFIG_USER_ENTRYPOINT
  // Do we need to prepare mono for special behavior?
  if(hcom_bbreg_read(HCOM_BATTERY_BACKED_REG_MONO_ACCESS) == HCOM_MONO_MAIN_ACCESS_KEY)
  {
    char *argv[1];
    char buffer[16];

    // Send the action to mono_main in argv
    itoa(hcom_bbreg_read(HCOM_BATTERY_BACKED_REG_MONO_ACTION), buffer, 10);
    argv[0] = buffer;
    uint32_t argc = HCOM_MONO_MAIN_ACCESS_KEY;
    
    // Call mono_main
    (*USERSPACE->us_entrypoint)((int)argc, argv);

    f7syslog(LOG_WARNING, "Mono is disabled and will not execute applications.\n");
  }
#endif
}

//===================================================================
//
bool hcom_is_mono_disabled()
{
#ifdef CONFIG_USER_ENTRYPOINT
  if(hcom_bbreg_read(HCOM_BATTERY_BACKED_REG_MONO_ACCESS) == HCOM_MONO_MAIN_ACCESS_KEY)
  {
    if(hcom_bbreg_read(HCOM_BATTERY_BACKED_REG_MONO_ACTION) != 0)
      return true;
  }
#endif
  return false;
}

//===================================================================
void f7syslog(int priority, FAR const IPTR char *fmt, ...)
{
  if ((g_syslog_mask & LOG_MASK(priority)) == 0)
    return;   // Nothing to do

  va_list args;
  va_start(args, fmt);
  vsyslog(priority, fmt, args);
  va_end(args);
  //usleep(10 * 1000);    // Helps prevent the overwriting of log output

  // If requested and task is hcom pid then forward
  if(hcom_bbreg_bit_test(HCOM_BATTERY_BACKED_REG_BIT_FLAGS, HCOM_BBREG_DIAG_MSG_TO_HOST_BIT_FLAG) &&
      _hcom_pid == getpid())
  {
    va_start(args, fmt);
    vf7syslog(priority, fmt, args);
    va_end(args);
    //usleep(50 * 1000);    // Helps prevent the overwriting of log output
  }
}

//===================================================================
// Use this for syslog calls that cannot call f7syslog without introducing
// a recursive call loop that never ends
void f7syslog_x(int priority, FAR const IPTR char *fmt, ...)
{
  if ((g_syslog_mask & LOG_MASK(priority)) == 0)
    return;   // Nothing to do

  va_list args;
  va_start(args, fmt);
  vsyslog(priority, fmt, args);
  va_end(args);
}

//===================================================================
// Route diagnostic logs to host
void vf7syslog(int priority, FAR const IPTR char *fmt, va_list args)
{
  char *hostMsg;
  hostMsg = malloc(HCOM_PROTOCOL_REQUEST_MAX_STRING_LEN);
  if(hostMsg == NULL)
  {
    f7syslog_x(LOG_ERR, "%s() @%d memory allocation error\n", __func__, __LINE__);
    return;
  }
  
  int stringLen = vsnprintf(hostMsg, HCOM_PROTOCOL_REQUEST_MAX_STRING_LEN - 1, fmt, args);
  // The snprintf return is considered to be written completely if and only if the returned value
  // is non-negative and less than buf_size.
  DEBUGASSERT(stringLen < HCOM_PROTOCOL_REQUEST_MAX_STRING_LEN);

  // Edge case where the buffer is filled yet has not terminating null
  if(stringLen == HCOM_PROTOCOL_REQUEST_MAX_STRING_LEN - 1)
    hostMsg[stringLen] = '\0';    // insure terminating null
  
  // Need to remove any trailing cr/lf
  size_t found = strcspn(hostMsg, "\r\n");
  hostMsg[found] = '\0';

  int ret = hcom_host_msg_bldr_send_short_text_msg(HcomProtoCtrlRequestDeviceDiag, 0, hostMsg);
  if (ret < 0)    // Watch out for recursion and an infinite loop
    f7syslog_x(LOG_ERR, "%s() @%d Host message error (%d).\n", __func__, __LINE__, ret);

  free(hostMsg);
}
