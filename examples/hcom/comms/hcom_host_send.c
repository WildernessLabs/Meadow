/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_send.c
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

// Both high level functions that prepare a message to be successfully sent
// and low level functions that do the transmission to the host PC or Mac
// are in this module.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;

static int _comms_write_fd;
static uint8_t *_encodedXmitBuff;
static sem_t _hostXmitSem;    /* Implements event waiting */
static bool _lastXmitBlocked;
static bool _notInitialized = true;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_host_send_build_msg_header(uint16_t requestType, uint16_t extraData,
        uint32_t userData, uint8_t *xmitBuffer);
static int hcom_host_send_buffered_msg(uint16_t requestType, uint16_t extraData,
        uint32_t userData, uint8_t *msgBuffer, size_t msgLen);

static int hcom_host_send_transmit_to_host(FAR uint8_t xmitBuffer[], size_t xmitLength);
static bool hcom_host_send_is_host_xmit_blocked(void);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_host_send_setup()
{
  _comms_write_fd = -1;
  _lastXmitBlocked = true; // Assume blocked
  _encodedXmitBuff = malloc(HCOM_SAFE_PACKET_BUF_SIZE);

  sem_init(&_hostXmitSem, 0, 1);
  
  _notInitialized = false;  
  return OK;
}

//--------------------------------------------------------------------
void hcom_host_send_shutdown()
{
  _shutting_down = true;

  close(_comms_write_fd);
  _comms_write_fd = -1;
  _lastXmitBlocked = true;

  free(_encodedXmitBuff);

  // use sem_destroy
  sem_destroy(&_hostXmitSem);
}

//=====================================================================
// Wait for the thread writing to exit
static void hcom_host_send_transmit_takesem(void)
{
  int ret;

  do
    {
      /* Take the semaphore (perhaps waiting) */
      ret = sem_wait(&_hostXmitSem);

      /* The only case that an error should occur here is if the wait was
       * awakened by a signal.
       */
      DEBUGASSERT(ret == OK || ret == -EINTR);
    }
  while (ret == -EINTR);
}

//=====================================================================
// THIS IS THE FUNCTION THAT SHOULD BE USED WHEN JUST SENDING A HEADER
// Just sends a header message and report the error here
void hcom_host_send_header_msg(uint16_t requestType, uint32_t userData,
      char *sourceFileName, int sourceLineNumber)
{
  int ret = hcom_host_send_buffered_msg(requestType, 0, userData, NULL, 0);
  if (ret < 0 && ret != -EAGAIN) // EAGAIN is not an error it means the message was blocked
    hcom_logging_syslog_x(LOG_ERR, "%s@%d-Host xmit err:%d\n", sourceFileName, sourceLineNumber, ret);
}

//=====================================================================
// THIS IS THE FUNCTION THAT SHOULD BE USED FOR ALL SIMPLE TEXT MESSAGE
// Prepare a simple line of text for transmission and output the error message here
void hcom_host_send_simple_string_msg(uint16_t requestType, uint32_t userData,
           char *shortText, char *sourceFileName, int sourceLineNumber)
{
  // Need to remove any trailing cr/lf. If none found strcspn() finds terminating '\0'
  // returning its offset.
  size_t trueStrLen = strcspn(shortText, "\r\n");

  int ret = hcom_host_send_buffered_msg(requestType, 0, userData, (uint8_t*) shortText, trueStrLen);
  if (ret < 0 && ret != -EAGAIN) // EAGAIN is not an error it means the message was blocked
    hcom_logging_syslog_x(LOG_ERR, "%s@%d-Host xmit err:%d\n", sourceFileName, sourceLineNumber, ret);
}

//=====================================================================
// THIS IS THE FUNCTION THAT SHOULD BE USED WHEN SPECIAL CIRCUMSTANCES EXIST
// Prepare a string for transmission, allowing any character
// This is called for various internal needs (e.g. mono redirect, diagnostic).
int hcom_host_send_raw_string_msg(uint16_t requestType, uint32_t userData, char *shortText, size_t msgLength,
        char *sourceFileName, int sourceLineNumber)
{
  int ret = hcom_host_send_buffered_msg(requestType, 0, userData, (uint8_t*) shortText, msgLength);
  if (ret < 0 && ret != -EAGAIN) // EAGAIN is not an error it means the message was blocked
  {
      hcom_logging_syslog_x(LOG_ERR, "%s@%d-Host xmit err:%d\n", thisFile, __LINE__, ret);
  }

  return ret;
}

//=====================================================================
// This function is intended to be the sole and final entry point for
// messages that needed to be sent to Meadow.CLI. Use one of the above
// to access this function.
// Requirements: needs to efficiently handle both ramlog and syslog
// configurations in 3 situations: 1) actively communicating with the
// CLI, 2) connected to host PC and CLI is not communicating and 3) 
// Meadow is not connected to a host PC, this is the most usually
// situation.
int hcom_host_send_buffered_msg(uint16_t requestType, uint16_t extraData,
        uint32_t userData, uint8_t *origMsg, size_t msgLen)
{
  int ret;

  if(_notInitialized)
    return -EAGAIN;

  // Only one thread / message at a time can be sent to host
  hcom_host_send_transmit_takesem();

  // hcom_host_send_is_host_xmit_blocked() MUST be called before calling
  // hcom_host_send_transmit_to_host() to send a message to the host.
  // hcom_host_send_is_host_xmit_blocked() verifies that transmission is
  // possible. That is, the host PC can be connected to and that message
  // are being received (not blocked). If it returns true (blocked)
  // then transmission is not possible at this time.
  if(hcom_host_send_is_host_xmit_blocked())
  {
    // This is a normal occurance since the host is usually not connected
    sem_post(&_hostXmitSem);
    return OK;   // Throw the message away. What else can be done?
  }

  int fullMsgLen = msgLen + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH;
  if(fullMsgLen > HCOM_PROTOCOL_REQUEST_MAX_SIMPLE_DATA_LEN)
  {
    // Truncate to fit
    fullMsgLen = HCOM_PROTOCOL_PACKET_MAX_SIZE;
  }

  if(msgLen > 0)
  {
    // Unique buffer for each call so multithreading can work (each thread
    // has a different stack and xmitBuffer is on that stack)
    uint8_t *xmitBuffer = malloc(fullMsgLen);

    // Uses the first part of message buffer for header
    hcom_host_send_build_msg_header(requestType, extraData, userData, xmitBuffer);

    // Copy the body of the message
    memcpy(xmitBuffer + HCOM_PROTOCOL_REQUEST_HEADER_LENGTH, origMsg, fullMsgLen - HCOM_PROTOCOL_REQUEST_HEADER_LENGTH);
    
    // Send the message without a body, just the header
    ret = hcom_host_send_transmit_to_host(xmitBuffer, fullMsgLen);
    free(xmitBuffer);
  }
  else
  {
    DEBUGASSERT(msgLen == 0);
    uint8_t headerOnlyMsg[HCOM_PROTOCOL_REQUEST_HEADER_LENGTH];

    // Uses the first part of message buffer for header
    hcom_host_send_build_msg_header(requestType, extraData, userData, headerOnlyMsg);

    // Send the message
    ret = hcom_host_send_transmit_to_host(headerOnlyMsg, fullMsgLen);
  }

  sem_post(&_hostXmitSem);
  return ret;
}

//=====================================================================
// Build the header
void hcom_host_send_build_msg_header(uint16_t requestType,
        uint16_t extraData, uint32_t userData, uint8_t *xmitBuffer)
{
  // Populate the header
  struct HcomProtocolHeader_s *hdr = (struct HcomProtocolHeader_s *) xmitBuffer;

  hdr->seqNumber = HCOM_PROTOCOL_REQUEST_HEADER_SIMPLE_SEQ_NUMBER;
  hdr->version = HCOM_PROTOCOL_HCOM_VERSION_NUMBER;
  hdr->rqstType = requestType;
  hdr->extraData = extraData;
  hdr->userData = userData;
}

//==========================================================================
// Attempt to open the connection to the host PC
static int hcom_host_send_open_transmit_connection(void)
{
  if(_comms_write_fd > 1)
    return OK;

  int openAttempts;
  #define HCOM_COMMS_MAX_XMIT_OPEN_ATTEMPTS 3

  // This will attempt to open the USB/ACM Serial port on the Meadow end
  _comms_write_fd = -1;
  for(openAttempts = 0; openAttempts < HCOM_COMMS_MAX_XMIT_OPEN_ATTEMPTS; openAttempts++)
  {
    _comms_write_fd = open(hcom_host_recv_get_device_name(), O_WRONLY | O_NONBLOCK);
    if(_comms_write_fd >= 0)
    {
      // If there's no CLI or equal running open will still be successful,
      // as long as the Host PC opens the correct USB Serial port.
      return OK;
    }

    usleep(250 * 1000);
  }
  
  _lastXmitBlocked = true;
  return _comms_write_fd;
}

//=====================================================================
// Usually, no host PC is running and connected. When this is the case these
// messages eventually will be blocked (after filling nuttx internal buffers).
// To workaround this, once we get a -EAGAIN error (i.e. blocked) we'll attempt
// to send 0x00 before every future message. This way, when the host PC begins to
// consume messages our 0x00 will be the first thing to arrive after whatever nuttx
// has internally buffered (which could be a partial message). The CLI ignores a
// single 0x00 byte message. Therefore, the first message sent, after the host
// connects, will be sent successfully and be properly parsed.
// The partially sent (corrupted) messages will be thrown away by the Meadow.CLI
// (or at least should be) after it reports an error.
//
bool hcom_host_send_is_host_xmit_blocked()
{
  int ret;

  // Last attempt was NOT blocked. Caller should attempt to send.
  if(!_lastXmitBlocked)
  {
    return false;
  }

  // Is the connection opened?
  if(_comms_write_fd == -1)
  {
    ret = hcom_host_send_open_transmit_connection();
    if(ret < 0)
    {
      // This is where message are ignored if there's no host 
      // PC connected. Can't connect.
      return true;    // Report blocked
    }
  }

  // Send a '0' as the message that the host knows to ignore. '0' is
  // the cots protocol framing character, so it does no harm.
  uint8_t oneZero[1];
  oneZero[0] = '\0';
  ssize_t writeRet = write(_comms_write_fd, &oneZero, 1);
  if(writeRet == 1)
  {
    // Write successfull, no longer blocked
    _lastXmitBlocked = false;
    return false;   // Not blocked
  }
  
  // This is where we exit if the host PC exists but CLI (or equal)
  // is not running (i.e. not consuming chararacters).
  _lastXmitBlocked = true;
  return true;    // blocked or some error
}

//===================================================================================
// All messages sent to host pass through here.
// At this time 2 threads use this method
int hcom_host_send_transmit_to_host(FAR uint8_t xmitBuffer[], size_t xmitLength)
{
  #define HCOM_XMIT_MAX_BLOCKED_TIME_DELAY  (50 * 1000)
  #define HCOM_XMIT_MAX_BLOCKED_COUNT_VALUE 30 // .05 * 30 = 1.5 seconds

  size_t remainingBytes;
  size_t toWriteOffset = 0;
  size_t blockedCount = 0;

  if(_shutting_down)
  {
    return OK;
  }

  // Encode
  size_t encodedLength = hcom_host_cobs_encoder(xmitBuffer, 0, xmitLength, _encodedXmitBuff);

  // Encoded message needs a terminating delimiter for COBS
  DEBUGASSERT(encodedLength < HCOM_SAFE_PACKET_BUF_SIZE - 1);
  _encodedXmitBuff[encodedLength] = HCOM_PROTOCOL_PACKET_DELIMITER_VALUE;

  encodedLength++;
  remainingBytes = encodedLength;

  // Since there's no guarantee all bytes written at one time, loop until message 100% written
  while (remainingBytes > 0)
  {
    // Based on observation - If O_NONBLOCK is not specified in the file_open call, the file_write
    // call blocks after writing some number of bytes. It's as if some internal buffer fills causing
    // the file_write call to begin blocking. This cannot be allowed, since it would block the calling
    // thread, preventing it from doing any work.
    ssize_t writeRet = write(_comms_write_fd, &_encodedXmitBuff[toWriteOffset], remainingBytes);
    if(writeRet >= 0)
    {
      remainingBytes -= writeRet;   // Note: if remainingBytes == 0 will exit while loop
      toWriteOffset += writeRet;

      hcom_logging_syslog_x(LOG_DEBUG, "%s@%d-Send %d bytes, sent %d (%d remaining) will %s\n\n",
          thisFile, __LINE__, encodedLength, writeRet, remainingBytes == 0 ? "exit" : "retry");

      continue;
    }

    // Examine error
    // EINTR is not an error... it simply means that this write was interrupted
    // by a signal before it wrote the data.
    if (errno == EINTR)
    {
      continue;
    }

    // Was write attempt was blocked? 
    if(errno == EAGAIN)
    {
      // In this case either host PC was disconnected from Meadow, CLI stopped running
      // or Meadow.CLI just can't keep up. We'll give it a chance to catchup.
      if(blockedCount < HCOM_XMIT_MAX_BLOCKED_COUNT_VALUE)
      {
        blockedCount++;
        usleep(HCOM_XMIT_MAX_BLOCKED_TIME_DELAY);
        hcom_logging_syslog_x(LOG_DEBUG, "%s@%d-Resend #%d\n", thisFile, __LINE__, blockedCount);
        continue;
      }

      // Set the flag - seems the host isn't connected or CLI not running
      _lastXmitBlocked = true;
      hcom_logging_syslog_x(LOG_DEBUG, "%s@%d-%d USB write attempts (wrote %d of %d bytes), message not sent\n",
                thisFile, __LINE__, blockedCount, remainingBytes, encodedLength);

      // No reason to close fd. The caller can sort out what to do with partial data sent.
      return -errno;
    }

    // Some unexpected error
    close(_comms_write_fd);
    _comms_write_fd = -1;
    _lastXmitBlocked = true;

    return -errno;
  } // while (remainingBytes > 0)

  // Success exit
  _lastXmitBlocked = false;
  return OK;
}
