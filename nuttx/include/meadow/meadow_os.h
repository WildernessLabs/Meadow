/****************************************************************************
 * meadow_os.h
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

#include <nuttx/config.h>

#include <meadow/hcom_shared_common.h>

#if defined(CONFIG_MEADOW_ITM_ENABLED)

//
//  The ITM ports are 4 bytes wide and start with the "printf" channel (0)
//  below.
//
//  There are 32 channels (0 - 31).
//
#define MEADOW_ITM_PRINTF_CHANNEL       ((volatile uint32_t *) 0xE0000000u)
#define MEADOW_ITM_MALLOC_CHANNEL       ((volatile uint32_t *) 0xE0000004u)
#define MEADOW_ITM_SEMAPHORE_CHANNEL    ((volatile uint32_t *) 0xE0000008u)

#define MEADOW_ITM_MALLOC_SIGNATURE     0xa5a5a500

//
//  Bit 0 in the malloc header.
//
#define MEADOW_ITM_MALLOC_KERNEL_HEAP   (0)
#define MEADOW_ITM_MALLOC_USER_HEAP     (1)

//
//  Bit 1 in the malloc header.
//
#define MEADOW_ITM_MALLOC               (0 << 1)
#define MEADOW_ITM_FREE                 (1 << 1)

//
//  Bit 0 in the semaphore header.
//
#define MEADOW_ITM_SEM_KERNEL           (0)
#define MEADOW_ITM_SEM_USER             (1)

//
//  Bit 1 in the semaphore header.
//
#define MEADOW_ITM_SEM_WAIT             (0 << 1)
#define MEADOW_ITM_SEM_POST             (1 << 1)

//
//  Extract the return address for the caller.  This maybe the line after the
//  call to the method.
//
#define MEADOW_GET_RETURN_ADDRESS(r) __asm volatile ("mov %0, lr\n" : "=r" (r));

void meadow_os_itm_send_string(char *);
void meadow_os_itm_send_word(volatile uint32_t *, uint32_t);
void meadow_os_itm_send_words(volatile uint32_t *, uint32_t *, uint32_t);
void meadow_os_itm_enable(void);
void meadow_os_itm_disable(void);

#endif

void meadow_os_config_free_resources(meadow_configuration_t *);
meadow_configuration_t *meadow_os_deep_copy_config(void);