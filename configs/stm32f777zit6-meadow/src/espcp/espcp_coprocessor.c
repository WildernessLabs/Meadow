/****************************************************************************
 * espcp_coprocessor.c
 *
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *   Author: Mark Stevens
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must resultain the above copyright
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

#ifdef CONFIG_MEADOW_ESPCP_USE_EXTERNAL_ESP32_BOARD

#warning "Configuring for external ESP32 board"

#endif

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdio.h>
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
#include <nuttx/mqueue.h>

#include "espcp_coprocessor.h"
#include "espcp_queue.h"
#include "espcp_message.h"
#include "espcp_message_dispatcher.h"
#include "espcp_thread.h"
#include "espcp_encoders.h"
#include "espcp_posix.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/*
 *  Object holding the configuration of the ESP32 coprocessor system.
 */
static espcp_configuration_t *g_espcp_configuration; 

/*
 *  Object holding the SPI configuration.
 */
static struct spi_dev_s *g_esp_spi_dev;

/*
 *  Static pointer to the name of the file being compiled.  This is used for
 *  logging and making it a static variable ensure that one one instance exists.
 */
static char *_thisFile = __FILE__;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_get_default_configuration
 *
 * Description:
 *  Get the default configuration object for the ESP32 communication system.
 *
 * Input Parameters:
 *  None
 *
 * returned Value:
 *  Pointer to a valid ESP32 configuration object, NULL if there was a problem
 *  allocating memory.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_configuration_t *espcp_get_default_configuration(void)
{
  espcp_configuration_t *config = (espcp_configuration_t *) malloc(sizeof(espcp_configuration_t));
  if (config != NULL)
  {
    memset(config, 0, sizeof(espcp_configuration_t));
    config->thread_running = false;
    config->send_data_to_esp32 = espcp_send_data_over_spi;
    config->header_only_buffer_size = espcp_calculate_spi_buffer_size(ESPCP_MESSAGE_HEADER_SIZE);
    config->header = (uint8_t *) malloc(config->header_only_buffer_size);
    if (config->header == NULL)
    {
      free(config);
      config = NULL;
    }
  }
  return(config);
}

/****************************************************************************
 * Name: espcp_get_default_configuration
 *
 * Description:
 *  Get the default configuration object for the ESP32 communication system.
 *
 * Input Parameters:
 *  queue_send_response_message_function - The address of the function that
 *    will add a "send data" message to the message queue.
 *
 * returned Value:
 *  Pointer to a valid ESP32 configuration object, NULL if there was a problem
 *  allocating memory.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_spi_setup(xcpt_t queue_send_response_message_function)
{
  int result;

  /*
   *  Two output pins are used, on for the STM32 SPI CS pin and one to allow the ESP32 to be reset.
   */
  result = stm32_configgpio(ESP32CP_SPI_RESET_PIN_OUTPUT);
  if (result < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-Config GPIO failed result:%d\n", _thisFile, __LINE__, result);
    return -1;
  }
  stm32_gpiowrite(ESP32CP_SPI_RESET_PIN_OUTPUT, false);  /* Set the pin low to prevent the ESp32 from running. */

  result = stm32_configgpio(ESP32CP_SPI_CS_PIN_OUTPUT);
  if (result < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-Config GPIO failed result:%d\n", _thisFile, __LINE__, result);
    return -1;
  }
  stm32_gpiowrite(ESP32CP_SPI_CS_PIN_OUTPUT, true);    // SPI CS is active low so deselect SPI.

  /*
   *  Input pins, one indicating that ESP32 SPI transaction can start (ESP32 SPI interface is ready)
   *  and one to indicate that the ESP32 has a response to a request from the STM32 or some other
   *  event has ocurred that the STM32 should be aware of.
   */
  result = stm32_configgpio(ESP32CP_SPI_READY_PIN_INPUT);
  if(result < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-Config GPIO failed result:%d\n", _thisFile, __LINE__, result);
    return -1;
  }

  result = stm32_configgpio(ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT);
  if(result < 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-Config GPIO failed result:%d\n", _thisFile, __LINE__, result);
    return -1;
  }

  g_esp_spi_dev = stm32_spibus_initialize(ESP32CP_SPI_COMMS_PORT);
  if (g_esp_spi_dev == 0)
  {
    hcom_utils_f7syslog(LOG_CRIT, "%s@%d-Error:Failed init esp32 SPI port %d\n", _thisFile, __LINE__, result);
    return -1;
  }

  SPI_SETFREQUENCY(g_esp_spi_dev, ESP32CP_SPI_COMMS_FREQUENCY);
  SPI_SETBITS(g_esp_spi_dev, 8);
  SPI_SETMODE(g_esp_spi_dev, SPIDEV_MODE3); /* CPOL=1 CHPHA=1 */

  /*
   *  ESP32 can now be allowed to start executing the coprocessor code,
   *  so set the reset line to high and then release it as it is no longer
   *  needed.
   */
  stm32_gpiowrite(ESP32CP_SPI_RESET_PIN_OUTPUT, true);
  stm32_unconfiggpio(ESP32CP_SPI_RESET_PIN_OUTPUT);

  /*
   *  We must wait until both the ESP SPI ready and the message waiting
   *  lines are low before attaching the message waiting interrupt.  If
   *  we don't then we will get a false interrupt raised.
   */
  bool waiting = true;
  while (waiting)
  {
    waiting = stm32_gpioread(ESP32CP_SPI_READY_PIN_INPUT);
    waiting |= stm32_gpioread(ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT);
  }
  stm32_gpiosetevent(ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT,
     /*risingedge=*/false, /*fallingedge=*/ true, true, queue_send_response_message_function, 0);

  return OK;
}

/****************************************************************************
 * Name: espcp_wait_for_message_waiting_signal
 *
 * Description:
 *  Wait for the message waiting pin to go high and then go low.
 *
 *  This is intended to allow testing of messages that require a response
 *  without having to use the message queue.
 * 
 *  This method facilitates testing and is not intended for use in the
 *  production OS.
 *
 * Returned Value:
 *  none.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_wait_for_message_waiting_signal(void)
{
  while (!stm32_gpioread(ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT))
  {
    usleep(500);
  }
  while (stm32_gpioread(ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT))
  {
    usleep(500);
  }
}

/****************************************************************************
 * Name: espcp_send_data_over_spi
 *
 * Description:
 *  Send, receive or exchange data with the ESP32.
 * 
 *  If tx is NULL then it is assumed that this is a receive only operation.
 *  If rx is NULL then it is assumed that this is a transmit only operation.
 *  If tx and rx are not NULL then an exchange is assumed.
 *  tx and rx both NULL is a no operation.
 *
 * Input Parameters:
 *  tx - Buffer of bytes to send to the ESP32.
 *  rx - buffer to hold bytes to be received from the ESP32.
 *  buffer_length - number of bytes to be sent / received (or both).
 *
 * Returned Value:
 *  none.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_send_data_over_spi(void *tx, void *rx, size_t buffer_length)
{
  if ((tx == NULL) && (rx == NULL))
  {
    return;
  }
  stm32_gpiowrite(ESP32CP_SPI_CS_PIN_OUTPUT, false);

  /*
   *  The ESP takes some time to initialise the SPI interface. A low signal
   *  on the SPI ready line indicates that it is still preparing the interface.
   *  The line will go high when it is ready to communicate.
   * 
   *  We could do this with a sempahore / interrupt etc but the initial version
   *  uses a loop for simplicity and also because the ESP should respond in a
   *  short time period so impact should be low.
   */
  while (!stm32_gpioread(ESP32CP_SPI_READY_PIN_INPUT));

  if (tx == NULL)
  {
    SPI_RECVBLOCK(g_esp_spi_dev, rx, buffer_length);
  }
  else
  {
    if (rx == NULL)
    {
      SPI_SNDBLOCK(g_esp_spi_dev, tx, buffer_length);
    }
    else
    {
      SPI_EXCHANGE(g_esp_spi_dev, tx, rx, buffer_length);
    }
  }

  stm32_gpiowrite(ESP32CP_SPI_CS_PIN_OUTPUT, true);
  /*
   *  There is a small delay between the end of message transmission and
   *  the ESP dropping the SPI Ready line in the post SPI callback.  This
   *  can (in certain circumstances) mean the STM starts the next transaction
   *  before the ESP has completed the current transaction.
   */
  while (stm32_gpioread(ESP32CP_SPI_READY_PIN_INPUT));
}

/****************************************************************************
 * Name: espcp_get_configuration
 *
 * Description:
 *  Get teh pointer to the current configuration of the ESP32 coprocessor.
 *
 * Input Parameters:
 *  None
 *
 * returned Value:
 *  Pointer to the current ESP32 configuration object.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_configuration_t *espcp_get_configuration(void)
{
  return g_espcp_configuration;
}

/****************************************************************************
 * Name: espcp_init
 *
 * Description:
 *  Initialise the ESP32 coprocessor system
 *
 * Input Parameters:
 *  None
 *
 * returned Value:
 *  Result of starting the thread or -1 if there is an error.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_init(void)
{
  int result = OK;

  g_espcp_configuration = espcp_get_default_configuration();
  if (g_espcp_configuration != NULL)
  {
    espcp_spi_setup(espcp_queue_send_response_message);
    /*
    *  The message queue must be created before the message handler thread as the
    *  message processor will wait on the message queue looking for messages.
    */
    g_espcp_configuration->request_queue = espcp_create_message_queue(ESPCP_MESSAGE_QUEUE_NAME);
    if (g_espcp_configuration->request_queue < 0)
    {
      return(-1);
    }
    espcp_setup_message_dispatcher();
    espcp_posix_network_init();

    /*
    *  We should now be good to start the thread.
    */
    result = espcp_thread_start(g_espcp_configuration);
  }
  else
  {
    result = -1;
  }

  return result;
}

