/****************************************************************************
 * examples/espcptest/generic_list.c
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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <debug.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "defines.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

#ifndef dbg
  #define dbg _warn
#endif

#ifndef ARRAY_SIZE
#  define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#define noinline

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static gl_linked_list_t *g_list;

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: Generic double-linked list test group setup
 *
 * Description:
 *   Setup function executed before each testcase in this test group
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
TEST_SETUP(GenericList)
{
  g_list  = gl_create_empty_linked_list();
}

/****************************************************************************
 * Name: Generic double-linked list test group teardown
 *
 * Description:
 *   Setup function executed after each testcase in this test group
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
TEST_TEAR_DOWN(GenericList)
{
  if (g_list != NULL)
  {
    gl_linked_list_item_t *item = g_list->head;
    while (item != NULL)
    {
      gl_linked_list_item_t *next = item->child;
      free(item);
      item = next;
    }

    free(g_list);
  }
}

/****************************************************************************
 * 
 * Create an empty list object.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, CreateList)
{
  TEST_ASSERT_EQUAL(true, g_list != NULL);
  TEST_ASSERT_EQUAL(NULL, g_list->head);
  TEST_ASSERT_EQUAL(NULL, g_list->tail);
  TEST_ASSERT_EQUAL(true, gl_is_empty(g_list));
}

int CountListEntries(void)
{
  int count = 0;
  gl_linked_list_item_t *item = g_list->head;

  while (item != NULL)
  {
    count++;
    item = item->child;
  }

  return(count);
}

/****************************************************************************
 * 
 * Create an empty list and add one item to the list.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, AddOneItem)
{
  TEST_ASSERT_EQUAL(true, gl_is_empty(g_list));
  TEST_ASSERT_EQUAL(0, CountListEntries());
  int integer = 45;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_head(g_list, &integer));
  TEST_ASSERT_EQUAL(true, g_list->head == g_list->tail);
  TEST_ASSERT_EQUAL(1, CountListEntries());
  TEST_ASSERT_EQUAL(false, gl_is_empty(g_list));
}

/****************************************************************************
 * 
 * Create an empty list and add more than one item to the list.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, AddMultipleItems)
{
  int integer = 45;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_head(g_list, &integer));
  int another_integer = 90;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_head(g_list, &another_integer));
  TEST_ASSERT_EQUAL(true, g_list->head != g_list->tail);
  TEST_ASSERT_EQUAL(2, CountListEntries());
  TEST_ASSERT_EQUAL(90, (int) *((int *) g_list->head->data));
  TEST_ASSERT_EQUAL(45, (int) *((int *) g_list->tail->data));
  TEST_ASSERT_EQUAL(false, gl_is_empty(g_list));
}

/****************************************************************************
 * 
 * Create an empty list and then remove the item from the head.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, RemoveHeadFromEmptyList)
{
  TEST_ASSERT_EQUAL(NULL, gl_remove_item_at_head(g_list));
}

/****************************************************************************
 * 
 * Create a list, add two items and then remove the item at the head of 
 * the list.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, RemoveItemFromHeadOfList)
{
  int integer = 45;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_head(g_list, &integer));
  int another_integer = 90;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_head(g_list, &another_integer));
  int *retrieved_value = gl_remove_item_at_head(g_list);
  TEST_ASSERT_EQUAL(true, g_list->head == g_list->tail);
  TEST_ASSERT_EQUAL(90, *retrieved_value);
}

/****************************************************************************
 * 
 * Create a list, add two items to the tail of the list and retrieve them
 * to check the values have been added to the right place.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, AddItemToTailOfList)
{
  int integer = 45;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_tail(g_list, &integer));
  TEST_ASSERT_EQUAL(true, g_list->head == g_list->tail);

  int another_integer = 90;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_tail(g_list, &another_integer));
  TEST_ASSERT_EQUAL(true, g_list->head != g_list->tail);

  int *retrieved_value = gl_remove_item_at_head(g_list);
  TEST_ASSERT_EQUAL(true, g_list->head == g_list->tail);
  TEST_ASSERT_EQUAL(45, *retrieved_value);

  retrieved_value = gl_remove_item_at_head(g_list);
  TEST_ASSERT_EQUAL(NULL, g_list->head);
  TEST_ASSERT_EQUAL(NULL, g_list->tail);
  TEST_ASSERT_EQUAL(90, *retrieved_value);
}


bool CompareInteger(uint32_t key, void *data)
{
  int value = (int) *((int *) data);
  return(key == value);
}

/****************************************************************************
 * 
 * Create a list and try to use retrieve to try and get an item from an
 * empty list.  The function should return a NULL pointer.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, RetrieveFromEmptyList)
{
  TEST_ASSERT_EQUAL(NULL, gl_remove_item(g_list, 45, CompareInteger));
}

/****************************************************************************
 * 
 * Create a list, add an item and then try to retrive it.
 * 
 * Also, check that searching for non-existant entries returns NULL for
 * single item lists.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, RetrieveFromListWithSingleItem)
{
  int integer = 45;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_tail(g_list, &integer));
  //
  //  Should not find 90 in the list.
  //
  TEST_ASSERT_EQUAL(NULL, gl_remove_item(g_list, 90, CompareInteger));
  TEST_ASSERT_EQUAL(false, gl_is_empty(g_list));
  //
  //  Now get 45 from the list.
  //
  int *value = (int *) gl_remove_item(g_list, 45, CompareInteger);
  TEST_ASSERT_EQUAL(45, *value);
  TEST_ASSERT_EQUAL(true, gl_is_empty(g_list));
  TEST_ASSERT_EQUAL(NULL, g_list->head);
  TEST_ASSERT_EQUAL(NULL, g_list->tail);
}

/****************************************************************************
 * 
 * Create a list, add two items and then try to retrieve the one from the
 * tail of the list.
 * 
 * Also, check that searching for a non-existant item in the list should
 * return a NULL pointer.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, RetrieveFromTailOfList)
{
  int integer = 45;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_tail(g_list, &integer));
  int another_integer = 90;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_tail(g_list, &another_integer));
  //
  //  Should not find 135 in the list.
  //
  TEST_ASSERT_EQUAL(NULL, gl_remove_item(g_list, 135, CompareInteger));
  TEST_ASSERT_EQUAL(false, gl_is_empty(g_list));
  //
  //  Now get 90 from the list.
  //
  int *value = (int *) gl_remove_item(g_list, 90, CompareInteger);
  TEST_ASSERT_EQUAL(90, *value);
  TEST_ASSERT_EQUAL(false, gl_is_empty(g_list));
  //
  //  At this point there should be one item remaining.
  //
  TEST_ASSERT_EQUAL(1, CountListEntries());
}

/****************************************************************************
 * 
 * Create a list, add three items and then try to retrieve the one from the
 * middle of the list.
 * 
 * Pre-requisite:
 *  None
 * 
 ****************************************************************************/
TEST(GenericList, RetrieveFromMiddleOfList)
{
  int integer1 = 45;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_tail(g_list, &integer1));
  int integer2 = 90;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_tail(g_list, &integer2));
  int integer3 = 135;
  TEST_ASSERT_EQUAL(true, gl_add_item_to_tail(g_list, &integer3));
  //
  //  Now get 90 from the list, it should be in the middle.
  //
  int *value = (int *) gl_remove_item(g_list, 90, CompareInteger);
  TEST_ASSERT_EQUAL(90, *value);
  TEST_ASSERT_EQUAL(false, gl_is_empty(g_list));
  //
  //  At this point there should be two items remaining.
  //
  TEST_ASSERT_EQUAL(2, CountListEntries());
  TEST_ASSERT_EQUAL(45, *((int *) g_list->head->data));
  TEST_ASSERT_EQUAL(135, *((int *) g_list->tail->data));
}

/****************************************************************************
 *
 * Run the test cases.
 * 
 ****************************************************************************/
TEST_GROUP(GenericList)
{
  RUN_TEST_CASE(GenericList, CreateList);
  RUN_TEST_CASE(GenericList, AddOneItem);
  RUN_TEST_CASE(GenericList, AddMultipleItems);
  RUN_TEST_CASE(GenericList, RemoveHeadFromEmptyList);
  RUN_TEST_CASE(GenericList, RemoveItemFromHeadOfList);
  RUN_TEST_CASE(GenericList, AddItemToTailOfList);
  RUN_TEST_CASE(GenericList, RetrieveFromEmptyList);
  RUN_TEST_CASE(GenericList, RetrieveFromListWithSingleItem);
  RUN_TEST_CASE(GenericList, RetrieveFromTailOfList);
  RUN_TEST_CASE(GenericList, RetrieveFromMiddleOfList);
}
