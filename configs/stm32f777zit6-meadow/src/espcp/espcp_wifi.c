/****************************************************************************
 * espcp_wifi.c
 *
 *  Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *  Author: Mark Stevens
 * 
 *  Methods dealing with direct control of the WiFi functionality of the
 *  ESP32 coprocessor.
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
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/semaphore.h>
#include <nuttx/pthread.h>

#include "espcp_wifi.h"
#include "espcp_shared_enums.h"
#include "espcp_message.h"
#include "espcp_encoders.h"
#include "espcp_system.h"
#include "espcp_common.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data / Variables
 ****************************************************************************/

/****************************************************************************
 * Function Implementation
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_start_wifi
 *
 * Description:
 *  Log on to the specified network if it is available.
 *
 * Input Parameters:
 *  network_name - Name of the network to connect to.
 *  password - Password for the specified network.
 *
 * Returned Value:
 *  Pointer to a structure holding the network information, NULL if there
 *  is a problem.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_ip_information_t *espcp_start_wifi(char *network_name, char *password)
{
  espcp_ip_information_t *result = NULL;
  // espcp_configuration_t *configuration = espcp_get_configuration();

  espcp_wi_fi_credentials_t *credentials = (espcp_wi_fi_credentials_t *) malloc(sizeof(espcp_wi_fi_credentials_t));
  if (credentials == NULL)
  {
      return(NULL);
  }
  credentials->network_name = network_name;
  credentials->password = password;
  uint32_t payload_length = espcp_wi_fi_credentials_buffer_size(credentials);
  uint8_t *payload = (uint8_t *) malloc(payload_length);
  if (payload == NULL)
  {
    free(credentials);
    return(NULL);
  }
  espcp_encode_wi_fi_credentials(credentials, payload);
  free(credentials);

  espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header, espcp_esp32_interfaces_wi_fi, 
        espcp_wi_fi_function_start, espcp_status_codes_completed_ok,
        espcp_get_next_message_id(), payload, payload_length);

  if (message == NULL)
  {
      free(payload);
      return(NULL);
  }

  if (espcp_queue_message_and_wait(message) == espcp_status_codes_completed_ok)
  {
    result = (espcp_ip_information_t *) malloc(sizeof(espcp_ip_information_t));
    if (result != NULL)
    {
        memset(result, 0, sizeof(espcp_ip_information_t));
        memcpy(result->ip, message->payload, 4);
    }
  }

  espcp_delete_message_and_payload(message);
  return(result);
}
