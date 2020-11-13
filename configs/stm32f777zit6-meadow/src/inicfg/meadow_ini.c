/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow_inicfg.c
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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
#include "meadow_ini.h"

#define MEADOW_CONFIG_USE_INI_PARSE_STRING 0

// Original Code here:https://github.com/benhoyt/inih
// Cloned 6-Nov-2020
// Modifications made by Peter Moody, Wilderness Labs Inc.
// This option is not needed at this time

// Start of original code vvvv
//--------------------------------------------------------------
/* inih -- simple .INI file parser

SPDX-License-Identifier: BSD-3-Clause

Copyright (C) 2009-2020, Ben Hoyt

inih is released under the New BSD license (see LICENSE.txt). Go to the project
home page for more info:

https://github.com/benhoyt/inih

*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

// #include "ini.h"

#if !INI_USE_STACK
#if INI_CUSTOM_ALLOCATOR
#include <stddef.h>
void *ini_malloc(size_t size);
void ini_free(void *ptr);
void *ini_realloc(void *ptr, size_t size);
#else
#include <stdlib.h>
#define ini_malloc kmm_malloc
#define ini_free kmm_free
#define ini_realloc realloc
#endif
#endif

#define MAX_SECTION 50
#define MAX_NAME 50

#if MEADOW_CONFIG_USE_INI_PARSE_STRING > 0
/* Used by ini_parse_string() to keep track of string parsing state. */
typedef struct
{
  const char *ptr;
  size_t num_left;
} ini_parse_string_ctx;
#endif

/* Strip whitespace chars off end of given string, in place. Return s. */
static char *rstrip(char *s)
{
  char *p = s + strlen(s);
  while (p > s && isspace((unsigned char)(*--p)))
    *p = '\0';
  return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char *lskip(const char *s)
{
  while (*s && isspace((unsigned char)(*s)))
    s++;
  return (char *)s;
}

/* Return pointer to first char (of chars) or inline comment in given string,
   or pointer to NUL at end of string if neither found. Inline comment must
   be prefixed by a whitespace character to register as a comment. */
static char *find_chars_or_comment(const char *s, const char *chars)
{
#if INI_ALLOW_INLINE_COMMENTS
  int was_space = 0;
  while (*s && (!chars || !strchr(chars, *s)) &&
         !(was_space && strchr(INI_INLINE_COMMENT_PREFIXES, *s)))
  {
    was_space = isspace((unsigned char)(*s));
    s++;
  }
#else
  while (*s && (!chars || !strchr(chars, *s)))
  {
    s++;
  }
#endif
  return (char *)s;
}

/* Similar to strncpy, but ensures dest (size bytes) is
   NUL-terminated, and doesn't pad with NULs. */
static char *strncpy0(char *dest, const char *src, size_t size)
{
  /* Could use strncpy internally, but it causes gcc warnings (see issue #91) */
  size_t i;
  for (i = 0; i < size - 1 && src[i]; i++)
    dest[i] = src[i];
  dest[i] = '\0';
  return dest;
}

/* See documentation in header file. */
int ini_parse_stream(int filefd, ini_handler handler, void *user)
{
  /* Uses a fair bit of stack (use heap instead if you need to) */
#if INI_USE_STACK
  char line[INI_MAX_LINE];
  int max_line = INI_MAX_LINE;
#else
  char *line;
  size_t max_line = INI_INITIAL_ALLOC;
#endif
#if INI_ALLOW_REALLOC && !INI_USE_STACK
  char *new_line;
  size_t offset;
#endif
  char section[MAX_SECTION] = "";
  char prev_name[MAX_NAME] = "";

  char *start;
  char *end;
  char *name;
  char *value;
  int lineno = 0;
  int error = 0;

#if !INI_USE_STACK
  line = (char *)ini_malloc(INI_INITIAL_ALLOC);
  if (!line)
  {
    return MEADOW_CONFIG_ERROR_MEM_ALLOC_ERROR;
  }
#endif

#if INI_HANDLER_LINENO
#define HANDLER(u, s, n, v) handler(u, s, n, v, lineno)
#else
#define HANDLER(u, s, n, v) handler(u, s, n, v)
#endif

  /* Scan through stream line by line */
  // int iniLoopCount = 0;
  int readReturn;

  while ((readReturn = meadow_config_get_next_line(filefd,
                            (uint8_t *)line, (size_t)max_line)) > 0)
  {

#if INI_ALLOW_REALLOC && !INI_USE_STACK
    offset = strlen(line);
    while (offset == max_line - 1 && line[offset - 1] != '\n')
    {
      max_line *= 2;
      if (max_line > INI_MAX_LINE)
        max_line = INI_MAX_LINE;
      new_line = ini_realloc(line, max_line);
      if (!new_line)
      {
        ini_free(line);
        return MEADOW_CONFIG_ERROR_MEM_ALLOC_ERROR;
      }
      line = new_line;
      if (reader(line + offset, (int)(max_line - offset), stream) == NULL)
        break;
      if (max_line >= INI_MAX_LINE)
        break;
      offset += strlen(line + offset);
    }
#endif

    lineno++;

    start = line;
#if INI_ALLOW_BOM
    if (lineno == 1 && (unsigned char)start[0] == 0xEF &&
                       (unsigned char)start[1] == 0xBB &&
                       (unsigned char)start[2] == 0xBF)
    {
      start += 3;
    }
#endif
    start = lskip(rstrip(start));

    if (strchr(INI_START_COMMENT_PREFIXES, *start))
    {
      /* Start-of-line comment */
    }
#if INI_ALLOW_MULTILINE
    else if (*prev_name && *start && start > line)
    {
      /* Non-blank line with leading whitespace, treat as continuation
               of previous name's value (as per Python configparser). */
      if (!HANDLER(user, section, prev_name, start) && !error)
        break; // done
    }
#endif
    else if (*start == '[')
    {
      /* A "[section]" line */
      end = find_chars_or_comment(start + 1, "]");
      if (*end == ']')
      {
        *end = '\0';
        strncpy0(section, start + 1, sizeof(section));
        *prev_name = '\0';
#if INI_CALL_HANDLER_ON_NEW_SECTION
        if (!HANDLER(user, section, NULL, NULL) && !error)
          break; // done
#endif
      }
      else if (!error)
      {
        /* No ']' found on section line */
        error = lineno;
      }
    }
    else if (*start)
    {
      /* Not a comment, must be a name[=:]value pair */
      end = find_chars_or_comment(start, "=:");
      if (*end == '=' || *end == ':')
      {
        *end = '\0';
        name = rstrip(start);
        value = end + 1;
#if INI_ALLOW_INLINE_COMMENTS
        end = find_chars_or_comment(value, NULL);
        if (*end)
          *end = '\0';
#endif
        value = lskip(value);
        rstrip(value);

        /* Valid name[=:]value pair found, call handler */
        strncpy0(prev_name, name, sizeof(prev_name));
        if (!HANDLER(user, section, name, value) && !error)
          break; // done
      }
      else if (!error)
      {
        /* No '=' or ':' found on name[=:]value line */
#if INI_ALLOW_NO_VALUE
        *end = '\0';
        name = rstrip(start);
        if (!HANDLER(user, section, name, NULL) && !error)
          break; // done
#else
        error = lineno;
#endif
      }
    }

#if INI_STOP_ON_FIRST_ERROR
    if (error)
      break;
#endif
  } // end while

#if !INI_USE_STACK
  ini_free(line);
#endif

  // Did meadow_config_get_next_line report an error?
  if (readReturn < 0)
  {
    return readReturn;
  }

  return error; // Actually line number
}

/* See documentation in header file. */
int ini_parse_file(int filefd, ini_handler handler, void *user)
{
  return ini_parse_stream(filefd, handler, user);
}

/* See documentation in header file. */
int ini_parse(const char *filename, ini_handler handler, void *user)
{
  int error;
  int filefd;

  filefd = open(filename, O_RDONLY);
  if (filefd == -1)
  {
    syslog(LOG_ERR, "%s@%d-file '%s' open failed, errno:%d\n", __FILE__, __LINE__,
           filename, errno);
    return MEADOW_CONFIG_ERROR_CFG_FILE_OPEN;
  }

  // Do the work
  error = ini_parse_file(filefd, handler, user);
  close(filefd);

  return error;
}

#if MEADOW_CONFIG_USE_INI_PARSE_STRING > 0
/* An ini_reader function to read the next line from a string buffer. This
   is the fgets() equivalent used by ini_parse_string(). */
static char *ini_reader_string(char *str, int num, void *stream)
{
  ini_parse_string_ctx *ctx = (ini_parse_string_ctx *)stream;
  const char *ctx_ptr = ctx->ptr;
  size_t ctx_num_left = ctx->num_left;
  char *strp = str;
  char c;

  if (ctx_num_left == 0 || num < 2)
    return NULL;

  while (num > 1 && ctx_num_left != 0)
  {
    c = *ctx_ptr++;
    ctx_num_left--;
    *strp++ = c;
    if (c == '\n')
      break;
    num--;
  }

  *strp = '\0';
  ctx->ptr = ctx_ptr;
  ctx->num_left = ctx_num_left;
  return str;
}

/* See documentation in header file. */
int ini_parse_string(const char *string, ini_handler handler, void *user)
{
  ini_parse_string_ctx ctx;

  ctx.ptr = string;
  ctx.num_left = strlen(string);
  return ini_parse_stream((ini_reader)ini_reader_string, &ctx, handler,
                          user);
}
#endif // #if MEADOW_CONFIG_USE_INI_PARSE_STRING > 0
