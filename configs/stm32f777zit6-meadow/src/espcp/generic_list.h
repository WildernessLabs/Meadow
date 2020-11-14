/****************************************************************************
 * generic_list.h
 *
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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

#ifndef __GENERIC_LIST_H
#define __GENERIC_LIST_H

#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <arch/irq.h>

#include <sys/socket.h>
#include <nuttx/semaphore.h>
#include <nuttx/net/net.h>
#include <nuttx/net/usrsock.h>

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Structures and Types
 ****************************************************************************/

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
 *  Function that can be used to find a data item in the linked list
 *  with the matching uint32_t key.
 * 
 *  uint32_t - key to find.
 *  void * - data item that should be checked.
 * 
 *  returns - true if the oject has the matching key, false otherwise.
 */
typedef bool (*comparison_function_t)(uint32_t, void *);

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/
gl_linked_list_t *gl_create_empty_linked_list(void);
bool gl_is_empty(gl_linked_list_t *);
void *gl_find_item(gl_linked_list_t *, uint32_t, comparison_function_t);
void *gl_remove_item(gl_linked_list_t *, uint32_t, comparison_function_t);
void *gl_remove_item_at_head(gl_linked_list_t *);
bool gl_add_item_to_head(gl_linked_list_t *, void *);
bool gl_add_item_to_tail(gl_linked_list_t *, void *);

#endif /* __GENERIC_LIST_H */
