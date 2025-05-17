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

#if defined(CONFIG_ESP_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>	//hostent
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../meadow-upd.h"
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_kernel_tests.h>
#include "../espcp/espcp_usrsock.h"
#include "../espcp/espcp_common.h"
#include "../espcp/espcp_file_system.h"
#include "../hcom_nx/hcom_nx_config_manager.h"

#include "../espcp/espcp_test_heap_tracing.h"

#include "network_tests.h"

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

    if (network_tests_configuration == NULL)
    {
        syslog(LOGGING_LEVEL, "    FAIL: No network tests configuration.\n");
        return;
    }

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
    access_point.network_name = network_tests_configuration->ssid;
    access_point.password = network_tests_configuration->password;

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
    
    GET_FINAL_HEAP_INFORMATION;
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
 * Name: espcp_test_file_system_format
 *
 * Description:
 *  Test formatting the file system on the ESP32.
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
static void espcp_test_file_system_format(void)
{
    if (espcp_file_system_format() == 0)
    {
        syslog(LOGGING_LEVEL, "    PASS: Formatting file system.\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Formatting file system.\n");
    }
}

/****************************************************************************
 * Name: espcp_test_file_system_list_files
 *
 * Description:
 *  Test getting the list of files from the ESP32 just after the file system
 *  has been formatted.
 * 
 *  The file system will be empty just after formatting.
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
static void espcp_test_file_system_list_files(void)
{
    espcp_file_system_info_t *files = espcp_file_system_list_files();
    if (files != NULL)
    {
        if ((files->number_of_files == 0) && (files->files == NULL))
        {
            syslog(LOGGING_LEVEL, "    PASS: Getting file details from the file system.\n");
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: 1 - Getting file details from the file system.\n");
        }
        espcp_file_system_info_dispose(files);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: 2 - Getting file details from the file system.\n");
    }
}

/****************************************************************************
 * Name: espcp_test_file_system_list_files2
 *
 * Description:
 *  Test getting the list of files from the ESP32 just after sme files have
 *  been written to the file system.
 *
 * Input Parameters:
 *   name1 - Name of the first file on the file system.
 *   name12 - Name of the second file on the file system.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void espcp_test_file_system_list_files2(char *name1, char *name2)
{
    espcp_file_system_info_t *files = espcp_file_system_list_files();
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
            }
            if (pass)
            {
                syslog(LOGGING_LEVEL, "    PASS: Getting file details (2) from the file system.\n");
            }
            else
            {
                syslog(LOGGING_LEVEL, "    FAIL: 1 - Getting file details (2) from the file system.\n");
            }
            espcp_file_system_info_dispose(files);
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
}

/****************************************************************************
 * Name: espcp_test_file_system_write_file
 *
 * Description:
 *  Test writing a file to the file system
 *
 * Input Parameters:
 *   name - Name of the file to write
 *   contents - Buffer holding the data to be written to the file.
 *   length - Number of bytes to be written.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void espcp_test_file_system_write_file(char *name, uint8_t *contents, int16_t length)
{
    int result;
 
    result = espcp_file_system_write_file(NULL, contents, length);
    if (result == -1)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing file to the file system (file name is NULL).\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing file to the file system (file name is NULL).\n");
    }
    
    result = espcp_file_system_write_file(name, NULL, length);
    if (result == -1)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing file to the file system (contents is NULL).\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing file to the file system (contents is NULL).\n");
    }
    
    result = espcp_file_system_write_file(name, contents, length);
    if (result == 0)
    {
        syslog(LOGGING_LEVEL, "    PASS: Writing file to the file system.\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Writing file to the file system.\n");
    }
}

/****************************************************************************
 * Name: espcp_test_file_system_read_file
 *
 * Description:
 *  Test reading a file from the file system.
 *
 * Input Parameters:
 *   name - Name of the file to be read.
 *   expectedContents - Data that should be in the file.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void espcp_test_file_system_read_file(char *name, char *expectedContents)
{
    int16_t length;
    uint8_t *contents;
    
    contents = espcp_file_system_read_file(NULL, &length);
    if (contents == NULL)
    {
        syslog(LOGGING_LEVEL, "    PASS: Reading a file from the file system (name is NULL).\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Reading a file from the file system (name is NULL).\n");
    }

    contents = espcp_file_system_read_file(name, NULL);
    if (contents == NULL)
    {
        syslog(LOGGING_LEVEL, "    PASS: Reading a file from the file system (&length is NULL).\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Reading a file from the file system (&length is NULL).\n");
    }

    contents = espcp_file_system_read_file("DoesNotExist", &length);
    if (contents == NULL)
    {
        syslog(LOGGING_LEVEL, "    PASS: Reading a file from the file system (file does not exist).\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Reading a file from the file system (file does not exist).\n");
    }

    contents = espcp_file_system_read_file(name, &length);
    if (contents == NULL)
    {
        syslog(LOGGING_LEVEL, "    FAIL: 1 - Reading a file from the file system.\n");
    }
    else
    {
        if ((length == strlen(expectedContents)) && (memcmp(contents, expectedContents, length) == 0))
        {
            syslog(LOGGING_LEVEL, "    PASS: Reading a file from the file system.\n");
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: 2 - Reading a file from the file system, length %d, contents: '%s'.\n", length, (char *) contents);
        }
        free(contents);
    }
}

/****************************************************************************
 * Name: espcp_test_file_system_delete_file
 *
 * Description:
 *  Test deleting a file from the file system.
 * 
 *  This assumes that two files are already on the file system and have the
 *  names specified.
 *
 * Input Parameters:
 *   name - Name of the file to be deleted.
 *   remainingFile - Name of the file that should be left on the file system.
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
static void espcp_test_file_system_delete_file(char *name, char *remainingFile)
{
    if (espcp_file_system_delete_file(NULL) == -1)
    {
        syslog(LOGGING_LEVEL, "    PASS: Delete a file from the file system (name is NULL).\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Delete a file from the file system (name is NULL).\n");
    }

    if (espcp_file_system_delete_file("DoesNotExist") == -ENOENT)
    {
        syslog(LOGGING_LEVEL, "    PASS: Delete a file from the file system (file does not exist).\n");
    }
    else
    {
        syslog(LOGGING_LEVEL, "    FAIL: Delete a file from the file system (file does not exist).\n");
    }

    if (espcp_file_system_delete_file(name) == 0)
    {
        espcp_file_system_info_t *files = espcp_file_system_list_files();
        if (files != NULL)
        {
            if ((files->number_of_files == 1) && (files->files != NULL) && (strcmp(files->files[0].name, remainingFile) == 0))
            {
                espcp_file_system_info_dispose(files);
                syslog(LOGGING_LEVEL, "    PASS: Deleting file from the file system.\n");
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
}

/****************************************************************************
 * Name: espcp_test_fill_file_system
 *
 * Description:
 *  Test adding files to the file system until it is full.
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
static void espcp_test_fill_file_system(void)
{
    #define MAXIMUM_FILE_SIZE   16384
    #define NUMBER_OF_FILES     12
    char name[20];
    uint8_t *file_contents;

    file_contents = (uint8_t *) malloc(MAXIMUM_FILE_SIZE);
    if (file_contents == NULL)
    {
        syslog(LOGGING_LEVEL, "    FAIL: Allocating memory for the file contents\n");
        return;
    }
    for (int index = 0; index < MAXIMUM_FILE_SIZE; index++)
    {
        file_contents[index] = 0xaa;
    }
    //
    //  Start with a clean file system.
    //
    if (espcp_file_system_format() != 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: Filling the file system (initialisation)\n");
        return;
    }
    //
    //  Start to fill the file system.  We should be able to get (NUMBER_OF_FILES - 1) x 16K files on the file system.
    //
    syslog(LOGGING_LEVEL, "    Info: Writing %d files to the file system, this may take some time.\n", NUMBER_OF_FILES);
    int index = 0;
    for (index = 0; index < NUMBER_OF_FILES; index++)
    {
        sprintf(name, "File%d", index);
        syslog(LOGGING_LEVEL, "    Info: Writing file %s file to the file system.\n", name);
        int result = espcp_file_system_write_file(name, file_contents, MAXIMUM_FILE_SIZE);
        if (result < 0)
        {
            syslog(LOGGING_LEVEL, "    FAIL: Filling the file system (writing file %d), result: %d\n", index, result);
            free(file_contents);
            return;
        }
    }
    //
    //  If we try to write file the next file as a 16K file, it should fail as there is not enough space on the file system.
    //
    sprintf(name, "File%d", index);
    syslog(LOGGING_LEVEL, "    Info: Writing file %s file to the file system.\n", name);
    if (espcp_file_system_write_file(name, file_contents, MAXIMUM_FILE_SIZE) == 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: Filling the file system (writing file %d as 16K file)\n", index);
        free(file_contents);
        return;
    }
    free(file_contents);

    if (espcp_file_system_format() != 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: Filling the file system (clean up)\n");
        return;
    }

    syslog(LOGGING_LEVEL, "    PASS: Filling the file system\n");
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

    espcp_test_file_system_format();

    espcp_test_file_system_list_files();

    char *myText = "Hello, world.";
    int16_t length;
    char *name1 = "hello1.txt";
    char *name2 = "hello2.txt";
    char buffer[20];

    strcpy(buffer, myText);
    length = strlen(buffer);
    syslog(LOGGING_LEVEL, "    INFO: Creating file %s, contents '%s', length %d\n", name1, buffer, length);
    espcp_test_file_system_write_file(name1, (uint8_t *) buffer, length);

    strcpy(buffer, myText);
    length = strlen(buffer);
    syslog(LOGGING_LEVEL, "    INFO: Creating file %s, contents '%s', length %d\n", name2, buffer, length);
    espcp_file_system_write_file(name2, (uint8_t *) buffer, length);

    espcp_test_file_system_read_file(name1, myText);

    espcp_test_file_system_list_files2(name1, name2);

    espcp_test_file_system_delete_file(name2, name1);

    espcp_test_fill_file_system();

    syslog(LOGGING_LEVEL, "    INFO: Reformatting file system\n");
    espcp_file_system_format();

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;
}

/****************************************************************************
 * Name: espcp_test_wait_for_esp_to_be_ready
 *
 * Description:
 *  Wait for the ESP32 to be ready and responding.
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
void espcp_test_wait_for_esp_to_be_ready(void)
{
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
}

/****************************************************************************
 * Name: meadow_kt_espcp_test_get_web_resource
 *
 * Description:
 *  Load test downloading a simple web page.
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
void meadow_kt_espcp_test_get_web_resource(uint32_t arg)
{
    syslog(LOGGING_LEVEL, "Testing the download web resources\n");

    espcp_test_wait_for_esp_to_be_ready();
    
    espcp_test_start_wifi();

    if (arg == 0)
    {
        arg = 1;
    }

    network_test_get_web_resource(arg, network_tests_configuration->server_ip, network_tests_configuration->server_port, network_tests_configuration->resource);

    syslog(LOGGING_LEVEL, "Download of multiple web resources completed.\n");
}

/****************************************************************************
 * Name: meadow_kt_espcp_tests
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
void meadow_kt_espcp_tests(uint32_t arg)
{
    syslog(LOGGING_LEVEL, "\n");
    syslog(LOGGING_LEVEL, "\n");
    syslog(LOGGING_LEVEL, "Executing ESP32 tests.\n");
    usleep(200);

    espcp_test_wait_for_esp_to_be_ready();

    espcp_test_file_system();
    
    espcp_test_heap_trace_messages();

    espcp_test_get_battery_level();
    espcp_test_configuration_items();

    espcp_test_enetdown();

    espcp_test_start_wifi();

    //
    //  We can start some actual network tests now we are connected to an 
    //  access point.
    //
    espcp_test_misc_network_functions();
    network_test_get_web_resource(arg, network_tests_configuration->server_ip, network_tests_configuration->server_port, network_tests_configuration->resource);

    syslog(LOGGING_LEVEL, "ESP32 tests completed.\n");
}

#endif