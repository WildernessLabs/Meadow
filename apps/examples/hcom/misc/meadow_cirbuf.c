/****************************************************************************
 * \apps\examples\hcom\misc\meadow_cirbuf.c
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

// This module contains an implementation of a circular buffer that is used
// in a couple of places. It allows 1-n bytes to be saved and a delimited
// packet to be retrieved.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/meadow_cirbuf.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Circular Buffer
// This circular buffer was written for the Meadow F7. Unlike the classic
// version, this version has a byte array as input (received data). It 
// returns a byte array consisting of everything from the head to and including
// the first byte whose value is 0. It's designed to work with the COBS
// encoding scheme. It both buffers and isolates the packets.
//
// Note: this is not thread safe as all functions share a common buffer.
// However, at this time only one thread access these functions / buffer.
int hcom_cirbuf_init(host_com_cir_buffer_t *hcbuf, size_t totalCapacity,
          uint8_t delimiter)
{
  hcbuf->delimiter = delimiter;

  hcbuf->bottom = (uint8_t *)malloc(totalCapacity);
  if (hcbuf->bottom == NULL)
    return HCOM_CIR_BUF_ALLOC_FAILED;

  hcbuf->top = hcbuf->bottom + totalCapacity;
  hcbuf->head = hcbuf->bottom;
  hcbuf->tail = hcbuf->bottom;

  return HCOM_CIR_BUF_INIT_OK;
}

//=====================================================================
size_t hcom_cirbuf_avail_space(host_com_cir_buffer_t *hcbuf)
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
int hcom_cirbuf_release_memory(host_com_cir_buffer_t *hcbuf)
{
  free(hcbuf->bottom);
  return HCOM_CIR_BUF_INIT_OK;
}

//==============================================================================
// Added this to clear the buffer if it is filled with text and no delimiter
int hcom_cirbuf_clear_buffer(host_com_cir_buffer_t *hcbuf)
{
  // Reinitialize head and tail pointers
  hcbuf->head = hcbuf->bottom;
  hcbuf->tail = hcbuf->bottom;
  return HCOM_CIR_BUF_INIT_OK;
}

//==============================================================================
// Add the bytes requested, if they will fit
int hcom_cirbuf_add_bytes(host_com_cir_buffer_t *hcbuf, uint8_t *newBytes,
          uint32_t bytesToAdd)
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
// Caller must supply packetDestBuf and it's size
int hcom_cirbuf_get_next_packet(host_com_cir_buffer_t *hcbuf, uint8_t *packetDestBuf,
                                size_t packetDestBufSize, size_t *packetLength)
{
  uint8_t *found;
  size_t sizeFoundTop;

  *packetLength = 0;
  
  if (hcbuf->head == hcbuf->tail)
    return HCOM_CIR_BUF_GET_NONE_FOUND; // Buffer empty

  // Scan the buffer looking for the delimiter
  if (hcbuf->head > hcbuf->tail)
  {
    // Simple case (no wrap around)
    found = (uint8_t *)memchr(hcbuf->tail, hcbuf->delimiter, hcbuf->head - hcbuf->tail);
    if (found == NULL)
      return HCOM_CIR_BUF_GET_NONE_FOUND;
  }
  else
  {
    found = (uint8_t *)memchr(hcbuf->tail, hcbuf->delimiter, hcbuf->top - hcbuf->tail);
  }

  // Move bytes of packet or it's first half
  if (found != NULL)
  {
    // Found the delimiter, message in one contiguous packet
    sizeFoundTop = found - hcbuf->tail + 1;
    if (sizeFoundTop > packetDestBufSize)
    {
      // Won't fit in caller supplied packet
      *packetLength = sizeFoundTop;   // Size needed
      return HCOM_CIR_BUF_GET_DEST_NO_ROOM;
    }

    // It will all fit we can copy and exit
    memcpy(packetDestBuf, hcbuf->tail, sizeFoundTop);
    hcbuf->tail = found + 1;
    *packetLength = sizeFoundTop;     // Size found
    return HCOM_CIR_BUF_GET_FOUND_MSG;
  }

  // Continue looking for the delimiter from the bottom up since we got
  // here because the delimiter was not found while scanning above
  found = (uint8_t *)memchr(hcbuf->bottom, hcbuf->delimiter, hcbuf->head - hcbuf->bottom);
  if (found == NULL)
    return HCOM_CIR_BUF_GET_NONE_FOUND;

  sizeFoundTop = hcbuf->top - hcbuf->tail;
  size_t sizeFoundBottom = found - hcbuf->bottom + 1;
  if (sizeFoundBottom + sizeFoundTop > packetDestBufSize)
  {
    // Won't fit in provided packet
    *packetLength = sizeFoundBottom + sizeFoundTop;
    return HCOM_CIR_BUF_GET_DEST_NO_ROOM;
  }

  memcpy(packetDestBuf, hcbuf->tail, sizeFoundTop);
  memcpy(packetDestBuf + sizeFoundTop, hcbuf->bottom, sizeFoundBottom);
  hcbuf->tail = found + 1;
  *packetLength = sizeFoundTop + sizeFoundBottom;
  return HCOM_CIR_BUF_GET_FOUND_MSG;
}
