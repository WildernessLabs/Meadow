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

#include <nuttx/config.h>

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
#include <nuttx/net/net.h>

#include <meadow/meadow_hw_version.h>
#include <meadow/hcom_shared_common.h>
#include "../hcom_nx/hcom_nx_common.h"
#include "../hcom_nx/hcom_nx_config_manager.h"
#include "espcp_coprocessor.h"
#include "espcp_queue.h"
#include "espcp_message.h"
#include "espcp_message_dispatcher.h"
#include "espcp_event_handlers.h"
#include "espcp_thread.h"
#include "espcp_encoders.h"
#include "espcp_usrsock.h"
#include "espcp_system.h"
#include "meadow/meadow_os.h"
#include <arch/board/board.h>

#include "espcp_network_monitor.h"

#ifdef CONFIG_BUILD_PROTECTED

#include <nuttx/pthread.h>

#endif

#ifdef CONFIG_MEADOW_ESPCP_USE_EXTERNAL_ESP32_BOARD

#error "Using external ESP32 development board."

#endif

// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/**
 *  Define a structure to hold the communication pins connecting the STM32
 *  and the ESP32 chips.
 * 
 *  This structure allows different pins to be used depending upon the board
 *  version.
 */
struct espcp_pins_s
{
    uint32_t reset;
    uint32_t boot;
    uint32_t spi_ready;
    uint32_t chip_select;
    uint32_t uart_rx;
    uint32_t uart_tx;
};
typedef struct espcp_pins_s espcp_pins_t;

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

/**
 *  Mutex to be used by any code that wants access to the configuration.
 */
static sem_t config_lock = { };

/**
 *  Pin definitions for the F7V1 board.
 */
static espcp_pins_t _f7v1_pins = 
{
    /* reset */ ESP32CP_RESET_PIN_OUTPUT,
    /* boot */ ESP32CP_BOOT_PIN_OUTPUT,
    /* spi_ready */ ESP32CP_SPI_READY_PIN_INPUT,
    /* chip_select */ ESP32CP_SPI_CS_PIN_OUTPUT,
    /* uart_rx */ GPIO_UART5_RX,
    /* uart_tx */ GPIO_UART5_TX_V1
};

/**
 *  Pin definitions for the F7V2 board and the Core-Compute module CCMv2.
 */
static espcp_pins_t _f7v2_pins = 
{
    /* reset */ ESP32CP_RESET_PIN_OUTPUT,
    /* boot */ ESP32CP_BOOT_PIN_OUTPUT,
    /* spi_ready */ ESP32CP_SPI_READY_PIN_INPUT,
    /* chip_select */ ESP32CP_SPI_CS_PIN_OUTPUT,
    /* uart_rx */ ESP32CP_UART_RX,
    /* uart_tx */ ESP32CP_UART_TX
};

/**
 *  Address of the current active pin set.  Default to F7V1 but can be changed
 *  in the espcp_init method.
 */
static espcp_pins_t *_active_pins = &_f7v1_pins;

/**
 *  Used to indicate if the SPI interface is free.
 */
sem_t espcp_spi_lock;

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
 *  Name: espcp_config_lock
 *
 *  Description:
 *      Lock the specified configuration object.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      The config_lock semaphore has been created and initialised correctly.
 *
 ****************************************************************************/
void espcp_config_lock()
{
    sem_wait(&config_lock);
}

/****************************************************************************
 *  Name: espcp_config_unlock
 *
 *  Description:
 *      Unlock the specified configuration object.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      The config_lock semaphore has been created and initialised correctly.
 *
 ****************************************************************************/
void espcp_config_unlock()
{
    sem_post(&config_lock);
}

/****************************************************************************
 *  Name: espcp_spi_interface_lock
 *
 *  Description:
 *      Lock the S{PI interface.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
void espcp_spi_interface_lock(void)
{
    sem_wait(&espcp_spi_lock);
}

/****************************************************************************
 *  Name: espcp_spi_interface_unlock
 *
 *  Description:
 *      Unlock the SPI interface.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
void espcp_spi_interface_unlock(void)
{
    sem_post(&espcp_spi_lock);
}

/****************************************************************************
 *  Name: espcp_get_default_configuration
 *
 *  Description:
 *      Get the default configuration object for the ESP32 communication system.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      Pointer to a valid ESP32 configuration object, NULL if there was a
 *      problem allocating memory.
 *
 *  Assumptions/Limitations:
 *      This is only called once and so we do not need to lock the
 *      configuration object.
 *
 ****************************************************************************/
static espcp_configuration_t *espcp_get_default_configuration(void)
{
    sem_init(&config_lock, 0, 0);                   //  This will lock the configuration (initial value = 0).
    sem_setprotocol(&config_lock, SEM_PRIO_NONE);

    espcp_configuration_t *config = (espcp_configuration_t *) kmm_zalloc(sizeof(espcp_configuration_t));
    if (config != NULL)
    {
        sem_init(&espcp_spi_lock, 0, 0);                      // This will lock the SPI interface (initial value = 0).
        sem_setprotocol(&espcp_spi_lock, SEM_PRIO_NONE);
        config->current_mode = espcp_mode_unknown;
        config->thread_running = false;
        config->esp_not_responding = true;
        config->uart_monitor_thread_running = false;
        config->send_data_to_esp32 = espcp_send_data_over_spi;
        config->header_only_buffer_size = espcp_calculate_spi_buffer_size(ESPCP_MESSAGE_HEADER_SIZE);
        config->header = (uint8_t *) kmm_zalloc(config->header_only_buffer_size);
        if (config->header == NULL)
        {
            kmm_free(config);
            config = NULL;
        }
    }

    espcp_config_unlock();
    return (config);
}

/****************************************************************************
 *  Name: espcp_spi_setup
 *
 *  Description:
 *      Set up the STM32 SPI interface for comms with the ESP32.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      OK if successful, ERROR otherwise.
 *
 *  Assumptions/Limitations:
 *      g_espcp_configuration is setup prior to calling this method.
 *
 ****************************************************************************/
static int espcp_spi_init(void)
{
    int result = OK;
    uint32_t frequency;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    if (config == NULL)
    {
        frequency = 8000000UL;
    }
    else
    {
        frequency = config->esp_spi_speed_hz;
    }
    hcom_nx_config_unlock();

    espcp_config_lock();
    espcp_configuration_t *esp_configuration = espcp_get_configuration();
    esp_configuration->spi_rx_buffer = (uint8_t *) kmm_malloc(ESPCP_MAXIMUM_SPI_FRAME_SIZE);
    if (esp_configuration->spi_rx_buffer == NULL)
    {
        esp_configuration->spi_tx_buffer = NULL;
        result = ERROR;
    }
    else
    {
        esp_configuration->spi_tx_buffer = (uint8_t *) kmm_malloc(ESPCP_MAXIMUM_SPI_FRAME_SIZE);
        if (esp_configuration->spi_tx_buffer == NULL)
        {
            kmm_free(esp_configuration->spi_rx_buffer);
            esp_configuration->spi_rx_buffer = NULL;
            result = ERROR;
        }
    }
    espcp_config_unlock();

    if (result == OK)
    {
        result = stm32_configgpio(_active_pins->chip_select);
        if (result < 0)
        {
            MEADOW_TRACE_CRITICAL("%s@%d Config SPI CS GPIO failed result:%d\n", __FILE__, __LINE__, result);
            return(ERROR);
        }
        stm32_gpiowrite(ESP32CP_SPI_CS_PIN_OUTPUT, true); // SPI CS is active low so deselect SPI.

        g_esp_spi_dev = stm32_spibus_initialize(ESP32CP_SPI_COMMS_PORT);
        if (g_esp_spi_dev == 0)
        {
            MEADOW_TRACE_CRITICAL("%s@%d Error:Failed init ESP32 SPI port %d\n", __FILE__, __LINE__, result);
            return(ERROR);
        }
        
        SPI_SETFREQUENCY(g_esp_spi_dev, frequency);
        SPI_SETBITS(g_esp_spi_dev, 8);
        SPI_SETMODE(g_esp_spi_dev, SPIDEV_MODE3); /* CPOL=1 CHPHA=1 */
    }

    MEADOW_TRACE_INFORMATION("SPI Configuration complete\n");

    return(OK);
}

/****************************************************************************
 *  Name: espcp_gpio_init
 *
 *  Description:
 *      Initialise the non-SPI GPIO pins used to control and communicate with
 *      the ESP32.
 *
 *  Returned Value:
 *      OK if successful, ERROR otherwise.
 *
 *  Assumptions/Limitations:
 *      g_espcp_configuration is setup prior to calling this method.
 *
 ****************************************************************************/
static int espcp_gpio_init(void)
{
    int result = stm32_configgpio(_active_pins->spi_ready);
    if (result < 0)
    {
        MEADOW_TRACE_CRITICAL("%s@%d Config Boot pin as input for SPI Ready signal result:%d\n", __FILE__, __LINE__, result);
        return(ERROR);
    }
    result = stm32_configgpio(_active_pins->uart_tx);
    if (result < 0)
    {
        MEADOW_TRACE_CRITICAL("%s@%d Config UART Tx as output result:%d\n", __FILE__, __LINE__, result);
        return(ERROR);
    }
    result = stm32_configgpio(_active_pins->uart_rx);
    if (result < 0)
    {
        MEADOW_TRACE_CRITICAL("%s@%d Config UART Rx as inout result:%d\n", __FILE__, __LINE__, result);
        return(ERROR);
    }
    result = stm32_configgpio(_active_pins->reset);
    if (result < 0)
    {
        MEADOW_TRACE_CRITICAL("%s@%d Config Reset pin failed result:%d\n", __FILE__, __LINE__, result);
        return(ERROR);
    }

    return(OK);
}

/****************************************************************************
 *  Name: espcp_send_data_over_spi
 *
 *  Description:
 *      Send, receive or exchange data with the ESP32.
 * 
 *      If tx is NULL then it is assumed that this is a receive only operation.
 *      If rx is NULL then it is assumed that this is a transmit only operation.
 *      If tx and rx are not NULL then an exchange is assumed.
 *      tx and rx both NULL is a no operation.
 *
 *  Input Parameters:
 *      tx - Buffer of bytes to send to the ESP32.
 *      rx - buffer to hold bytes to be received from the ESP32.
 *      buffer_length - number of bytes to be sent / received (or both).
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
void espcp_send_data_over_spi(void *tx, void *rx, size_t buffer_length)
{
    if ((tx == NULL) && (rx == NULL))
    {
        return;
    }

    SPI_LOCK(g_esp_spi_dev, true);
    stm32_gpiowrite(_active_pins->chip_select, false);
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
    stm32_gpiowrite(_active_pins->chip_select, true);
    SPI_LOCK(g_esp_spi_dev, false);
}

/****************************************************************************
 *  Name: espcp_should_reset_at_startup
 *
 *  Description:
 *      Check the system configuration to determine if the ESP should be
 *      reset at startup.
 * 
 *      The default configuration for this is to force a reset.  Debugging often
 *      requires the ESP to have a debugger attached when the STM restarts which
 *      would then force the ESP to be reset and the debugger to be detached.
 *      The configuration allows this to be overridden to allow debugging to
 *      continue.
 * 
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
bool espcp_should_reset_at_startup(void)
{
    bool perform_reset = true;

    hcom_nx_config_lock();
    meadow_configuration_t *config = hcom_nx_config_get_pointer();
    if (config != NULL)
    {
        perform_reset = (config->reset_esp32_at_startup == 1);
    }
    hcom_nx_config_unlock();

    return (perform_reset);
}

/****************************************************************************
 *  Name: escpcp_hold_in_reset
 *
 *  Description:
 *      Configure the ESP reset pin and take the reset line low.
 * 
 *      This method can be used in two ways:
 *          - Hold the chip in reset
 *          - Start the reset process.
 * 
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
void espcp_hold_in_reset(void)
{
    if (espcp_should_reset_at_startup())
    {
        stm32_gpiowrite(_active_pins->reset, false);
    }
}

/****************************************************************************
 *  Name: espcp_reset
 *
 *  Description:
 *      Reset the ESP32.
 * 
 *      Note that this method checks the configuration to determine if the ESP
 *      should be reset at startup.
 * 
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
void espcp_reset(void)
{
    if (espcp_should_reset_at_startup())
    {
        espcp_config_lock();
        espcp_configuration_t *config = espcp_get_configuration();
        config->expecting_reset = true;
        espcp_config_unlock();

        espcp_hold_in_reset();
        usleep(500);
        stm32_gpiowrite(_active_pins->reset, true);
    }
}

/****************************************************************************
 *  Name: espcp_process_reset_control_signal
 *
 *  Description:
 *      Process the "+++RST" (reset) control message from the ESP32.
 * 
 *  Input Parameters:
 *      line - text that was sent by the ESP32.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
void espcp_process_reset_control_signal(const char *line)
{
    MEADOW_TRACE_INFORMATION("++RST\n");
    espcp_config_lock();
    espcp_configuration_t *config = espcp_get_configuration();
    bool expecting_reset = config->expecting_reset;

    if (expecting_reset)
    {
        config->expecting_reset = false;
    }
    espcp_config_unlock();

    if (!expecting_reset)
    {
        meadow_os_raise_simple_exception(espcp_status_codes_unexpected_coprocessor_restart);
    }
}

/****************************************************************************
 *  Name: espcp_enter_programming_mode
 *
 *  Description:
 *      Put the ESP32 into programming mode.  This is done by pulling the boot pin
 *      low, resetting the ESP and then pulling the boot pin high.  The boot pin
 *      will be left unconfigured at the end of this process.
 * 
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      A system (STM) reset will need to be performed in order for the comms
 *      with the ESP to be reinstated.
 *
 ****************************************************************************/
int espcp_enter_programming_mode(void)
{
    //
    //  First, reconfigure the BOOT pin as this is shared with the SPI interface
    //  ready signal.
    //
    int result = stm32_gpiosetevent(_active_pins->spi_ready, /*risingedge=*/false, /*fallingedge=*/false, true, NULL, 0);
    if (result < 0)
    {
        MEADOW_TRACE_CRITICAL("%s@%d Disabling SPI Ready interrupt result:%d\n", __FILE__, __LINE__, result);
        return(ERROR);
    }
    stm32_unconfiggpio(_active_pins->spi_ready);
    result = stm32_configgpio(_active_pins->boot);
    if (result < 0)
    {
        MEADOW_TRACE_CRITICAL("%s@%d Config BOOT pin as output result:%d\n", __FILE__, __LINE__, result);
        return(ERROR);
    }
    //
    //  Record the change of mode.
    //
    espcp_config_lock();
    espcp_configuration_t *config = espcp_get_configuration();
    config->current_mode = espcp_mode_programming;
    espcp_config_unlock();
    //
    //  Now put the ESP32 into programming mode.
    //
    usleep(20 * 1000);
    stm32_gpiowrite(_active_pins->boot, false);
    espcp_reset();
    usleep(20 * 1000);
    stm32_gpiowrite(_active_pins->boot, true);

    return(OK);
}

/****************************************************************************
 *  Name: espcp_enter_run_mode
 *
 *  Description:
 *      Reset the ESP and enter run mode.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      OK on success, -1 on failure.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
int espcp_enter_run_mode(void)
{
    //
    //  Record the change of mode.
    //
    espcp_config_lock();
    espcp_configuration_t *config = espcp_get_configuration();
    config->current_mode = espcp_mode_run;
    espcp_config_unlock();
    //
    //  Now reset to enter run mode.
    //
    espcp_reset();

    return (OK);
}

/****************************************************************************
 *  Name: espcp_deep_sleep
 *
 *  Description:
 *      Put the ESP32 coprocessor to sleep.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
void espcp_deep_sleep(void)
{
    espcp_message_t *message = (espcp_message_t *) zalloc(sizeof(espcp_message_t));
    message->message_type = espcp_message_types_header;
    message->interface = espcp_esp32_interfaces_system;
    message->function = espcp_system_function_deep_sleep;
    message->semaphore = NULL;
    //
    //  We need to explicitly wait for the message to be sent to the ESP before
    //  returning.  This is necessary as we could find ourselves queuing the
    //  message and entering sleep mode before the message is actually sent.
    //
    sem_t sent = { };
    sem_init(&sent, 0, 0);
    sem_setprotocol(&sent, SEM_PRIO_NONE);
    message->message_sent = &sent;
    espcp_queue_message(message, false);
    //
    //  Now clear all of the socket information held by NuttX as putting the
    //  ESP into deep sleep will destroy all of the sockets.
    //
    //  This code is derived from the various methods in net_sockets.c
    //
    struct socketlist *list = sched_getsockets();
    if (list)
    {
        int result;
        while ((result = net_lockedwait(&list->sl_sem)) < 0)
        {
            /* The only case that an error should occr here is if
            * the wait was awakened by a signal.
            */
            DEBUGASSERT(ret == -EINTR || ret == -ECANCELED);
        }
        for (int index = 0; index < CONFIG_NSOCKET_DESCRIPTORS; index++)
        {
            memset(&list->sl_sockets[index], 0, sizeof(struct socket));
        }
        
        nxsem_post(&list->sl_sem);
    }
    //
    //  Lastly, wait for the message to have been sent.
    //
    sem_wait(&sent);
    espcp_delete_message_and_payload(message);
}

/****************************************************************************
 *  Name: espcp_wakeup
 *
 *  Description:
 *      Wakeup the ESP32 coprocessor.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
void espcp_wakeup(void)
{
    espcp_reset();
}

/****************************************************************************
 *  Name: espcp_get_configuration
 *
 *  Description:
 *      Get a pointer to the current configuration of the ESP32 coprocessor.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      Pointer to the current ESP32 configuration object.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
espcp_configuration_t *espcp_get_configuration(void)
{
    return g_espcp_configuration;
}

/****************************************************************************
 *  Name: espcp_early_init
 *
 *  Description:
 *      Perform the early initialisation of the system.  This is the part of
 *      the initialisation that puts supporting structures, mutexes and
 *      message queues in place.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
void espcp_early_init(void)
{
    g_espcp_configuration = espcp_get_default_configuration();
    if (g_espcp_configuration != NULL)
    {
        if (espcp_create_message_queues(g_espcp_configuration))
        {
            uint32_t hardware_version = meadow_hw_version_get();
            if ((hardware_version == MEADOW_F7_HW_VERSION_NUMB_F7V2) || (hardware_version == MEADOW_F7_HW_VERSION_NUMB_CCMV2))
            {
                _active_pins = &_f7v2_pins;
            }
            espcp_setup_message_dispatcher();
            espcp_event_handlers_init();
        }
        else
        {
            MEADOW_TRACE_CRITICAL("%s@%d Error creating ESP32 message queues.\n", __FILE__, __LINE__);
            g_espcp_configuration->esp_not_responding = true;
        }
    }
}

/****************************************************************************
 *  Name: espcp_late_init
 *
 *  Description:
 *      Perform late initialisation of the ESP system.  This is the part of
 *      the initialisation that puts the network components in place.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      Result of starting the thread or -ENETDOWN if there is an error.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
int espcp_late_init(void)
{
    int result = -ENETDOWN;

    if (g_espcp_configuration != NULL)
    {
        espcp_usrsock_init();
        result = espcp_thread_start(g_espcp_configuration);
        usrsock_register_sockif(&g_usrsock_sockif_esp32);
        espcp_gpio_init();
        espcp_spi_init();

        espcp_network_monitor_start();
        espcp_reset();
        result = OK;
    }
    
    return result;
}
