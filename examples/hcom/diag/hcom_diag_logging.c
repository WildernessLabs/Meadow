/****************************************************************************
 * \apps\examples\hcom\misc\hcom_logging_utils.c
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

// This functions are used to process the logging request. There was a time
// we memory was unavailable for all but the briefest syslog messages. Much
// of what that is in here is the result of the memory reduction effort.

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <ctype.h>
#include "hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_bbreg_defn.h>
#include <meadow/hcom_nuttx_shared.h>

#include <nuttx/config.h>
#include "syslog.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static int _syslogMask;
static sem_t _f7syslogSem;      /* Implements event waiting */
static char *_f7syslogTextBuf;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_diag_logging_setup()
{
  _f7syslogTextBuf = malloc(HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN);
  if(_f7syslogTextBuf == NULL)
    return -1;
    
  sem_init(&_f7syslogSem, 0, 1);
  return OK;
}
//============================================================
void hcom_diag_logging_shutdown()
{
  sem_destroy(&_f7syslogSem);
  free(_f7syslogTextBuf);
}

//============================================================
// Make syslogMask available
int hcom_diag_logging_get_syslog_mask()
{
  return _syslogMask;
}

//============================================================
// This function is called by startup_manager and gets the
// syslog mask set by hcom_nx
int hcom_logging_syslog_mask_init()
{
  bool isPowerOnRestart;

#if defined(CONFIG_STM32F7_PWR)
  // This BBR was set by hcom nx since it starts first
  _syslogMask = hcom_bbreg_read_bbr_and_right_justify(HCOM_BBREG_RESTART_SYSLOG_CONFIG_VALUE_MASK);
  
  // Check ini config file for trace levels that may be added
  int iniValue = hcom_via_nx_ini_cfg_get_int_default(NULL, MEADOW_INI_CFG_STARTUP_SECTION,
              MEADOW_INI_CFG_DIAG_TRACE_LEVEL_KEY, 0);
  switch(iniValue)
  {
    case 0:
      break;
    case 1:
      _syslogMask |= LOG_MASK(LOG_NOTICE);
      break;
    case 2:
      _syslogMask |= LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_INFO);
      break;
    case 3:
      _syslogMask |= LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_INFO) | LOG_MASK(LOG_DEBUG);
      break;
    default:
      hcom_logging_syslog(LOG_WARNING, "%s@%d-Unsupported config trace level of %d ignored\n",
                  thisFile, __LINE__, iniValue);
      break;
  }

  setlogmask(_syslogMask);

  // Check if this is a reboot or a power-on restart. The MCU on Power-on
  // clears all 32 battery backed registers to 0.
  if(hcom_bbreg_read_bbr() == 0)
  {
    // Power-on restart
    isPowerOnRestart = true;
  }
  else
  {
    // Rebooted - it's safe to use the battery backed registers values
    isPowerOnRestart = false;
  }
#else
#warning "CONFIG_STM32F7_PWR not defined\n"
  // Without battery backed registers the best we can do is guess
  isPowerOnRestart = true;
#endif

#if defined(CONFIG_STM32F7_PWR)
  // Check the host and uart1 bits
  char *traceCombo[] = {
    "none",
    "Host",
    "UART1",
    "Host+UART1"};
  uint32_t destValue = hcom_bbreg_read_bbr_and_right_justify(HCOM_BBREG_TRACE_MSG_TO_HOST_AND_UART1_BIT_MASK);
  char *traceDest = traceCombo[destValue];
#else
  char *traceDest = "unknown";
#endif

  // Provide some information that may be useful
  hcom_logging_syslog(LOG_NOTICE, "Meadow %s (%s@%s) %s, Mono:%s, Trace level:0x%02x, to:%s, type:%s\n",
        HCOM_DEVICE_INFO_MEADOW_OS_VERSION, __DATE__, __TIME__, 
        isPowerOnRestart ? "power-on restart" :"rebooted",
        hcom_mono_ctrl_is_mono_enabled() ? "Enabled" : "Disabled",
        _syslogMask, traceDest,
#if defined CONFIG_RAMLOG_SYSLOG
        "ramlog");
#else
        "syslog");
#endif

  return OK;
}

//=======================================================================================
void hcom_diag_logging_change_trace_level(uint32_t userData)
{
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;
  char *traceNew;
  char *traceOld;
  
  // Minimum default
  int newSyslogMask = LOG_MASK(LOG_EMERG) | LOG_MASK(LOG_ALERT) | LOG_MASK(LOG_CRIT) |
                   LOG_MASK(LOG_ERR) | LOG_MASK(LOG_WARNING);

  switch (userData)
  {
    case HCOM_TRACE_LEVEL_NOTICE:
      newSyslogMask |= LOG_MASK(LOG_NOTICE);
      traceNew = "Notice";
      break;

    case HCOM_TRACE_LEVEL_NOTICE_INFO:
      newSyslogMask |= LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_INFO);
      traceNew = "Notice and Information";
      break;

    case HCOM_TRACE_LEVEL_NOTICE_INFO_DEBUG:
      newSyslogMask |= LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_INFO) | LOG_MASK(LOG_DEBUG);
      traceNew = "Notice, Information and Debug";
      break;
    
    case HCOM_TRACE_LEVEL_DEFAULT:
    traceNew = "Normal";
    default:    // minumum newSyslogMask
      break;
  }

#if HCOM_FORCE_SYSLOG_MASK_F7_AND_UART1 > 0
  newSyslogMask |= LOG_MASK(LOG_NOTICE) | LOG_MASK(LOG_INFO);
#endif

  hcom_bbreg_clear_then_set_bbr_bits(HCOM_BBREG_RESTART_SYSLOG_CONFIG_VALUE_MASK, newSyslogMask);
  int oldSyslogMask = setlogmask(newSyslogMask);
  _syslogMask = newSyslogMask;

  switch (oldSyslogMask)
  {
    case HCOM_TRACE_MASK_NOTICE:
      traceOld = "Notice";
      break;

    case HCOM_TRACE_MASK_NOTICE_INFO:
      traceOld = "Notice and Information";
      break;

    case HCOM_TRACE_MASK_NOTICE_INFO_DEBUG:
      traceOld = "Notice, Information and Debug";
      break;
    
    case HCOM_TRACE_MASK_DEFAULT:
    default:    // minumum newSyslogMask
      traceOld = "Normal";
      break;
  }

  if(oldSyslogMask != newSyslogMask)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Trace level changed from '%s' (0x%02x) to '%s' (0x%02x)",
             traceOld, oldSyslogMask, traceNew, newSyslogMask);
  }
  else
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Trace level remained at '%s' (0x%02x)",
            traceNew, newSyslogMask);
  }

  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg,
          thisFile, __LINE__);

  hcom_logging_syslog(LOG_NOTICE, "%s@%d-%s\n\n",
            thisFile, __LINE__, hostMsg);
}

//===================================================================
// This is part of memory saving effort to remove from all the syslog
// messages the need to identify the type of log. It's done here and
// not in 100+ places.
static int hcom_diag_logging_log_priority_to_text(int priority, char *textPri)
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
static int hcom_diag_logging_build_syslog_string(int priority, FAR const IPTR char * fmtStr, va_list argsList,
        char* finalString, int maxStringLen)
{
  // Adding the prefix here saves memory by removing
  // the text at the start of each message
  char labelPrefix[16];
  int prefixLen = hcom_diag_logging_log_priority_to_text(priority, labelPrefix);
  int fmtLength = strlen(fmtStr);

  char *finalFmt = malloc(prefixLen + fmtLength + 1); // room for '\0'
  DEBUGASSERT(finalFmt != NULL);

  memcpy(finalFmt, labelPrefix, prefixLen);
  memcpy(finalFmt + prefixLen, fmtStr, fmtLength + 1); // include fmt's '\0'

  // Create the complete message with prefix
  int stringLen = vsnprintf(finalString, maxStringLen - 1, finalFmt, argsList);

  // The snprintf return is considered to be written completely if and only if the returned value
  // is non-negative and less than buf_size. Otherwise, the string may be truncated.
  DEBUGASSERT(stringLen < HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN);

  free(finalFmt);
  return stringLen;
}

//=====================================================================
// Wait for the thread writing to exit
static void hcom_diag_logging_takesem(sem_t *semaphore)
{
  int ret;
  DEBUGASSERT(semaphore != NULL);

  do
    {
      /* Take the semaphore (perhaps waiting) */
      ret = sem_wait(semaphore);

      /* The only case that an error should occur here is if the wait was
       * awakened by a signal.
       */
      DEBUGASSERT(ret == OK || ret == -EINTR);
    }
  while (ret == -EINTR);
}

//===================================================================
// This function is for those places where hcom is processing text to
// send to the host PC. If hcom_logging_syslog had been used these calls
// will introduce recursion as each syslog will create another syslog.
void hcom_logging_syslog_x(int priority, FAR const IPTR char *fmt, ...)
{
#if defined CONFIG_RAMLOG_SYSLOG
  // Can't send to syslog because this will be a recursive. Can't send
  // directly to the host because this also would be recursive too.
  return;
#else
  if ((_syslogMask & LOG_MASK(priority)) == 0)
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
// Use this for syslogs writes that can be routed to host, which are
// 99% of all syslog calls
void hcom_logging_syslog(int priority, FAR const IPTR char *fmt, ...)
{
  // Prevent multiple threads from garbling message
  hcom_diag_logging_takesem(&_f7syslogSem);

  if ((_syslogMask & LOG_MASK(priority)) == 0)
  {
    sem_post(&_f7syslogSem);
    return;   // Nothing to do
  }

  va_list args;
  va_start(args, fmt);
  int stringLen = hcom_diag_logging_build_syslog_string(priority, fmt, args, _f7syslogTextBuf,
            HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN);
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
  // Note: there's no timestamp for these messages
  if(hcom_bbreg_is_bbr_bit_set(HCOM_BBREG_ROUTE_TRACE_MSG_TO_HOST_BIT))
  {
    // Strip off cr/lf since Meadow.CLI takes care of this
    if(_f7syslogTextBuf[stringLen - 1] == 0x0a || _f7syslogTextBuf[stringLen - 1] == 0x0d)
      stringLen--;
    if(_f7syslogTextBuf[stringLen - 1] == 0x0a || _f7syslogTextBuf[stringLen - 1] == 0x0d)
      stringLen--;
  
    hcom_host_send_raw_string_msg(HCOM_HOST_REQUEST_TEXT_TRACE_MSG, 0, _f7syslogTextBuf,
            stringLen, thisFile, __LINE__);
  }
#endif
  sem_post(&_f7syslogSem);
}

#if defined (CONFIG_RAMLOG_SYSLOG)
//===================================================================
// hcom_logging_safe_ramlog solves the problem that with ramlog 
// enabled, we can't use syslog while processing the syslog message,
// because it will cause "feedback", every syslog message would
// generated another ramlog message. So, we go directly to the host
// without going to syslog -> ramlog -> host PC.
//===================================================================
void hcom_logging_safe_ramlog(int priority, FAR const IPTR char *fmt,
          va_list args)
{
  // Check priority but otherwise ignore syslog.
  if ((_syslogMask & LOG_MASK(priority)) == 0)
    return;   // Nothing to do
  
  char *_safeRamlogText;
  _safeRamlogText = malloc(HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN);
  DEBUGASSERT(_safeRamlogText != NULL);

  int stringLen = hcom_diag_logging_build_syslog_string(priority, fmt, args, _safeRamlogText,
            HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN);

  // Note: no time stamp to these messages
  if(_safeRamlogText[stringLen - 1] == 0x0a || _safeRamlogText[stringLen - 1] == 0x0d)
    stringLen--;
  if(_safeRamlogText[stringLen - 1] == 0x0a || _safeRamlogText[stringLen - 1] == 0x0d)
    stringLen--;

  hcom_host_send_raw_string_msg(HCOM_HOST_REQUEST_TEXT_TRACE_MSG, 0, _safeRamlogText,
          stringLen, thisFile, __LINE__);
  free(_safeRamlogText);
}
#endif
