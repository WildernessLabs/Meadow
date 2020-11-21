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
#include <nuttx/mqueue.h>
#include <nuttx/config.h>

#include "../inicfg/meadow_inicfg.h"
#include "espcp_coprocessor.h"
#include "espcp_queue.h"
#include "espcp_message.h"
#include "espcp_message_dispatcher.h"
#include "espcp_thread.h"
#include "espcp_encoders.h"
#include "espcp_posix.h"
#include "espcp_usrsock.h"
#include "espcp_system.h"

#ifdef CONFIG_BUILD_PROTECTED

#include <nuttx/pthread.h>

#endif

#ifdef CONFIG_MEADOW_ESPCP_USE_EXTERNAL_ESP32_BOARD

#error "Using external ESP32 development board."

#endif

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
static espcp_configuration_t *g_espcp_configuration = NULL;

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
 * Returned Value:
 *  Pointer to a valid ESP32 configuration object, NULL if there was a problem
 *  allocating memory.
 *
 * Assumptions/Limitations:
 *  This is only called once and so we do not need to lock the configuration
 *  object.
 *
 ****************************************************************************/
espcp_configuration_t *espcp_get_default_configuration(void)
{
    espcp_configuration_t *config = (espcp_configuration_t *)malloc(sizeof(espcp_configuration_t));
    if (config != NULL)
    {
        memset(config, 0, sizeof(espcp_configuration_t));
        sem_init(&config->lock, 0, 1);
        sem_setprotocol(&config->lock, SEM_PRIO_NONE);
        sem_init(&config->spi_lock, 0, 1);
        sem_setprotocol(&config->spi_lock, SEM_PRIO_NONE);
        config->thread_running = false;
        config->esp_not_responding = true;
        config->send_data_to_esp32 = espcp_send_data_over_spi;
        config->header_only_buffer_size = espcp_calculate_spi_buffer_size(ESPCP_MESSAGE_HEADER_SIZE);
        int reset = meadow_ini_cfg_get_int(NULL, "startup", "ResetEsp32AtStartup");
        config->reset_esp_at_startup = (reset == 1) || (reset < 0);
        config->header = (uint8_t *)malloc(config->header_only_buffer_size);
        if (config->header == NULL)
        {
            free(config);
            config = NULL;
        }
        config->esp_config = NULL;
    }
    return (config);
}

/****************************************************************************
 * Name: espcp_config_lock
 *
 * Description:
 *  Lock the specified configuration object.
 *
 * Input Parameters:
 *  config - Pointer to an espcp_configuration_t object to be locked.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_config_lock(espcp_configuration_t *config)
{
    sem_wait(&config->lock);
}

/****************************************************************************
 * Name: espcp_config_unlock
 *
 * Description:
 *  Unlock the specified configuration object.
 *
 * Input Parameters:
 *  config - Pointer to an espcp_configuration_t object to be locked.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_config_unlock(espcp_configuration_t *config)
{
    sem_post(&config->lock);
}

/****************************************************************************
 * Name: espcp_spi_setup
 *
 * Description:
 *  Set up the STM32 SPI interface for comms with the ESP32.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK if successful, -1 if not.
 *
 * Assumptions/Limitations:
 *  g_espcp_configuration is setup prior to calling this method.
 *
 ****************************************************************************/
int espcp_spi_setup()
{
    int result;

    result = stm32_configgpio(ESP32CP_SPI_CS_PIN_OUTPUT);
    if (result < 0)
    {
        syslog(LOG_CRIT, "%s@%d Config SPI CS GPIO failed result:%d\n", _thisFile, __LINE__, result);
        return (-1);
    }
    stm32_gpiowrite(ESP32CP_SPI_CS_PIN_OUTPUT, true); // SPI CS is active low so deselect SPI.

    g_esp_spi_dev = stm32_spibus_initialize(ESP32CP_SPI_COMMS_PORT);
    if (g_esp_spi_dev == 0)
    {
        syslog(LOG_CRIT, "%s@%d Error:Failed init ESP32 SPI port %d\n", _thisFile, __LINE__, result);
        return (-1);
    }

    SPI_SETFREQUENCY(g_esp_spi_dev, ESP32CP_SPI_COMMS_FREQUENCY);
    SPI_SETBITS(g_esp_spi_dev, 8);
    SPI_SETMODE(g_esp_spi_dev, SPIDEV_MODE3); /* CPOL=1 CHPHA=1 */

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
    while (!stm32_gpioread(ESP32CP_SPI_READY_PIN_INPUT))
        ;

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
 * Name: espcp_should_reset_at_startup
 *
 * Description:
 *  Check the system configuration to determine if the ESP should be reset
 *  at startup.
 * 
 *  The default configuration for this is to force a reset.  Debugging often
 *  requires the ESP to have a debugger attached when the STM restarts which
 *  would then force the ESP to be reset and the debugger to be detacted.
 *  The configuration allows this to be overridden to allow debugging to
 *  continue.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  none.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
bool espcp_should_reset_at_startup(void)
{
    bool perform_reset = true;

    espcp_configuration_t *config = espcp_get_configuration();
    if (config != NULL)
    {
        espcp_config_lock(config);
        perform_reset = config->reset_esp_at_startup;
        espcp_config_unlock(config);
    }
    return (perform_reset);
}

/****************************************************************************
 * Name: espcp_reset
 *
 * Description:
 *  Reset the ESP32.
 * 
 *  Note that this method checks the configuration to determine if the ESP
 *  should be reset at startup.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  none.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void espcp_reset(void)
{
    if (espcp_should_reset_at_startup())
    {
        espcp_hold_in_reset();
        usleep(10000);
        stm32_gpiowrite(ESP32CP_RESET_PIN_OUTPUT, true);
        stm32_unconfiggpio(ESP32CP_RESET_PIN_OUTPUT);
    }
}

/****************************************************************************
 * Name: escpcp_hold_in_reset
 *
 * Description:
 *  Configure the ESP reset pin and take the reset line low.
 * 
 *  This method can be used in two ways:
 *    - Hold the chip in reset
 *    - Start the reset process.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  none.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_hold_in_reset(void)
{
    if (espcp_should_reset_at_startup())
    {
        int result = stm32_configgpio(ESP32CP_RESET_PIN_OUTPUT);
        if (result < 0)
        {
            syslog(LOG_CRIT, "%s@%d Config Reset GPIO failed result:%d\n", _thisFile, __LINE__, result);
            return;
        }
        stm32_gpiowrite(ESP32CP_RESET_PIN_OUTPUT, false);
    }
}

/****************************************************************************
 * Name: espcp_release_shared_gpio
 *
 * Description:
 *  Unconfigure the GPIO pins that are used for signalling between the ESP
 *  and STM chips.
 * 
 *  This method is necessary as the message waiting and SPI interface ready
 *  signals are used for programming the ESP.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  none.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
void espcp_release_shared_gpio(void)
{
    int result = stm32_gpiosetevent(ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT, /*risingedge=*/false, /*fallingedge=*/false, true, NULL, 0);
    if (result < 0)
    {
        syslog(LOG_CRIT, "%s@%d Disabling Message Waiting interrupt result:%d\n", _thisFile, __LINE__, result);
        return;
    }
    result = stm32_gpiosetevent(ESP32CP_SPI_READY_PIN_INPUT, /*risingedge=*/false, /*fallingedge=*/false, true, NULL, 0);
    if (result < 0)
    {
        syslog(LOG_CRIT, "%s@%d Disabling SPI Ready interrupt result:%d\n", _thisFile, __LINE__, result);
        return;
    }
    stm32_unconfiggpio(ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT);
    stm32_unconfiggpio(ESP32CP_SPI_READY_PIN_INPUT);
    stm32_unconfiggpio(ESP32CP_BOOT_PIN_OUTPUT);
    stm32_unconfiggpio(GPIO_UART5_TX);
    stm32_unconfiggpio(GPIO_UART5_RX);
}

/****************************************************************************
 * Name: espcp_enter_programming_mode
 *
 * Description:
 *  Put the ESP32 into programming mode.  This is done by pulling the boot pin
 *  low, resetting the ESP and then pulling the boot pin high.  The boot pin
 *  will be left unconfigured at the end of this process.
 * 
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  none.
 *
 * Assumptions/Limitations:
 *  A system (STM) reset will need to be performed in order for the comms
 *  with the ESP to be reinstated.
 *
 ****************************************************************************/
void espcp_enter_programming_mode(void)
{
    espcp_release_shared_gpio();
    //
    //  Now reconfigure the needed resources.
    //
    stm32_configgpio(GPIO_UART5_TX);
    stm32_configgpio(GPIO_UART5_RX);
    int result = stm32_configgpio(ESP32CP_BOOT_PIN_OUTPUT);
    if (result < 0)
    {
        syslog(LOG_CRIT, "%s@%d Config Boot pin as output result:%d\n", _thisFile, __LINE__, result);
        return;
    }
    usleep(20 * 1000);
    stm32_gpiowrite(ESP32CP_BOOT_PIN_OUTPUT, false);
    espcp_reset();
    usleep(20 * 1000);
    stm32_gpiowrite(ESP32CP_BOOT_PIN_OUTPUT, true);
}

/****************************************************************************
 * Name: espcp_enter_run_mode
 *
 * Description:
 *  Reset the ESP and enter run mode.
 *
 * Input Parameters:
 *  None
 *
 * Returned Value:
 *  OK on success, -1 on failure.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int espcp_enter_run_mode(void)
{
    espcp_release_shared_gpio();
    //
    //  Input pins, one indicating that ESP32 SPI transaction can start (ESP32 SPI interface is ready)
    //  and one to indicate that the ESP32 has a response to a request from the STM32 or some other
    //  event has ocurred that the STM32 should be aware of.
    //
    int result = stm32_configgpio(ESP32CP_SPI_READY_PIN_INPUT);
    if (result < 0)
    {
        syslog(LOG_CRIT, "%s@%d Config SPI Ready pin failed result: %d\n", _thisFile, __LINE__, result);
        return -1;
    }

    result = stm32_configgpio(ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT);
    if (result < 0)
    {
        syslog(LOG_CRIT, "%s@%d Config Message Waiting pin failed result: %d\n", _thisFile, __LINE__, result);
        return -1;
    }
    //
    //  Lock the SPI interface so that the thread processing the message cannot continue
    //  until the SPI interface and signalling has entered a known good configuration.
    //
    espcp_configuration_t *config = espcp_get_configuration();
    espcp_config_lock(config);
    sem_wait(&config->spi_lock);
    espcp_config_unlock(config);
    //
    //  We reset the chip and then pause for 1 ms to let the ESP lines stabilise before we attach
    //  the interrupt handlers.
    //
    espcp_reset();
    usleep(1000);
    stm32_gpiosetevent(ESP32CP_SPI_READY_PIN_INPUT, /*risingedge=*/true, /*fallingedge=*/false, true, espcp_spi_ready, 0);

    return (OK);
}

/****************************************************************************
 * Name: espcp_spi_ready
 *
 * Description:
 *  Interrupt generated when the ESP has generated the SPI interface ready
 *  signal.
 * 
 * Input Parameters:
 *  irq
 *  context
 *  arg
 *
 * Returned Value:
 *  OK.
 *
 * Assumptions/Limitations:
 *  This method must be quick as it is intended to be called from an
 *  interrupt handler.
 *
 ****************************************************************************/
int espcp_spi_ready(int irq, void *context, void *arg)
{
    //
    //  We are only interested in the first interrupt on this pin.  It indicates
    //  the completion of the reset process and the ESP has completed the
    //  initialisation of the SPI interface and can now receive messages.
    //
    int result = stm32_gpiosetevent(ESP32CP_SPI_READY_PIN_INPUT, /*risingedge=*/false, /*fallingedge=*/false, true, NULL, 0);
    if (result < 0)
    {
        syslog(LOG_CRIT, "%s@%d Disabling SPI Ready interrupt result:%d\n", _thisFile, __LINE__, result);
        return (-1);
    }
    //
    //  Once the SPI interface has indicated that it is ready (on the ESP) then
    //  we need to setup the Message Waiting interrupt.
    //
    stm32_gpiosetevent(ESP32CP_SPI_MESSAGE_WAITING_PIN_INPUT, /*risingedge=*/false, /*fallingedge=*/true, true, espcp_queue_send_response_message, 0);

    sem_post(&g_espcp_configuration->spi_lock);
    g_espcp_configuration->esp_not_responding = false;
    return (OK);
}

/****************************************************************************
 * Name: espcp_get_configuration
 *
 * Description:
 *  Get a pointer to the current configuration of the ESP32 coprocessor.
 *
 * Input Parameters:
 *  None
 *
 * Returned Value:
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
 *  Name: espcp_init
 *
 *  Description:
 *      Initialise the ESP32 coprocessor system
 *
 *  Input Parameters:
 *      None
 *
 *  Returned Value:
 *      Result of starting the thread or -ENETDOWN if there is an error.
 *
 *  Assumptions/Limitations:
 *      None
 *
 ****************************************************************************/
int espcp_init(void)
{
    int result = OK;

    g_espcp_configuration = espcp_get_default_configuration();
    if (g_espcp_configuration != NULL)
    {
        /*
         *  The message queue must be created before the message handler thread as the
         *  message processor will wait on the message queue looking for messages.
         */
        mqd_t queue_id = espcp_create_message_queue(ESPCP_MESSAGE_QUEUE_NAME);
        espcp_config_lock(g_espcp_configuration);
        g_espcp_configuration->request_queue = queue_id;
        espcp_config_unlock(g_espcp_configuration);

        if (queue_id < 0)
        {
            syslog(LOG_CRIT, "%s@%d Error creating ESP32 message queue result: %d\n", _thisFile, __LINE__, queue_id);
            result = -ENETDOWN;
        }
        else
        {
            espcp_setup_message_dispatcher();
            espcp_usrsock_init();
            espcp_posix_network_init();
            espcp_spi_setup();
            result = espcp_thread_start(g_espcp_configuration);
        }
    }
    else
    {
        result = -ENETDOWN;
    }

    return result;
}
