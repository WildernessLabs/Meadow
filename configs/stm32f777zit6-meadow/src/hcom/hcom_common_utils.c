/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_common_utils.c
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

#include <nuttx/config.h>
#include "syslog.h"
#include "hcom_common.h"
#include <nuttx/userspace.h>
#include "chip/stm32f76xx77xx_memorymap.h"
#include "chip/stm32_rtcc.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

uint8_t g_syslog_mask;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Implementation
 ****************************************************************************/

void hcom_diag_print_buffer(const uint8_t buffer[], const int bufLen, uint8_t logPriority)
{
#if 1
  if ((g_syslog_mask & LOG_MASK(logPriority)) == 0)
    return;

#define HCOM_UTIL_BYTES_PER_LINE 16
#define HCOM_UTIL_LEADING_SPACES 2
#define HCOM_UTIL_HEXADECIMAL_OFFSET (8 + HCOM_UTIL_LEADING_SPACES)
#define HCOM_UTIL_ASCII_OFFSET (57 + HCOM_UTIL_LEADING_SPACES)
#define HCOM_UTIL_DISPLAY_LENGTH (HCOM_UTIL_ASCII_OFFSET + HCOM_UTIL_BYTES_PER_LINE + 2)

  int totalColumnOffset, rowByteOffset;
  char lineBuff[HCOM_UTIL_DISPLAY_LENGTH];
  int hexOffset;
  int asciiOffset;

  // There are offsets used in lineBuffer
  for (totalColumnOffset = 0; totalColumnOffset < bufLen; totalColumnOffset += HCOM_UTIL_BYTES_PER_LINE)
  {
    memset(lineBuff, 0x20, HCOM_UTIL_DISPLAY_LENGTH);
    snprintf(&lineBuff[HCOM_UTIL_LEADING_SPACES], HCOM_UTIL_DISPLAY_LENGTH, "%08x ", totalColumnOffset);

    hexOffset = HCOM_UTIL_HEXADECIMAL_OFFSET;
    asciiOffset = HCOM_UTIL_ASCII_OFFSET;

    for (rowByteOffset = 0; rowByteOffset < HCOM_UTIL_BYTES_PER_LINE; rowByteOffset++)
    {
      if (totalColumnOffset + rowByteOffset >= bufLen)
      {
        //snprintf(&lineBuff[hexOffset], HCOM_UTIL_DISPLAY_LENGTH - hexOffset, "   ");
        hexOffset += 3;
        continue;
      }

      uint8_t nextByte = buffer[totalColumnOffset + rowByteOffset];

      // place the hex
      snprintf(&lineBuff[hexOffset], HCOM_UTIL_DISPLAY_LENGTH - hexOffset, " %02x", nextByte);
      hexOffset += 3;

      // place the ascii
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

    lineBuff[hexOffset] = 0x20;   // Replace last null with a space
    lineBuff[asciiOffset] = 0x00; // Follow last character with null

    syslog(logPriority, "%s\n", lineBuff);
  }

  syslog(logPriority, "\n");
#endif
}

//===================================================================
// Get from a battery backed register
int hcom_read_persisted_trace_level_mask()
{
  return *((uint32_t *) STM32_RTC_BK31R);
}

//===================================================================
// Save in a battery backed register
void hcom_persist_trace_level_mask(int newTraceLevelMask)
{
  *((uint32_t *) STM32_RTC_BK31R) = newTraceLevelMask;
}

//===================================================================
uint32_t hcom_battery_backed_reg_read(uint32_t regNumber)
{
  return *((uint32_t *) regNumber);
}

//===================================================================
void hcom_battery_backed_reg_save(uint32_t regNumber, uint32_t value)
{
  *((uint32_t *) regNumber) = value;
}

//===================================================================
// This is called during startup, before the hcom thread is created
void hcom_boot_time_mono_check()
{
#ifdef CONFIG_USER_ENTRYPOINT
  // Do we need to prepare mono for special behavior?
  if(hcom_battery_backed_reg_read(STM32_RTC_BK30R) == HCOM_MONO_MAIN_ACCESS_KEY)
  {
    char *argv[1];
    char buffer[16];

    // Send the action to mono_main in argv
    itoa(hcom_battery_backed_reg_read(STM32_RTC_BK29R), buffer, 10);
    argv[0] = buffer;
    uint32_t argc = HCOM_MONO_MAIN_ACCESS_KEY;
    
    // Call mono_main
    (*USERSPACE->us_entrypoint)((int)argc, argv);

    f7syslog(LOG_WARNING, "Mono is disabled and will not execute applications.\n");
  }
#endif
}

//===================================================================
bool hcom_is_mono_disabled()
{
#ifdef CONFIG_USER_ENTRYPOINT
  if(hcom_battery_backed_reg_read(STM32_RTC_BK30R) == HCOM_MONO_MAIN_ACCESS_KEY)
  {
    if(hcom_battery_backed_reg_read(STM32_RTC_BK29R) != 0)
      return true;
  }
#endif
  return false;
}

//===================================================================
// Route diagnostic logs
void f7syslog(int priority, FAR const IPTR char *fmt, ...)
{
  if ((g_syslog_mask & LOG_MASK(priority)) == 0)
    return;

  va_list ap;
  va_start(ap, fmt);
  vsyslog(priority, fmt, ap);
  va_end(ap);

  fflush(stdout);

  //syslog_dev_flush();
  //usleep(50 * 1000);    // This helps prevent the overwriting of log output
}
