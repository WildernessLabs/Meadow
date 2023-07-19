/****************************************************************************
 * /include/meadow/meadow_thread_config.h
 * 
 *  Copyright (C) 2021-2022 Wilderness Labs. All rights reserved.
 *  Author:  Wilderness Labs
 * 
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
#ifndef __MEADOW_THREAD_CONFIG_H
#define __MEADOW_THREAD_CONFIG_H

#include <nuttx/config.h>

/**
 * This file contains the definitions for the threads as used by Meadow OS.  It
 * contains the defintions of the following parameters:
 * 
 *      1. Thread priority
 *      2. Thread name
 *      3. Thread stack size
 * 
 * The thread name is only used by kthreads at the moment but is provided for all
 * threads for reference and future use.
 * 
 * This file has been put together to make it easier to change the properties of
 * any single thread in context with the other threads in the system.
 * 
 * It is important that each group of properties for a thread includes the reasoning
 * behind the priority and stack size values in order to guide future changes.
 * 
 * The thread information below is presented in priority order with the highest at
 * the top and the lowest at the bottom.  Please preserve this order when adding
 * new entries.
 */


/**
 *      CONFIG_SCHED_HPWORKPRIORITY
 * 
 * This value is set using the NuttX configuration system.
 * 
 * Default value: 224
 * 
 * This is used by the SD card driver.
*/

/**
 * This thread is dedicated to the Point-to-Point Protocol Daemon (PPPD) 
 * utilized in Cell networking for establishing communication with the modules.
*/

#define HCOM_THREAD_PRIORITY_CELL_PPPD 200
#define HCOM_THREAD_NAME_CELL_PPPD "CellPPPD"
#define HCOM_THREAD_STACKSIZE_CELL_PPPD 4096

/**
 * The ESPCP thread is responsible for managing the communication between the STM32
 * main processor and the ESP32 coprocessor.  As such it should be considered to be
 * a hardware driver as the ESP32 is providing network communication for the STM32.
 * 
 * Note that the value for the thread priority is set slightly above the ESP32 event
 * handler thread.  The event handler thread needs communication to have completed
 * before it will have any data to process.
 * 
 * This thread will spend most of its time waiting on a message queue.  Messages can
 * come from the following sources:
 *      1. Network requests from NuttX
 *      2. Events from the ESP32
 *      3. Requests from Meadow.Core (Bluetooth, connect to WiFi etc.)
 * 
 * When a message is received the thread will communicate with the ESP32, process the
 * data and then go to sleep waiting for the next message.
 * 
 * This thread spends most of its time waiting for messages on a message queue.
 * 
 * The fact that this thread can be called independently from the network system suggests
 * we should avoid clashes with Ethernet or cellular drivers.
 */
#define ESPCP_THREAD_PRIORITY                       199
#define ESPCP_THREAD_NAME                           "EspcpMainThread"
#define ESPCP_THREAD_STACKSIZE                      4096

/**
 * HCOM is the main communication thread for the Meadow OS.  It is responsible for
 * starting many of the other threads in the system.  It also supervises communication
 * with the host computer.  As such it should have a high priority.
 */
#define HCOM_THREAD_PRIORITY_HCOM_RECEIVE           180
#define HCOM_THREAD_NAME_HCOM_RECEIVE               "HcomRecv"
#define HCOM_THREAD_STACKSIZE_HCOM_RECEIVE          2048

/**
 * Testing showed with priority of Process being higher than Receive there
 * were very rare download errors. This is probably in hcom_host_enq_deq.c.
 * With equal priority no errors have been detected.
 * I believe there is room for improvement in hcom_host_enq_deq.c.
 * 
 * Stack size is set by CONFIG_USERMAIN_STACKSIZE.
 * 
 * This value is set using the NuttX configuration system.
 */
#define HCOM_THREAD_PRIORITY_HCOM_PROCESS           180
#define HCOM_THREAD_NAME_HCOM_PROCESS               "HcomProc"
#define HCOM_THREAD_STACKSIZE_HCOM_PROCESS          CONFIG_USERMAIN_STACKSIZE

/**
 * The ESPCP event handler thread is responsible for processing the events that are
 * generated by the ESP32 coprocessor.  These events are mainly for Meadow.Core but
 * there are a few events which will impact the OS (network disconnect etc).
 * 
 * This thread relies upon data being made available through the main ESPCP thread.
 * 
 * The thread spend most of its time waiting for messages on the event message queue and
 * follows the same pattern as the ESP32 main thread.  Messages on this thread should
 * be rare.
 */
#define ESPCP_EVENT_HANDLER_THREAD_PRIORITY         170
#define ESPCP_EVENT_HANDLER_THREAD_NAME             "EspcpEventHandler"
#define ESPCP_EVENT_HANDLER_THREAD_STACKSIZE        4096

/**
 * This thread is used for remote debugging mono apps.  This thread takes the
 * requests from HCOM and passes them on to the mono debugger.
 */
#define HCOM_THREAD_PRIORITY_REMOTE_DBG             130
#define HCOM_THREAD_NAME_REMOTE_DBG                 "RemoteDbg"
#define HCOM_THREAD_STACKSIZE_REMOTE_DBG            2048

/**
 * Ensure HCOM recv thread runs before ESP32 recv, which is only used to program the
 * ESP32 from HCOM. Here this thread's priority is boosted ahead of most of the HCOM 
 * threads.
 */
#define HCOM_THREAD_PRIORITY_ESP32_RECEIVE          125
#define HCOM_THREAD_NAME_ESP32_RECEIVE              "EspRecv"
#define HCOM_THREAD_STACKSIZE_ESP32_RECEIVE         2048

/**
 * The ramlog is part of nuttx and contains the syslog text
 */
#define HCOM_THREAD_PRIORITY_TRACE_RAMLOG           120
#define HCOM_THREAD_NAME_TRACE_RAMLOG               "RamlogRead"
#define HCOM_THREAD_STACKSIZE_TRACE_RAMLOG          2048

/**
 * This thread reads stdout and forwards to the Host
 */
#define HCOM_THREAD_PRIORITY_STDERR_REDIRECT        120
#define HCOM_THREAD_NAME_STDOUT_REDIRECT            "MonoOut"
#define HCOM_THREAD_STACKSIZE_STDOUT_REDIRECT       2048

/**
 * This thread reads stderr and forwards to the Host
 */
#define HCOM_THREAD_PRIORITY_STDOUT_REDIRECT        120
#define HCOM_THREAD_NAME_STDERR_REDIRECT            "MonoErr"
#define HCOM_THREAD_STACKSIZE_STDERR_REDIRECT       2048

/**
 * 
 */
#define HCOM_THREAD_PRIORITY_CLI_TRANSPORT          120
#define HCOM_THREAD_NAME_CLI_TRANSPORT              "CliXport"
#define HCOM_THREAD_STACKSIZE_CLI_TRANSPORT         2048

/**
 * 
 */
#define HCOM_THREAD_PRIORITY_HOST_TRANSPORT         120
#define HCOM_THREAD_NAME_HOST_TRANSPORT             "HostXport"
#define HCOM_THREAD_STACKSIZE_HOST_TRANSPORT        2048

/**
 * This thread works out how far the LSI drifts from real time.  The thread
 * is transient running for about 5 seconds at startup.  The thread priority
 * is set to 120 as this thread should not be running when any of the threads 
 * used for communications with mono are running.
 */
#define PWRMGMT_CAL_LSI_THREAD_NAME                 "LSI Calibrate"
#define PWRMGMT_CAL_LSI_THREAD_PRIORITY             (120)
#define PWRMGMT_CAL_LSI_THREAD_STACKSIZE            (2048)

/**
 *      CONFIG_SCHED_LPWORKPRIORITY
 * 
 * This value is set using the NuttX configuration system.
 * 
 * Default value: 100
 * 
 * This is used by the Ethernet and cell drivers.
*/

/**
 *      CONFIG_USERMAIN_PRIORITY and SCHED_PRIORITY_DEFAULT
 * 
 * This value is set using the NuttX configuration system.
 * 
 * Default value: 100
 * 
 * See nx_bringup.c
 */


/**
 * Task monitoring for USB device connect / disconnect.
 */
#define USBHOST_TASK_PRIORITY                       100
#define USBHOST_TASK_NAME                           "USBHost"
#define USBHOST_TASK_STACKSIZE                      1024

/**
 * The long period scheduler is used to execute tasks that need to be run regularly but 
 * over a long time base, i.e. they are infrequent.  Examples of this are the NTP time
 * synchronisation which run over a period that varies from minutes to hours or even days.
 * 
 * This thread will spend most of its time in a wait state.  It will wake at most once every
 * 60 seconds, process the queue to see if anything needs running, run stuff if needed and
 * then go back to sleep.
 */
#define LPSDAEMON_THREAD_PRIORITY                   90
#define LSPDAEMON_THREAD_NAME                       "LpsDaemon"
#define LPSDAEMON_STACKSIZE                         4096

/**
 * This task runs the mono runtime system and any threads created as part of
 * application execution.  Note that threads launched by Mono will also run at
 * this priority.  This has the effect of putting in a round-robin scheduling
 * pattern.  Care should be taken when creating any other threads at this priority
 * as they will be impacted by the number of threads created by mono and the user
 * application.
 * 
 * **** IMPORTANT ****
 * There is also a definition for MONO_THREAD_PRIORITY in the file
 * mono/mono/metadata/threads.c any change to this priority must be replicated in
 * both files.
 */
#define MONO_TASK_PRIORITY                          80
#define MONO_TASK_NAME                              "Mono"
#define MONO_TASK_STACKSIZE                         CONFIG_PTHREAD_STACK_DEFAULT

#endif // __MEADOW_THREAD_CONFIG_H