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
#define SEMIHOSTING_FSTAT 0x0B
#define SEMIHOSTING_FLEN 0x0C
#define SEMIHOSTING_TMPNAM 0x0D
#define SEMIHOSTING_REMOVE 0x0E
#define SEMIHOSTING_LSEEK 0x0F
#define SEMIHOSTING_CLOCK 0x10
#define SEMIHOSTING_TIME 0x11
#define SEMIHOSTING_SYSTEM 0x12
#define SEMIHOSTING_ERRNO 0x13
#define SEMIHOSTING_STAT 0x14
#define SEMIHOSTING_GET_CMDLINE 0x15
#define SEMIHOSTING_HEAPINFO 0x16
#define SEMIHOSTING_ELAPSED 0x30
#define SEMIHOSTING_TICKFREQ 0x31

/* FD values below this will be forwarded to semihosting by NuttX */
/* This is to deal with the base streams such as stdin, stdout, stderr. */
#define SEMIHOSTING_MIN_FD 2

/* Only FD values above this will be forwarded to semihosting by NuttX */
/* Keep in sync with stlink semihosting.c */
#define SEMIHOSTING_BASE_FD 32

#ifdef CONFIG_SEMIHOSTING

int semihosting_open(const char *parm1, int parm2, ...);
ssize_t semihosting_read(int parm1, FAR void *parm2, size_t parm3);
ssize_t semihosting_write(int parm1, FAR const void *parm2, size_t parm3);
int semihosting_fstat(int fd, FAR struct stat *buf);
int semihosting_stat(FAR const char *name, FAR struct stat *buf);
off_t semihosting_lseek(int fd, off_t offset, int whence);

#endif