/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_comsupport.c
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

/****************************************************************************
 * Public Functions
 ***************************************************************************/
// Consistent Overhead Byte Stuffing (COBS) is a scheme to take binary data
// replace an arbituary byte value, usually 0x00, with an encoding that replaces
// this value, in a way, that allows the orginal data can be recovered while
// creating frames around the data.
//
// The following C# code was ported from a 'C' example licensed under MIT License
// https://github.com/bakercp/PacketSerial/blob/master/src/Encoding/COBS.h.
// After porting, I found errors. I referred to the original authors paper at
// http://conferences.sigcomm.org/sigcomm/1997/papers/p062.pdf for additional insights.
// Modifications were needed and adding a starting offset to support large buffers was
// added to allow sub-segments to be encoded.
//

// This function removes all 0x00 values from the source buffer. It allows
// any length packet to be encoded but adds at least 1 byte every 254 bytes.
// To used this encoded packet, a 0x00 is added to the end of this encoded
// message as a packet delimiter. This algorithm is known as 'COBS'
size_t hcom_com_support_cobs_encoder(uint8_t source[], size_t startingOffset, size_t length, uint8_t encoded[])
{
  DEBUGASSERT(length <= HCOM_PROTOCOL_PACKET_MAX_SIZE);
  
  size_t sourceOffset = startingOffset; // Offset to pre-encoded data buffer
  size_t encodedOffset = 1;             // Offset to encoded data buffer
  size_t replaceOffset = 0;             // Offset where 0 is being tracked
  uint8_t replacement = 1;              // Value that will be inserted to indicate 0 replaced

  while (sourceOffset < length + startingOffset)
  {
    // Is source value is the one we want to replace?
    if (source[sourceOffset] == HCOM_PROTOCOL_PACKET_DELIMITER_VALUE)
    {
      encoded[replaceOffset] = replacement; // Replace '0' value with offset
      replaceOffset = encodedOffset++;      // Update replacement offset and bump encoded offset
      replacement = 1;                      // Reset replacement
    }
    else
    {
      encoded[encodedOffset++] = source[sourceOffset]; // Just copy original value
      replacement++;

      // 0xff is reserved for when when longer than 254 bytes
      if (replacement == 0xff)
      {
        encoded[replaceOffset] = replacement; // see above for identical code
        replaceOffset = encodedOffset++;
        replacement = 1;
      }
    }
    sourceOffset++; // Point to next source value
  }

  encoded[replaceOffset] = replacement;
  DEBUGASSERT(encodedOffset <= HCOM_SAFE_PACKET_BUF_SIZE);
  return encodedOffset; // Number of bytes written to result buffer
}

//-------------------------------------------------------------------------
// This function restores the removed 0x00s, thus returning the packet to it's
// original content.
size_t hcom_com_support_cobs_decoder(uint8_t encoded[], size_t length, uint8_t decoded[])
{
  size_t encodedOffset = 0; // Offset into original (encoded) buffer
  size_t decodedOffset = 0; // Offset into destination (decoded) buffer
  uint8_t replacement = 0;  // Value that will be inserted to indicate replaced value

  while (encodedOffset < length)
  {
    replacement = encoded[encodedOffset]; // Grab next byte
    if (((encodedOffset + replacement) > length) && (replacement != 1))
      return 0;

    encodedOffset++; // Point to next source

    // Copy all unchanged bytes (use memcpy?)
    for (int i = 1; i < replacement; i++)
      decoded[decodedOffset++] = encoded[encodedOffset++];

    // Sometimes don't need a trailing delimiter added
    if (replacement < 0xff && encodedOffset != length)
      decoded[decodedOffset++] = HCOM_PROTOCOL_PACKET_DELIMITER_VALUE;
  }

  return decodedOffset;
}

//=====================================================================
// Circular Buffer
// This circular buffer was written for the Meadow F7. Unlike the classic
// version, this version has a byte array as input (received data). It 
// returns a byte array consisting of everything from the head to and including
// the first byte whose value is 0. It's designed to work with the COBS
// encoding scheme. It both buffers and isolates the packets.
//
int hcom_cirbuf_init(struct host_com_cir_buffer_s *hcbuf, size_t totalCapacity)
{
  hcbuf->bottom = (uint8_t *)malloc(totalCapacity);
  if (hcbuf->bottom == NULL)
    return HCOM_CIR_BUF_INIT_FAILED;

  f7syslog(LOG_DEBUG, "Circular buffer size:'%d' at 0x%p\n", totalCapacity, (void *)hcbuf->bottom);

  hcbuf->top = hcbuf->bottom + totalCapacity;
  hcbuf->head = hcbuf->bottom;
  hcbuf->tail = hcbuf->bottom;

  return HCOM_CIR_BUF_INIT_OK;
}

//=====================================================================
size_t hcom_cirbuf_avail_space(struct host_com_cir_buffer_s *hcbuf)
{
  // We leave one free byte so the head and tail are equal only if
  // empty not when full. Full means 1 free byte.
  if (hcbuf->head < hcbuf->tail)
    return hcbuf->tail - hcbuf->head - 1;
  else
    return ((hcbuf->top - hcbuf->bottom) - (hcbuf->head - hcbuf->tail)) - 1;
}

//==============================================================================
//
int hcom_cirbuf_release_memory(struct host_com_cir_buffer_s *hcbuf)
{
  free(hcbuf->bottom);
  free(hcbuf);
  return HCOM_CIR_BUF_INIT_OK;
}

//==============================================================================
int hcom_cirbuf_add_bytes(struct host_com_cir_buffer_s *hcbuf, uint8_t *newBytes, uint32_t bytesToAdd)
{
  if (bytesToAdd == 0)
    return HCOM_CIR_BUF_ADD_BAD_ARG;

  if (hcom_cirbuf_avail_space(hcbuf) < bytesToAdd)
    return HCOM_CIR_BUF_ADD_WONT_FIT;

  uint8_t *newHead = hcbuf->head + bytesToAdd;
  if (newHead < hcbuf->top)
  {
    // Simple case (no wrap around)
    memcpy(hcbuf->head, newBytes, bytesToAdd);
    hcbuf->head = newHead;
  }
  else
  {
    // Wrap around - fill up head-top space and use bottom space too
    size_t spaceFreeOnTop = (hcbuf->top - hcbuf->head);
    memcpy(hcbuf->head, newBytes, spaceFreeOnTop);
    memcpy(hcbuf->bottom, newBytes + spaceFreeOnTop, bytesToAdd - spaceFreeOnTop);
    hcbuf->head = hcbuf->bottom + bytesToAdd - spaceFreeOnTop;
  }
  return HCOM_CIR_BUF_ADD_SUCCESS;
}

//==============================================================================
// Caller must supply packetDestBuf and size
int hcom_cirbuf_get_next_packet(struct host_com_cir_buffer_s *hcbuf, uint8_t *packetDestBuf,
                                size_t packetDestBufSize, size_t *packetLength)
{
  uint8_t *found;
  size_t sizeFoundTop;

  *packetLength = 0;
  
  if (hcbuf->head == hcbuf->tail)
    return HCOM_CIR_BUF_GET_NONE_FOUND; // Buffer empty

  // Scan the buffer looking for the delimiter 0x00
  if (hcbuf->head > hcbuf->tail)
  {
    // Simple case (no wrap around)
    found = (uint8_t *)memchr(hcbuf->tail, HCOM_PROTOCOL_PACKET_DELIMITER_VALUE, hcbuf->head - hcbuf->tail);
    if (found == NULL)
      return HCOM_CIR_BUF_GET_NONE_FOUND;
  }
  else
  {
    found = (uint8_t *)memchr(hcbuf->tail, HCOM_PROTOCOL_PACKET_DELIMITER_VALUE, hcbuf->top - hcbuf->tail);
  }

  // Move first part
  if (found != NULL)
  {
    // Found the delimiter, message in one contiguous packet
    sizeFoundTop = found - hcbuf->tail + 1;
    if (sizeFoundTop > packetDestBufSize)
    {
      *packetLength = sizeFoundTop;
      return HCOM_CIR_BUF_GET_DEST_NO_ROOM;
    }

    memcpy(packetDestBuf, hcbuf->tail, sizeFoundTop);
    hcbuf->tail = found + 1;
    *packetLength = sizeFoundTop;
    return HCOM_CIR_BUF_GET_FOUND_MSG;
  }

  // Continue looking for the delimiter from the bottom up since we got
  // here because the delimiter was not found while scanning above
  found = (uint8_t *)memchr(hcbuf->bottom, HCOM_PROTOCOL_PACKET_DELIMITER_VALUE, hcbuf->head - hcbuf->bottom);
  if (found == NULL)
    return HCOM_CIR_BUF_GET_NONE_FOUND;

  sizeFoundTop = hcbuf->top - hcbuf->tail;
  size_t sizeFoundBottom = found - hcbuf->bottom + 1;
  if (sizeFoundBottom + sizeFoundTop > packetDestBufSize)
  {
    *packetLength = sizeFoundBottom + sizeFoundTop;
    return HCOM_CIR_BUF_GET_DEST_NO_ROOM;
  }

  memcpy(packetDestBuf, hcbuf->tail, sizeFoundTop);
  memcpy(packetDestBuf + sizeFoundTop, hcbuf->bottom, sizeFoundBottom);
  hcbuf->tail = found + 1;
  *packetLength = sizeFoundTop + sizeFoundBottom;
  return HCOM_CIR_BUF_GET_FOUND_MSG;
}
