/****************************************************************************
 * espcp_tests.c
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <debug.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <nuttx/mm/mm.h>
#include <assert.h>
#include <sys/socket.h>
#include <netdb.h>	//hostent
#include <arpa/inet.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../meadow-upd.h"
#include <meadow/hcom_shared_common.h>
#include "espcp_usrsock.h"
#include "espcp_common.h"
#include "espcp_coprocessor.h"

/****************************************************************************
 * Local defines.
 ****************************************************************************/
//
//  The definitions below are placeholders to make the code compile when
//  HCOM_INCLUDE_ESPCP_TESTS is set to 0.  Do not make changes to the
//  definitions in case the file is checked into source control.
//  
//  Instead:
//  * Edit <nuttx/hcom_shared_common> and set the define for
//    HCOM_INCLUDE_ESPCP_TESTS to a non-zero value.
//  * Add a secrets.h file to this source directory and add the definitions
//    there.  secrets.h is excluded from git.
//
#if HCOM_INCLUDE_ESPCP_TESTS > 0
#include "secrets.h"
#else
#define WIFI_NETWORK    "Dummy, do not use"
#define WIFI_PASSWORD   "Use contents of secrets.h"
#define SIMPLE_WEB_SERVER_NAME "pi4-ubuntu-001"
#define SIMPLE_WEB_PAGE "/"
#define WEB_SERVER_IP_ADDRESS "127.0.0.1"
#endif

//
//  Default logging level for this file.
//
#define LOGGING_LEVEL   1

//
//  Macros to help with the task of getting memory snapshots.
//
#define ALLOCATE_HEAP_STRUCTURES      struct mallinfo start, end, kstart, kend;
#define GET_INITIAL_HEAP_INFORMATION  espcp_test_get_mallinfo(&start, &kstart);
#define GET_FINAL_HEAP_INFORMATION    espcp_test_get_mallinfo(&end, &kend);
#define COPY_FINAL_TO_START           memcpy(&start, &end , sizeof(struct mallinfo)); memcpy(&kstart, &kend, sizeof(struct mallinfo));
#define HEAP_USAGE_PASS_OR_FAIL       espcp_test_check_heap_usage(&start, &end, &kstart, &kend, __func__);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#pragma GCC diagnostic ignored "-Wunused-function"

/****************************************************************************
 * Name: espcp_test_output_memory_info
 *
 * Description:
 *  Output the memory allocate statistics to syslog.
 *
 * Input Parameters:
 *  before - Pointer to structure holding initial user heap usage.
 *  after - Pointer to structure holding final user heap usage.
 *  kbefore - Pointer to structure holding initial kernel heap usage.
 *  kafter - Pointer to structure holding final kernel heap usage.
 *  test_name - Name of the test being executed.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void espcp_test_output_memory_info(const struct mallinfo *before, const struct mallinfo *after, 
                                          const struct mallinfo *kbefore, const struct mallinfo *kafter, const char *test_name)
{
    struct mallinfo difference, kdifference;

    difference.arena = before->arena - after->arena;
    difference.uordblks = before->uordblks - after->uordblks;
    if (difference.uordblks < 0)
    {
        difference.uordblks = -difference.uordblks;
    }
    difference.fordblks = before->fordblks - after->fordblks;
    difference.mxordblk = before->mxordblk - after->mxordblk;

    kdifference.arena = kbefore->arena - kafter->arena;
    kdifference.uordblks = kbefore->uordblks - kafter->uordblks;
    if (kdifference.uordblks < 0)
    {
        kdifference.uordblks = -kdifference.uordblks;
    }
    kdifference.fordblks = kbefore->fordblks - kafter->fordblks;
    kdifference.mxordblk = kbefore->mxordblk - kafter->mxordblk;

    if (test_name != NULL)
    {
        syslog(LOGGING_LEVEL, "%s\n", test_name);
    }
    syslog(LOGGING_LEVEL, "                    %11s%11s%11s%11s\n", "Total", "Used", "Free", "Largest");
    syslog(LOGGING_LEVEL, "User Before:        %11d%11d%11d%11d\n", before->arena, before->uordblks, before->fordblks, before->mxordblk);
    syslog(LOGGING_LEVEL, "User After:         %11d%11d%11d%11d\n", after->arena, after->uordblks, after->fordblks, after->mxordblk);
    syslog(LOGGING_LEVEL, "User Difference:    %11d%11d%11d%11d\n", difference.arena, difference.uordblks, difference.fordblks, difference.mxordblk);
    syslog(LOGGING_LEVEL, "Kernel Before:      %11d%11d%11d%11d\n", kbefore->arena, kbefore->uordblks, kbefore->fordblks, kbefore->mxordblk);
    syslog(LOGGING_LEVEL, "Kernel After:       %11d%11d%11d%11d\n", kafter->arena, kafter->uordblks, kafter->fordblks, kafter->mxordblk);
    syslog(LOGGING_LEVEL, "Kernel Difference:  %11d%11d%11d%11d\n", kdifference.arena, kdifference.uordblks, kdifference.fordblks, kdifference.mxordblk);
}

/****************************************************************************
 * Name: espcp_test_check_heap_usage
 *
 * Description:
 *  Check the heap usage and output a message to systlog to indicate if
 *  we have released all of the memory we allocated (PASS) or if we have
 *  consumed more memory than we have released (FAIL). 
 *
 * Input Parameters:
 *  before - Pointer to structure holding initial user heap usage.
 *  after - Pointer to structure holding final user heap usage.
 *  kbefore - Pointer to structure holding initial kernel heap usage.
 *  kafter - Pointer to structure holding final kernel heap usage.
 *  test_name - Name of the test being executed.
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static void espcp_test_check_heap_usage(const struct mallinfo *before, const struct mallinfo *after, 
                                        const struct mallinfo *kbefore, const struct mallinfo *kafter, const char *test_name)
{
    int user_heap = before->uordblks - after->uordblks;
    if (user_heap < 0)
    {
        user_heap = -user_heap;
    }
    int kernel_heap = kbefore->uordblks - kafter->uordblks;
    if (kernel_heap < 0)
    {
        kernel_heap = -kernel_heap;
    }
    if ((user_heap != 0) || (kernel_heap != 0))
    {
        syslog(LOGGING_LEVEL, "    FAIL: %s, Memory not released: user %d, kernel %d\n", test_name, user_heap, kernel_heap);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: Heap memory checks %s\n", test_name);
    }
}

/****************************************************************************
 * Name: espcp_test_get_mallinfo
 *
 * Description:
 *  Get memory information.
 *
 * Input Parameters:
 *   mem - Pointer to structure in which to put the user heap information.
 *   kmem - Pointer to structure in which to put the kernel heap information.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void espcp_test_get_mallinfo(struct mallinfo *mem, struct mallinfo *kmem)
{
#ifdef CONFIG_CAN_PASS_STRUCTS
  *mem = mallinfo();
  *kmem = kmm_mallinfo();
#else
  (void) mallinfo(mem);
  (void) kmm_mallinfo(kmem);
#endif
}

/****************************************************************************
 * Name: espcp_delete_allocated_buffers
 *
 * Description:
 *  Delete any buffers used to hold arguments or results for an ESP32
 *  command.
 *
 * Input Parameters:
 *   command - pointer to the structure holding the command information.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void espcp_delete_allocated_buffers(struct upd_esp32_command *command)
{
    if (command->payload_length != 0)
    {
        if (command->payload != NULL)
        {
            free(command->payload);
        }
    }
    if (command->result_length != 0)
    {
        if (command->result != NULL)
        {
            free(command->result);
        }
    }
}

/****************************************************************************
 * Name: espcp_test_check_result_equal
 *
 * Description:
 *  Check the result for a method for equality and print a pass or fail
 *  message.
 *
 * Input Parameters:
 *  expected - expected value.
 *  actual - actual result.
 *  method_name - method being called.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static void espcp_test_check_result_equal(int expected, int actual, char *method_name)
{
    if (expected == actual)
    {
        syslog(LOGGING_LEVEL, "    PASS: %s\n", method_name);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAILED: %s, expected result = %d, actual result = %d\n", method_name, expected, actual);
    }
}

/****************************************************************************
 * Name: espcp_test_check_result_not_equal
 *
 * Description:
 *  Check the result for a method for inequality and print a pass or fail
 *  message.
 *
 * Input Parameters:
 *  not_expected - value should not be equal to this.
 *  actual - actual result.
 *  method_name - method being called.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void espcp_test_check_result_not_equal(int not_expected, int actual, char *method_name)
{
    if (not_expected != actual)
    {
        syslog(LOGGING_LEVEL, "    PASS: %s\n", method_name);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAILED: %s, expected actual not to equal %d\n", method_name, not_expected);
    }
}

/****************************************************************************
 * Name: espcp_test_check_result_greater
 *
 * Description:
 *  Check the result for a method is greater than a specified value and 
 *  print a pass or fail message.
 * 
 *  For system calls, the expected use of this method will be to check if
 *  the return value of a method is greater than zero.
 *
 * Input Parameters:
 *  limit - value under the lowest value expected.
 *  actual - actual result.
 *  method_name - method being called.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
static void espcp_test_check_result_greater(int limit, int actual, char *method_name)
{
    if (actual > limit)
    {
        syslog(LOGGING_LEVEL, "    PASS: %s\n", method_name);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAILED: %s, expected %d to be greater than %d\n", method_name, actual, limit);
    }
}

/****************************************************************************
 * Name: espcp_test_start_wifi
 *
 * Description:
 *  Connect to a WiFi access point.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void espcp_test_start_wifi(void)
{
    syslog(LOGGING_LEVEL, "********** Starting WiFi.\n");

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    //
    //  Connecting to the WiFi generates two events (if all goes well), One 
    //  once the network interface has started and one once the connection
    //  has completed.
    //
    mqd_t event_queue = mq_open("/Esp32Events", O_RDONLY | O_NONBLOCK);
    DEBUGASSERT(event_queue != -1);

    espcp_wi_fi_credentials_t credentials;
    credentials.network_name = WIFI_NETWORK;
    credentials.password = WIFI_PASSWORD;

    struct upd_esp32_command message;
    memset(&message, 0, sizeof(struct upd_esp32_command));
    message.interface = espcp_esp32_interfaces_wi_fi;
    message.function = espcp_wi_fi_function_connect_to_access_point;
    message.payload_length = espcp_wi_fi_credentials_buffer_size(&credentials);
    message.payload = (uint8_t *) malloc(message.payload_length);
    espcp_encode_wi_fi_credentials(&credentials, message.payload);
    message.block = 1;

    //
    //  Using the UPD method as the .NET managed code passes messages
    //  through this route.
    //
    upd_handle_esp32_command(&message);

    espcp_delete_allocated_buffers(&message);

    //
    //  We need to wait to give the ESP32 time to send both event messages to the STM32.
    //
    usleep(2000000);

    //
    //  Now we need to absorb any events, there should be two for a successful
    //  WiFi connection:
    //  * StartInterfaceEvent
    //  * ConnectEvent
    //
    for (int index = 0; index < 2; index++)
    {
        uint8_t encoded_event_header[22];
        memset(encoded_event_header, 0, sizeof(espcp_event_data_t));
        unsigned int priority;
        int result = mq_receive(event_queue, (char *) &encoded_event_header, 22, &priority);
        DEBUGASSERT(result > 0);
        if (result > 0)
        {
            espcp_event_data_t *event_header = espcp_extract_event_data(encoded_event_header);
            if (event_header->message_id != 0)
            {
                espcp_event_data_payload_t request;
                memset(&request, 0, sizeof(espcp_event_data_payload_t));
                request.message_id = event_header->message_id;

                const int default_payload_length = 4000;
                request.payload_length = default_payload_length;
                if (request.message_id != 0)
                {
                    request.payload = (uint8_t *) malloc(default_payload_length);
                }
                result = upd_handle_esp32_get_event_result(&request);
                DEBUGASSERT(result == OK);
                if (request.payload_length > 0)
                {
                    free(request.payload);
                }
            }
            free(event_header);
        }
    }

    mq_close(event_queue);

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;
}

/****************************************************************************
 * Name: espcp_test_get_battery_level
 *
 * Description:
 *  Get the battery charge level from the ESP32.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void espcp_test_get_battery_level(void)
{
    syslog(LOGGING_LEVEL, "********** Getting battery level\n");

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    struct upd_esp32_command message;
    memset(&message, 0, sizeof(struct upd_esp32_command));
    message.interface = espcp_esp32_interfaces_system;
    message.function = espcp_system_function_get_battery_charge_level;
    message.result_length = 100;
    message.result = (uint8_t *) malloc(message.result_length);
    message.block = 1;

    //
    //  Using the UPD method as the .NET managed code passes messages
    //  through this route.
    //
    upd_handle_esp32_command(&message);

    espcp_delete_allocated_buffers(&message);

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;
}

/****************************************************************************
 * Name: espcp_test_enetdown
 *
 * Description:
 *  Test the POSIX methods when there is no WiFi connection.  All of the
 *  methods should return -ENETDOWN.
 * 
 *  The amount of heap memory is also checked at the end of the method.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *  The ESP32 chip should NOT be connected to an access point for
 *  these tests.
 *
 ****************************************************************************/
static void espcp_test_enetdown(void)
{
    int result;
    struct socket psock = { };
    const size_t buffer_length = 1000;
    char *buffer = (char *) malloc(buffer_length);
    socklen_t sockaddr_length = sizeof(struct sockaddr);
    int option_value = 1;
    struct sockaddr sa = { };

    syslog(LOGGING_LEVEL, "********** ENETDOWN error code response checks.\n");

    //
    //  Note that the heap memory usage is collected after the buffer
    //  is allocated.  It is important that the buffer is freed after
    //  the heap usage data is checked at the end of the method otherwise
    //  the buffer allocation will appear in the statistics.
    //
    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    memset(buffer, 0, buffer_length);
    result = espcp_usrsock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, &psock);
    espcp_test_check_result_equal(-ENETDOWN, result, "socket");
    //
    //  Now assign a dummy socket ID for the rest of the tests.  The initial
    //  socket ID for the ESP32 is 54.
    //
    psock.s_esp32_sockfd = 54;
    //
    result = espcp_usrsock_setsockopt(&psock, 0, 0xb, (void *) &option_value, sizeof(option_value));
    espcp_test_check_result_equal(-ENETDOWN, result, "setsockopt");
    //
    result = espcp_usrsock_connect(&psock, &sa, sizeof(struct sockaddr));
    espcp_test_check_result_equal(-ENETDOWN, result, "connect");
    //
    result = espcp_usrsock_send(&psock, buffer, buffer_length, 0);
    espcp_test_check_result_equal(-ENETDOWN, result, "send");
    //
    result = espcp_usrsock_sendto(&psock, buffer, buffer_length, 0, NULL, 0);
    espcp_test_check_result_equal(-ENETDOWN, result, "sendto");
    //
    result = espcp_usrsock_recvfrom(&psock, buffer, buffer_length, 0, NULL, 0);
    espcp_test_check_result_equal(-ENETDOWN, result, "recvfrom");
    //
    struct socket new_sock = { };
    socklen_t new_sock_length = sizeof(new_sock);
    result = espcp_usrsock_accept(&psock, &sa, &new_sock_length, &new_sock);
    espcp_test_check_result_equal(-ENETDOWN, result, "accept");
    //
    result = espcp_usrsock_bind(&psock, &sa, sizeof(struct sockaddr));
    espcp_test_check_result_equal(-ENETDOWN, result, "bind");
    //
    result = espcp_usrsock_getpeername(&psock, &sa, &sockaddr_length);
    espcp_test_check_result_equal(-ENETDOWN, result, "getpeername");
    //
    result = espcp_usrsock_getsockname(&psock, &sa, &sockaddr_length);
    espcp_test_check_result_equal(-ENETDOWN, result, "getsockname");
    //
    result = espcp_usrsock_ioctl(&psock, 0, (void *) &sa, sockaddr_length);
    espcp_test_check_result_equal(-ENETDOWN, result, "ioctl");
    //
    result = espcp_usrsock_listen(&psock, 0);
    espcp_test_check_result_equal(-ENETDOWN, result, "listen");
    //
    result = espcp_usrsock_read(&psock, buffer, buffer_length);
    espcp_test_check_result_equal(-ENETDOWN, result, "read");
    //
    result = espcp_usrsock_close(&psock);
    espcp_test_check_result_equal(-ENETDOWN, result, "close");
    //

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;

    //
    //  See note at the head of this method for the reason this is
    //  freed after the heap statistics are checked.
    //
    free(buffer);
}

/****************************************************************************
 * Name: espcp_get_simple_web_page_test
 *
 * Description:
 *  Get a simple web page from a web server.
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *  Assumes that WiFi is started and the test web server is accessible.
 * 
 *  The server connects directly to an IP address.  The IP address is defined
 *  in the file secrets.h.
 *
 ****************************************************************************/
void espcp_test_get_simple_web_page(void)
{
    syslog(LOGGING_LEVEL, "********** Getting a simple web page from %s.\n", WEB_SERVER_IP_ADDRESS);

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: socket - Failed to create socket.\n");
        return;
    }
    else
    {
        syslog(LOGGING_LEVEL, "    Pass: socket - Created socket.\n");
    }
    
    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(WEB_SERVER_IP_ADDRESS);
	server.sin_family = AF_INET;
	server.sin_port = htons( 80 );

	if (connect(sd, (struct sockaddr *) &server, sizeof(server)) < 0)
	{
		syslog(LOGGING_LEVEL, "    FAIL: connect - Failed to connect to %s.\n", WEB_SERVER_IP_ADDRESS);
		return;
	}
    else
    {
        syslog(LOGGING_LEVEL, "    Pass: connect - Connected to %s.\n", WEB_SERVER_IP_ADDRESS);
    }

    int buffer_length = 1024;
    char buffer[buffer_length];
    sprintf(buffer, "GET / HTTP/1.1\r\n\r\n");
	if (send(sd, buffer, strlen(buffer), 0) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: send - Failed to send GET request message.\n");
        return;
    }
    else
    {
        syslog(LOGGING_LEVEL, "    Pass: send - Sent GET request message.\n");
    }

    int bytes_read = recvfrom(sd, buffer, buffer_length, 0, NULL, 0);
    if (bytes_read < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: recvfrom - Failed to receive server reply.\n");
        return;
    }
    else
    {
        syslog(LOGGING_LEVEL, "    Pass: recvfrom - Received server reply (%d bytes).\n", bytes_read);
    }

    if (close(sd) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: close - Failed to close socket.\n");
        return;
    }
    else
    {
        syslog(LOGGING_LEVEL, "    Pass: close - Closed socket.\n");
    }

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;
}

/****************************************************************************
 * Name: espcp_execute_network_tests
 *
 * Description:
 *  Execute any network tests.
 *
 * Input Parameters:
 *   arg - Argument passed to kernel test via CLI
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
void espcp_execute_tests(uint32_t arg)
{
    syslog(LOGGING_LEVEL, "Executing network tests.\n");
    usleep(200);

    syslog(LOGGING_LEVEL, "Waiting for ESP32 to indicate it is ready.\n");
    bool waiting_for_esp32 = true;
    while (waiting_for_esp32)
    {
      espcp_config_lock();
      espcp_configuration_t *config = espcp_get_configuration();
      if (!config->esp_not_responding)
      {
        waiting_for_esp32 = false;
      }
      espcp_config_unlock();
      if (waiting_for_esp32)
      {
          usleep(500000);   // 500 ms
      }
    }

    syslog(LOGGING_LEVEL, "ESP32 is now responding.\n");
    usleep(500000);

    espcp_test_get_battery_level();
    espcp_test_enetdown();

    espcp_test_start_wifi();
    //
    //  We can start some actual network tests now we are connected to an 
    //  access point.
    //
    espcp_test_get_simple_web_page();

    syslog(LOGGING_LEVEL, "Network tests completed.\n");
}