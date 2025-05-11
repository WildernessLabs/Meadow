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

/****************************************************************************
 * Private variables and associated macros.
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#pragma GCC diagnostic ignored "-Wunused-function"

/****************************************************************************
 * Name: network_tests_get_resource
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
int network_tests_get_resource(char *webserver_ip, int webserver_port, char *resource)
{
    syslog(LOGGING_LEVEL, "********** Getting a large file, URL: http://%s:%d/%s.\n", webserver_ip, webserver_port, resource);

    int sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sd < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: socket - Failed to create socket.\n");
        return(-1);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: socket - Created socket.\n");
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(webserver_ip);
	server.sin_family = AF_INET;
	server.sin_port = htons(webserver_port);

	if (connect(sd, (struct sockaddr *) &server, sizeof(server)) < 0)
	{
		syslog(LOGGING_LEVEL, "    FAIL: connect - Failed to connect to %s.\n", webserver_ip);
        return(-1);
	}
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: connect - Connected to %s.\n", webserver_ip);
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
        if ((sin->sin_addr.s_addr == inet_addr(webserver_ip)) && (sin->sin_port == htons(webserver_port)))
        {
            syslog(LOGGING_LEVEL, "    PASS: getpeername - Socket address details are correct.\n");
        }
        else
        {
            syslog(LOGGING_LEVEL, "    FAIL: getpeername - Socket address details are incorrect.\n");
            return(-1);
        }
    }

    struct pollfd pollfds[] = { { sd, POLLIN | POLLOUT, 0} };
    if (poll(pollfds, 1, 500) < 0)
	{
		syslog(LOGGING_LEVEL, "    FAIL: poll - Failed.\n");
        return(-1);
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
            return(-1);
        }
    }

    int buffer_length = 1024;
    char buffer[buffer_length];
    sprintf(buffer, "GET /%s HTTP/1.1\r\n\r\n", resource);
	if (send(sd, buffer, strlen(buffer), 0) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: send - Failed to send GET request message.\n");
        return(-1);
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
        return(-1);
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
            return(-1);
        }
    }

    int bytes_read = 1;             // Force the system to make on attempt.
    int attempt = 0;
    while (bytes_read >= 0)
    {
        bytes_read = recvfrom(sd, buffer, buffer_length, 0, NULL, 0);
        if ((attempt == 0) && (bytes_read < 0))
        {
            syslog(LOGGING_LEVEL, "    FAIL: recvfrom - Failed to receive server reply.\n");
            return(-1);
        }
        else
        {
            attempt++;
        }
    }

    if (close(sd) < 0)
    {
        syslog(LOGGING_LEVEL, "    FAIL: close - Failed to close socket.\n");
        return(-1);
    }
    else
    {
        syslog(LOGGING_LEVEL, "    PASS: close - Closed socket.\n");
    }

    usleep(DELAY);

    return(0);
}

/****************************************************************************
 * Name: network_test_get_multiple_web_pages
 *
 * Description:
 *  Get a simple web page from a web server multiple times.
 *
 * Input Parameters:
 *   number_of_requests - number of requests to make.
 *   webserver_ip - IP address of the web server.
 *   webserver_port - Port number on the web server.
 *
 * Returned Value:
 *   0 on success, -1 on failure.
 *
 * Assumptions/Limitations:
 *  Assumes that WiFi is started and the test web server is accessible.
 *
 *  The server connects directly to an IP address.  The IP address is defined
 *  in the file secrets.h.
 *
 ****************************************************************************/
int network_test_get_multiple_web_pages(int number_of_requests, char *webserver_ip, uint16_t webserver_port, char *page)
{
    int result = OK;

    syslog(LOGGING_LEVEL, "********** Getting a simple web page from %s.\n", webserver_ip);

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    for (int index = 0; index < number_of_requests; index++)
    {
        if (network_tests_get_resource(webserver_ip, webserver_port, page) < 0)
        {
            result = -1;
            break;
        }
    }

    usleep(2 * DELAY);

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;

    return(result);
}

/****************************************************************************
 * Name: network_test_get_multiple_large_files
 *
 * Description:
 *  Get a large file from a web server multiple times.
 *
 * Input Parameters:
 *   number_of_requests - number of requests to make.
 *   webserver_ip - IP address of the web server.
 *   webserver_port - Port number on the web server.
 *   resource - Resource to be retrieved.
 *
 * Returned Value:
 *   0 on success, -1 on failure.
 *
 * Assumptions/Limitations:
 *  Assumes that WiFi is started and the test web server is accessible.
 *
 ****************************************************************************/
int network_test_get_multiple_large_files(int number_of_requests, char *webserver_ip, uint16_t webserver_port, char *resource)
{
    int result = OK;

    syslog(LOGGING_LEVEL, "********** Getting a large file, URL: http://%s:%d/%s.\n", webserver_ip, webserver_port, resource);

    if (number_of_requests < 1)
    {
        syslog(LOGGING_LEVEL, "    FAIL: number_of_requests - Must be greater than 0.\n");
        return(-1);
    }

    ALLOCATE_HEAP_STRUCTURES;
    GET_INITIAL_HEAP_INFORMATION;

    //
    //  Get the file once to make sure that the server is active and any caching has been completed.
    //
    if (network_tests_get_resource(webserver_ip, webserver_port, resource) < 0)
    {
        result = -1;
    }
    else
    {
        //
        //  We can now run the test now we know that the file has been loaded.
        //
        for (int index = 0; index < number_of_requests; index++)
        {
            if (network_tests_get_resource(webserver_ip, webserver_port, resource) < 0)
            {
                result = -1;
                break;
            }
        }
    }

    usleep(2 * DELAY);

    GET_FINAL_HEAP_INFORMATION;
    HEAP_USAGE_PASS_OR_FAIL;

    return(result);
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

#endif /* CONFIG_ESP_TESTS */