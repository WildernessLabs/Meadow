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
static bool _currentExpectBinaryMsg;
static uint8_t _currentExpectRecvCommand;
static struct hcom_esp32_cir_buffer_s *_esp_cir_buf;
static uint32_t _cr_lf_esp32_crlf_counter;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_esp32_recv_pull_and_process(void);
static int hcom_esp32_recv_handle_bin_packet(uint8_t *binRecvdData, ssize_t binRecvdLen);
static int hcom_esp32_recv_handle_text_packet(uint8_t *text_buffer, ssize_t length);

static int hcom_esp32_recv_buff_init(struct hcom_esp32_cir_buffer_s *espbuf, size_t totalCapacity);
static size_t hcom_esp32_recv_buff_avail_space(struct hcom_esp32_cir_buffer_s *espbuf);
static int hcom_esp32_recv_buff_release_mem(struct hcom_esp32_cir_buffer_s *espbuf);
static int hcom_esp32_recv_buff_add_bytes(struct hcom_esp32_cir_buffer_s *espbuf,
                              uint8_t *newBytes, uint32_t bytesToAdd);
static int hcom_esp32_recv_buff_get_next_packet(struct hcom_esp32_cir_buffer_s *espbuf, uint8_t *packetDestBuf,
                                size_t packetDestBufSize, size_t *packetLength);
static int hcom_esp32_recv_slip_decoder(uint8_t *encodedMsg, ssize_t encodedMsgLen,
      uint8_t *decoded);
static uint8_t *hcom_esp32_recv_buff_find_delimiter(const uint8_t *s, uint8_t expectedDelimiter, size_t n);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_esp32_recv_setup_lazy()
{
  _shutting_down = false;
  _currentExpectBinaryMsg = false;
  _currentExpectRecvCommand = Esp32CommandUndefined;  
  _cr_lf_esp32_crlf_counter = 0;

  _esp_cir_buf = (struct hcom_esp32_cir_buffer_s *)malloc(sizeof(struct hcom_esp32_cir_buffer_s));
  if (_esp_cir_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-circular buffer allocation failed\n", thisFile, __LINE__);
    return -1;
  }

  int ret = hcom_esp32_recv_buff_init(_esp_cir_buf, HCOM_ESP_COMMS_MAX_ESP_PACKET_SIZE * 4);
  if (ret == HCOM_ESP32_BUF_INIT_FAILED)
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
  hcom_esp32_recv_buff_release_mem(_esp_cir_buf);

  mq_close(recvMsgQueue);
  mq_unlink(HCOM_ESP_COMMS_MSG_QUEUE_NAME);
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
// When data received the esp thread calls here for processing
int hcom_esp32_recv_handle_data(uint8_t *esp32_read_buffer, ssize_t bytesToAdd)
{
  int ret;
  
  while (true)
  {
    ret = hcom_esp32_recv_buff_add_bytes(_esp_cir_buf, esp32_read_buffer, bytesToAdd);
    if(ret == HCOM_ESP32_BUF_ADD_SUCCESS)
      break;

    if(ret == HCOM_ESP32_BUF_ADD_WONT_FIT)
    {
      // Wasn't possible to put these bytes in the buffer. We need to
      // process a few packets and then retry to add this data
      ret = hcom_esp32_recv_pull_and_process();
      if (ret == HCOM_ESP32_BUF_GET_FOUND_BIN ||
          ret == HCOM_ESP32_BUF_GET_FOUND_TEXT ||
          ret == HCOM_ESP32_BUF_GET_NONE_FOUND)
          continue;   // There should be room now for the failed add

      if (ret == HCOM_ESP32_BUF_GET_DEST_NO_ROOM)
      {
          // The buffer to receive the message is too small? Probably 
          // corrupted data in buffer.
          hcom_logging_syslog(LOG_DEBUG, "%s@%d-No room for new data, need:%d\n",
                  thisFile, __LINE__, bytesToAdd);
          DEBUGASSERT(false);
      }
    }
    else if (ret == HCOM_ESP32_BUF_ADD_BAD_ARG)
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
  DEBUGASSERT(ret == HCOM_ESP32_BUF_GET_FOUND_BIN ||
              ret == HCOM_ESP32_BUF_GET_FOUND_TEXT ||
              ret == HCOM_ESP32_BUF_GET_NONE_FOUND);
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
      // Get the next packet, binary packet or text line
      ret = hcom_esp32_recv_buff_get_next_packet(_esp_cir_buf, packetBuffer,
                    HCOM_ESP_COMMS_MAX_ESP_PACKET_SIZE, &packetLength);
      if (ret == HCOM_ESP32_BUF_GET_NONE_FOUND)
          break;      // We've emptied buffer of all full packets

      if (ret == HCOM_ESP32_BUF_GET_DEST_NO_ROOM)
      {
          // The buffer to accept the packets is too small! Need to enlarge
          hcom_logging_syslog(LOG_DEBUG, "%s@%d-No room for additional data, need:%d\n",
                  thisFile, __LINE__, packetLength);
          DEBUGASSERT(false);
      }

      if(ret == HCOM_ESP32_BUF_GET_FOUND_BIN)
      {
        ret = hcom_esp32_recv_handle_bin_packet(packetBuffer, packetLength);
        if(ret < 0)
        {
          hcom_logging_syslog(LOG_ERR, "%s@%d-Bin message error:%d\n", thisFile, __LINE__, ret);
        }
      }
      else if (ret == HCOM_ESP32_BUF_GET_FOUND_TEXT)
      {
        ret = hcom_esp32_recv_handle_text_packet(packetBuffer, packetLength);
        if(ret < 0)
        {
          hcom_logging_syslog(LOG_ERR, "%s@%d-Text handling error:%d\n", thisFile, __LINE__, ret);
        }
      }
      else
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Orphan Data:0x%02x\n",
                  thisFile, __LINE__, packetBuffer[packetLength-1]);
      }
    }

    free(packetBuffer);
    return ret;
}

//=====================================================================
// This function is pretty much just diagnostic in nature
int hcom_esp32_recv_handle_text_packet(uint8_t *text_buffer, ssize_t length)
{
  // Ignore if not LOG_INFO
  if ((hcom_diag_logging_get_syslog_mask() & LOG_MASK(LOG_INFO)) == 0)
    return OK;   // Nothing to do

  // There are times when the ESP32 sends endless cr/lf very fast
  if(length == 2)
  {
#if HCOM_ESP32_PROCESS_CR_LF_ENDLESS_TEXT == 0
    return OK;    // Just ignore
  }
#else
    _cr_lf_esp32_crlf_counter++;

    if(_cr_lf_esp32_crlf_counter % 500000 == 0)
    {
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Another 500,000 cr/lf %d\n",
                thisFile, __LINE__, _cr_lf_esp32_crlf_counter);
      return OK;
    }

    if(_cr_lf_esp32_crlf_counter > 4)
      return OK;

    if(_cr_lf_esp32_crlf_counter == 4)
    {
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Appears to be endless stream of cr/lf\n",
                thisFile, __LINE__);
      return OK;
    }
  }
  else
  {
    _cr_lf_esp32_crlf_counter = 0;
  }
#endif

  if(length >= HCOM_ESP_COMMS_MAX_ESP_PACKET_SIZE)
    return OK;
  
  // Since text is not SLIP encoded we can see it, if it's null
  // terminated. So, we'll terminate and display it via syslog. Strip
  // off ending cr/lf (0x0d, 0x0a) which all messages seem to have.
  if(text_buffer[length - 1] == 0x0a || text_buffer[length - 1] == 0x0d)
    length--;
  if(text_buffer[length - 1] == 0x0a || text_buffer[length - 1] == 0x0d)
    length--;
  text_buffer[length] = '\0';   // null terminate
  
  // This cannot be routed to Meadow.CLI so send directly to syslog
  hcom_logging_syslog(LOG_INFO, "ESP32 Text:'%s'\n", text_buffer);
  return OK;
}

//=====================================================================
// This is where the received binary message are decoded and processed
int hcom_esp32_recv_handle_bin_packet(uint8_t *binRecvdData, ssize_t binRecvdLen)
{
  int ret;
  struct HcomEsp32MqRecvdData_s mqRecvdData;

  // This is really a test of the circular buffer
  DEBUGASSERT(binRecvdData[0] == 0xc0);
  DEBUGASSERT(binRecvdData[binRecvdLen - 1] == 0xc0);

  // All SLIP encoded messages from ESP32 start with 0xc0 (SLIP framing)
  // and 0x01 (direction).
  if(binRecvdData[1] != 0x01)
  {
    // Only saw this when system tick set to 100 usec (it's now 1 ms) received
    // 0x0c 0x0c with nothing in between. But leaving test, just in case.
    sleep(1);
  }
  DEBUGASSERT(binRecvdData[1] == 0x01);

  // It is assumed that the only one that cares about responses from the ESP32
  // is the transmitter. Therefore, the transmitter sets _currentExpectRecvCommand
  // before sending the command. If not needed the message is ignored.
  // binRecvdData[2] stores the esp command
  if(binRecvdData[2] != _currentExpectRecvCommand)
  {
    hcom_logging_syslog(LOG_DEBUG, "%s@%d-Recvd cmd 0x%02x-ignored\n",
             thisFile, __LINE__, binRecvdData[2], _currentExpectRecvCommand);
    return OK;    // Not an error. Just not needed.
  }

  // This message is the one expected, so inhibit duplicates by
  // telling receiver we're expecting nothing
  hcom_esp32_recv_expect_command_type(Esp32CommandUndefined);

  // Allocate a buffer to build the message for transmitter. It's a bit
  // too big because it includes 0xc0 packet delimiters and any values
  // that are slip encoded.
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

  // If the command was request was to Read Register then place the received
  // register value into the value field.
  // The 'value' field (4-7) is only used by READ_REG, ESP32 docs say, "Read data
  // as 32-bit word in value field."
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
  // Skip leading 0xc0 and ignore trailing 0xc0
  for(int source = 1; source < encodedMsgLen - 1; source++)
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

//=====================================================================
// ESP32 Circular Buffer
// Unlike the classic circular buffer, this version has a byte array as
// input (received data). It returns a byte array consisting of everything
// from the head to the end of a packet. For ESP32 a packet is define as
// either a 0x0a (line feed) terminated text message or a binary message
// SLIP encoded which starts and ends with 0xc0.
//
// Note: this is not thread safe as all functions share a common buffer.
// However, at this time only one thread access these functions.
int hcom_esp32_recv_buff_init(struct hcom_esp32_cir_buffer_s *espbuf, size_t totalCapacity)
{
  espbuf->bottom = (uint8_t *)malloc(totalCapacity);
  if (espbuf->bottom == NULL)
    return HCOM_ESP32_BUF_INIT_FAILED;

  espbuf->top = espbuf->bottom + totalCapacity;
  espbuf->head = espbuf->bottom;
  espbuf->tail = espbuf->bottom;

  return HCOM_ESP32_BUF_INIT_OK;
}

//=====================================================================
size_t hcom_esp32_recv_buff_avail_space(struct hcom_esp32_cir_buffer_s *espbuf)
{
  // We leave one free byte so the head and tail are equal only if
  // empty not when full. Full means 1 free byte.
  if (espbuf->head < espbuf->tail)
    return espbuf->tail - espbuf->head - 1;
  else
    return ((espbuf->top - espbuf->bottom) - (espbuf->head - espbuf->tail)) - 1;
}

//==============================================================================
//
int hcom_esp32_recv_buff_release_mem(struct hcom_esp32_cir_buffer_s *espbuf)
{
  free(espbuf->bottom);
  free(espbuf);
  return HCOM_ESP32_BUF_INIT_OK;
}

//==============================================================================
// Pretty much "throw and go"
int hcom_esp32_recv_buff_add_bytes(struct hcom_esp32_cir_buffer_s *espbuf,
                              uint8_t *newBytes, uint32_t bytesToAdd)
{
  if (bytesToAdd == 0)
    return HCOM_ESP32_BUF_ADD_BAD_ARG;

  if (hcom_esp32_recv_buff_avail_space(espbuf) < bytesToAdd)
    return HCOM_ESP32_BUF_ADD_WONT_FIT;

  uint8_t *newHead = espbuf->head + bytesToAdd;
  if (newHead < espbuf->top)
  {
    // Simple case (no wrap around)
    memcpy(espbuf->head, newBytes, bytesToAdd);
    espbuf->head = newHead;
  }
  else
  {
    // Wrap around - fill up head-top space and use bottom space too
    size_t spaceFreeOnTop = (espbuf->top - espbuf->head);
    memcpy(espbuf->head, newBytes, spaceFreeOnTop);
    memcpy(espbuf->bottom, newBytes + spaceFreeOnTop, bytesToAdd - spaceFreeOnTop);
    espbuf->head = espbuf->bottom + bytesToAdd - spaceFreeOnTop;
  }
  return HCOM_ESP32_BUF_ADD_SUCCESS;
}

//==============================================================================
// Caller must supply packetDestBuf and it's size
int hcom_esp32_recv_buff_get_next_packet(struct hcom_esp32_cir_buffer_s *espbuf, uint8_t *packetDestBuf,
                                size_t packetDestBufSize, size_t *packetLength)
{
  uint8_t *found;
  size_t sizeFoundTop;
  uint8_t delimiter;
  uint8_t *activeTail = espbuf->tail;

  *packetLength = 0;
  
  if (espbuf->head == espbuf->tail)
    return HCOM_ESP32_BUF_GET_NONE_FOUND; // Buffer empty

  // ESP SLIP encoded binary data can always be identified because it starts
  // and ends with 0xc0. However, the ESP32 also sends ascii on the same line.
  // Since 'tail' points to the next available byte in the buffer (i.e. first
  // byte in message). We can safely assume if this is 0xc0 it's a slip encoded
  // binary message.
  if(*espbuf->tail == 0xc0)   // Assume, first character of slip (but could be last)
  {
    // Sanity check - was the previous a valid EOM character?
    // In other words, are we starting at the beginning of a new
    // message?
    if(espbuf->tail > espbuf->bottom)
      DEBUGASSERT(*(espbuf->tail-1) == 0xc0 || *(espbuf->tail-1) == 0x0a);

    activeTail++;   // Skip this 0xc0 in following search
    if (espbuf->head == activeTail)
      return HCOM_ESP32_BUF_GET_NONE_FOUND; // Buffer has single 0xc0

    delimiter = 0xc0;
  }
  else
  {
    // Assume ascii - this is a bit dangerous because if we get out of sync
    // there's probably no way to get back in sync quickly.
    delimiter = 0x0a;
  }
  
  //---------------------------------------------------------
  // Scan the buffer looking for the delimiter
  if (espbuf->head > activeTail)
  {
    // Simple case (no wrap around)
    found = (uint8_t *)hcom_esp32_recv_buff_find_delimiter(activeTail, delimiter, espbuf->head - activeTail);
    if (found == NULL)
      return HCOM_ESP32_BUF_GET_NONE_FOUND;
  }
  else
  {
    found = (uint8_t *)hcom_esp32_recv_buff_find_delimiter(activeTail, delimiter, espbuf->top - activeTail);
  }

  if (found != NULL)
  {
    // Found the delimiter and message in one contiguous memory block
    sizeFoundTop = found - espbuf->tail + 1;
    if (sizeFoundTop > packetDestBufSize)
    {
      *packetLength = sizeFoundTop;
      return HCOM_ESP32_BUF_GET_DEST_NO_ROOM;
    }

  // ESP32 will at times send partial text, just starting binary
  // without the ending 0x0a. The following repairs the incomplete
  // test and hope things stay in sync afterward.
    if(*found != delimiter && delimiter == 0x0a)
    {
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Delimiter:0x%02x found, not expected:0x%02x\n",
              thisFile, __LINE__, *found, delimiter);

      DEBUGASSERT(sizeFoundTop + 1 <= packetDestBufSize);
      memcpy(packetDestBuf, espbuf->tail, sizeFoundTop);
      packetDestBuf[sizeFoundTop - 1] = 0x0d;   // Over write 0xc0 with CR
      packetDestBuf[sizeFoundTop] = 0x0a;       // Postpend text with LF
      *(found - 1) = 0x0a;                      // So next time can pass sanity check
      
      // Set tail just before unexpected so the following message can be found next time
      espbuf->tail = found;
      *packetLength = sizeFoundTop + 1;         // Account for LF
      return HCOM_ESP32_BUF_GET_FOUND_TEXT;
    }

    // Include leading and trailing 0xc0 in copied data
    memcpy(packetDestBuf, espbuf->tail, sizeFoundTop);
    espbuf->tail = found + 1;
    *packetLength = sizeFoundTop;

    if (delimiter == 0xc0)
      return HCOM_ESP32_BUF_GET_FOUND_BIN;
    else
      return HCOM_ESP32_BUF_GET_FOUND_TEXT;
  }

  //----------------------------------------------------------
  // Continue looking for the delimiter from the bottom up.
  found = (uint8_t *)hcom_esp32_recv_buff_find_delimiter(espbuf->bottom, delimiter, espbuf->head - espbuf->bottom);
  if (found == NULL)
    return HCOM_ESP32_BUF_GET_NONE_FOUND;

  sizeFoundTop = espbuf->top - espbuf->tail;
  size_t sizeFoundBottom = found - espbuf->bottom + 1;
  if (sizeFoundBottom + sizeFoundTop > packetDestBufSize)
  {
    *packetLength = sizeFoundBottom + sizeFoundTop;
    return HCOM_ESP32_BUF_GET_DEST_NO_ROOM;
  }

  // ESP32 will at times send partial text, just starting binary
  // without the ending 0x0a. The following repairs the incomplete
  // test and hope things stay in sync afterward.
  if(*found != delimiter && delimiter == 0x0a)
  {
    hcom_logging_syslog(LOG_DEBUG, "%s@%d-Delimiter:0x%02x found, not expected:0x%02x\n",
            thisFile, __LINE__, *found, delimiter);
    DEBUGASSERT(sizeFoundTop + sizeFoundBottom + 1 <= packetDestBufSize);
    memcpy(packetDestBuf, espbuf->tail, sizeFoundTop);
    memcpy(packetDestBuf + sizeFoundTop, espbuf->bottom, sizeFoundBottom);
    packetDestBuf[sizeFoundTop + sizeFoundBottom - 1] = 0x0d; // Over write 0xc0 with CR
    packetDestBuf[sizeFoundTop + sizeFoundBottom] = 0x0a;     // Postpend text with LF
    *(found - 1) = 0x0a;                                      // Add missing 0x0a for sanity check
    // Set tail just before unexpected so the following message can be found next time
    espbuf->tail = found;
    *packetLength = sizeFoundTop + sizeFoundBottom + 1;       // Account for LF
    return HCOM_ESP32_BUF_GET_FOUND_TEXT;
  }

  // Sanity check #3
  if(delimiter == 0x0a)
  {
    // Verify that we found 0x0a and not a byte > 0x7f
    DEBUGASSERT(*found == delimiter);
  }

  memcpy(packetDestBuf, espbuf->tail, sizeFoundTop);
  memcpy(packetDestBuf + sizeFoundTop, espbuf->bottom, sizeFoundBottom);
  espbuf->tail = found + 1;
  *packetLength = sizeFoundTop + sizeFoundBottom;

  if (delimiter == 0xc0)
    return HCOM_ESP32_BUF_GET_FOUND_BIN;
  else
    return HCOM_ESP32_BUF_GET_FOUND_TEXT;
}

//==================================================================
// If binary expected uses memchr, if ascii then verify that no non-ascii
// characters found.
// Return a pointer to the last character checked
uint8_t *hcom_esp32_recv_buff_find_delimiter(const uint8_t *start, uint8_t expectedDelimiter, size_t numb)
{
  uint8_t *nextCheck = (uint8_t *)start;

  // For ascii we want to verify that this is not binary
  if(expectedDelimiter == 0x0a)
  {
    while(numb--)
    {
      if( *nextCheck == expectedDelimiter || *nextCheck > 0x7f )
        return nextCheck;   // delimiter or above range for ascii
      else
        nextCheck++;
    }
    return 0;
  }
  else
  {
    // For binary there are no illegal characters, just look for 0xc0
    return memchr(start, expectedDelimiter, numb);
  }
}