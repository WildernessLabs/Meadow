/****************************************************************************
 * include/sys/stat.h
 *
 *   Copyright (C) 2007-2009, 2012 Gregory Nutt. All rights reserved.
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

#ifndef __INCLUDE_SYS_STAT_H
#define __INCLUDE_SYS_STAT_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>
#include <time.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* mode_t bit settings (most of these do not apply to Nuttx). This assumes
 * that the full size of a mode_t is 16-bits. (However, mode_t must be size
 * 'int' because it is promoted to size int when passed in varargs).
 *
 *   LTTT ...U UUGG GOOO
 *
 *   Bits 0-2:   Permissions for others
 *   Bits 3-5:   Group permissions
 *   Bits 6-8:   Owner permissions
 *   Bits 9-11:  Not used
 *   Bits 12-14: File type bits
 *   Bit 15:     Symbolic link
 */

#define S_IXOTH     00001  
#define S_IWOTH     00002
#define S_IROTH     00004
#define S_IRWXO     00007

#define S_IXGRP     00010   
#define S_IWGRP     00020
#define S_IRGRP     00040
#define S_IRWXG     00070

#define S_IXUSR     00100 
#define S_IWUSR     00200
#define S_IRUSR     00400
#define S_IRWXU     00700

#define S_ISVTX     01000          /* "Sticky" bit (not used) */
#define S_ISGID     02000      /* Set group ID bit (not used)*/
#define S_ISUID     04000      /* Set UID bit (not used) */

#define S_IFIFO     0010000          
#define S_IFCHR     0020000
#define S_IFDIR     0040000
#define S_IFBLK     0060000
#define S_IFREG     0100000
#define S_IFLNK     0120000
#define S_IFSOCK    0140000

// MEADOW: FIXME
#define S_IFMQ      0
#define S_IFSEM     0
#define S_IFSHM     0

#define S_IFMT      00170000

/* File type macros that operate on an instance of mode_t */

#define S_ISBLK(m)      (((m) & S_IFMT) == S_IFBLK)     /* block special */
#define S_ISCHR(m)      (((m) & S_IFMT) == S_IFCHR)     /* char special */
#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)     /* directory */
#define S_ISFIFO(m)     (((m) & S_IFMT) == S_IFIFO)     /* fifo or socket */
#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)     /* regular file */
#define S_ISLNK(m)      (((m) & S_IFMT) == S_IFLNK)     /* symbolic link */
#define S_ISSOCK(m)     (((m) & S_IFMT) == S_IFSOCK)    /* socket */

#define S_ISMQ(m)       (((m) & S_IFMT) == S_IFMQ)
#define S_ISSEM(m)      (((m) & S_IFMT) == S_IFSEM)
#define S_ISSHM(m)      (((m) & S_IFMT) == S_IFSHM)

/****************************************************************************
 * Type Definitions
 ****************************************************************************/

/* This is the simplified struct stat as returned by stat() and fstat().
 * This structure provides information about a specific file or directory in
 * the file system.
 */

struct stat
{
  /* Required, standard fields */

  mode_t    st_mode;    /* File type, atributes, and access mode bits */
  off_t     st_size;    /* Size of file/directory, in bytes */
  blksize_t st_blksize; /* Blocksize used for filesystem I/O */
  blkcnt_t  st_blocks;  /* Number of blocks allocated */
  time_t    st_atime;   /* Time of last access */
  time_t    st_mtime;   /* Time of last modification */
  time_t    st_ctime;   /* Time of last status change */

  /* Internal fields.  These are part this specific implementation and
   * should not referenced by application code for portability reasons.
   */

#ifdef CONFIG_PSEUDOFS_SOFTLINKS
  uint8_t   st_count;   /* Used internally to limit traversal of links */
#endif
};

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

int mkdir(FAR const char *pathname, mode_t mode);
int mkfifo(FAR const char *pathname, mode_t mode);
int stat(FAR const char *path, FAR struct stat *buf);
int fstat(int fd, FAR struct stat *buf);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __INCLUDE_SYS_STAT_H */
