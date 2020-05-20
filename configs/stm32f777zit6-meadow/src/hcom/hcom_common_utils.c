/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_common_utils.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

#include "stm32_gpio.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

// The g_syslog_mask is external and set by NuttX. Don't make static
extern uint8_t g_syslog_mask;

static char *thisFile = __FILE__;
static sem_t _f7syslogSem;    /* Implements event waiting */
static char *_f7syslogTextBuf;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

#if HCOM_COMMON_UTILS_GPIO_TEST_PROBE > 0
static void hcom_utils_dbg_gpio_init(void);
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_utils_setup()
{
  _f7syslogTextBuf = malloc(HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN);
  sem_init(&_f7syslogSem, 0, 1);
  return OK;
}

//============================================================================
void hcom_utils_shutdown()
{
  free(_f7syslogTextBuf);
  sem_destroy(&_f7syslogSem);
}

//===================================================================
// Reads any of the 32 battery backed registers
uint32_t hcom_utils_bbreg_read(uint32_t regNumber)
{
  return *((uint32_t *) regNumber);
}

//===================================================================
// Writes any of the 32 battery backed registers
void hcom_utils_bbreg_write(uint32_t regNumber, uint32_t value)
{
  *((uint32_t *) regNumber) = value;
}

//===================================================================
// Reads bit(s) in any of the 32 battery backed registers
bool hcom_utils_bbreg_is_bit_set_clear(uint32_t regNumber, uint32_t value)
{
  uint32_t reg = *((uint32_t *) regNumber);
  *((uint32_t *) regNumber) = reg & (~value);
  return (value & reg) != 0;
}

//===================================================================
// Reads bit(s) in any of the 32 battery backed registers
bool hcom_utils_bbreg_is_bit_set(uint32_t regNumber, uint32_t value)
{
  uint32_t reg = *((uint32_t *) regNumber);
  return (value & reg) != 0;
}

//===================================================================
void hcom_utils_bbreg_clear_bit(uint32_t regNumber, uint32_t value)
{
  uint32_t reg = *((uint32_t *) regNumber);
  *((uint32_t *) regNumber) = reg & (~value);
}

//===================================================================
// Set bit(s) in any of the 32 battery backed registers
void hcom_utils_bbreg_set_bit(uint32_t regNumber, uint32_t value)
{
  uint32_t reg = *((uint32_t *) regNumber);
  *((uint32_t *) regNumber) = reg | value;
}

//===================================================================
// This is called during startup, before the hcom thread is created,
// to check if we are running under the QEMU virtualization model.
// 

#define QEMU_BOOT_INFO_MAGIC 0x12341234
#define QEMU_BOOT_INFO_OFFSET_FROM_SDRAM_END 1024
#define QEMU_BOOT_INFO_ADDRESS \
  (CONFIG_HEAP2_BASE + CONFIG_HEAP2_SIZE - QEMU_BOOT_INFO_OFFSET_FROM_SDRAM_END)

bool hcom_utils_boot_time_qemu_check()
{
    // As part of the booting process, QEMU writes a token value
    // to the first page of SDRAM. This logic is implemented at
    // qemu/hw/arm/meadow.c:meadow_machine_reset.

    uint32_t *addr = (uint32_t *)QEMU_BOOT_INFO_ADDRESS; 
    return *addr == QEMU_BOOT_INFO_MAGIC;
}

//===================================================================
// This is called during startup, before the hcom thread is created.
// Its purpose is to allow inform mono (via mono_main) whether it is
// to run or not run.
void hcom_utils_boot_time_mono_check()
{
#ifdef CONFIG_USER_ENTRYPOINT
  // Do we need to prepare mono for special behavior?
  if(hcom_utils_bbreg_read(HCOM_BATTERY_BACKED_REG_MONO_ACCESS) == HCOM_MONO_MAIN_ACCESS_KEY)
  {
    char *argv[1];
    char buffer[16];

    // Send the action to mono_main in argv
    itoa(hcom_utils_bbreg_read(HCOM_BATTERY_BACKED_REG_MONO_ACTION), buffer, 10);
    argv[0] = buffer;
    uint32_t argc = HCOM_MONO_MAIN_ACCESS_KEY;
    
    // Call mono_main
    (*USERSPACE->us_entrypoint)((int)argc, argv);
  }
#endif
}

//===================================================================
//
bool hcom_utils_is_mono_disabled()
{
#ifdef CONFIG_USER_ENTRYPOINT
  if(hcom_utils_bbreg_read(HCOM_BATTERY_BACKED_REG_MONO_ACCESS) == HCOM_MONO_MAIN_ACCESS_KEY)
  {
    if(hcom_utils_bbreg_read(HCOM_BATTERY_BACKED_REG_MONO_ACTION) != 0)
      return true;
  }
#endif
  return false;
}

//============================================================================
// For diagnostic use only
void hcom_utils_diag_print_buffer(const uint8_t buffer[], const int bufLen, uint8_t msgPriority)
{
#if HCOM_COMMON_UTILS_DIAG_PRINT_BUFFER > 0

#define HCOM_UTIL_BYTES_PER_LINE 16
#define HCOM_UTIL_LEADING_SPACES 2
#define HCOM_UTIL_HEXADECIMAL_OFFSET (8 + HCOM_UTIL_LEADING_SPACES)
#define HCOM_UTIL_ASCII_OFFSET (57 + HCOM_UTIL_LEADING_SPACES)
#define HCOM_UTIL_DISPLAY_LENGTH (HCOM_UTIL_ASCII_OFFSET + HCOM_UTIL_BYTES_PER_LINE + 3)

  if ((g_syslog_mask & LOG_MASK(msgPriority)) == 0)
    return;

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
  hcom_utils_f7syslog(msgPriority, lineBuff);
  }

#endif
}

//===================================================================
// This is part of memory saving effort to remove from all the syslog
// messages the need to identify the type of log. It's done here and
// not in 100+ places.
static int hcom_utils_syslog_priority_to_text(int priority, char *textPri)
{
  switch (priority)
  {
  case LOG_EMERG:
    strcpy(textPri, "(Emerg) ");
    break;
  case LOG_ALERT:
    strcpy(textPri, "(Alert) ");
    break;
  case LOG_CRIT:
    strcpy(textPri, "(Crit) ");
    break;
 case LOG_ERR:
    strcpy(textPri, "(Error) ");
    break;
  case LOG_WARNING:
    strcpy(textPri, "(Warn) ");
    break;
  case LOG_NOTICE:
    strcpy(textPri, "(Note) ");
    break;
  case LOG_INFO:
    strcpy(textPri, "(Info) ");
    break;
   case LOG_DEBUG:
    strcpy(textPri, "(Debug) ");
    break;  
  default:
    strcpy(textPri, "(Pri ?) ");
    break;
  }

  return strlen(textPri);
}

//===================================================================
// Builds the string for syslogs
static int hcom_util_build_syslog_string(int priority, FAR const IPTR char * fmtStr, va_list argsList,
        char* finalString, int maxStringLen)
{
  // Adding the prefix here saves memory by removing
  // the text at the start of each message
  char labelPrefix[16];
  int prefixLen = hcom_utils_syslog_priority_to_text(priority, labelPrefix);
  int fmtLength = strlen(fmtStr);

  char *finalFmt = malloc(prefixLen + fmtLength + 1); // room for '\0'
  DEBUGASSERT(finalFmt != NULL);

  memcpy(finalFmt, labelPrefix, prefixLen);
  memcpy(finalFmt + prefixLen, fmtStr, fmtLength + 1); // include fmt's '\0'

  // Create the complete message with prefix
  int stringLen = vsnprintf(finalString, maxStringLen - 1, finalFmt, argsList);

  // The snprintf return is considered to be written completely if and only if the returned value
  // is non-negative and less than buf_size.
  DEBUGASSERT(stringLen < HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN);

  free(finalFmt);
  return stringLen;
}

//=====================================================================
// Wait for the thread writing to exit
static void hcom_utils_f7syslog_takesem(void)
{
  int ret;

  do
    {
      /* Take the semaphore (perhaps waiting) */
      ret = sem_wait(&_f7syslogSem);

      /* The only case that an error should occur here is if the wait was
       * awakened by a signal.
       */
      DEBUGASSERT(ret == OK || ret == -EINTR);
    }
  while (ret == -EINTR);
}

//===================================================================
// This function is for those places where hcom is processing text to
// send to the host PC. If hcom_utils_f7syslog had been used these calls
// will introduce recursion as each syslog will create another syslog.
void hcom_utils_f7syslog_x(int priority, FAR const IPTR char *fmt, ...)
{
#if defined CONFIG_RAMLOG_SYSLOG
  // Can't send to syslog because this will be a recursive. Can't send
  // directly to the host because this also would be recursive too.
  return;
#else
  if ((g_syslog_mask & LOG_MASK(priority)) == 0)
  {
    return;   // Nothing to do
  }

  // If nuttx is configured for outputting syslogs to the console
  // then this works. But, these cannot be routed to the host PC.
  va_list args;
  va_start(args, fmt);
  vsyslog(priority, fmt, args);
  va_end(args);
#endif
}

//===================================================================
// Use this for syslogs writes that can be routed to host.
void hcom_utils_f7syslog(int priority, FAR const IPTR char *fmt, ...)
{
  // Prevent multiple threads from garbling message
  hcom_utils_f7syslog_takesem();

  if ((g_syslog_mask & LOG_MASK(priority)) == 0)
  {
    sem_post(&_f7syslogSem);
    return;   // Nothing to do
  }

  va_list args;
  va_start(args, fmt);
  int stringLen = hcom_util_build_syslog_string(priority, fmt, args, _f7syslogTextBuf,
            HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN);
  va_end(args);  

  // Depending on the nuttx configuration these messages may go to the
  // ramlog, to the serial console or the bit-bucket.
  //
  // If ramlog is configured then the message is sent to syslog only. Nuttx
  // adds a syslog timestamp (if so configured) and puts it in the nuttx
  // ramlog buffer. The hcom_host_trace_ramlog read function reads it
  // from ramlog and forwards the message to the host PC, if requested.
  //
  // If syslog is configured then the call to syslog writes the message to
  // the configured UART for serial output. On return from the syslog call
  // this code (immediately below) forwards it to the host PC, if trace is
  // enabled
  //
  syslog(priority, _f7syslogTextBuf);

#if defined CONFIG_RAMLOG_SYSLOG
  stringLen = stringLen;    // Keep conpiler from warning (I know there's a better way...)
#else
  // syslog - check if these should be routed to host.
  // Note: no timestamp for these messages
  if(hcom_utils_bbreg_is_bit_set(HCOM_BATTERY_BACKED_REG_BIT_FLAGS,
          HCOM_BBREG_TRACE_MSG_TO_HOST_BIT_FLAG))
  {
    // Strip off cr/lf since Meadow.CLI takes care of this
    if(_f7syslogTextBuf[stringLen - 1] == 0x0a || _f7syslogTextBuf[stringLen - 1] == 0x0d)
      stringLen--;
    if(_f7syslogTextBuf[stringLen - 1] == 0x0a || _f7syslogTextBuf[stringLen - 1] == 0x0d)
      stringLen--;
  
    hcom_comms_send_raw_string_msg(HCOM_HOST_REQUEST_TEXT_TRACE_MSG, 0, _f7syslogTextBuf,
            stringLen, thisFile, __LINE__);
  }
#endif
  sem_post(&_f7syslogSem);
}

#if defined (CONFIG_RAMLOG_SYSLOG)
//===================================================================
// hcom_utils_safe_ramlog solves the problem that with ramlog 
// enabled, we can't use syslog while processing the syslog message,
// because it will cause "feedback", every syslog message would
// generated another ramlog message. So, we go directly to the host
// without going to syslog -> ramlog -> host PC.
//===================================================================
void hcom_utils_safe_ramlog(int priority, FAR const IPTR char *fmt,
          va_list args)
{
  // Check priority but otherwise ignore syslog.
  if ((g_syslog_mask & LOG_MASK(priority)) == 0)
    return;   // Nothing to do
  
  char *_safeRamlogText;
  _safeRamlogText = malloc(HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN);
  DEBUGASSERT(_safeRamlogText != NULL);

  int stringLen = hcom_util_build_syslog_string(priority, fmt, args, _safeRamlogText,
            HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN);

  // Note: no time stamp to these messages
  if(_safeRamlogText[stringLen - 1] == 0x0a || _safeRamlogText[stringLen - 1] == 0x0d)
    stringLen--;
  if(_safeRamlogText[stringLen - 1] == 0x0a || _safeRamlogText[stringLen - 1] == 0x0d)
    stringLen--;

  hcom_comms_send_raw_string_msg(HCOM_HOST_REQUEST_TEXT_TRACE_MSG, 0, _safeRamlogText,
          stringLen, thisFile, __LINE__);
  free(_safeRamlogText);
}
#endif

#if HCOM_COMMON_UTILS_GPIO_TEST_PROBE > 0
//=================================================================
// This for testing only
#define HCOM_MEADOW_GPIO_D00_OUTPUT  (GPIO_OUTPUT | GPIO_PORTI | GPIO_PIN9 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define HCOM_MEADOW_GPIO_D01_OUTPUT  (GPIO_OUTPUT | GPIO_PORTH | GPIO_PIN13| GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define HCOM_MEADOW_GPIO_D02_OUTPUT  (GPIO_OUTPUT | GPIO_PORTC | GPIO_PIN6 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define HCOM_MEADOW_GPIO_D03_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN8 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define HCOM_MEADOW_GPIO_D04_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN9 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define HCOM_MEADOW_GPIO_D05_OUTPUT  (GPIO_OUTPUT | GPIO_PORTC | GPIO_PIN7 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define HCOM_MEADOW_GPIO_D06_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN0 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define HCOM_MEADOW_GPIO_D07_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN7 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)
#define HCOM_MEADOW_GPIO_D08_OUTPUT  (GPIO_OUTPUT | GPIO_PORTB | GPIO_PIN6 | GPIO_PULLUP | GPIO_OPENDRAIN | GPIO_SPEED_100MHz)

//=================================================================
void hcom_utils_dbg_gpio_init()
{
  static bool is_dbg_gpio_initialized = false;

  if(is_dbg_gpio_initialized)
    return;
  
  stm32_configgpio(HCOM_MEADOW_GPIO_D00_OUTPUT);
  stm32_configgpio(HCOM_MEADOW_GPIO_D01_OUTPUT);
  stm32_configgpio(HCOM_MEADOW_GPIO_D02_OUTPUT);
  stm32_configgpio(HCOM_MEADOW_GPIO_D03_OUTPUT);
  stm32_configgpio(HCOM_MEADOW_GPIO_D04_OUTPUT);
  stm32_configgpio(HCOM_MEADOW_GPIO_D05_OUTPUT);
  stm32_configgpio(HCOM_MEADOW_GPIO_D06_OUTPUT);
  stm32_configgpio(HCOM_MEADOW_GPIO_D07_OUTPUT);
  stm32_configgpio(HCOM_MEADOW_GPIO_D08_OUTPUT);
  
  usleep(100 * 1000);   // allow chip to recover
  is_dbg_gpio_initialized = true;
}

//=================================================================
// Assumes gpio (D8 on meadow) can output as binary
void hcom_utils_dbg_gpio_1led_update(bool ledOn)
{
  // Note: Low (false) turns led on for an open drain
  stm32_gpiowrite(HCOM_MEADOW_GPIO_D08_OUTPUT, !ledOn);
}

//=================================================================
// Assumes gpio (D0-D7 on meadow) can output 8-bit as binary
void hcom_utils_dbg_gpio_8bit_update(uint8_t newValue, bool ledOn)
{
  hcom_utils_dbg_gpio_init();
  
  // Note: Low (false) turns led on for an open drain
  stm32_gpiowrite(HCOM_MEADOW_GPIO_D00_OUTPUT, (newValue & 0x01) > 0 ?  false : true);
  stm32_gpiowrite(HCOM_MEADOW_GPIO_D01_OUTPUT, (newValue & 0x02) > 0 ?  false : true);
  stm32_gpiowrite(HCOM_MEADOW_GPIO_D02_OUTPUT, (newValue & 0x04) > 0 ?  false : true);
  stm32_gpiowrite(HCOM_MEADOW_GPIO_D03_OUTPUT, (newValue & 0x08) > 0 ?  false : true);
  stm32_gpiowrite(HCOM_MEADOW_GPIO_D04_OUTPUT, (newValue & 0x10) > 0 ?  false : true);
  stm32_gpiowrite(HCOM_MEADOW_GPIO_D05_OUTPUT, (newValue & 0x20) > 0 ?  false : true);
  stm32_gpiowrite(HCOM_MEADOW_GPIO_D06_OUTPUT, (newValue & 0x40) > 0 ?  false : true);
  stm32_gpiowrite(HCOM_MEADOW_GPIO_D07_OUTPUT, (newValue & 0x80) > 0 ?  false : true);

  hcom_utils_dbg_gpio_1led_update(ledOn);
  usleep(50 * 1000);  // Give time to see the results

  // if(ledOn)
  // {
  //   for(int i = 0; i < 5; i++)
  //   {
  //     // flash
  //     hcom_utils_dbg_gpio_1led_update(i % 2 == 0 ? true : false);
  //     usleep(100 * 1000);
  //   }
  //   hcom_utils_dbg_gpio_1led_update(false);
  // }
  // else
  // {
  //   hcom_utils_dbg_gpio_1led_update(false);
  //   usleep(50 * 1000);
  // }
}
#endif
