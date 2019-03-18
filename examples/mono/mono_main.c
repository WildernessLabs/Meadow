/****************************************************************************
 * examples/hello/hello_main.c
 *
 *   Copyright (C) 2008, 2011-2012 Gregory Nutt. All rights reserved.
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
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <syscall.h>

#include "nuttx-functions.h"


/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * hello_main
 ****************************************************************************/

extern int mono_main (int argc, char* argv[]);
extern void mono_dl_register_library(char *name, MonoDlMapping *mappings);


char *foo = "hello";
int a = 0;

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

typedef struct
{
    const char *parm1;
    int parm2;
    int parm3;
    uintptr_t parm4;
    uintptr_t parm5;
    uintptr_t parm6;
} open_args_semihosting_t;

int __semihosting_open(const char *parm1, int parm2, ...)
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

    return (int)__semihost_call(0x01, &args);
}

typedef struct
{
    int parm1;
    void *parm2;
    size_t parm3;
} read_args_semihosting_t;

ssize_t __semihosting_read(int parm1, FAR void *parm2, size_t parm3)
{
    int ret;
    read_args_semihosting_t args;
    args.parm1 = parm1;
    args.parm2 = parm2;
    args.parm3 = parm3;

    ret = (int)__semihost_call(0x06, &args);

    if (ret < 0)
        return ret;

    cacheflush(parm2, ret, CACHE_DCACHE);
    
#ifdef DEBUG_SEMIHOSTING
    __semihost_hexdump("read buffer:", parm2, nread);
#endif
    return ret;
}

extern void symtab_initialize(void);

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int nuttx_mono_main(int argc, char *argv[])
#endif
{
  symtab_initialize();

  int ret;
  const int mono_argc = 4;
  const char *mono_argv[] = {"mono", "--trace", "--interp", "/tmp/app.exe"};
  /*
  {
    int block_fd = open("/dev/mtdblock0", O_RDWR);
    int corlib_fd = __semihosting_open("/tmp/mscorlib.dll", O_RDONLY);
    char buffer[4096];
    int nread;

    do {
      nread = __semihosting_read (corlib_fd, buffer, 4096);
      printf ("read: %d\n", nread);
      nread = write(block_fd, buffer, nread);
      printf ("write: %d\n", nread);
    } while ((nread > 0) || (nread == -1 && errno == EINTR));

  }
  */
  setenv("MONO_PATH", "/tmp", 1);
  setenv("MONO_LOG_LEVEL", "debug", 1);
  mono_dl_register_library("nuttx", meadow_os_mappings);
  ret = mono_main (mono_argc, mono_argv);
  return ret;
}
