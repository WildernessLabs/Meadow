/****************************************************************************
 * syscall/syscall_semihosting.c
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
#include <syscall.h>
#include <unistd.h>
#include <stdarg.h>

#define SEMIHOSTING_OPEN 0x01
#define SEMIHOSTING_CLOSE 0x02
#define SEMIHOSTING_WRITEC 0x03
#define SEMIHOSTING_WRITE0 0x04
#define SEMIHOSTING_WRITE 0x05
#define SEMIHOSTING_READ 0x06
#define SEMIHOSTING_READC 0x07
#define SEMIHOSTING_ISERROR 0x08
#define SEMIHOSTING_ISTTY 0x09
#define SEMIHOSTING_SEEK 0x0A
#define SEMIHOSTING_FLEN 0x0C
#define SEMIHOSTING_TMPNAM 0x0D
#define SEMIHOSTING_REMOVE 0x0E
#define SEMIHOSTING_CLOCK 0x10
#define SEMIHOSTING_TIME 0x11
#define SEMIHOSTING_SYSTEM 0x12
#define SEMIHOSTING_ERRNO 0x13
#define SEMIHOSTING_GET_CMDLINE 0x15
#define SEMIHOSTING_HEAPINFO 0x16
#define SEMIHOSTING_ELAPSED 0x30
#define SEMIHOSTING_TICKFREQ 0x31

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

#ifdef CONFIG_SEMIHOSTING_OPEN
typedef struct
{
    const char *parm1;
    int parm2;
    uintptr_t parm3;
    uintptr_t parm4;
    uintptr_t parm5;
    uintptr_t parm6;
} open_args_semihosting_t;

int open(const char *parm1, int parm2, ...)
{
    va_list ap;
    open_args_semihosting_t args;

    args.parm1 = parm1;
    args.parm2 = parm2;

    va_start(ap, parm2);
    args.parm3 = va_arg(ap, uintptr_t);
    args.parm4 = va_arg(ap, uintptr_t);
    args.parm5 = va_arg(ap, uintptr_t);
    args.parm6 = va_arg(ap, uintptr_t);
    va_end(ap);

    return (int)__semihost_call(SEMIHOSTING_OPEN, &args);
}
#endif

#ifdef CONFIG_SEMIHOSTING_READ
typedef struct
{
    int parm1;
    void *parm2;
    size_t parm3;
} read_args_semihosting_t;

ssize_t read(int parm1, FAR void *parm2, size_t parm3)
{
    int ret;
    read_args_semihosting_t args;
    args.parm1 = parm1;
    args.parm2 = parm2;
    args.parm3 = parm3;

    ret = (int)__semihost_call(SEMIHOSTING_READ, &args);

    if (ret < 0)
		return ret;

	return (ssize_t)(parm3 - ret);
}
#endif

#ifdef CONFIG_SEMIHOSTING_WRITE
typedef struct
{
    int parm1;
    void *parm2;
    size_t parm3;
} write_args_semihosting_t;

ssize_t write(int parm1, FAR const void *parm2, size_t parm3)
{
    int ret;
    write_args_semihosting_t args;
    args.parm1 = parm1;
    args.parm2 = parm2;
    args.parm3 = parm3;

    ret = __semihost_call(SEMIHOSTING_WRITE, &args);

    if (ret < 0)
		return ret;

	return (ssize_t)(parm3 - ret);
}
#endif
