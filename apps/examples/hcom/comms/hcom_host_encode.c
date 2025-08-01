/****************************************************************************
 * apps\examples\hcom\comms\hcom_host_encode.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>

/****************************************************************************
 * Private data
 ***************************************************************************/

/****************************************************************************
 * Public Functions
 ***************************************************************************/
// Consistent Overhead Byte Stuffing (COBS) is a scheme to take binary data
// replace an arbitrary byte value, usually 0x00, with an encoding that replaces
// this value, in a way, that allows the original data can be recovered while
// creating frames around the data.
//
// The following C# code was ported from a 'C' example licensed under MIT License
// https://github.com/bakercp/PacketSerial/blob/master/src/Encoding/COBS.h.
// After porting, I found errors. I referred to the original authors paper at
// http://conferences.sigcomm.org/sigcomm/1997/papers/p062.pdf for additional insights.
// Modifications were needed and adding a starting offset to support large buffers was
// added to allow sub-segments to be encoded.
//
// This function removes all delimiter values (usually 0x00)from the source buffer.
// It allows any length packet to be encoded. It adds at least 1 byte every 254 bytes,
// sometimes 1 more byte. Therefore. this algorithm is known as 'COBS' (Consistent
// Overhead Byte Stuffing), because the overhead is pretty consistent.
//
// To used this encoded packet, a delimiter must be added to the end of this encoded
// message by the caller. Also, while not always needed it can also be preceeded by
// the delimiter.

size_t hcom_host_cobs_encoder(uint8_t source[], size_t startingOffset,
          size_t length, uint8_t encoded[])
{
  size_t sourceOffset = startingOffset; // Offset to pre-encoded data buffer
  size_t encodedOffset = 1;             // Offset to encoded data buffer
  size_t replaceOffset = 0;             // Offset where 0 is being tracked
  uint8_t replacement = 1;              // Value that will be inserted to indicate 0 replaced

  while (sourceOffset < length + startingOffset)
  {
    // Is source value is the delimiter (0)?
    if (source[sourceOffset] == HCOM_PROTOCOL_COBS_DELIMITER)
    {
      encoded[replaceOffset] = replacement; // Replace '0' value with offset
      replaceOffset = encodedOffset++;      // Update replacement offset and bump encoded offset
      replacement = 1;                      // Reset replacement offset
    }
    else
    {
      encoded[encodedOffset++] = source[sourceOffset]; // Just copy original value
      replacement++;                        // Keep replacement offset right

      // 0xff is reserved for longer than 254 byte packets. If 0xff then
      // replace it with the offset.
      if (replacement == 0xff)
      {
        encoded[replaceOffset] = replacement;
        replaceOffset = encodedOffset++;
        replacement = 1;
      }
    }
    sourceOffset++; // Point to next source value
  }

  encoded[replaceOffset] = replacement;
  return encodedOffset; // Number of bytes written to result buffer
}

//-------------------------------------------------------------------------
// This function restores the removed 0x00s, thus returning the packet to it's
// original content.
// This function returns '0' if no terminating delimiter.
size_t hcom_host_cobs_decoder(uint8_t encoded[], size_t length, uint8_t decoded[])
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
      decoded[decodedOffset++] = HCOM_PROTOCOL_COBS_DELIMITER;
  }

  return decodedOffset;
}
