/****************************************************************************
 * \apps\examples\hcom\esp32\hcom_esp32_proc_recv.c
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
#include <meadow/meadow_cirbuf.h>
#include "hcom_esp32_comms.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static mqd_t recvMsgQueue = 0;
static struct mq_attr recvMsgQAttr;
static bool _waitingForBinary;
static uint8_t _currentExpectRecvCommand;
static host_com_cir_buffer_t *_esp_cir_buf;

static int32_t _diagBufferedCount;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_esp32_recv_pull_and_process(void);
static int hcom_esp32_recv_handle_bin_packet(uint8_t *binRecvdData, ssize_t binRecvdLen);
static int hcom_esp32_recv_slip_decoder(uint8_t *encodedMsg, ssize_t encodedMsgLen,
      uint8_t *decoded);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_esp32_recv_setup_lazy()
{
  _shutting_down = false;
  _waitingForBinary = true;
  _currentExpectRecvCommand = Esp32CommandUndefined;  
  _diagBufferedCount = 0;

  _esp_cir_buf = (host_com_cir_buffer_t *)malloc(sizeof(host_com_cir_buffer_t));
  if (_esp_cir_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-circular buffer allocation failed\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Shared structure, size and message delimiter
  // The protocol used for ESP Comms is Serial Line Internet Protocol or slip
  int ret = hcom_cirbuf_init(_esp_cir_buf, HCOM_ESP_COMMS_RCV_ESP_BUFFER_SIZE,
            HCOM_ESP32_SLIP_FRAME_END_C0);
  if (ret == HCOM_CIR_BUF_ALLOC_FAILED)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-ESP32 buffer allocation failed:%d\n", thisFile, __LINE__, ret);
    return -1;
  }

  recvMsgQAttr.mq_maxmsg = HCOM_ESP_COMMS_MSG_QUEUE_MAX_MSGS;
  recvMsgQAttr.mq_msgsize = HCOM_ESP32_MQ_RECVD_DATA_STRUCT_LENGTH;
  recvMsgQAttr.mq_flags = 0;
  recvMsgQueue = mq_open(HCOM_ESP_COMMS_MSG_QUEUE_NAME, O_WRONLY | O_CREAT | O_NONBLOCK, 0666, &recvMsgQAttr);
  if (recvMsgQueue == (mqd_t)-1)
  {
    int errcode = get_errno();
    hcom_logging_syslog(LOG_ERR, "%s@%d-mq_open(%s) failed:%d\n",
              thisFile, __LINE__, HCOM_ESP_COMMS_MSG_QUEUE_NAME, errcode);
    return -errcode;
  }

  return OK;
}

//===================================================================
void hcom_esp32_recv_shutdown()
{
  _shutting_down = true;

  mq_close(recvMsgQueue);
  mq_unlink(HCOM_ESP_COMMS_MSG_QUEUE_NAME);
  hcom_cirbuf_release_memory(_esp_cir_buf);
  free(_esp_cir_buf);

  _shutting_down = false;
  _waitingForBinary = true;
  _currentExpectRecvCommand = Esp32CommandUndefined;  
  _diagBufferedCount = 0;
}

//===================================================================
// This prevents unneeded items from being put into the mq
void hcom_esp32_recv_expect_command_type(uint8_t expectCommand)
{
  // Since all responses contain the command number of the orginal
  // command, we can know which commands should be queued.
  _currentExpectRecvCommand = expectCommand;
}

//===================================================================
// Called when about to enter programming mode. This prevents text from
// being put into the cirbuf until binary data is received
void hcom_esp32_recv_starting_communications()
{
  _waitingForBinary = true;
}

//===================================================================
// When data received the esp thread calls here for processing
int hcom_esp32_recv_handle_data(uint8_t *esp32_read_buffer, ssize_t bytesToAdd)
{
  int ret;
  
  if(_shutting_down)
    return OK;

  // When first put into programming mode there's a flood of text. This needs
  // to be ignored until the slip data is seen. Slip encoded data from the
  // esp32 start and end with 0xc0. If this text is added, overfills the cirbuf.
  if(_waitingForBinary)
  {
    while(*esp32_read_buffer != HCOM_ESP32_SLIP_FRAME_END_C0)
    {
      esp32_read_buffer++;
      bytesToAdd--;
      if(bytesToAdd == 0)
      {
        return OK;
      }
    }
    _waitingForBinary = false;
  }

  while (true)
  {
    ret = hcom_cirbuf_add_bytes(_esp_cir_buf, esp32_read_buffer, bytesToAdd);    
    if(ret == HCOM_CIR_BUF_ADD_SUCCESS)
    {
      _diagBufferedCount += bytesToAdd;

      if(_diagBufferedCount > HCOM_ESP_COMMS_RCV_ESP_BUFFER_SIZE)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-_diagBufferedCount:%d exceed buffer:%d\n",
                  thisFile, __LINE__, _diagBufferedCount,
                  HCOM_ESP_COMMS_RCV_ESP_BUFFER_SIZE);
        return -EFBIG;
      }
      break;
    }

    if(ret == HCOM_CIR_BUF_ADD_WONT_FIT)
    {
      // Wasn't possible to put these bytes in the buffer. We need to
      // process a few packets and then retry to add this data
      ret = hcom_esp32_recv_pull_and_process();
      if (ret == HCOM_CIR_BUF_GET_FOUND_MSG ||
          ret == HCOM_CIR_BUF_GET_NONE_FOUND)
          continue;   // There should be room now for the failed add

      if (ret == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
      {
        // The buffer to receive the message is too small?
        hcom_logging_syslog(LOG_ERR, "%s@%d-Dest buffer too small, need:%d\n",
                thisFile, __LINE__, bytesToAdd);
        return -EFBIG;
      }
    }
    else if (ret == HCOM_CIR_BUF_ADD_BAD_ARG)
    {
        // Something wrong with implemenation
        hcom_logging_syslog(LOG_ERR, "%s@%d-Bad argument:%d\n",
                thisFile, __LINE__, ret);
        return -EINVAL;
    }
    else
    {
        // Undefined return value, should never happen
        hcom_logging_syslog(LOG_ERR, "%s@%d-Undefined ret:%d\n",
                thisFile, __LINE__, ret);
        return -EINVAL;
    }
  }   // while(true);

  ret = hcom_esp32_recv_pull_and_process();

  // Destination buffer too small
  if(ret == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Dest buffer too small, need:%d\n",
            thisFile, __LINE__, ret);
    return -EFBIG;
  }
  return OK;
}

//===================================================================
// Pull packets from the circular buffer
int hcom_esp32_recv_pull_and_process()
{
    int ret;
    size_t packetLength;
    uint8_t *packetBuffer = malloc(HCOM_ESP_COMMS_MAX_ESP_PACKET_SIZE);
    if(packetBuffer == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

    while (true)
    {
      // Get the next packet, we only get binary
      ret = hcom_cirbuf_get_next_packet(_esp_cir_buf, packetBuffer,
                    HCOM_ESP_COMMS_MAX_ESP_PACKET_SIZE, &packetLength);

      if (ret == HCOM_CIR_BUF_GET_NONE_FOUND)
          break;      // We've emptied buffer of all full packets

      if(ret == HCOM_CIR_BUF_GET_FOUND_MSG)
      {
        // Look for a single 0xc0 which indicates the start of the message
        // We can safely throw this away.
        if(packetLength == 1)
        {
          if(*packetBuffer != HCOM_ESP32_SLIP_FRAME_END_C0)
          {
            syslog(LOG_ERR, "%s@%d-Protocol error, expected:0x%02x, recvd:0x%02x\n",
                      __FILE__, __LINE__, HCOM_ESP32_SLIP_FRAME_END_C0,
                      *packetBuffer);
            return -EPROTO; 
          }
          continue;  
        }

        _diagBufferedCount -= packetLength;

        ret = hcom_esp32_recv_handle_bin_packet(packetBuffer, packetLength);
        if(ret < 0)
        {
          hcom_logging_syslog(LOG_ERR, "%s@%d-Bin message error:%d\n", thisFile, __LINE__, ret);
        }
      }
      else if (ret == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
      {
          // The buffer to accept the packets is too small! Need to enlarge
          hcom_logging_syslog(LOG_ERR, "%s@%dDest buffer too small, need:%d\n",
                  thisFile, __LINE__, packetLength);
          return -EFBIG;
      }
      else
      {
        _diagBufferedCount -= packetLength;

        hcom_logging_syslog(LOG_ERR, "%s@%d-Orphan Data:0x%02x\n",
                  thisFile, __LINE__, packetBuffer[packetLength-1]);
      }
    }

    free(packetBuffer);
    return ret;
}
//=====================================================================
// This is where the received binary message are decoded and processed
int hcom_esp32_recv_handle_bin_packet(uint8_t *binRecvdData, ssize_t binRecvdLen)
{
  int ret;
  struct HcomEsp32MqRecvdData_s mqRecvdData;
  
  // Last character already checked by caller

  // All SLIP encoded messages from ESP32 start with 0xc0 (SLIP framing)
  // and 0x01 (direction). However, we strip the leading 0xc0 before
  // the message is buffered, so here the first byte is the second
  // in the full message.
  if(*binRecvdData != 0x01)
  {
    // Only saw this not be 1 (direction) when system tick set to 100 usec
    // (it's now 1 ms) received 0x0c 0x0c with nothing in between. But leaving,
    // just in case.
    hcom_logging_syslog(LOG_ERR, "%s@%d-Expected direction of:1 not:%d\n",
              thisFile, __LINE__, *binRecvdData);

    // Wait a moment before returning
    usleep(100 * 1000);
    return -EIO;
  }

  // It is assumed that the only one that cares about responses from the ESP32
  // is the transmitter. Therefore, the transmitter sets _currentExpectRecvCommand
  // before sending the command. If not needed the message is ignored.
  // binRecvdData[1] stores the esp command
  if(binRecvdData[1] != _currentExpectRecvCommand)
  {

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
    hcom_logging_syslog(LOG_DEBUG, "%s@%d-Recvd cmd 0x%02x-ignoring\n",
             thisFile, __LINE__, binRecvdData[1], _currentExpectRecvCommand);
#endif

    return OK;    // Not an error. Just not needed.
  }

  // This message is the one expected, so inhibit duplicates by
  // telling receiver we're expecting nothing
  hcom_esp32_recv_expect_command_type(Esp32CommandUndefined);

  // Allocate a buffer for decoded message.
  uint8_t *decodedMsg = malloc(binRecvdLen);
  if(decodedMsg == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  // Decode the SLIP encoding
  int decodedLen = hcom_esp32_recv_slip_decoder(binRecvdData, binRecvdLen, decodedMsg);

  if(decodedLen < HCOM_ESP32_PROTOCOL_RECV_HDR_LENGTH)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Msg too small, expected:%d, recvd:%d\n",
              __FILE__, __LINE__, HCOM_ESP32_PROTOCOL_RECV_HDR_LENGTH,
              decodedLen);
    return -EPROTO; 
  }

  // Populate header
  mqRecvdData.espMqHdr.direction = decodedMsg[0];

  if(mqRecvdData.espMqHdr.direction != 1)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Expected direct of 1, not:%d\n",
              thisFile, __LINE__, mqRecvdData.espMqHdr.direction);
    return -EIO;
  }

  mqRecvdData.espMqHdr.command = decodedMsg[1];   // ESP32 command is an echo of caller's
  mqRecvdData.espMqHdr.size = (uint16_t)decodedMsg[2] + ((uint16_t)decodedMsg[3] << 8);

  // Valid message?
  if(decodedLen != mqRecvdData.espMqHdr.size + HCOM_ESP32_PROTOCOL_RECV_HDR_LENGTH)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Msg length error, expected:%d, not:%d\n",
              thisFile, __LINE__, decodedLen,
              mqRecvdData.espMqHdr.size + HCOM_ESP32_PROTOCOL_RECV_HDR_LENGTH);
    return -EMSGSIZE;
  }

  // If the request was to Read Register then place the received
  // register value into the value field.
  // The 'value' field (4-7) is only used by READ_REG, ESP32 docs say,
  // "Read data as 32-bit word in value field."
  if(mqRecvdData.espMqHdr.command == Esp32CommandReadRegister)
    mqRecvdData.espMqHdr.value = (uint32_t)decodedMsg[4] + ((uint32_t)decodedMsg[5] << 8) +
              ((uint32_t)decodedMsg[6] << 16) + ((uint32_t)decodedMsg[7] << 24);
  else
    mqRecvdData.espMqHdr.value = 0;

#if (HCOM_DIAG_INCLUDE_LOG_DEBUG_IN_BUILD > 0)
  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Recvd cmd:0x%02x, size:%d, value:0x%08x\n", thisFile, __LINE__,
      mqRecvdData.espMqHdr.command, mqRecvdData.espMqHdr.size, mqRecvdData.espMqHdr.value);
#endif

  // Populate the mq data structure
  mqRecvdData.recvdMqData = decodedMsg;    // Transmitter will free mem after using what it wants
  mqRecvdData.recvdMqDataLen = decodedLen;

  // Status and Error are at the end of the message and for the ESP32
  // 4 bytes are sent. The last 2 are reserved.
  // If there's an error Status == 1
  // 0x05 - "Received message is invalid" (parameters or length field is invalid)
  // 0x06 - "Failed to act on received message"
  // 0x07 - "Invalid CRC in message"
  // 0x08 - "flash write error" - after writing a block of data to flash,
  //            the ROM loader reads the value back and the 8-bit CRC is compared to the
  //            data read from flash. If they don't match, this error is returned.
  // 0x09 - "flash read error" - SPI read failed
  // 0x0a - "flash read length error" - SPI read request length is too long
  mqRecvdData.esp32Status = decodedMsg[decodedLen - 4];
  mqRecvdData.esp32Error = decodedMsg[decodedLen - 3];

  // Will notify the transmitter, telling it we've received what it's waiting for
  ret = mq_send(recvMsgQueue, (char *)&mqRecvdData, HCOM_ESP32_MQ_RECVD_DATA_STRUCT_LENGTH, 0);
  if(ret < 0)
  {
    int errn = get_errno();
    if(errn == EAGAIN)
    {
      hcom_logging_syslog(LOG_WARNING, "%s@%d-mq full, cmd:0x%02x not added\n",
                thisFile, __LINE__, mqRecvdData.espMqHdr.command);
    }
    else
    {
      // EAGAIN (11). The queue was full and the O_NONBLOCK flag was set for the message queue
      //                 description referred to by mqdes.
      // EINVAL (22). Either msg or mqdes is NULL or the value of prio is invalid.
      // EPERM (1). Message queue not opened for writing.
      // EMSGSIZE (122). 'msglen' was greater than the maxmsgsize attribute of the message queue.
      // EINTR (4). The call was interrupted by a signal handler.
      hcom_logging_syslog(LOG_ERR, "%s@%d-mq_send errno:%d\n", thisFile, __LINE__, errn);
    }
  }

  return ret;
}

//======================================================================
// Returns the length of the decoded message
ssize_t hcom_esp32_recv_slip_decoder(uint8_t *encodedMsg, ssize_t encodedMsgLen,
      uint8_t *decoded)
{
  // SLIP - Within the packet, all occurrences of 0xC0 are replaced with '0xDB 0xDC'
  //  and 0xDB replaced with with '0xDB 0xDD'
  int dest = 0;
  // Ignore trailing 0xc0
  for(int source = 0; source < encodedMsgLen - 1; source++)
  {
    if(encodedMsg[source] == HCOM_ESP32_SLIP_FRAME_ESCAPE_DB)
    {
      if(encodedMsg[source + 1] == HCOM_ESP32_SLIP_FRAME_TRANSPOSED_END_DC)
      {
        decoded[dest++] = HCOM_ESP32_SLIP_FRAME_END_C0;
      }
      else
      {
        if(encodedMsg[source + 1] != HCOM_ESP32_SLIP_FRAME_TRANSPOSED_ESCAPE_DD)
        {
          syslog(LOG_ERR, "%s@%d-Protocol error, expected:0x%02x, recvd:0x%02x\n",
                __FILE__, __LINE__, HCOM_ESP32_SLIP_FRAME_TRANSPOSED_ESCAPE_DD,
                encodedMsg[source + 1]);
          return -EPROTO; 
        }

        decoded[dest++] = HCOM_ESP32_SLIP_FRAME_ESCAPE_DB;
      }
      source++;
    }
    else
    {
      decoded[dest++] = encodedMsg[source];
    }
  }
  return dest;
}
