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
    return -1;
  }

  // Shared structure, size and message delimiter
  int ret = hcom_cirbuf_init(_esp_cir_buf, HCOM_ESP_COMMS_MAX_ESP_PACKET_SIZE * 4, 0xc0);
  if (ret == HCOM_CIR_BUF_INIT_FAILED)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-hcom_esp32_recv_buff_init failed:%d\n", thisFile, __LINE__, ret);
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
    while(*esp32_read_buffer != 0xc0)
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

      DEBUGASSERT(_diagBufferedCount <= HCOM_ESP_COMMS_MAX_ESP_PACKET_SIZE * 4);
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
          // The buffer to receive the message is too small? Probably 
          // corrupted data in buffer.
          hcom_logging_syslog(LOG_DEBUG, "%s@%d-No room for new data, need:%d\n",
                  thisFile, __LINE__, bytesToAdd);
          DEBUGASSERT(false);
      }
    }
    else if (ret == HCOM_CIR_BUF_ADD_BAD_ARG)
    {
        // Something wrong with implemenation
        DEBUGASSERT(false);
    }
    else
    {
        // Undefined return value, cannot happen
        DEBUGASSERT(false);
    }
  }   // while(true);

  ret = hcom_esp32_recv_pull_and_process();

  // Any other response is an error
  DEBUGASSERT(ret == HCOM_CIR_BUF_GET_FOUND_MSG ||
              ret == HCOM_CIR_BUF_GET_NONE_FOUND);
  return OK;
}

//===================================================================
// Pull packets from the circular buffer
int hcom_esp32_recv_pull_and_process()
{
    int ret;
    size_t packetLength;
    uint8_t *packetBuffer = malloc(HCOM_ESP_COMMS_MAX_ESP_PACKET_SIZE);

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
          DEBUGASSERT(*packetBuffer == 0xc0);
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
          hcom_logging_syslog(1, "%s@%d-No room for additional data, need:%d\n",
                  thisFile, __LINE__, packetLength);
          usleep(20 * 1000);
          DEBUGASSERT(false);
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
  
  // Check last character
  DEBUGASSERT(binRecvdData[binRecvdLen - 1] == 0xc0);

  // All SLIP encoded messages from ESP32 start with 0xc0 (SLIP framing)
  // and 0x01 (direction). However, we strip the leading 0xc0 before
  // the message is buffered, so here the first byte is the second
  // in the full message.
  if(*binRecvdData != 0x01)
  {
    // Only saw this when system tick set to 100 usec (it's now 1 ms) received
    // 0x0c 0x0c with nothing in between. But leaving, just in case.
    usleep(250 * 1000);
  }
  DEBUGASSERT(*binRecvdData == 0x01);

  // It is assumed that the only one that cares about responses from the ESP32
  // is the transmitter. Therefore, the transmitter sets _currentExpectRecvCommand
  // before sending the command. If not needed the message is ignored.
  // binRecvdData[1] stores the esp command
  if(binRecvdData[1] != _currentExpectRecvCommand)
  {
    hcom_logging_syslog(LOG_DEBUG, "%s@%d-Recvd cmd 0x%02x-ignoring\n",
             thisFile, __LINE__, binRecvdData[1], _currentExpectRecvCommand);
    return OK;    // Not an error. Just not needed.
  }

  // This message is the one expected, so inhibit duplicates by
  // telling receiver we're expecting nothing
  hcom_esp32_recv_expect_command_type(Esp32CommandUndefined);

  // Allocate a buffer for decoded message.
  uint8_t *decodedMsg = malloc(binRecvdLen);

  // Decode the SLIP encoding
  int decodedLen = hcom_esp32_recv_slip_decoder(binRecvdData, binRecvdLen, decodedMsg);

  DEBUGASSERT(decodedLen >= HCOM_ESP32_PROTOCOL_RECV_HDR_LENGTH);  // Must be at least a header
  DEBUGASSERT(decodedLen < binRecvdLen);

  // Populate header
  mqRecvdData.espMqHdr.direction = decodedMsg[0];
  DEBUGASSERT(mqRecvdData.espMqHdr.direction == 1);
  mqRecvdData.espMqHdr.command = decodedMsg[1];   // ESP32 command is an echo of caller's
  mqRecvdData.espMqHdr.size = (uint16_t)decodedMsg[2] + ((uint16_t)decodedMsg[3] << 8);

  DEBUGASSERT(decodedLen == mqRecvdData.espMqHdr.size + HCOM_ESP32_PROTOCOL_RECV_HDR_LENGTH);

  // If the request was to Read Register then place the received
  // register value into the value field.
  // The 'value' field (4-7) is only used by READ_REG, ESP32 docs say,
  // "Read data as 32-bit word in value field."
  if(mqRecvdData.espMqHdr.command == Esp32CommandReadRegister)
    mqRecvdData.espMqHdr.value = (uint32_t)decodedMsg[4] + ((uint32_t)decodedMsg[5] << 8) +
              ((uint32_t)decodedMsg[6] << 16) + ((uint32_t)decodedMsg[7] << 24);
  else
    mqRecvdData.espMqHdr.value = 0;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Recvd cmd:0x%02x, size:%d, value:0x%08x\n", thisFile, __LINE__,
      mqRecvdData.espMqHdr.command, mqRecvdData.espMqHdr.size, mqRecvdData.espMqHdr.value);

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
    if(encodedMsg[source] == 0xdb)
    {
      if(encodedMsg[source + 1] == 0xdc)
      {
        decoded[dest++] = 0xc0;
      }
      else
      {
        DEBUGASSERT(encodedMsg[source + 1] == 0xdd);
        decoded[dest++] = 0xdb;
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
