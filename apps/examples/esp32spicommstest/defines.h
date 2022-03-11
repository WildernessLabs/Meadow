/****************************************************************************
 * examples/espcptest/defines.h
 * Common defines for all espcp testcases
 *
 *   Copyright (C) 2015, 2017 Haltian Ltd. All rights reserved.
 *   Author: Roman Saveljev <roman.saveljev@haltian.com>
 *           Jussi Kivilinna <jussi.kivilinna@haltian.com>
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

#ifndef __EXAMPLES_ESPCPTEST_DEFINES_H
#define __EXAMPLES_ESPCPTEST_DEFINES_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <assert.h>
#include <semaphore.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "espcp_shared_enums.h"
#include "espcp_encoders.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ESPCP_MESSAGE_QUEUE_NAME   "Esp32MessageOutboundQueue"



#define espcp_NODE "/dev/espcp"

#define espcpTEST_DAEMON_CONF_DEFAULTS \
  { \
    .max_sockets = UINT_MAX, \
    .supported_domain = AF_INET, \
    .supported_type = SOCK_STREAM, \
    .supported_protocol = 0, \
    .delay_all_responses = false, \
    .endpoint_addr = "127.0.0.1", \
    .endpoint_port = 255, \
    .endpoint_block_connect = false, \
    .endpoint_block_send = false, \
    .endpoint_recv_avail_from_start = true, \
    .endpoint_recv_avail = 4, \
  }

/* Test case macros */

#define utest_match(type, should_be_value, test_value) do { \
    type tmp_utest_test_value = (type)(test_value); \
    type tmp_utest_should_value = (type)(should_be_value); \
    if (!espcptest_assert_print_value(__func__, __LINE__, \
                                        "(" #type ")(" #test_value ") == " \
                                        "(" #type ")(" #should_be_value ")", \
                                        tmp_utest_test_value, \
                                        tmp_utest_should_value)) \
      { \
        espcptest_test_failed = true; \
        return; \
      } \
  } while(false)

#define utest_no_match(type, should_be_value, test_value) do { \
    type tmp_utest_test_value = (type)(test_value); \
    type tmp_utest_should_value = (type)(should_be_value); \
    if (espcptest_assert_print_value(__func__, __LINE__, \
                                        "(" #type ")(" #test_value ") == " \
                                        "(" #type ")(" #should_be_value ")", \
                                        tmp_utest_test_value, \
                                        tmp_utest_should_value)) \
      { \
        espcptest_test_failed = true; \
        return; \
      } \
  } while(false)

#define utest_match_buf(should_be_valuebuf, test_valuebuf, test_valuebuflen) do { \
    if (!espcptest_assert_print_buf(__func__, __LINE__, \
                                      "memcmp(" #should_be_valuebuf ", " \
                                                #test_valuebuf ", " \
                                                #test_valuebuflen ") == 0", \
                                      should_be_valuebuf, \
                                      test_valuebuf, \
                                      test_valuebuflen)) \
      { \
        espcptest_test_failed = true; \
        return; \
      } \
  } while(false)

#define RUN_TEST_CASE(g,t) do { \
    if (espcptest_test_failed) \
      { \
        return; \
      } \
    espcptest_group_##g##_setup(); \
    espcptest_test_##g##_##t(); \
    espcptest_group_##g##_teardown(); \
    if (espcptest_test_failed) \
      { \
        return; \
      } \
  } while (false)

#define RUN_TEST_GROUP(g) do { \
    extern const char *espcptest_group_##g; \
    void espcptest_group_##g##_run(void); \
    run_tests(espcptest_group_##g, espcptest_group_##g##_run); \
  } while (false)

#define TEST_GROUP(g)        const char *espcptest_group_##g = #g; \
                             void espcptest_group_##g##_run(void)
#define TEST_SETUP(g)        static void espcptest_group_##g##_setup(void)
#define TEST_TEAR_DOWN(g)    static void espcptest_group_##g##_teardown(void)
#define TEST(g,t)            static void espcptest_test_##g##_##t(void)

#define TEST_ASSERT_TRUE(v)    utest_match(bool, true, (v))
#define TEST_ASSERT_FALSE(v)   utest_match(bool, false, (v))
#define TEST_ASSERT_EQUAL(e,v) utest_match(ssize_t, (e), (v))
#define TEST_ASSERT_NOT_EQUAL(e,v) utest_no_match(ssize_t, (e), (v))
#define TEST_ASSERT_EQUAL_UINT8_ARRAY(e,v,s) utest_match_buf((e), (v), (s))

/****************************************************************************
 * Public Types
 ****************************************************************************/

/*
 *  Type definition for the function that will send data to the ESP32.
 */
typedef void (*espcp_send_data_function_t)(void *, void *, size_t); 

/*
 *  Linked list data item object.
 * 
 *  data - this is a pointer to the data to be held in the list.  It is a
 *         void * object and it it the responsibility of the application
 *         code to cast the data back to the correct type.
 * 
 *  parent - pointer to the previous item in the list.
 *  child - pointer to the next item in the list.
 */
struct gl_linked_list_item_s
{
    struct gl_linked_list_item_s *parent;
    struct gl_linked_list_item_s *child;
    void *data;
};
typedef struct gl_linked_list_item_s gl_linked_list_item_t;

/*
 *  Linked list object.
 * 
 *  head - Pointer to the linked list data item at the head of the list.
 *  tail - Pointer to the linked list data item at the tail (end)  of the list.
 */
struct gl_linked_list_s
{
    gl_linked_list_item_t *head;
    gl_linked_list_item_t *tail;
};
typedef struct gl_linked_list_s gl_linked_list_t;

/*
 *  Configuration information for the ESP32 coprocessor.
 */
struct espcp_configuration_s
{
  /*
   *  Indicates if the thread processing the messages for the ESP32
   *  is running.
   */
  bool thread_running;

  /*
   *  ID of the thread processing the messages for the ESP32.
   */
  pthread_t thread;

  /*
   *  ID of the queue of messages that are waiting to be sent to the ESP32.
   */
  mqd_t request_queue;

  /*
   *  List of messages that have been sent to the ESP where a response is
   *  pending.
   */
  gl_linked_list_t messages_pending_response;

  /*
   *  Exit code for the thread processing the messages for the ESP32.
   */
  int exit_code;

  /*
   *  Method that will send / receive data to / from the ESP32.
   */
  espcp_send_data_function_t send_data_to_esp32;

  /*
   *  Size of a buffer required to hold a headers worth of data.  This is a
   *  frequently used value so we store it here rather than recalculate it
   *  every time it is needed.
   */
  uint32_t header_only_buffer_size;

  /*
   *  Pointer to a buffer that can take a header (and only a header) worth of data.
   */
  uint8_t *header;
};
typedef struct espcp_configuration_s espcp_configuration_t;

/*
 *  Function that can be used to find a data item in the linked list
 *  with the matching uint32_t key.
 * 
 *  uint32_t - key to find.
 *  void * - data item that should be checked.
 * 
 *  returns - true if the oject has the matching key, false otherwise.
 */
typedef bool (*comparison_function_t)(uint32_t, void *);

typedef int (*queue_send_response_message_function_t)(int, void *, void*);


/*
 *  IP address, subnet mask etc.
 */
struct espcp_ip_information_s
{
    uint8_t ip[4];
    uint8_t subnet[4];
    uint8_t gateway[4];
};
typedef struct espcp_ip_information_s espcp_ip_information_t;

/****************************************************************************
 * Public Data
 ****************************************************************************/

extern int espcptest_endp_malloc_cnt;
extern int espcptest_dcmd_malloc_cnt;
// extern const struct espcptest_daemon_conf_s espcptest_daemon_defconf;
// extern struct espcptest_daemon_conf_s espcptest_daemon_config;

extern bool espcptest_test_failed;

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

bool espcptest_assert_print_value(FAR const char *func,
                                    const int line,
                                    FAR const char *check_str,
                                    long int test_value,
                                    long int should_be);

bool espcptest_assert_print_buf(FAR const char *func,
                                  const int line,
                                  FAR const char *check_str,
                                  FAR const void *test_buf,
                                  FAR const void *expect_buf,
                                  size_t buflen);

/*
 *    Coprocessor prototypes.
 */
int espcp_init(void);
int espcp_spi_setup(queue_send_response_message_function_t);
espcp_configuration_t *espcp_get_default_configuration(void);
void espcp_wait_for_message_waiting_signal(void);

/*
 *    Thread prototypes.
 */
int espcp_thread_start(espcp_configuration_t *);
bool espcp_is_thead_running(espcp_configuration_t *);
int espcp_thread_stop(espcp_configuration_t *);

/*
 *    Queue prototypes.
 */
mqd_t espcp_create_message_queue(char *name);
int espcp_delete_message_queue(mqd_t);
int espcp_add_message_to_queue(mqd_t, espcp_message_t *);
void *espcp_get_message_from_queue(mqd_t);
void espcp_queue_kill_nuttx_thread_message(mqd_t);

/*
 *    Generic list prototypes.
 */
gl_linked_list_t *gl_create_empty_linked_list(void);
bool gl_is_empty(gl_linked_list_t *);
void *gl_remove_item(gl_linked_list_t *, uint32_t, comparison_function_t);
void *gl_remove_item_at_head(gl_linked_list_t *);
bool gl_add_item_to_head(gl_linked_list_t *, void *);
bool gl_add_item_to_tail(gl_linked_list_t *, void *);

/*
 *    Message dispatcher methods.
 */
int espcp_setup_message_dispatcher(void);
int espcp_teardown_message_dispatcher(void);
int espcp_queue_send_response_message(int, void *, void *);
uint32_t espcp_get_next_message_id(void);
int espcp_send_message(espcp_configuration_t *, espcp_message_t *);
int espcp_send_packet(espcp_configuration_t *, espcp_message_t *);
void espcp_send_acknowledgement(espcp_configuration_t *, espcp_message_t *, int);
int espcp_send_message_body(espcp_configuration_t *, espcp_message_t *);
espcp_message_t *espcp_get_message_body(espcp_configuration_t *, espcp_message_t *);
int espcp_get_message_header_acknowledgement(espcp_configuration_t *, espcp_message_t *, espcp_message_t **);
espcp_message_t *espcp_get_message_header(espcp_configuration_t *);
int espcp_process_transport_message(espcp_configuration_t *, espcp_message_t *);
int espcp_get_response_from_esp32(espcp_configuration_t *);

/*
 *  ESP32 system methods.
 */
int32_t espcp_get_battery_charge_level(void);

/*
 *  ESP32 WiFi functions.
 */
espcp_ip_information_t *espcp_start_wifi(char *, char *);

/*
 * ESP32 POSIX methods.
 */
int32_t espcp_getaddrinfo(const char *, const char *, const struct addrinfo *, struct addrinfo **);
int32_t espcp_socket(int, int, int);
int32_t espcp_connect(int, const struct sockaddr *, socklen_t);
void espcp_freeaddrinfo(struct addrinfo *);
int32_t espcp_setsockopt(int, int, int, const void *, socklen_t);
int32_t espcp_write(int, const void *, size_t);
int32_t espcp_read(int, const void *, size_t);
int32_t espcp_close(int);

#endif /* __EXAMPLES_ESPCPTEST_DEFINES_H */
