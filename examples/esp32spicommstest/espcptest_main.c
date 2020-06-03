/****************************************************************************
 * examples/espcptest/espcptest_main.c
 *
 *   Copyright (C) 2020 Wilderness Labs
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

#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "defines.h"
#include "secrets.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/
#define usrsocktest_dbg(...) ((void)0)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static struct
{
  unsigned int ok;
  unsigned int failed;

  unsigned int nchecks;
} overall;
/****************************************************************************
 * Public Data
 ****************************************************************************/
int usrsocktest_endp_malloc_cnt = 0;
int usrsocktest_dcmd_malloc_cnt = 0;
bool usrsocktest_test_failed = false;
bool espcptest_test_failed;

/****************************************************************************
 * Private Functions
 ****************************************************************************/



static void get_mallinfo(struct mallinfo *mem)
{
#ifdef CONFIG_CAN_PASS_STRUCTS
  *mem = mallinfo();
#else
  (void)mallinfo(mem);
#endif
}

static void print_mallinfo(const struct mallinfo *mem, const char *title)
{
  if (title)
    printf("%s:\n", title);
  printf("       %11s%11s%11s%11s\n", "total", "used", "free", "largest");
  printf("Mem:   %11d%11d%11d%11d\n",
         mem->arena, mem->uordblks, mem->fordblks, mem->mxordblk);
}

static void utest_assert_print_head(FAR const char *func, const int line,
                                    FAR const char *check_str)
{
  printf("\t[TEST ASSERT FAILED!]\n"
         "\t\tIn function \"%s\":\n"
         "\t\tline %d: Assertion `%s' failed.\n", func, line, check_str);
}


static void run_tests(FAR const char *name, void (CODE *test_fn)(void))
{
  printf("Testing group \"%s\" =>\n", name);
  fflush(stdout);
  fflush(stderr);

  usrsocktest_test_failed = false;
  test_fn();
  if (!usrsocktest_test_failed)
    {
      printf("\tGroup \"%s\": [OK]\n", name);
      overall.ok++;
    }
  else
    {
      printf("\tGroup \"%s\": [FAILED]\n", name);
      overall.failed++;
    }

  fflush(stdout);
  fflush(stderr);
}

/****************************************************************************
 * Name: runAllTests
 *
 * Description:
 *   Sequentially runs all included test groups
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
// static void runAllTests(void)
// {
//   RUN_TEST_GROUP(GenericList);
//   RUN_TEST_GROUP(EspcpCoprocessor);
//   RUN_TEST_GROUP(EspcpQueue);
//   RUN_TEST_GROUP(EspcpMessageDispatcher);
//   RUN_TEST_GROUP(EspcpThread);
// }

/****************************************************************************
 * Name: get_google_homepage
 *
 * Description:
 *  Get the Google home page using emulated POSIX calls.  These emulated
 *  calls will be passed to the ESP32.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 * Assumptions/Limitations:
 *   None
 *
 ****************************************************************************/
void get_google_homepage(void)
{
  int result = 0;

  printf("Initialising the network and restarting the ESP32.\n");
  espcp_init();

  printf("Starting the WiFi\n");
  espcp_ip_information_t *ip = espcp_start_wifi(NETWORK_NAME, NETWORK_PASSWORD);
  printf("IP Address: %d.%d.%d.%d\n", ip->ip[0], ip->ip[1], ip->ip[2], ip->ip[3]);

  printf("Looking up Googles' IP address.\n");
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_addrlen = 0;
  hints.ai_addr = NULL;
  hints.ai_canonname = "";
  struct addrinfo *res;

  #define WEB_SERVER "www.google.com"
  #define WEB_PORT "80"

  result = espcp_getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);
  if (result != 0)
  {
    printf("getaddr failed to lookup the server %s:%s, result: %d\n", WEB_SERVER, WEB_PORT, result);
    return;
  }
  printf("getaddr has found the server %s:%s\n", WEB_SERVER, WEB_PORT);

  printf("Opening socket.\n");
  int socket_handle = espcp_socket(res->ai_family, res->ai_socktype, 0);
  if (socket_handle < 0)
  {
    printf("socket failed to return socket handle.\n");
    return;
  }
  printf("Socket handle: 0x%02x\n", socket_handle);

  printf("Opening connection.\n");
  result = espcp_connect(socket_handle, res->ai_addr, res->ai_addrlen);
  if (result != 0)
  {
    printf("connect failed to connect to server, result code: %d\n", result);
    return;
  }
  printf("Connected to server.\n");

  printf("Freeing addrinfo structure.\n");
  espcp_freeaddrinfo(res);
  printf("addrinfo structure released.\n");

  printf("Sending request to server:\n");
  #define WEB_PATH "/"
  #define REQUEST "GET " WEB_PATH " HTTP/1.0\r\nHost: " WEB_SERVER ":" WEB_PORT "\r\n" \
                  "User-Agent: Meadow\r\n\r\n"

  printf("-------------------- REQUEST --------------------\n");
  printf("%s", REQUEST);
  printf("----------------- END OF REQUEST ----------------\n");
  int amount_written = espcp_write(socket_handle, REQUEST, strlen(REQUEST));
  if (amount_written < 0)
  {
    printf("Failed to write full data buffer to server, result code: %d\n", errno);
    return;
  }
  printf("%d bytes sent to server\n", amount_written);

  struct timeval receiving_timeout;
  receiving_timeout.tv_sec = 5;
  receiving_timeout.tv_usec = 0; 
  printf("Setting socket receive timeout to %d seconds.\n", receiving_timeout.tv_sec);
  result = espcp_setsockopt(socket_handle, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout));
  if (result < 0)
  {
    printf("Failed to set the timeout, result code: %d\n", result);
    return;
  }
  printf("Socket receive time out set.\n");

  printf("Reading data from server.\n");
  const int BUFFER_SIZE = 1024;
  uint8_t buffer[BUFFER_SIZE];
  int total_bytes = 0;
  int amount_read = 0;
  bool first_block = true;
  int count = 0;
  do
  {
    memset(buffer, 0, BUFFER_SIZE);
    amount_read = espcp_read(socket_handle, buffer, BUFFER_SIZE);
    count++;
    if (amount_read > 0)
    {
        total_bytes += amount_read;
        if (first_block)
        {
          printf("-------------------- FIRST BLOCK (100 bytes)--------------------\n");
          char small_block[200];
          strncpy(small_block, (char *) buffer, 100);
          printf("%s\n", small_block);
          printf("------------------------- END OF BLOCK -------------------------\n");
        }
    }
    first_block = false;
  } while (amount_read > 0);
  printf("Read completed, %d bytes read in %d operations.\n", total_bytes, count);

  printf("Closing the socket.\n");
  result = espcp_close(socket_handle);
  if (result < 0)
  {
    printf("Failed to close the socket, result code: %d\n", result);
  }
  printf("Socket closed.\n\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
bool espcptest_assert_print_value(FAR const char *func,
                                    const int line,
                                    FAR const char *check_str,
                                    long int test_value,
                                    long int should_be)
{
  ++overall.nchecks;

  if (test_value == should_be)
    {
      int keep_errno = errno;

      usrsocktest_dbg("%d => OK.\n", line);

      errno = keep_errno;
      return true;
    }

  utest_assert_print_head(func, line, check_str);

  printf("\t\t\tgot value: %ld\n", test_value);
  printf("\t\t\tshould be: %ld\n", should_be);

  fflush(stdout);
  fflush(stderr);

  return false;
}

bool espcptest_assert_print_buf(FAR const char *func,
                                  const int line,
                                  FAR const char *check_str,
                                  FAR const void *test_buf,
                                  FAR const void *expect_buf,
                                  size_t buflen)
{
  ++overall.nchecks;

  if (memcmp(test_buf, expect_buf, buflen) == 0)
    {
      int keep_errno = errno;

      usrsocktest_dbg("%d => OK.\n", line);

      errno = keep_errno;
      return true;
    }

  utest_assert_print_head(func, line, check_str);

  fflush(stdout);
  fflush(stderr);

  return false;
}

/****************************************************************************
 * espcptest_main
 ****************************************************************************/

#ifdef BUILD_MODULE
int main(int argc, FAR char *argv[])
#else
int esp32spicommstest_main(int argc, char *argv[])
#endif
{

  struct mallinfo mem_before, mem_after;

  memset(&overall, 0, sizeof(overall));

  printf("Starting Tests...\n");
  fflush(stdout);
  fflush(stderr);

  get_mallinfo(&mem_before);

  // runBatteryLevelTest();
  get_google_homepage();

  printf("Tests complete.\n\n\n");

  // printf("Unit-test groups done... OK:%d, FAILED:%d, TOTAL:%d\n",
  //        overall.ok, overall.failed, overall.ok + overall.failed);
  // printf(" -- number of checks made: %d\n", overall.nchecks);
  // fflush(stdout);
  // fflush(stderr);

  // get_mallinfo(&mem_after);

  // print_mallinfo(&mem_before, "HEAP BEFORE TESTS");
  // print_mallinfo(&mem_after, "HEAP AFTER TESTS");

  fflush(stdout);
  fflush(stderr);
  exit(0);

  return 0;
}

