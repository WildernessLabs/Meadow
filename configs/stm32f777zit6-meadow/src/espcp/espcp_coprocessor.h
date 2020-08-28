/****************************************************************************
 * espcp_coprocessor.h
 *
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *   Author: Mark Stevens
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

#ifndef __ESPCP_COPROCESSOR_H
#define __ESPCP_COPROCESSOR_H

#pragma once

#include "../hcom/hcom_common.h"

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <arch/irq.h>

#include <sys/socket.h>
#include <nuttx/semaphore.h>
#include <nuttx/net/net.h>
#include <nuttx/net/usrsock.h>
#include <nuttx/pthread.h>
#include <nuttx/board.h>
#include <nuttx/config.h>
#include <arch/board/board.h>
#include <nuttx/spi/spi.h>
#include "stm32_spi.h"
#include "stm32_gpio.h"

#include "generic_list.h"
#include "espcp_wifi.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/*
 *  Name of the protected mode thread that deals with the ESP32 Coprocessor.
 */
#define ESPCP_THREAD_NAME   "EspcpThread"

/*
 *  Define the SPI and GPIO pins used for communication based upon the
 *  SPI interface being used.
 * 
 *  In production, the ESP32 code will be running on the ESP32 built into
 *  the Meadow board.
 * 
 *  In development it may be necessary to connect a logic analyser to the
 *  SPI bus and / or the GPIO pins signalling between the two chips.  In
 *  this case and ESP32 development board can be connected to the external
 *  SPI and GPIO pins.
 */
#ifdef CONFIG_MEADOW_ESPCP_USE_EXTERNAL_ESP32_BOARD

/*
 *  SPI interface used to communicate with the ESP32.
 */
#define ESP32CP_SPI_COMMS_PORT 3

/*
 *  The ESP32 will indicate when the SPI interface is ready to receive data.  The STM32
 *  should not send data until this line goes high.
 * 
 *  On the external interface this is PC7 (Meadow DO5).
 */
#define ESP32CP_SPI_READY_PIN_INPUT     (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN7)

/*
 *  Pin used to indicate that the ESP32 has completed a requested task and has
 *  a response ready for the STM32.
 * 
 *  On the external interface this is PC6 (Meadow D02).
 */
#define ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT   (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN6)

/*
 *  Chip select pin.
 * 
 *  On the external interface this is PH13 (Meadow D01).
 */
#define ESP32CP_SPI_CS_PIN_OUTPUT   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTH | GPIO_PIN13)

/*
 *  Reset pin.
 * 
 *  On the external interface this is PB9 (Meadow D04).
 */
#define ESP32CP_SPI_RESET_PIN_OUTPUT    (GPIO_OUTPUT | GPIO_PULLUP | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN9)

/*
 *  SPI operating speed.
 */

#define ESP32CP_SPI_COMMS_FREQUENCY 1000000 // 762 kHz

#else

/*
 *  SPI interface used to communicate with the ESP32.
 */
#define ESP32CP_SPI_COMMS_PORT 2

/*
 *  The ESP32 will indicate when the SPI interface is ready to receive data.  The STM32
 *  should not send data until this line goes high.
 * 
 *  On the internal interface this is PI10 (BOOT).
 */
#define ESP32CP_SPI_READY_PIN_INPUT     MEADOW_ESP32_ONBOARD_BOOT_PIN_INPUT

/*
 *  Pin used to indicate that the ESP32 has completed a requested task and has
 *  a response ready for the STM32.
 * 
 *  On the internal interface this is PB13 (UART0 RX)
 */
#define ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN13)

/*
 *  Chip select pin.
 * 
 *  On the internal interface this is PI2 (SPI CS)
 */
#define ESP32CP_SPI_CS_PIN_OUTPUT    (GPIO_OUTPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTI | GPIO_PIN0)

/*
 *  Reset Pin
 * 
 *  On the internal interface this is PF7 (Enable - Reset) and will have already been defined 
 *  in the HCOM interface files.
 */
#define ESP32CP_SPI_RESET_PIN_OUTPUT    MEADOW_ESP32_ONBOARD_RESET_PIN_OUTPUT

/*
 *  SPI operating speed.
 */
#define ESP32CP_SPI_COMMS_FREQUENCY 1000000 //04000000UL

#endif /* CONFIG_MEADOW_ESP32CP_USE_EXTERNAL_ESP32_BOARD */


/****************************************************************************
 * Public Types
 ****************************************************************************/

/*
 *  Type definition for the function that will send data to the ESP32.
 */
typedef void (*espcp_send_data_function_t)(void *, void *, size_t);

/*
 *  Configuration information for the ESP32 coprocessor.
 */
struct espcp_configuration_s
{
  /*
   *  Indicates if the thread processing the messages for the ESP32
   *  is running.
   */
  bool thread_running;

  /*
   *  ID of the thread processing the messages for the ESP32.
   */
  pthread_t thread;

  /*
   *  ID of the queue of messages that are waiting to be sent to the ESP32.
   */
  mqd_t request_queue;

  /*
   *  List of messages that have been sent to the ESP where a response is
   *  pending.
   */
  gl_linked_list_t messages_pending_response;

  /*
   *  Exit code for the thread processing the messages for the ESP32.
   */
  int exit_code;

  /*
   *  Method that will send / receive data to / from the ESP32.
   */
  espcp_send_data_function_t send_data_to_esp32;

  /*
   *  Size of a buffer required to hold a headers worth of data.  This is a
   *  frequently used value so we store it here rather than recalculate it
   *  every time it is needed.
   */
  uint32_t header_only_buffer_size;

  /*
   *  Pointer to a buffer that can take a header (and only a header) worth of data.
   */
  uint8_t *header;
};
typedef struct espcp_configuration_s espcp_configuration_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
int espcp_init(void);
espcp_configuration_t *espcp_get_default_configuration(void);
int espcp_spi_setup(xcpt_t);
void espcp_send_data_over_spi(void *, void *, size_t);
espcp_configuration_t *espcp_get_configuration(void);
void espcp_reset(void);

#endif /* __ESPCP_COPROCESSOR_H */