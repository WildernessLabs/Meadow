/****************************************************************************
 * mm/mm_heap/mm_checkcorruption.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <assert.h>
#include <sched.h>

#include <nuttx/arch.h>
#include <nuttx/mm/mm.h>
#include <nuttx/irq.h>

#include "nuttx/mm/mm.h"

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void checknode(struct mm_allocnode_s *node)
{
  if ((node->preceding & MM_ALLOC_BIT) != 0)
    {
      assert(node->size >= SIZEOF_MM_ALLOCNODE);
    }
  else
    {
      struct mm_freenode_s *fnode = (void *) node;
      struct mm_freenode_s *next = (struct mm_freenode_s *)((char *) node + node->size);

      assert(node->size >= MM_MIN_CHUNK);
      assert((next->preceding & ~MM_ALLOC_BIT) == node->size);
      assert(fnode->blink->flink == fnode);
      assert((fnode->flink == NULL) ||
             (fnode->flink->blink == fnode));
      //
      //  Check with mm_malloc and mm_free and add extra link checking here.
      //
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mm_checkcorruption
 *
 * Description:
 *   mm_checkcorruption is used to check whether memory heap is normal.
 *
 ****************************************************************************/

void mm_checkcorruption(FAR struct mm_heap_s *heap)
{
  FAR struct mm_allocnode_s *node;
  // FAR struct mm_allocnode_s *prev;
  size_t nodesize;
  int region;

  mm_takesemaphore(heap);

  for (region = 0; region < heap->mm_nregions; region++)
    {
      // prev = NULL;
      for (node = heap->mm_heapstart[region];
           node < heap->mm_heapend[region];
           node = (FAR struct mm_allocnode_s *)((FAR char *)node + node->size))
        {
          checknode(node);
        }
    }

  mm_givesemaphore(heap);
}
