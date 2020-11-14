/****************************************************************************
 * generic_list.c
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

#include "generic_list.h"

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
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Name: gl_create_empty_linked_list
 *
 * Description:
 *  Create a new empty linked list and return a pointer to the list.
 *
 * Input Parameters:
 *  None
 *
 * Returned Value:
 *  Pointer to a newly created linked list.
 * 
 *  NULL can be returned if the memory allocation fails.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
gl_linked_list_t *gl_create_empty_linked_list(void)
{
    gl_linked_list_t *new_list = (gl_linked_list_t *) malloc(sizeof(gl_linked_list_t));

    if (new_list != NULL)
    {
        new_list->head = NULL;
        new_list->tail = NULL;
    }

    return(new_list);
}

/****************************************************************************
 * Name: gl_is_empty
 *
 * Description:
 *  Check to see if the list is empty.
 *
 * Input Parameters:
 *  list - pointer to the linked list.
 *
 * Returned Value:
 *  true if the list is empty, false otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
bool gl_is_empty(gl_linked_list_t *list)
{
    return(list->head == NULL);
}

/****************************************************************************
 * Name: gl_find_item
 *
 * Description:
 *  Search the list for a data item with the specified uint32_t key.
 *
 * Input Parameters:
 *  list - Pointer to the linked list.
 *  key - key value to search for.
 *  compare - user supplied function that will test the keys for equality.
 *
 * Returned Value:
 *  Pointer to the data item (as a void *) if a matching key was found,
 *  NULL if no match was found.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void *gl_find_item(gl_linked_list_t *list, uint32_t key, comparison_function_t compare)
{
    gl_linked_list_item_t *item = list->head;
    while (item != NULL)
    {
        if ((*compare)(key, item->data))
        {
            return((void *) item->data);

        }
        item = item->child;
    }
    return(NULL);
}

/****************************************************************************
 * Name: gl_remove_item
 *
 * Description:
 *  Search the list for a data item with the specified uint32_t key.  If the
 *  item is found then unlink it from the list and return a pointer to the
 *  data item.
 *
 * Input Parameters:
 *  list - Pointer to the linked list.
 *  key - key value to search for.
 *  compare - user supplied function that will test the keys for equality.
 *
 * Returned Value:
 *  Pointer to the data item (as a void *) if a matching key was found,
 *  NULL if no match was found.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void *gl_remove_item(gl_linked_list_t *list, uint32_t key, comparison_function_t compare)
{
    if ((list == NULL) || (compare == NULL))
    {
        return(NULL);
    }

    gl_linked_list_item_t *item = list->head;
    void *data = NULL;
    while ((item != NULL) && (data == NULL))
    {
        if ((*compare)(key, item->data))
        {
            data = item->data;
            if (item == list->head)
            {
                if (list->head == list->tail)
                {
                    /*
                    *  Item is the only item in the list so the head and the tail are the same.
                    *  Simply empty the list by setting the head and tail to NULL;
                    */
                    list->head = NULL;
                    list->tail = NULL;
                }
                else
                {
                    /*
                    *  Item is the head of a list with more than one item.
                    *  The child of the head of the list becomes the head of the list
                    *  and it then has no parent.
                    */
                    list->head = list->head->child;
                    list->head->parent = NULL;
                }
            }
            else
            {
                if (item == list->tail)
                {
                    /*
                    *  Item is the tail of a list with more than one item.
                    *  Note that if this had been the only item in the list then
                    *  the if clause above would have caught this case.
                    * 
                    *  The parent of the tail becomes the tail and it has no children.
                    */
                    list->tail = list->tail->parent;
                    list->tail->child = NULL;
                }
                else
                {
                    /*
                    *  Item is in the middle of a list and there is more than one
                    *  item in the list.
                    */
                   item->parent->child = item->child;
                   item->child->parent = item->parent;
                }
            }
            free(item);
        }
        else
        {
            item = item->child;     /* No match so move on to next item in the list */
        }
    }

    return(data);
}

/****************************************************************************
 * Name: gl_remove_item_at_head
 *
 * Description:
 *  Remove the item at the head of the linked list.
 *
 * Input Parameters:
 *  list - Pointer the linked list.
 *
 * Returned Value:
 *  Pointer (as held in the linked list structure) to the data item at the
 *  head of the list.  This is a generic void * pointer and it is the
 *  responsibility of the calling application to cast the pointer to the
 *  correct type.
 * 
 *  NULL will be returned if the list is empty or list iteself is NULL.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void *gl_remove_item_at_head(gl_linked_list_t *list)
{
    void *result = NULL;

    if (list != NULL)
    {
        if (list->head != NULL)
        {
            gl_linked_list_item_t *item = list->head;
            if (list->tail == list->head)
            {
                /*
                 *  Only one item in the list so clear the list.
                 */
                list->head = NULL;
                list->tail = NULL;                
            }
            else
            {
                list->head = item->child;       /* Head is now the second item. */
                list->head->parent = NULL;      /* Head of a list has no parents. */
            }
            result = item->data;
            free(item);
        }
    }
    return(result);
}

/****************************************************************************
 * Name: gl_add_item_to_head
 *
 * Description:
 *  Add a data item to the head of the list.  The current head of the list
 *  will become a child of the newly created linked list entry,
 *
 * Input Parameters:
 *  list - Pointer to the linked list.
 *  data - data item to add to the linked list.
 *
 * Returned Value:
 *  true if the item is added successfully, false otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
bool gl_add_item_to_head(gl_linked_list_t *list, void *data)
{
    if ((list == NULL) || (data == NULL))
    {
        return(false);
    }

    gl_linked_list_item_t *item = (gl_linked_list_item_t *) malloc(sizeof(gl_linked_list_item_t));

    if (item != NULL)
    {
        item->data = data;
        item->parent = NULL;
        item->child = list->head;
        if (list->head == NULL)
        {
            list->tail = item;
        }
        else
        {
            list->head->parent = item;
        }
        list->head = item;
    }

    return(item != NULL);
}

/****************************************************************************
 * Name: gl_add_item_to_tail
 *
 * Description:
 *  Add a data item to the tail of the list.  The current tail item of the list
 *  will become a parent of the newly created linked list entry,
 *
 * Input Parameters:
 *  list - Pointer to the linked list.
 *  data - data item to add to the linked list.
 *
 * Returned Value:
 *  true if the item could be added to the end of the list, false otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
bool gl_add_item_to_tail(gl_linked_list_t *list, void *data)
{
    if ((list == NULL) || (data == NULL))
    {
        return(false);
    }

    gl_linked_list_item_t *item = (gl_linked_list_item_t *) malloc(sizeof(gl_linked_list_item_t));

    if (item != NULL)
    {
        item->data = data;
        item->child = NULL;
        if (list->tail == NULL)
        {
            list->head = item;
        }
        else
        {
            list->tail->child = item;
        }
        item->parent = list->tail;
        list->tail = item;
    }

    return(item != NULL);
}

