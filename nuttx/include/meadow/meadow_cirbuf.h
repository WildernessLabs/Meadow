/****************************************************************************
 * nuttx\include\meadow\meadow_cirbuf.h
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

#ifndef __CONFIGS_MEADOW_SRC_MEADOW_CIRCULAR_BUFFER__H
#define __CONFIGS_MEADOW_SRC_MEADOW_CIRCULAR_BUFFER__H

#include <nuttx/config.h>

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct host_com_cir_buffer_s
{
  uint8_t *bottom;    // bottom of buffer
  uint8_t *top;       // top end of buffer
  uint8_t *head;      // add data here
  uint8_t *tail;      // remove from here
  uint8_t delimiter;  // custom message delimiter
}host_com_cir_buffer_t;

// Circular buffer return values
enum hcom_comms_recv_buffer_return
{
  HCOM_CIR_BUF_INIT_OK,
  HCOM_CIR_BUF_ALLOC_FAILED,

  HCOM_CIR_BUF_ADD_SUCCESS,
  HCOM_CIR_BUF_ADD_WONT_FIT,
  HCOM_CIR_BUF_ADD_BAD_ARG,

  HCOM_CIR_BUF_GET_FOUND_MSG,
  HCOM_CIR_BUF_GET_NONE_FOUND,
  HCOM_CIR_BUF_GET_DEST_NO_ROOM
};

int hcom_cirbuf_init(host_com_cir_buffer_t *hcom_cbuf, size_t totalCapacity,
                      uint8_t delimiter);
size_t hcom_cirbuf_avail_space(host_com_cir_buffer_t *hcom_cbuf);
int hcom_cirbuf_add_bytes(host_com_cir_buffer_t *hcom_cbuf, uint8_t *newBytes,
                      uint32_t bytesToAdd);
int hcom_cirbuf_get_next_packet(host_com_cir_buffer_t *hcom_cbuf, uint8_t *packetBuffer,
                                size_t packetBufferSize, size_t *packetLength);
int hcom_cirbuf_release_memory(host_com_cir_buffer_t *hcom_cbuf);
int hcom_cirbuf_clear_buffer(host_com_cir_buffer_t *hcbuf);

#endif  // __CONFIGS_MEADOW_SRC_MEADOW_CIRCULAR_BUFFER__H