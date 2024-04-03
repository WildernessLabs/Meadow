/****************************************************************************
 * espcp_network_monitor.c
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
#include <nuttx/kthread.h>
#include <limits.h>

#include <ctype.h>

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 *  @brief Stack size for the network monitor thread.
 */
#define ESPCP_NETWORK_MONITOR_STACK_SIZE        8192

/**
 *  @brief Priority for the network monitor thread.
 */
#define ESPCP_NETWORK_MONITOR_PRIORITY          190

/**
 *  @brief Name of the network monitor thread.
 */
#define ESPCP_NETWORK_MONITOR_NAME              "ESPMonitorThread"

/**
 *  @brief UART connected to the ESP32.
 * 
 *  This is UART5 on the STM32.
 */
#define ESPCP_NETWORK_MONITOR_UART_NAME         "/dev/ttyS2"

/**
 * @brief Number of bytes to allocate for the incoming line of text
 *        from the UART connected to the ESP32.
 */
#define ESPCP_NETWORK_MONITOR_BUFFER_LENGTH     256

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/**
 * @brief Should the network monitor be shut down?
 */
static bool _network_monitor_shutting_down = false;

/**
 * @brief Is the network monitor thread running ?
 */
static bool _network_monitor_running = false;

/**
 * @brief Thread IS for the network monitor thread.
 */
static pthread_t _network_monitor_thread_handle = 0;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 *  Name: espcp_network_monitor_signal_handler
 *
 *  Description:
 *      Process any signals that are sent to the network monitor thread.
 *
 *  Input Parameters:
 *      signal_number - The signal number.
 *      information - Information about the signal.
 *      context - The context of the signal.
 *
 *  Returned Value:
 *      None.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
static void espcp_network_monitor_signal_handler(int signal_number, FAR siginfo_t *information, FAR void *context)
{
    _network_monitor_shutting_down = true;
}

/****************************************************************************
 *  Name: espcp_network_monitor_thread
 *
 *  Description:
 *      Monitor the network connected to the ESP32.
 *
 *  Input Parameters:
 *      argv - Array of arguments (not used).
 *
 *  Returned Value:
 *      NULL.
 *
 *  Assumptions/Limitations:
 *      None.
 *
 ****************************************************************************/
static void *espcp_network_monitor_thread(int argc, char *argv[])
{
    int uart_handle = open(ESPCP_NETWORK_MONITOR_UART_NAME, O_RDONLY);

    if (uart_handle >= 0)
    {
        struct sigaction action;

        action.sa_sigaction = espcp_network_monitor_signal_handler;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);

        if (sigaction(SIGALRM, &action, NULL) < 0)
        {
            _network_monitor_shutting_down = true;
            close(uart_handle);
        }
    }

    char *line = zalloc(ESPCP_NETWORK_MONITOR_BUFFER_LENGTH);
    char *incoming_bytes = zalloc(ESPCP_NETWORK_MONITOR_BUFFER_LENGTH);
    if ((line == NULL) || (incoming_bytes == NULL))
    {
        _network_monitor_shutting_down = true;
    }

    int buffer_index = 0;
    while (!_network_monitor_shutting_down)
    {
        memset(incoming_bytes, 0, ESPCP_NETWORK_MONITOR_BUFFER_LENGTH);
        int number_of_bytes = read(uart_handle, incoming_bytes, ESPCP_NETWORK_MONITOR_BUFFER_LENGTH - 1);
        if (!_network_monitor_shutting_down)
        {
            if (number_of_bytes > 0)
            {
                incoming_bytes[number_of_bytes] = '\0';
                for (int index = 0; index < number_of_bytes; index++)
                {
                    char ch = incoming_bytes[index];
                    if (ch == '\n')
                    {
                        syslog(1, "%s\n", line);
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
    }
    //
    //  Tidy up.
    //
    free(line);
    free(incoming_bytes);
    close(uart_handle);
    _network_monitor_running = false;
    kthread_delete(0);

    return(NULL);
}

/****************************************************************************
 *  Name: espcp_network_monitor_start
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
int espcp_network_monitor_start(void)
{
    if (_network_monitor_running)
    {
        return(EALREADY);
    }

    int result = ERROR;

    if (!_network_monitor_shutting_down)
    {
        pthread_attr_t attr;
        struct sched_param param;

        param.sched_priority = ESPCP_NETWORK_MONITOR_PRIORITY;
        pthread_attr_init(&attr);
        pthread_attr_setschedparam(&attr, &param);
        pthread_attr_setstacksize(&attr, ESPCP_NETWORK_MONITOR_STACK_SIZE);

        _network_monitor_thread_handle =  kthread_create(ESPCP_NETWORK_MONITOR_NAME, ESPCP_NETWORK_MONITOR_PRIORITY,
                               ESPCP_NETWORK_MONITOR_STACK_SIZE, (main_t) espcp_network_monitor_thread, (char *const *) NULL);
        if (_network_monitor_thread_handle < 0)
        {
            syslog(LOG_CRIT, "%s@%d-Failed to create %s thread. Error: %d\n",
                    __FILE__, __LINE__, ESPCP_NETWORK_MONITOR_NAME, result);
        }
        else
        {
            result = OK;
        }
    }

    return(result);
}
