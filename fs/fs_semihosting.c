/****************************************************************************
 * fs/fs_semihosting.c
 *
 *   Copyright (C) 2018 Geoff Norton. All rights reserved.
 *   Author: Geoff Norton <grompf@gmail.com>
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
#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "fs_semihosting.h"

#ifdef CONFIG_SEMIHOSTING

//#define DEBUG_SEMIHOSTING

__attribute__((always_inline)) static inline int __semihost_call(int op, void *args)
{
    int res;
    __asm__ volatile(
        "mov r0, %[op]\n"
        "mov r1, %[args]\n"
        "bkpt 0xab\n"
        "mov %[res], r0\n"
        : [res] "=r"(res)
        : [op] "r"(op), [args] "r"(args)
        : "r0", "r1", "r2", "r4", "ip", "lr", "memory", "cc");

    return res;
}

#ifdef DEBUG_SEMIHOSTING
void __semihost_hexdump (char *desc, void *addr, int len)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;

    // Output description if given.
    if (desc != NULL)
        printf ("%s:\n", desc);

    if (len == 0) {
        printf("  ZERO LENGTH\n");
        return;
    }
    if (len < 0) {
        printf("  NEGATIVE LENGTH: %i\n",len);
        return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  |%s|\n", buff);

            // Output the offset.
            printf ("%08x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);
        if ((i % 8) == 0 && (i % 16) != 0)
            printf(" ");

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff); 
}
#endif

typedef struct
{
    const char *parm1;
    int parm2;
    int parm3;
    uintptr_t parm4;
    uintptr_t parm5;
    uintptr_t parm6;
} open_args_semihosting_t;

int semihosting_open(const char *parm1, int parm2, ...)
{
    va_list ap;
    open_args_semihosting_t args;

    args.parm1 = parm1;
    args.parm2 = parm2;
    args.parm3 = strlen(parm1);

    va_start(ap, parm2);
    args.parm4 = va_arg(ap, uintptr_t);
    args.parm5 = va_arg(ap, uintptr_t);
    args.parm6 = va_arg(ap, uintptr_t);
    va_end(ap);

    return (int)__semihost_call(SEMIHOSTING_OPEN, &args);
}

typedef struct
{
    int parm1;
    void *parm2;
    size_t parm3;
} read_args_semihosting_t;

ssize_t semihosting_read(int parm1, FAR void *parm2, size_t parm3)
{
    int ret;
    read_args_semihosting_t args;
    args.parm1 = parm1;
    args.parm2 = parm2;
    args.parm3 = parm3;

    ret = (int)__semihost_call(SEMIHOSTING_READ, &args);

    if (ret < 0)
        return ret;

    cacheflush(parm2, ret, CACHE_DCACHE);
    
#ifdef DEBUG_SEMIHOSTING
    __semihost_hexdump("read buffer:", parm2, nread);
#endif
    return ret;
}

typedef struct
{
    int parm1;
    void *parm2;
    size_t parm3;
} write_args_semihosting_t;

ssize_t semihosting_write(int parm1, FAR const void *parm2, size_t parm3)
{
    int ret;
    write_args_semihosting_t args;
    args.parm1 = parm1;
    args.parm2 = (void *) parm2;
    args.parm3 = parm3;

    ret = __semihost_call(SEMIHOSTING_WRITE, &args);

    if (ret < 0)
        return ret;

    return (ssize_t)(parm3 - ret);
}

typedef struct
{
    int parm1;
    struct stat *parm2;
} fstat_args_semihosting_t;

int semihosting_fstat(int fd, FAR struct stat *buf)
{
    fstat_args_semihosting_t args;
    args.parm1 = fd;
    args.parm2 = buf;

    cacheflush(buf, sizeof(struct stat), CACHE_DCACHE);
    return __semihost_call(SEMIHOSTING_FSTAT, &args);
}

typedef struct
{
    const char *parm1;
    struct stat *parm2;
    int parm3;
} stat_args_semihosting_t;

int semihosting_stat(FAR const char *name, FAR struct stat *buf)
{
    stat_args_semihosting_t args;
    args.parm1 = name;
    args.parm2 = buf;
    args.parm3 = strlen(name);

    cacheflush(buf, sizeof(struct stat), CACHE_DCACHE);
    return __semihost_call(SEMIHOSTING_STAT, &args);
}

typedef struct
{
    int parm1;
    off_t parm2;
    int parm3;
} lseek_args_semihosting_t;

off_t semihosting_lseek(int fd, off_t offset, int whence)
{
    lseek_args_semihosting_t args;
    args.parm1 = fd;
    args.parm2 = offset;
    args.parm3 = whence;

    return __semihost_call(SEMIHOSTING_LSEEK, &args);
}

#endif
