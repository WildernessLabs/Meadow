/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/meadow-upd.h
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
#ifndef __CONFIGS_MEADOW_SRC_MEADOW_UPD__H
#define __CONFIGS_MEADOW_SRC_MEADOW_UPD__H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>
#include <nuttx/mqueue.h>
#include "espcp/espcp_encoders.h"

#define QUEUE_NAME          "/mdw_int"
#define QUEUE_MSG_SIZE      2
#define QUEUE_MAX_MSGS      16

extern mqd_t s_int_queue;

struct upd_gpio_int_config
{
  // Must match ...\Meadow\Meadow.Core\source\Meadow.Core\Interop\Interop.upd.cs
  uint32_t port;                // 0 - 15 (A-K)
  uint32_t pin;                 // 0 - 15
  uint32_t enable;              // 1 = enable
  uint32_t risingEdge;          // 1 = enable
  uint32_t fallingEdge;         // 1 = enable
  uint32_t resistorMode;        // 0 = float, 1 = pull up, 2 = pull down
  uint32_t debounceDuration;    // millisec * 10
  uint32_t glitchDuration;      // millisec * 10
};

/*
 *  Information about the function that should be requested to
 *  be performed by the ESP32.
 */
struct upd_esp32_command
{
  uint8_t interface;          // Interface (WiFi, System etc.) to perform the request.
  uint32_t function;          // Function number to be executed.
  uint32_t status_code;       // Status code returned by the ESP32.
  uint8_t *payload;           // Pointer to the data required by the function.
  uint32_t payload_length;    // Length of the data block.
  uint8_t *result;            // Pointer to the result.
  uint32_t result_length;     // Length of the result data block.
  uint8_t block;              // Is this a blocking call?
};
typedef struct upd_esp32_command upd_esp32_command_t;

/*
 *  Information about the event data being requested.
 */
struct upd_event_data_request
{
  uint32_t message_address;   // Pointer to the message generating the event.
  uint32_t status_code;       // Status code returned by the ESP32.
  uint8_t *payload;           // Pointer to the data required by the function.
  uint32_t payload_length;    // Length of the data block.
};

/*
 *  Information about the read or write configuration value request
 */
struct upd_get_set_configuration_value_s
{
  int32_t item;                   // Item number to be read or written.
  uint8_t direction;              // GRead or write the value, 1 = read, 0 = write.
  int32_t buffer_length;          // Size of the buffer available for the request.
  uint8_t *buffer;                // Buffer holding the data or to be used to hold the result.
  int32_t returned_data_length;   // Amount of data returned (string or byte data).
};
typedef struct upd_get_set_configuration_value_s upd_get_set_configuration_value_t;

// in meadow-upd-interrupt.c called from meadow-upd.c
int upd_config_interrupt(struct upd_gpio_int_config* cfg);
int upd_handle_esp32_command(struct upd_esp32_command *);
int upd_handle_esp32_get_event_result(espcp_event_data_payload_t *);


#endif  // __CONFIGS_MEADOW_SRC_MEADOW_UPD__H