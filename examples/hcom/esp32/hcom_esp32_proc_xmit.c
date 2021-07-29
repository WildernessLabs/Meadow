/****************************************************************************
 * \apps\examples\hcom\esp32\hcom_esp32_proc_xmit.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include "hcom_esp32_comms.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static mqd_t xmitMsgQueue;
static struct mq_attr xmitMsgQAttr;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static ssize_t hcom_esp32_xmit_slip_encoder(uint8_t *unencodedMsg, ssize_t unencodedMsgLen,
      uint8_t *encodedMsg, ssize_t encodedOffset);
static int hcom_esp32_xmit_wait_for_response(struct HcomEsp32MqRecvdData_s *mqRecvdData,
          long milliSecDelay, uint8_t espCommand);
static int hcom_esp32_xmit_send_complete_msg(uint8_t *completeMsg, ssize_t completeMsgLen,
        uint8_t espCommand, long millisecDelay, struct HcomEsp32MqRecvdData_s *mqRecvdData);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

// We'll do a minimum of initialization because this is a rarely used feature
int hcom_esp32_xmit_setup_lazy()
{  
  xmitMsgQAttr.mq_maxmsg = HCOM_ESP_COMMS_MSG_QUEUE_MAX_MSGS;
  xmitMsgQAttr.mq_msgsize = HCOM_ESP32_MQ_RECVD_DATA_STRUCT_LENGTH;
  xmitMsgQAttr.mq_flags = 0;
  xmitMsgQAttr.mq_curmsgs = 0;

  xmitMsgQueue = mq_open(HCOM_ESP_COMMS_MSG_QUEUE_NAME, O_RDONLY | O_CREAT, 0666, &xmitMsgQAttr);
  if (xmitMsgQueue == (mqd_t)-1)
  {
    int errcode = get_errno();
    hcom_logging_syslog(LOG_ERR, "%s@%d-mq_open(%s) errno:%d\n", thisFile, __LINE__,
          HCOM_ESP_COMMS_MSG_QUEUE_NAME, errcode);
    return -errcode;
  }

  return OK;
}

//====================================================================
void hcom_esp32_xmit_shutdown()
{
  mq_close(xmitMsgQueue);
  mq_unlink(HCOM_ESP_COMMS_MSG_QUEUE_NAME);
}

//====================================================================
static uint8_t hcom_esp32_xmit_calculate_checksum(uint8_t *msgBuffer, int length)
{
  uint8_t check = 0xef;
  for(int i = 0; i < length; i++)
    check ^= msgBuffer[i];

  return check;
}

//====================================================================
// Returns the number of bytes read or -error
int hcom_esp32_xmit_build_and_send_msg(uint8_t *msgBody, ssize_t msgBodyLen,
        uint8_t espCommand, long millisecDelay, struct HcomEsp32UserRecvdData_s *recvdData)
{
  int ret;
  struct HcomEsp32MqRecvdData_s mqRecvdData[1];

  // Insure both are set or not set
  DEBUGASSERT((millisecDelay > 0 && recvdData != NULL) || (millisecDelay <= 0 && recvdData == NULL));

  // Guess at a safe allocation for encoding (150%)
  ssize_t bufferSize = (msgBodyLen + sizeof(struct HcomEsp32XmitHeader_s));
  bufferSize += bufferSize / 2; // assume no more than 150% expansion
  uint8_t *encodedMsg = malloc(bufferSize);
  ssize_t encodedOffset;

  // Build header
  struct HcomEsp32XmitHeader_s espSendHdr;
  espSendHdr.direction = 0;   // always 0 if going to ESP32
  espSendHdr.command = espCommand;
  espSendHdr.size = msgBodyLen;

  if(espCommand == Esp32CommandMemData ||
     espCommand == Esp32CommandFlashData ||
     espCommand == Esp32CommandFlashDeflData)
  {
    // Those commands that need a checksum have a 16 bytes "secondary-header" too.
    // The "secondary-header" must be ignored in the checksum calculation.
    int offset = HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH;
    espSendHdr.checksum = hcom_esp32_xmit_calculate_checksum(msgBody + offset, msgBodyLen - offset);
  }
  else
  {
    espSendHdr.checksum = 0;
  }

  // Encode this message
  encodedMsg[0] = 0xc0;
  encodedOffset = 1; // SLIP frame used 1 byte
  encodedOffset = hcom_esp32_xmit_slip_encoder((uint8_t*)&espSendHdr, 
              sizeof(struct HcomEsp32XmitHeader_s), encodedMsg, encodedOffset);  // Offset of start
  encodedOffset = hcom_esp32_xmit_slip_encoder(msgBody, msgBodyLen, encodedMsg, encodedOffset);
  encodedMsg[encodedOffset++] = 0xc0;

  // Can't continue because we've overrun the buffer we guessed at above
  DEBUGASSERT(encodedOffset < bufferSize);

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Transmitting '%s' (0x%02x) cmd to ESP32\n",
            thisFile, __LINE__, hcom_esp32_util_convert_esp32_cmd_to_string(espCommand), espCommand);
#endif

  // Send the completed message and wait for the response or the timeout
  ret = hcom_esp32_xmit_send_complete_msg(encodedMsg, encodedOffset,
        espCommand, millisecDelay, mqRecvdData);
  if(ret < 0)
  {
    // Timeout is assumed to mean nothing received from ESP32
    // not really an error
    if(ret != -ETIMEDOUT)   // ETIMEDOUT = 116
      hcom_logging_syslog(LOG_ERR, "%s@%d-Sending to ESP failed:%d\n", thisFile, __LINE__, ret);

    free(encodedMsg);
    return ret;
  }

  if(millisecDelay > 0)
  {
    // Only given received data if successfully received data and the data expected
    // Copy the data to the user supplied structure
    recvdData->espHdr.direction = mqRecvdData->espMqHdr.direction;
    recvdData->espHdr.command = mqRecvdData->espMqHdr.command;
    recvdData->espHdr.size = mqRecvdData->espMqHdr.size;
    recvdData->espHdr.value = mqRecvdData->espMqHdr.value;
    recvdData->esp32Status = mqRecvdData->esp32Status;
    recvdData->esp32Error = mqRecvdData->esp32Error;

    // Note: This is NOT the whole message, it excludes the header and
    // the last 4 bytes which are for error indication and error value.
    size_t sizeOfRealData = mqRecvdData->recvdMqDataLen - HCOM_ESP32_PROTOCOL_RECV_HDR_LENGTH - 4;
    recvdData->recvdDataLen = sizeOfRealData;
    if(sizeOfRealData > 0)
    {
      memcpy(recvdData->recvdData,
            mqRecvdData->recvdMqData + HCOM_ESP32_PROTOCOL_RECV_HDR_LENGTH,
            sizeOfRealData);
    }
  }

  free(mqRecvdData->recvdMqData);    // Free memory allocated by receiver
  free(encodedMsg);
  return ret;
}

//====================================================================
// Returns the number of bytes read or -error
int hcom_esp32_xmit_send_complete_msg(uint8_t *completeMsg, ssize_t completeMsgLen,
        uint8_t espCommand, long millisecDelay, struct HcomEsp32MqRecvdData_s *mqRecvdData)
{
  int ret;

  // Insure both are set or not set
  DEBUGASSERT((millisecDelay > 0 && mqRecvdData != NULL) || (millisecDelay <= 0 && mqRecvdData == NULL));

  // If no delay, assume not expecting a response
  if(millisecDelay > 0)
  {
    // Inform receiving code so it can know what command to listen for and queue.
    hcom_esp32_recv_expect_command_type(espCommand);
  }

  // Send the command to the esp32
  ret = hcom_esp32_uart_comms_write_serial(completeMsg, completeMsgLen);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-UART write:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  if(millisecDelay > 0)
  {
    // Read next received message via mq or return if no response after delay
    ret = hcom_esp32_xmit_wait_for_response(mqRecvdData, millisecDelay, espCommand);
  }

  return ret;
}

//====================================================================
// If sender cares about response this function will take care of processing it.
// Returns the number of bytes read or -error
// Note: The caller must free(mqRecvdData) memory
int hcom_esp32_xmit_wait_for_response(struct HcomEsp32MqRecvdData_s *mqRecvdData,
        long milliSecDelay, uint8_t espCommand)
{
  int ret;
  ssize_t bytes_read = 0;
  struct timespec timeoutTime;

  for(;;)
  {
    // Calculate where the clock should be when this would times out
    clock_gettime(CLOCK_REALTIME, &timeoutTime);
    long secDelayComponent = milliSecDelay/1000;
    timeoutTime.tv_sec += secDelayComponent;
    timeoutTime.tv_nsec += (milliSecDelay - (secDelayComponent * 1000)) * 1000 * 1000;

    if (timeoutTime.tv_nsec >= 1000 * 1000 * 1000)
    {
      timeoutTime.tv_sec++;
      timeoutTime.tv_nsec -= 1000 * 1000 * 1000;
    }

    bytes_read = mq_timedreceive(xmitMsgQueue, (char *)mqRecvdData, 
              HCOM_ESP32_MQ_RECVD_DATA_STRUCT_LENGTH, NULL, &timeoutTime);
    if(bytes_read >= 0)
    {
      // We have data from ESP32, but did ESP32 report an error?
      if(mqRecvdData->esp32Status != 0)
      {
        // The ROM loader sends the following error values
        // 0x05 - "Received message is invalid" (parameters or length field is invalid)
        // 0x06 - "Failed to act on received message"
        // 0x07 - "Invalid CRC in message"
        // 0x08 - "flash write error" - after writing a block of data to flash, the ROM loader reads the value back and the 8-bit CRC is compared to the data read from flash. If they don't match, this error is returned.
        // 0x09 - "flash read error" - SPI read failed
        // 0x0a - "flash read length error" - SPI read request length is too long
        // 0x0b - "Deflate error" (ESP32 compressed uploads only)

        hcom_logging_syslog(LOG_ERR, "%s@%d-Msg Cmd:0x%02x ESP32 err:0x%02x, status:%u\n",
            thisFile, __LINE__, espCommand, mqRecvdData->esp32Status, mqRecvdData->esp32Error);
        ret = -mqRecvdData->esp32Status; // For bootloader 05 - 0b
        return ret;
      }

      return bytes_read;
    }

    // ret < 0, some error - either try again or exit
    int errn = get_errno();
    if(errn == ETIMEDOUT)
    {
      ret = -ETIMEDOUT;
      break;
    }
    else if(errn == EINTR)
    {
      continue;   // interrupted by a signal, repeat read attempt
    }
    else
    {
      // ETIMEDOUT (116): The call timed out before a message could be transferred.      
      // EINTR (4): The call was interrupted by a signal handler.
      // EAGAIN (11): The queue was empty and the O_NONBLOCK flag was set for the message queue description referred to by mqdes.
      // EPERM (1): Message queue opened not opened for reading.
      // EMSGSIZE (122): msglen was less than the maxmsgsize attribute of the message queue.
      // EINVAL (22): Invalid msg or mqdes or abstime
      hcom_logging_syslog(LOG_ERR, "%s@%d-mq_timedreceive errno:%d\n", thisFile, __LINE__, errn);
      ret = -errn;
      break;
    }
  }   // for(;;)

  return ret;
}

//====================================================================
// Returns the new offset
ssize_t hcom_esp32_xmit_slip_encoder(uint8_t *unencodedMsg, ssize_t unencodedMsgLen,
      uint8_t *encodedMsg, ssize_t encodedOffset)
{
  // SLIP - Within the packet, all occurrences of 0xC0 are replaced with '0xDB 0xDC'
  //  and 0xDB replaced with with '0xDB 0xDD'
  int dest = encodedOffset;
  for(int source = 0; source < unencodedMsgLen; source++)
  {
    if(unencodedMsg[source] == 0xc0)
    {
      encodedMsg[dest++] = 0xdb;
      encodedMsg[dest++] = 0xdc;
    }
    else if(unencodedMsg[source] == 0xdb)
    {
      encodedMsg[dest++] = 0xdb;
      encodedMsg[dest++] = 0xdd;
    }
    else
    {
      encodedMsg[dest++] = unencodedMsg[source];
    }
  }
  return dest;
}
