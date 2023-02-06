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
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../meadow-upd.h"
#include <meadow/hcom_shared_common.h>
#include "espcp_usrsock.h"
#include "espcp_common.h"
#include "espcp_coprocessor.h"
#include "espcp_system.h"
#include "espcp_file_system.h"
#include "../hcom_nx/hcom_nx_config_manager.h"

#include "espcp_test_heap_tracing.h"

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
#define WIFI_NETWORK                "Dummy, do not use"
#define WIFI_PASSWORD               "Use contents of secrets.h"
#define SIMPLE_WEB_SERVER_NAME      "pi4-ubuntu-001"
#define SIMPLE_WEB_PAGE             "/"
#define WEB_SERVER_IP_ADDRESS       "127.0.0.1"
#define WEB_SERVER_PORT             80
#endif

//
//  Default logging level for this file.
//
#define LOGGING_LEVEL   1

//
//  Delay following the tests to allow any events to be processed.
//
#define DELAY           2000000

/****************************************************************************
 * Private variables and associated macros.
 ****************************************************************************/

/**
 * @brief Number of tests that have been executed.
 */
uint tests_run = 0;

/**
 * @brief Number of tests that have passed.
 */
uint tests_passed = 0;

/**
 * @brief Number of tests that have failed.
 */
uint tests_failed = 0;

#define TEST_PASSED     tests_run++; tests_passed++;
#define TEST_FAILED     tests_run++; tests_failed++;

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
void espcp_test_output_memory_info(const struct mallinfo *before, const struct mallinfo *after, 
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
void espcp_test_check_heap_usage(const struct mallinfo *before, const struct mallinfo *after, 
                                 const struct mallinfo *kbefore, const struct mallinfo *kafter, const char *test_name)
{
    int user_heap = after->uordblks - before->uordblks;
    int kernel_heap = kafter->uordblks - kbefore->uordblks;
    if ((user_heap != 0) || (kernel_heap != 0))
    {
        if ((user_heap > 0) || (kernel_heap > 0))
        {
            syslog(LOGGING_LEVEL, "    FAIL: %s, Memory not released: user %d, kernel %d\n", test_name, user_heap, kernel_heap);
        }
        else
        {
            syslog(LOGGING_LEVEL, "    CHECK: %s, Change in memory allocation: user %d, kernel %d\n", test_name, user_heap, kernel_heap);
        }
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
void espcp_test_get_mallinfo(struct mallinfo *mem, struct mallinfo *kmem)
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

    //
    //  Connecting to the WiFi generates two events (if all goes well), One 
    //  once the network interface has started and one once the connection
    //  has completed.
    //
    mqd_t event_queue = mq_open("/Esp32Events", O_RDONLY | O_NONBLOCK);
    DEBUGASSERT(event_queue != -1);

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    espcp_access_point_information_t access_point = { };
    access_point.network_name = WIFI_NETWORK;
    access_point.password = WIFI_PASSWORD;

    struct upd_esp32_command message;
    memset(&message, 0, sizeof(struct upd_esp32_command));
    message.interface = espcp_esp32_interfaces_wi_fi;
    message.function = espcp_wi_fi_function_connect_to_access_point;
    message.payload_length = espcp_access_point_information_buffer_size(&access_point);
    message.payload = (uint8_t *) malloc(message.payload_length);
    espcp_encode_access_point_information(&access_point, message.payload);
    message.block = 1;

    //
    //  Using the UPD method as the .NET managed code passes messages
    //  through this route.
    //
    upd_handle_esp32_command(&message);

    espcp_delete_allocated_buffers(&message);

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
    usleep(2 * DELAY);

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;

    mq_close(event_queue);
}
/****************************************************************************
 * Name: espcp_test_configuration_items
 *
 * Description:
 *  Set a configuration item.
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
static void espcp_test_configuration_items(void)
{
    syslog(LOGGING_LEVEL, "********** Checking configuration items.\n");

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    int retry_count = 4;
    int result = hcom_nx_config_set_esp_integer_value(espcp_configuration_items_maximum_retry_count, retry_count);
    if (result == OK)
    {
        syslog(LOGGING_LEVEL, "    PASS: Setting MaximumRetryCount\n");
        return;
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Setting MaximumRetryCount\n");
    }

    usleep(DELAY);

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

    usleep(DELAY);

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
    struct pollfd pollfds[] = { { psock.s_esp32_sockfd, POLLIN | POLLOUT, 5} };
    result = espcp_usrsock_poll(&psock, pollfds, 1);
    espcp_test_check_result_equal(-ENETDOWN, result, "poll");
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

    usleep(DELAY);

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;

    //
    //  See note at the head of this method for the reason this is
    //  freed after the heap statistics are checked.
    //
    free(buffer);
}

/****************************************************************************
 * Name: espcp_tests_get_html_page
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
void espcp_tests_get_html_page(void)
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
        syslog(LOGGING_LEVEL, "    PASS: socket - Created socket.\n");
    }
    
    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(WEB_SERVER_IP_ADDRESS);
	server.sin_family = AF_INET;
	server.sin_port = htons(WEB_SERVER_PORT);

	if (connect(sd, (struct sockaddr *) &server, sizeof(server)) < 0)
	{
		syslog(LOGGING_LEVEL, "    FAIL: connect - Failed to connect to %s.\n", WEB_SERVER_IP_ADDRESS);
		return;
	}
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: connect - Connected to %s.\n", WEB_SERVER_IP_ADDRESS);
    }

    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    if (getpeername(sd, &addr, &addrlen) < 0)
    {
		syslog(LOGGING_LEVEL, "    FAIL: getpeername - Failed.\n");
		return;
    }
    else
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) &addr;
        if ((sin->sin_addr.s_addr == inet_addr(WEB_SERVER_IP_ADDRESS)) && (sin->sin_port == htons(WEB_SERVER_PORT)))
        {
            syslog(LOGGING_LEVEL, "    PASS: getpeername - Socket address details are correct.\n");
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: getpeername - Socket address details are incorrect.\n");
        }
    }

    struct pollfd pollfds[] = { { sd, POLLIN | POLLOUT, 0} };
    if (poll(pollfds, 1, 500) < 0)
	{
		syslog(LOGGING_LEVEL, "    FAIL: poll - Failed.\n");
		return;
	}
    else
    {
        if (pollfds[0].revents & POLLOUT)
        {
            syslog(LOGGING_LEVEL, "    PASS: poll - Socket ready for output.\n");
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: poll - Socket is not ready for output.\n");
        }
    }

    int buffer_length = 1024;
    char buffer[buffer_length];
    sprintf(buffer, "GET /get.html HTTP/1.1\r\n\r\n");
	if (send(sd, buffer, strlen(buffer), 0) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: send - Failed to send GET request message.\n");
        return;
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: send - Sent GET request message.\n");
    }

    pollfds[0].fd = sd;
    pollfds[0].events = POLLIN | POLLOUT;
    pollfds[0].revents = 0;
    if (poll(pollfds, 1, 500) < 0)
	{
		syslog(LOGGING_LEVEL, "    FAIL: poll - Failed.\n");
		return;
	}
    else
    {
        if (pollfds[0].revents & POLLIN)
        {
            syslog(LOGGING_LEVEL, "    PASS: poll - Socket ready for input.\n");
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: poll - Socket is not ready for input.\n");
        }
    }

    int bytes_read = recvfrom(sd, buffer, buffer_length, 0, NULL, 0);
    if (bytes_read < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: recvfrom - Failed to receive server reply.\n");
        return;
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: recvfrom - Received server reply (%d bytes).\n", bytes_read);
    }

    if (close(sd) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: close - Failed to close socket.\n");
        return;
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: close - Closed socket.\n");
    }

    usleep(DELAY);

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;
}

/****************************************************************************
 * Name: espcp_test_get_multiple_web_pages
 *
 * Description:
 *  Get a simple web page from a web server multiple times.
 *
 * Input Parameters:
 *   number_of_requests - number of requests to make.
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
void espcp_test_get_multiple_web_pages(int number_of_requests)
{
    syslog(LOGGING_LEVEL, "********** Getting a simple web page from %s.\n", WEB_SERVER_IP_ADDRESS);

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    for (int index = 0; index < number_of_requests; index++)
    {
        espcp_tests_get_html_page();
    }

    usleep(2 * DELAY);

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;
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

    espcp_tests_get_html_page();

    usleep(2 * DELAY);

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;
}

/****************************************************************************
 * Name: espcp_test_misc_network_functions
 *
 * Description:
 *  Testing misc network functions
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
static void espcp_test_misc_network_functions(void)
{
    syslog(LOGGING_LEVEL, "********** Testing misc network functions.\n");

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    //
    //  getifaddrs will generate calls to ioctl on the ESP32.
    //
    struct ifaddrs *ifa;
    if (getifaddrs(&ifa) == ERROR)
    {
        syslog(LOGGING_LEVEL, "    FAIL: getifaddrs (ioctl) - Failed to get interface names.\n");
        return;
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: getifaddrs (ioctl)\n");
        freeifaddrs(ifa);
    }
    
    HEAP_USAGE_PASS_OR_FAIL;
    COPY_FINAL_TO_START;

    int errors = 0;
    int sd = socket(NET_SOCK_FAMILY, NET_SOCK_TYPE, NET_SOCK_PROTOCOL);
    if (sd < 0)
    {
        errors = 1;
    }
    else
    {
        struct lifreq req;

        memset(&req, 0, sizeof(req));
        req.lifr_ifindex = 1;

        if (ioctl(sd, SIOCGIFNAME, (unsigned long) &req) < 0)
        {
            errors = 1;
        }

        if (ioctl(sd, SIOCGIFFLAGS, (unsigned long) &req) < 0)
        {
            errors = 1;
        }
        close(sd);
    }
    if (errors == 1)
    {
        syslog(LOGGING_LEVEL, "    FAIL: ioctl\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: ioctl\n");
    }

    usleep(DELAY);          // Wait for the messages to be processed.

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;
}

/****************************************************************************
 * Name: espcp_test_heap_trace_messages
 *
 * Description:
 *  Test turning the heap tracing (on the ESP32) on and off
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
static void espcp_test_heap_trace_messages(void)
{
    syslog(LOGGING_LEVEL, "********** Turning heap tracing on the ESP32 on and off.\n");

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    espcp_system_start_esp_heap_trace();
    
    usleep(DELAY);          // Wait for the messages to be processed.

    espcp_system_stop_esp_heap_trace();

    usleep(DELAY);          // Wait for the messages to be processed.

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;
}

/****************************************************************************
 * Name: espcp_test_file_system
 *
 * Description:
 *  Test the file system on the ESP32.
 *      - Format the file system.
 *      - List files on the file system
 *      - Write a file to the file system
 *      - Read a file from the file system
 *      - Delete a file from the file system
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
static void espcp_test_file_system(void)
{
    syslog(LOGGING_LEVEL, "********** Checking ESP32 file system.\n");

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;
    //
    //  Format.
    //
    if (espcp_file_system_format() == 0)
    {
        syslog(LOGGING_LEVEL, "    PASS: Formatting file system.\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Formatting file system.\n");
    }
    //
    //  List files.
    //
    espcp_file_system_info_t *files = espcp_file_system_list_files();
    if (files != NULL)
    {
        if ((files->number_of_files == 0) && (files->files == NULL))
        {
            syslog(LOGGING_LEVEL, "    PASS: Getting file details from the file system.\n");
            free(files);
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: 1 - Getting file details from the file system.\n");
        }
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: 2 - Getting file details from the file system.\n");
    }
    //
    //  Write a file.
    //
    uint32_t length;
    char *myText = "Hello, world.";
    char *name1 = "hello1.txt";
    char *name2 = "hello2.txt";
    int result = espcp_file_system_write_file(name1, (uint8_t *) myText, strlen(myText));
    if (result == 0)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing file to the file system.\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing file to the file system.\n");
    }
    //
    //  Read a file.
    //
    uint8_t *contents = espcp_file_system_read_file(name1, &length);
    if (contents == NULL)
    {
        syslog(LOGGING_LEVEL, "    FAIL: 1 - Reading a file from the file system.\n");
    }
    else
    {
        if ((length == strlen(myText)) && (memcmp(contents, myText, length) == 0))
        {
            syslog(LOGGING_LEVEL, "    PASS: Reading a file from the file system.\n");
            free(contents);
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: 2 - Reading a file from the file system, length %d, contents: '%s'.\n", length, (char *) contents);
        }
    }
    //
    //  Write a second file ready for retesting list files.
    //
    result = espcp_file_system_write_file(name2, (uint8_t *) myText, strlen(myText));
    if (result == 0)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing second file to the file system.\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing second file to the file system.\n");
    }
    files = espcp_file_system_list_files();
    if (files != NULL)
    {
        if ((files->number_of_files == 2) && (files->files != NULL))
        {
            bool pass = true;
            for (int index = 0; index < files->number_of_files; index++)
            {
                if ((strcmp(files->files[index].name, name1) != 0) && (strcmp(files->files[index].name, name2) != 0))
                {
                    pass = false;
                }
                free(files->files[index].name);
            }
            free(files->files);
            free(files);
            if (pass)
            {
                syslog(LOGGING_LEVEL, "    PASS: Getting file details (2) from the file system.\n");
            }
            else
            {
                syslog(LOGGING_LEVEL, "    FAIL: 1 - Getting file details (2) from the file system.\n");
            }
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: 2 - Getting file details (2) from the file system.\n");
        }
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: 3 - Getting file details (2) from the file system.\n");
    }    
    //
    //  Delete file.
    //
    if (espcp_file_system_delete_file(name2) == 0)
    {
        files = espcp_file_system_list_files();
        if (files != NULL)
        {
            if ((files->number_of_files == 1) && (files->files != NULL) && (strcmp(files->files[0].name, name1) == 0))
            {
                free(files->files[0].name);
                free(files->files);
                free(files);
                syslog(LOGGING_LEVEL, "    PASS: GDeleting file from the file system.\n");
            }
            else
            {
                syslog(LOGGING_LEVEL, "    FAIL: 1 -  file from the file system.\n");
            }
        }
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: 2 - Deleting file from the file system.\n");
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
    syslog(LOGGING_LEVEL, "\n");
    syslog(LOGGING_LEVEL, "\n");
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

    espcp_test_heap_trace_messages();

    espcp_test_get_battery_level();
    espcp_test_configuration_items();

    espcp_test_file_system();
    
    espcp_test_enetdown();

    espcp_test_start_wifi();
    //
    //  We can start some actual network tests now we are connected to an 
    //  access point.
    //
    espcp_test_misc_network_functions();
    espcp_test_get_simple_web_page();
    if (arg > 0)
    {
        espcp_test_get_multiple_web_pages(arg);
    }

    syslog(LOGGING_LEVEL, "Network tests completed.\n");
}