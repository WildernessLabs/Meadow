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

#include <nuttx/config.h>
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
#include <nuttx/board.h>
#include <nuttx/mqueue.h>
#include <arch/board/board.h>
#include <nuttx/spi/spi.h>
#include "stm32_spi.h"
#include "stm32_gpio.h"

#include "generic_list.h"
#include "espcp_wifi.h"
#include "espcp_encoders.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/*
 *  Name of the protected mode thread that deals with the ESP32 Coprocessor.
 */
#define ESPCP_THREAD_NAME "EspcpThread"

#define ESPCP_EVENT_HANDLER_THREAD_NAME "EspcpEventHandler"

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
#define ESP32CP_SPI_READY_PIN_INPUT (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN7)

/*
 *  Pin used to indicate that the ESP32 has completed a requested task and has
 *  a response ready for the STM32.
 * 
 *  On the external interface this is PC6 (Meadow D02).
 */
#define ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN6)

/*
 *  Chip select pin.
 * 
 *  On the external interface this is PH13 (Meadow D01).
 */
#define ESP32CP_SPI_CS_PIN_OUTPUT (GPIO_OUTPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTH | GPIO_PIN13)

/*
 *  Reset pin.
 * 
 *  On the external interface this is PB9 (Meadow D04).
 */
#define ESP32CP_RESET_PIN_OUTPUT (GPIO_OUTPUT | GPIO_PULLUP | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN9)

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
#define ESP32CP_SPI_READY_PIN_INPUT (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTI | GPIO_PIN10)
#define ESP32CP_BOOT_PIN_OUTPUT (GPIO_OUTPUT | GPIO_FLOAT | GPIO_OPENDRAIN | GPIO_SPEED_100MHz | GPIO_PORTI | GPIO_PIN10)

/*
 *  Pin used to indicate that the ESP32 has completed a requested task and has
 *  a response ready for the STM32.
 * 
 *  On the internal interface this is PB13 on F7V1 and PC12 on F7V2 and it is connected to ESP UART0 RX.
 */
#define ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT_F7V1 (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN13)
#define ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT_F7V2 (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN12)

/*
 *  Chip select pin.
 * 
 *  On the internal interface this is PI2 (SPI CS).
 */
#define ESP32CP_SPI_CS_PIN_OUTPUT (GPIO_OUTPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTI | GPIO_PIN0)

/*
 *  Reset Pin
 * 
 *  On the internal interface this is PF7 (Enable - Reset).
 */
#define ESP32CP_RESET_PIN_OUTPUT (GPIO_OUTPUT | GPIO_FLOAT | GPIO_OPENDRAIN | GPIO_SPEED_100MHz | GPIO_PORTF | GPIO_PIN7)

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
     *  Semaphore indicating that the configuration is being read or updated.
     *  This is used as a mutex through the espcp_config_lock and
     *  espcp_config_unlock methods.
     */
    sem_t lock;

    /*
     *  Indicates if the thread processing the messages for the ESP32
     *  is running.
     */
    bool thread_running;

    /**
     *  @brief Indicate if the incoming event handler thread is running.
     *         This is used to make sure we do not start the event handler
     *         thread more than once.
     */
    bool incoming_event_handler_thread_running;

    /*
     *  ID of the thread processing the messages for the ESP32.
     */
#ifdef CONFIG_BUILD_PROTECTED
    int thread;
#else
    pthread_t thread;
#endif

    /*
     *  ID of the thread processing the messages for the ESP32.
     */
#ifdef CONFIG_BUILD_PROTECTED
    int incoming_event_thread;
#else
    pthread_t incoming_event_thread;
#endif

    /*
     *  ID of the queue of messages that are waiting to be sent to the ESP32.
     */
    mqd_t request_queue;

    /*
     *  ID of the queue of messages that are waiting to be sent to the ESP32.
     */
    mqd_t managed_event_queue;

    /**
     *  @brief Event queue to hold the events generated by the ESP32 in order that
     *         they can be processed by the event thread.
     */
    mqd_t incoming_event_queue;

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
     *  Indicate if the initialisation detected the ESP32 as running normally.
     */
    bool esp_not_responding;

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

    /**
     * @brief Default gateway from the ESP32.
     */
    uint32_t default_gateway;

    /**
     *  Pointer to the buffer to be used to receive data from the ESP32.
     */
    uint8_t *spi_rx_buffer;

    /**
     *  Pointer to the buffer to be used to send data to the ESP32.
     */
    uint8_t *spi_tx_buffer;
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
void espcp_lock_spi_interface(void);
void espcp_release_spi_interface(void);
int espcp_init(void);
espcp_configuration_t *espcp_get_default_configuration(void);
int espcp_spi_setup(void);
void espcp_send_data_over_spi(void *, void *, size_t);
espcp_configuration_t *espcp_get_configuration(void);
bool espcp_should_reset_at_startup(void);
void espcp_hold_in_reset(void);
void espcp_reset(void);
void espcp_enter_programming_mode(void);
void espcp_config_lock(void);
void espcp_config_unlock(void);
void espcp_release_shared_gpio(void);
int espcp_enter_run_mode(void);
int espcp_spi_ready(int, void *, void *);

#endif /* __ESPCP_COPROCESSOR_H */