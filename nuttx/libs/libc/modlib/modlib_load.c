/****************************************************************************
 * libs/libc/modlib/modlib_load.c
 *
 *   Copyright (C) 2015, 2017 Gregory Nutt. All rights reserved.
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

#include <sys/types.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/lib/modlib.h>

#include "libc.h"
#include "modlib/modlib.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ELF_ALIGN_MASK   ((1 << CONFIG_MODLIB_ALIGN_LOG2) - 1)
#define ELF_ALIGNUP(a)   (((unsigned long)(a) + ELF_ALIGN_MASK) & ~ELF_ALIGN_MASK)
#define ELF_ALIGNDOWN(a) ((unsigned long)(a) & ~ELF_ALIGN_MASK)

#ifndef MAX
#  define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#  define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: modlib_elfsize
 *
 * Description:
 *   Calculate total memory allocation for the ELF file.
 *
 * Returned Value:
 *   0 (OK) is returned on success and a negated errno is returned on
 *   failure.
 *
 ****************************************************************************/

static void modlib_elfsize(struct mod_loadinfo_s *loadinfo)
{
  size_t textsize;
  size_t datasize;
  int i;

  /* Accumulate the size each section into memory that is marked SHF_ALLOC */

  textsize = 0;
  datasize = 0;

  for (i = 0; i < loadinfo->ehdr.e_phnum; i++)
    {
      FAR Elf32_Phdr *phdr = &loadinfo->phdr[i];
      FAR void *textaddr = NULL;

      if (phdr->p_type == PT_LOAD)
	{
	  if (phdr->p_flags & PF_X) 
            {
	      textsize += phdr->p_memsz;
	      textaddr = phdr->p_vaddr;
            }
	  else
            {
	      datasize += phdr->p_memsz;
              loadinfo->datasec = phdr->p_vaddr;
              loadinfo->segpad  = phdr->p_vaddr - ((uintptr_t) textaddr + textsize);
            }
	}
    }

  /* Save the allocation size */

  loadinfo->textsize = textsize;
  loadinfo->datasize = datasize;
}

/****************************************************************************
 * Name: modlib_loadfile
 *
 * Description:
 *   Read the section data into memory. Section addresses in the shdr[] are
 *   updated to point to the corresponding position in the memory.
 *
 * Returned Value:
 *   0 (OK) is returned on success and a negated errno is returned on
 *   failure.
 *
 ****************************************************************************/

static inline int modlib_loadfile(FAR struct mod_loadinfo_s *loadinfo)
{
  FAR uint8_t *text;
  FAR uint8_t *data;
  FAR uint8_t **pptr;
  int ret;
  int i;

  /* Read each PT_LOAD area into memory */

  binfo("Loading sections - text: %p.%x data: %p.%x\n",loadinfo->textalloc,loadinfo->textsize,loadinfo->datastart,loadinfo->datasize);
  text = (FAR uint8_t *)loadinfo->textalloc;
  data = (FAR uint8_t *)loadinfo->datastart;

  for (i = 0; i < loadinfo->ehdr.e_phnum; i++)
    {
      FAR Elf32_Phdr *phdr = &loadinfo->phdr[i];

      if (phdr->p_type == PT_LOAD)
        {
          if (phdr->p_flags & PF_X)
              ret = modlib_read(loadinfo, text, phdr->p_filesz, phdr->p_offset);
          else 
	    {
	      int bssSize = phdr->p_memsz - phdr->p_filesz;
              ret = modlib_read(loadinfo, data, phdr->p_filesz, phdr->p_offset);
	      memset((FAR void *)((uintptr_t) data + phdr->p_filesz), 0, bssSize);
	    }
          if (ret < 0)
            {
              berr("ERROR: Failed to read section %d: %d\n", i, ret);
              return ret;
            }
        }
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: modlib_load
 *
 * Description:
 *   Loads the binary into memory, allocating memory, performing relocations
 *   and initializing the data and bss segments.
 *
 * Returned Value:
 *   0 (OK) is returned on success and a negated errno is returned on
 *   failure.
 *
 ****************************************************************************/

int modlib_load(FAR struct mod_loadinfo_s *loadinfo)
{
  int ret;

  binfo("loadinfo: %p\n", loadinfo);
  DEBUGASSERT(loadinfo && loadinfo->filfd >= 0);

  /* Load section and program headers into memory */

  ret = modlib_loadhdrs(loadinfo);
  if (ret < 0)
    {
      berr("ERROR: modlib_loadhdrs failed: %d\n", ret);
      goto errout_with_buffers;
    }

  /* Determine total size to allocate */

  modlib_elfsize(loadinfo);

  /* Allocate (and zero) memory for the ELF file. */

  /* Allocate memory to hold the ELF image */

  loadinfo->textalloc = (uintptr_t)lib_malloc(loadinfo->textsize + loadinfo->datasize + loadinfo->segpad);
  if (!loadinfo->textalloc)
    {
      berr("ERROR: Failed to allocate memory for the module\n");
      ret = -ENOMEM;
      goto errout_with_buffers;
    }

  loadinfo->datastart = loadinfo->textalloc + loadinfo->textsize + loadinfo->segpad;

  /* Load ELF section data into memory */

  ret = modlib_loadfile(loadinfo);
  if (ret < 0)
    {
      berr("ERROR: modlib_loadfile failed: %d\n", ret);
      goto errout_with_buffers;
    }

  return OK;

  /* Error exits */

errout_with_buffers:
  modlib_unload(loadinfo);
  return ret;
}
