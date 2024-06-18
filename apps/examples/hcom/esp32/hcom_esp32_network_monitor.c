 /****************************************************************************
 * hcom_esp32_network_monitor.c
 *
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
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

#include <nuttx/compiler.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <crc32.h>
#include <errno.h>
#include <debug.h>
#include <limits.h>
#include <poll.h>

#include <ctype.h>

#include "../hcom_common.h"
#include "hcom_esp32_comms.h"
#include "hcom_esp32_network_monitor.h"

#include <meadow/meadow_os.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_thread_config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 * @brief Number of bytes to allocate for the incoming line of text
 *        from the UART connected to the ESP32.
 */
#define ESPCP_NETWORK_MONITOR_BUFFER_LENGTH     256

/****************************************************************************
 * Local method prototypes.
 ****************************************************************************/

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/**
 * @brief Is the network monitor thread running ?
 */
static bool _uart_monitor_running = false;

/**
 * @brief Handle for the pthread that will monitor the UART connected to the ESP32.
 */
pthread_t _uart_thread_handle;

/**
 * @brief Pipe handles that will be used to indicate to the monitor thread that it should terminate.
 */
int _pipe_handles[2];

/**
 * @brief Should we send the ESP logging information the the UART ?
 * 
 * This is a configuration item in the meadow.config.yaml file.
 */
bool _send_log_to_uart = false;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/


/****************************************************************************
 *  Name: hcom_esp32_network_monitor_running
 *
 *  Description:
 *      It the ESP UART monitor thread running?
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      1 - the thread is running.
 *      0 - the thread is not running.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
int hcom_esp32_network_monitor_running(void)
{
    return(_uart_monitor_running ? 1 : 0);
}

/****************************************************************************
 *  Name: hcom_esp32_network_monitor_process_line
 *
 *  Description:
 *      Process the line of text that has been sent by the ESP32.
 * 
 *      Control messages will all start with "+++" followed by a signal code.
 *          * MW - Message waiting (the most common message).
 *          * RST - ESP32 has just completed the reset sequence.
 * 
 *      Lines not starting with "+++" are log messages from the ESP32.
 *
 *  Input Parameters:
 *      line - Line of text from the ESP32.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      Case of the strings sent by the ESP32 matches those in this method.
 * 
 *      String is '\0' terminated and the '\n' character has been removed.
 *
 ****************************************************************************/
static void hcom_esp32_network_monitor_process_line(char *line)
{
    if (line)
    {
        if (strncmp(line, "+++", 3) == 0)
        {
            meadow_os_espcp_monitor_process_line(line);
        }
        else
        {
            if (_send_log_to_uart)
            {
                hcom_logging_syslog(LOG_INFO, "ESP Log: %s\n", line);
            }
        }
    }
}

/****************************************************************************
 *  Name: hcom_esp32_network_monitor_thread
 *
 *  Description:
 *      Monitor the network connected to the ESP32.
 *
 *  Input Parameters:
 *      argc - Number of arguments (not used).
 *      argv - Array of arguments (not used).
 *
 *  Returned Value:
 *      NULL.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
static void *hcom_esp32_network_monitor_thread(void *parameters)
{
    _uart_monitor_running = true;

    bool shutting_down = false;
    int uart_handle = open(ESPCP_NETWORK_MONITOR_UART_NAME, O_RDONLY);

    int result;

    struct pollfd fds[2];
    if (uart_handle >= 0)
    {
        result = pipe(_pipe_handles);
        if (result < 0)
        {
            hcom_logging_syslog(LOG_CRIT, "Failed to create pipe. Error: %d\n", errno);
            shutting_down = true;
        }
        else
        {
            fds[0].fd = uart_handle;
            fds[0].events = POLLIN;
            fds[1].fd = _pipe_handles[0];
            fds[1].events = POLLIN;
        }
    }
    else
    {
        hcom_logging_syslog(LOG_CRIT, "%s@%d-Failed to open UART " ESPCP_NETWORK_MONITOR_UART_NAME ". Error: %d\n", __FILE__, __LINE__, errno);
        shutting_down = true;
    }

    char *line = zalloc(ESPCP_NETWORK_MONITOR_BUFFER_LENGTH);
    char *incoming_bytes = zalloc(ESPCP_NETWORK_MONITOR_BUFFER_LENGTH);
    if ((line == NULL) || (incoming_bytes == NULL))
    {
        shutting_down = true;
    }

    int buffer_index = 0;
    while (!shutting_down)
    {
        memset(incoming_bytes, 0, ESPCP_NETWORK_MONITOR_BUFFER_LENGTH);
        result = poll(fds, 2, -1);
        if (result < 0)
        {
            hcom_logging_syslog(LOG_CRIT, "%s@%d-Failed to poll. Error: %d\n", __FILE__, __LINE__, errno);
            shutting_down = true;
        }
        else
        {
            if (fds[0].revents & POLLIN)
            {
                int number_of_bytes = read(uart_handle, incoming_bytes, ESPCP_NETWORK_MONITOR_BUFFER_LENGTH - 1);
                if (number_of_bytes > 0)
                {
                    incoming_bytes[number_of_bytes] = '\0';
                    for (int index = 0; index < number_of_bytes; index++)
                    {
                        char ch = incoming_bytes[index];
                        if (ch == '\n')
                        {
                            hcom_esp32_network_monitor_process_line(line);
                            memset(line, 0, ESPCP_NETWORK_MONITOR_BUFFER_LENGTH);
                            buffer_index = 0;
                        }
                        else
                        {
                            if (ch != '\r')
                            {
                                if (!isprint(ch))
                                {
                                    ch = '.';
                                }
                                if (buffer_index < (ESPCP_NETWORK_MONITOR_BUFFER_LENGTH - 1))
                                {
                                    line[buffer_index++] = ch;
                                }
                                else
                                {
                                    memset(line, 0, ESPCP_NETWORK_MONITOR_BUFFER_LENGTH);
                                    buffer_index = 0;
                                }
                            }
                        }
                    }
                }
            }
            if (fds[1].revents & POLLIN)
            {
                shutting_down = true;
            }
        }
    }
    //
    //  Tidy up.
    //
    free(line);
    free(incoming_bytes);
    close(uart_handle);
    close(_pipe_handles[0]);
    close(_pipe_handles[1]);
    _uart_monitor_running = false;

    return(NULL);
}

/****************************************************************************
 *  Name: hcom_esp32_network_monitor_start
 *
 *  Description:
 *      Initialise the network communications between the ESP32 and the STM32.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      OK - Success, otherwise ERROR.
 *
 *  Assumptions/Limitations:
 *      UART will be closed by the monitoring thread.
 *
 ****************************************************************************/
int hcom_esp32_network_monitor_start(void)
{
    if (_uart_monitor_running)
    {
        return(EALREADY);
    }

    meadow_configuration_t *config = meadow_os_deep_copy_config();
    if (config == NULL)
    {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Deep copy config failed\n", __FILE__, __LINE__);
        return -ENOMEM;
    }
    if ((config->esp_log_destination == esp_log_destination_uart) && config->use_uart1_for_trace)
    {
        _send_log_to_uart = true;
    }
    meadow_os_config_free_resources(config);

    int result = ERROR;

    pthread_attr_t thread_attributes;
    result = pthread_attr_init(&thread_attributes);
    if (result != OK)
    {
        return (-result);
    }

    struct sched_param scheduler_parameters;
    scheduler_parameters.sched_priority = ESPCP_NETWORK_MONITOR_PRIORITY;
    result = pthread_attr_setschedparam(&thread_attributes, &scheduler_parameters);
    if (result != OK)
    {
        return (-result);
    }

    result = pthread_attr_setstacksize(&thread_attributes, ESPCP_NETWORK_MONITOR_STACK_SIZE);
    if (result != OK)
    {
        return (-result);
    }

    result = pthread_create(&_uart_thread_handle, &thread_attributes, hcom_esp32_network_monitor_thread, NULL);
    if (result != OK)
    {
        return (-result);
    }

    return(result);
}

/****************************************************************************
 *  Name: hcom_esp32_network_monitor_stop
 *
 *  Description:
 *      Stop UART monitoring.  This is required to allow HCOM to shutdown the
 *      monitoring thread and allow HCOM to talk to the ESP32 for reprogramming.
 *
 *  Input Parameters:
 *      None.
 *
 *  Returned Value:
 *      OK - Success, otherwise ERROR.
 *
 *  Assumptions/Limitations:
 *      UART will be closed by the monitoring thread.
 *
 ****************************************************************************/
int hcom_esp32_network_monitor_stop(void)
{
    if (hcom_esp32_network_monitor_running())
    {
        write(_pipe_handles[1], "X", 1);
        while (hcom_esp32_network_monitor_running())
        {
            usleep(10 * 1000);
        }

        pthread_cancel(_uart_thread_handle);

        int result = pthread_join(_uart_thread_handle, NULL);
        if(result != 0)
        {
            hcom_logging_syslog(LOG_ERR, "%s@%d-pthread join failed:%d, errno:%d\n", __FILE__, __LINE__, result, errno);
            return(ERROR);
        }
        hcom_logging_syslog(LOG_INFO, "%s@%d-ESP32 UART monitor thread stopped.\n", __FILE__, __LINE__);
    }
    else
    {
        hcom_logging_syslog(LOG_INFO, "%s@%d-ESP32 UART monitor thread is not running.\n", __FILE__, __LINE__);
    }

    return(OK);
}