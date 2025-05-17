/****************************************************************************
 * network_tests.c
 *
 *   Copyright (C) 2023 Wilderness Labs. All rights reserved.
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
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

#if defined(CONFIG_ESP_TESTS) || defined(CONFIG_ETHERNET_TESTS) || defined(CONFIG_BG77_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS)

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>
#include <nuttx/mm/mm.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <nuttx/net/usrsock.h>

#include "../espcp/espcp_test_heap_tracing.h"

/****************************************************************************
 * Compiler directives.
 ****************************************************************************/

 #pragma GCC diagnostic ignored "-Wunused-function"

/****************************************************************************
 * Local defines.
 ****************************************************************************/

//
//  Default logging level for this file.
//
#define LOGGING_LEVEL   1

//
//  Delay following the tests to allow any events to be processed.
//
#define DELAY           2000000

//
//  Size of the buffer used to send / receive data to / from the server.
//
#define BUFFER_SIZE     4096

/**
 * @brief Time out for any poll requests.
 */
const int POLL_TIMEOUT = 500;

/****************************************************************************
 * Private variables.
 ****************************************************************************/

 /**
  * @brief Buffer to hold the data received from the server.
  */
 static uint8_t _read_buffer[BUFFER_SIZE];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: network_test_wait_for_poll_event
 *
 * Description:
 *  Poll the specified socket for the specified events.  Repeat the poll
 *  until the event is received or the number of attempts is exceeded.
 *
 * Input Parameters:
 *   sd - Socket descriptor to poll.
 *   events - Events to poll for.
 *   attempts - Number of attempts to make.
 *
 * Returned Value:
 *   0 on success, -1 on failure.
 *
 * Assumptions/Limitations:
 *   None.
 *
 ****************************************************************************/
static int network_test_wait_for_poll_event(int sd, int events, int attempts)
{
    struct pollfd pollfds[] = { { sd, events, 0} };
    uint32_t attempt = 0;
    while ((poll(pollfds, 1, 500) < 0) || ((pollfds[0].revents & events) == 0))
    {
        if (attempt > 10)
        {
            return(-1);
        }
        usleep(10000);
        attempt++;
    }

    return(pollfds[0].revents & events ? 0 : -1);
}

/****************************************************************************
 * Name: network_test_number_with_commas
 *
 * Description:
 *   Generate a string that has the number provided formatted with commas in
 *   the correct place for thousands etc.abort
 *
 *   e.g.
 *       4096 becomes 4,096
 *
 * Input Parameters:
 *   number - Number to be formatted.
 *
 * Returned Value:
 *   Pointer to a buffer holding the formatted number, NULL indicates failure.
 *
 * Assumptions/Limitations:
 *   It is the responsibility of the caller to dispose of the memory holding
 *   the number by calling free.
 *
 ****************************************************************************/
char *network_test_number_with_commas(uint32_t number)
{
    uint32_t buffer_size = 14;              // 10 digits, plus 3 commas (max) + terminator.
    char *buffer = malloc(buffer_size);

    if (buffer == NULL)
    {
        return NULL;
    }

    size_t len = snprintf(buffer, buffer_size, "%u", number);

    if (len <= 3)
    {
        return buffer;
    }

    int commas = (len - 1) / 3;
    char *p = buffer + len;
    for (int index = 0; index < commas; index++)
    {
        p -= 3;
        memmove(p + 1, p, strlen(p) + 1);
        *p = ',';
    }

    return buffer;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: network_test_get_resource
 *
 * Description:
 *   Get a resource from a server.
 *
 * Input Parameters:
 *   address - IP address of the server.
 *   port - Port number on the server.
 *   request - Request to be sent to the server.
 *
 * Returned Value:
 *   0 on success, -1 on failure.
 *
 * Assumptions/Limitations:
 *  1 - The network (Ethernet / WiFi etc.) is available and the test web
 *      server is accessible.
 *  2 - The caller will validate heap usage.
 *
 ****************************************************************************/
int network_test_get_resource(in_addr_t address, in_port_t port, char *request)
{
    int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: socket - Failed to create socket.\n");
        return(-1);
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = address;
    server.sin_family = AF_INET;
    server.sin_port = port;

	if (connect(sd, (struct sockaddr *) &server, sizeof(server)) < 0)
	{
		syslog(LOGGING_LEVEL, "    FAIL: connect - Failed to connect server.\n");
        return(-1);
	}

    struct sockaddr addr;
    socklen_t addrlen = sizeof(addr);
    if (getpeername(sd, &addr, &addrlen) < 0)
    {
		syslog(LOGGING_LEVEL, "    FAIL: getpeername - Failed.\n");
        return(-1);
    }
    else
    {
        struct sockaddr_in *sin = (struct sockaddr_in *) &addr;
        if (!((sin->sin_addr.s_addr == address)) && (sin->sin_port == port))
        {
            syslog(LOGGING_LEVEL, "    FAIL: getpeername - Socket address details are incorrect.\n");
            return(-1);
        }
    }

    if (network_test_wait_for_poll_event(sd, POLLOUT, 10) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: poll - Socket is not ready for output.\n");
        return(-1);
    }

	if (send(sd, request, strlen(request), 0) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: send - Failed to send GET request message.\n");
        return(-1);
    }

    if (network_test_wait_for_poll_event(sd, POLLIN, 10) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: poll - Socket is not ready for input.\n");
        return(-1);
    }

    int bytes_read = 0;
    const int amount_to_read = BUFFER_SIZE;
    int total_bytes = 0;
    do
    {
        bytes_read = recvfrom(sd, _read_buffer, amount_to_read, 0, NULL, 0);
        if (bytes_read < 0)
        {
            syslog(LOGGING_LEVEL, "    FAIL: recvfrom - Failed to receive server reply.\n");
            return(-1);
        }
        total_bytes += bytes_read;
    }
    while (bytes_read > 0);

    if (close(sd) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: close - Failed to close socket.\n");
        return(-1);
    }

    return(total_bytes);
}

/****************************************************************************
 * Name: network_test_get_web_resource
 *
 * Description:
 *  Get a large file from a web server.
 *
 *  The file location is made up of three component:
 *
 *  webserver_ip:webserver_port/resource
 *
 *  This method will retrieve the resource only, no validation is performed
 *  and all data is disposed of after the method calls recvfrom.
 *
 * Input Parameters:
 *   webserver_ip - IP address of the web server.
 *   webserver_port - Port number on the web server.
 *   resource - Resource to be retrieved.
 *
 * Returned Value:
 *   0 on success, -1 on failure.
 *
 * Assumptions/Limitations:
 *  1 - The network (Ethernet / WiFi etc.) is available and the test web
 *      server is accessible.
 *  2 - The caller will validate heap usage.
 *
 ****************************************************************************/
int network_test_get_web_resource(uint32_t number_of_requests, char *webserver_ip, uint16_t webserver_port, char *resource)
{
    if (number_of_requests == 0)
    {
        number_of_requests = 1;
    }

    syslog(LOGGING_LEVEL, "********** Getting %s from %s, %u request%s\n", resource, webserver_ip, number_of_requests, number_of_requests == 1 ? "" : "s");

    in_addr_t address = inet_addr(webserver_ip);
    in_port_t port = htons(webserver_port);

    char buffer[100];
    snprintf(buffer, 100, "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", resource, webserver_ip);

    time_t start;
    time(&start);
    uint32_t total_bytes = 0;
    for (uint32_t index = 0; index < number_of_requests; index++)
    {
        int result = network_test_get_resource(address, port, buffer);
        if (result < 0)
        {
            syslog(LOGGING_LEVEL, "    FAIL: network_test_get_resource - Failed to get resource.\n");
            return(-1);
        }
        total_bytes += result;
    }
    time_t end;
    time(&end);
    double seconds = difftime(end, start);

    if (seconds == 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: network_test_get_resource - Time taken is 0.\n");
        return(-1);
    }

    char *formatted_number = network_test_number_with_commas(total_bytes);
    if (formatted_number != NULL)
    {
        syslog(LOGGING_LEVEL, "    PASS: Downloaded %s bytes in %.2f seconds, %.2f KBytes per second\n", formatted_number, seconds, (total_bytes / seconds) / 1024);
        free(formatted_number);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: Downloaded %d bytes in %.2f seconds, %.2f KBytes per second\n", total_bytes, seconds, (total_bytes / seconds) / 1024);
    }

    return(0);
}

/**
 * @brief Run a network performance test.
 *
 * This method will get the files LargeFile1.html - LargeFile9.html and record the throughput of the system.
 *
 * @return int OK if successful, ERROR if there is a problem.
 */
int network_test_performance(char *webserver_ip, uint16_t webserver_port, char *resource)
{
    syslog(LOGGING_LEVEL, "********** Testing network performance.\n");

    int sockfd;
    struct sockaddr_in server_addr;
    char *buffer;
    ssize_t bytes_received;
    char file_name[100];

    for (int index = 1; index < 10; index++)
    {
        snprintf(file_name, sizeof(file_name), "GET /LargeFile%d.html HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", index, webserver_ip);
        syslog(LOGGING_LEVEL, "Download request %d.\n", index);
        time_t start;
        time(&start);

        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            syslog(LOGGING_LEVEL, "    FAIL: socket - Failed to create socket.\n");
            return(ERROR);
        }

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(webserver_port);
        if (inet_pton(AF_INET, webserver_ip, &server_addr.sin_addr) <= 0)
        {
            close(sockfd);
            syslog(LOGGING_LEVEL, "    FAIL: inet_pton - Invalid address.\n");
            return(ERROR);
        }

        if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        {
            close(sockfd);
            syslog(LOGGING_LEVEL, "    FAIL: connect - Failed to connect to server.\n");
            return(ERROR);
        }

        if (send(sockfd, file_name, strlen(file_name), 0) < 0)
        {
            close(sockfd);
            syslog(LOGGING_LEVEL, "    FAIL: send - Failed to send request.\n");
            return(ERROR);
        }

        int total_bytes = 0;
        buffer = (char *) malloc(BUFFER_SIZE);
        if (buffer == NULL)
        {
            close(sockfd);
            syslog(LOGGING_LEVEL, "    FAIL: malloc - Failed to allocate memory.\n");
            return(ERROR);
        }

        while ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) > 0)
        {
            total_bytes += bytes_received;
        }

        free(buffer);

        if (bytes_received < 0)
        {
            syslog(LOGGING_LEVEL, "    FAIL: recv - Failed to receive data.\n");
            return(ERROR);
        }

        close(sockfd);

        time_t end;
        time(&end);
        double seconds = difftime(end, start);

        syslog(LOGGING_LEVEL, "File downloaded successfully as LargeFile%d.html\n", index);
        syslog(LOGGING_LEVEL, "Downloaded %d bytes in %.2f seconds\n", total_bytes, seconds);
    }
    usleep(DELAY);

    return(OK);
}

/****************************************************************************
 * Name: network_test_misc_network_functions
 *
 * Description:
 *  Testing misc network functions
 *
 * Input Parameters:
 *   None.
 *
 * Returned Value:
 *   0 on success, -1 on failure.
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
int network_test_misc_network_functions(void)
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
        return(-1);
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
        return(-1);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: ioctl\n");
    }

    usleep(DELAY);          // Wait for the messages to be processed.

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;

    return(0);
}

#endif /* defined(CONFIG_ESP_TESTS) || defined(CONFIG_ETHERNET_TESTS) || defined(CONFIG_BG77_TESTS) || defined(CONFIG_ALL_MEADOW_TESTS) */
