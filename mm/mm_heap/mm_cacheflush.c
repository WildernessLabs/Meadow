/****************************************************************************
 * mm/mm_heap/mm_calloc.c
 *
 *   Copyright (C) 2007, 2009, 2014 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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
#include <sys/mman.h>

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: cacheflush
 *
 * Descriptor:
 *   cacheflush() flushes the requested cache lines
 * 
 ****************************************************************************/

#define arm_isb(n) __asm__ __volatile__ ("isb " #n : : : "memory")
#define arm_dsb(n) __asm__ __volatile__ ("dsb " #n : : : "memory")
#define arm_dmb(n) __asm__ __volatile__ ("dmb " #n : : : "memory")

#define putreg32(v,a) (*(volatile uint32_t *)(a) = (v))

#define ARMV7M_NVIC_BASE                0xe000e000
#define NVIC_ICIALLU_OFFSET             0x0f50 /* I-Cache Invalidate All to PoU (Cortex-M7) */
#define NVIC_ICIALLU                    (ARMV7M_NVIC_BASE + NVIC_ICIALLU_OFFSET)

int cacheflush(FAR const void *addr, size_t size, int type)
{
    int ret = -1;
    if (type == CACHE_ICACHE) {
        arm_dsb(15);
        arm_isb(15);

        putreg32(0, NVIC_ICIALLU);

        arm_dsb(15);
        arm_isb(15);
 
        ret = 0;
    } else if (type == CACHE_DCACHE) {
        arch_invalidate_dcache(addr, addr+size);
        ret = 0;
    }
    
    return ret;
}