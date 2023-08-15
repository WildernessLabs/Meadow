/****************************************************************************
 * libs/libc/modlib/modlib_bind.c
 *
 *   Copyright (C) 2015, 2017, 2019 Gregory Nutt. All rights reserved.
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

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/elf.h>
#include <nuttx/lib/modlib.h>

#include "libc.h"
#include "modlib/modlib.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* REVISIT:  This naming breaks the NuttX coding standard, but is consistent
 * with legacy naming of other ELF32 types.
 */

typedef struct
{
  dq_entry_t      entry;
  Elf32_Sym       sym;
  int             idx;
} Elf32_SymCache;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: modlib_readrels
 *
 * Description:
 *   Read the (ELF32_Rel structure * buffer count) into memory.
 *
 ****************************************************************************/

static inline int modlib_readrels(FAR struct mod_loadinfo_s *loadinfo,
                                  FAR const Elf32_Shdr *relsec,
                                  int index, FAR Elf32_Rel *rels,
                                  int count)
{
  off_t offset;
  int size;

  /* Verify that the symbol table index lies within symbol table */

  if (index < 0 || index > (relsec->sh_size / sizeof(Elf32_Rel)))
    {
      berr("ERROR: Bad relocation symbol index: %d\n", index);
      return -EINVAL;
    }

  /* Get the file offset to the symbol table entry */

  offset = sizeof(Elf32_Rel) * index;
  size   = sizeof(Elf32_Rel) * count;
  if (offset + size > relsec->sh_size)
    {
      size = relsec->sh_size - offset;
    }

  /* And, finally, read the symbol table entry into memory */

  return modlib_read(loadinfo, (FAR uint8_t *)rels, size,
                     relsec->sh_offset + offset);
}

/****************************************************************************
 * Name: modlib_relocate and modlib_relocateadd
 *
 * Description:
 *   Perform all relocations associated with a section.
 *
 * Returned Value:
 *   0 (OK) is returned on success and a negated errno is returned on
 *   failure.
 *
 ****************************************************************************/

static int modlib_relocate(FAR struct module_s *modp,
                           FAR struct mod_loadinfo_s *loadinfo, int relidx)

{
  FAR Elf32_Shdr *relsec = &loadinfo->shdr[relidx];
  FAR Elf32_Shdr *dstsec = &loadinfo->shdr[relsec->sh_info];
  FAR Elf32_Rel  *rels;
  FAR Elf32_Rel  *rel;
  FAR Elf32_SymCache *cache;
  FAR Elf32_Sym  *sym;
  FAR dq_entry_t *e;
  dq_queue_t      q;
  uintptr_t       addr;
  int             symidx;
  int             ret;
  int             i;
  int             j;

  rels = lib_malloc(CONFIG_MODLIB_RELOCATION_BUFFERCOUNT * sizeof(Elf32_Rel));
  if (!rels)
    {
      berr("Failed to allocate memory for elf relocation rels\n");
      return -ENOMEM;
    }

  dq_init(&q);

  /* Examine each relocation in the section.  'relsec' is the section
   * containing the relations.  'dstsec' is the section containing the data
   * to be relocated.
   */

  ret = OK;

  for (i = j = 0; i < relsec->sh_size / sizeof(Elf32_Rel); i++)
    {
      /* Read the relocation entry into memory */

      rel = &rels[i % CONFIG_MODLIB_RELOCATION_BUFFERCOUNT];

      if (!(i % CONFIG_MODLIB_RELOCATION_BUFFERCOUNT))
        {
          ret = modlib_readrels(loadinfo, relsec, i, rels, CONFIG_MODLIB_RELOCATION_BUFFERCOUNT);
          if (ret < 0)
          {
              berr("ERROR: Section %d reloc %d: Failed to read relocation entry: %d\n",
                   relidx, i, ret);
              break;
          }
        }
 
      /* Get the symbol table index for the relocation.  This is contained
       * in a bit-field within the r_info element.
       */

      symidx = ELF32_R_SYM(rel->r_info);

      /* First try the cache */

      sym = NULL;
      for (e = dq_peek(&q); e; e = dq_next(e))
        {
          cache = (FAR Elf32_SymCache *)e;
          if (cache->idx == symidx)
            {
              dq_rem(&cache->entry, &q);
              dq_addfirst(&cache->entry, &q);
              sym = &cache->sym;
              break;
            }
        }

      /* If the symbol was not found in the cache, we will need to read the
       * symbol from the file.
       */

      if (sym == NULL)
        {
          if (j < CONFIG_MODLIB_SYMBOL_CACHECOUNT)
            {
              cache = lib_malloc(sizeof(Elf32_SymCache));
              if (!cache)
                {
                  berr("Failed to allocate memory for elf symbols\n");
                  ret = -ENOMEM;
                  break;
                }
              j++;
            }
          else
            {
              cache = (FAR Elf32_SymCache *)dq_remlast(&q);
            }

          sym = &cache->sym;

          /* Read the symbol table entry into memory */

          ret = modlib_readsym(loadinfo, symidx, sym, &loadinfo->shdr[loadinfo->symtabidx]);
          if (ret < 0)
            {
              berr("ERROR: Section %d reloc %d: Failed to read symbol[%d]: %d\n",
                   relidx, i, symidx, ret);
              lib_free(cache);
              break;
            }

          /* Get the value of the symbol (in sym.st_value) */

          ret = modlib_symvalue(modp, loadinfo, sym, loadinfo->shdr[loadinfo->strtabidx].sh_offset);
          if (ret < 0)
            {
              /* The special error -ESRCH is returned only in one condition:  The
               * symbol has no name.
               *
               * There are a few relocations for a few architectures that do
               * no depend upon a named symbol.  We don't know if that is the
               * case here, but we will use a NULL symbol pointer to indicate
               * that case to up_relocate().  That function can then do what
               * is best.
               */

              if (ret == -ESRCH)
                {
                  berr("ERROR: Section %d reloc %d: Undefined symbol[%d] has no name: %d\n",
                      relidx, i, symidx, ret);
                }
              else
                {
                  berr("ERROR: Section %d reloc %d: Failed to get value of symbol[%d]: %d\n",
                      relidx, i, symidx, ret);
                  lib_free(cache);
                  break;
                }
            }

          cache->idx = symidx;
          dq_addfirst(&cache->entry, &q);
        }

      if (sym->st_shndx == SHN_UNDEF && sym->st_name == 0)
        {
          sym = NULL;
        }

      /* Calculate the relocation address. */

      if (rel->r_offset < 0 || rel->r_offset > dstsec->sh_size - sizeof(uint32_t))
        {
          berr("ERROR: Section %d reloc %d: Relocation address out of range, offset %d size %d\n",
               relidx, i, rel->r_offset, dstsec->sh_size);
          ret = -EINVAL;
          break;
        }

      addr = dstsec->sh_addr + rel->r_offset;

      /* Now perform the architecture-specific relocation */

      ret = up_relocate(rel, sym, addr);
      if (ret < 0)
        {
          berr("ERROR: Section %d reloc %d: Relocation failed: %d\n", relidx, i, ret);
          break;
        }
    }

  lib_free(rels);
  while ((e = dq_peek(&q)))
    {
      dq_rem(e, &q);
      lib_free(e);
    }

  return ret;
}

static int modlib_relocateadd(FAR struct module_s *modp,
                           FAR struct mod_loadinfo_s *loadinfo, int relidx)
{
  berr("ERROR: Not implemented\n");
  return -ENOSYS;
}

/****************************************************************************
 * Name: modlib_relocatedyn
 *
 * Description:
 *   Perform all relocations associated with a dynamic section.
 *
 * Returned Value:
 *   0 (OK) is returned on success and a negated errno is returned on
 *   failure.
 *
 ****************************************************************************/

static int modlib_relocatedyn(FAR struct module_s *modp,
                              FAR struct mod_loadinfo_s *loadinfo, int relidx)

{
  FAR Elf32_Shdr *shdr = &loadinfo->shdr[relidx];
  FAR Elf32_Shdr *symhdr;
  FAR Elf32_Dyn  *dyn = NULL;
  FAR Elf32_Rel  *rels = NULL;
  FAR Elf32_Rel  *rel;
  FAR Elf32_Sym  *sym = NULL;
  uintptr_t       addr,
                  offset;
  int             ret;
  int             i, iRel, iSym;
  struct {
	int	strOff;		/* Offset to string table */
	int	symOff;		/* Offset to symbol table */
	int	lSymTab;	/* Size of symbol table */
	int	relEntSz;	/* Size of relocation entry */
  	int	relOff[2];	/* Offset to the relocation section */
	int	relSz[2];	/* Size of relocation table */
#define I_REL	0
#define I_PLT	1
#define N_RELS	2
  } relData;

  dyn = lib_malloc(shdr->sh_size);
  ret = modlib_read(loadinfo, (FAR uint8_t *) dyn, shdr->sh_size, shdr->sh_offset);
  if (ret < 0) 
    {
      berr("Failed to read dynamic section header");
      return ret;
    }

  rels = lib_malloc(CONFIG_MODLIB_RELOCATION_BUFFERCOUNT * sizeof(Elf32_Rel));
  if (!rels)
    {
      berr("Failed to allocate memory for elf relocation rels\n");
      lib_free(dyn);
      return -ENOMEM;
    }

  memset((void *) &relData, 0, sizeof(relData));

  for (i = 0; dyn[i].d_tag != DT_NULL; i++) 
    {
      switch(dyn[i].d_tag) 
	{
          case DT_REL :
              relData.relOff[I_REL] = dyn[i].d_un.d_val;
              break;
          case DT_RELSZ :
              relData.relSz[I_REL] = dyn[i].d_un.d_val;
              break;
          case DT_RELENT :
              relData.relEntSz = dyn[i].d_un.d_val;
              break;
	  case DT_SYMTAB :
	      relData.symOff = dyn[i].d_un.d_val;
 	      break;
	  case DT_STRTAB :
	      relData.strOff = dyn[i].d_un.d_val;
 	      break;
	  case DT_JMPREL :
	      relData.relOff[I_PLT] = dyn[i].d_un.d_val;
 	      break;
	  case DT_PLTRELSZ :
	      relData.relSz[I_PLT] = dyn[i].d_un.d_val;
 	      break;
        }
    }

  symhdr = &loadinfo->shdr[loadinfo->dsymtabidx];
  sym = lib_malloc(symhdr->sh_size);
  if (!sym)
    {
      berr("Error obtaining storage for dynamic symbol table");
      lib_free(rels);
      lib_free(dyn);
      return -ENOMEM;
    }

  ret = modlib_read(loadinfo, (uint8_t *) sym, symhdr->sh_size, symhdr->sh_offset);
  if (ret < 0) 
    {
      berr("Error reading dynamic symbol table - %d", ret);
      lib_free(sym);
      lib_free(rels);
      lib_free(dyn);
      return ret;
    }

  relData.lSymTab = relData.strOff - relData.symOff;

  for (iRel = 0; iRel < N_RELS; iRel++)
    {
      if (relData.relOff[iRel] == 0)
          continue;

      /* Examine each relocation in the .rel.* section.
       */

      ret = OK;

      for (i = 0; i < relData.relSz[iRel] / relData.relEntSz; i++)
        {
          /* Process each relocation entry */

          rel = &rels[i % CONFIG_MODLIB_RELOCATION_BUFFERCOUNT];

          if (!(i % CONFIG_MODLIB_RELOCATION_BUFFERCOUNT))
            {
              size_t relSize = (sizeof(Elf32_Rel) * CONFIG_MODLIB_RELOCATION_BUFFERCOUNT);

              if (relData.relSz[iRel] < relSize)
                {
                  relSize = relData.relSz[iRel];
                }
              ret = modlib_read(loadinfo, (FAR uint8_t *) rels, 
                                relSize,
                                relData.relOff[iRel] + i * sizeof(Elf32_Rel));
              if (ret < 0)
                {
                  berr("ERROR: Section %d reloc %d: Failed to read relocation entry: %d\n",
                       relidx, i, ret);
                  break;
                }
            }

          /* Calculate the relocation address. */

          if (rel->r_offset < 0)
            {
              berr("ERROR: Section %d reloc %d: Relocation address out of range, offset %d\n",
                   relidx, i, rel->r_offset);
              ret = -EINVAL;
              lib_free(sym);
              lib_free(rels);
              lib_free(dyn);
              return ret;
            }

          /* Now perform the architecture-specific relocation */

          if ((iSym = ELF32_R_SYM(rel->r_info)) != 0) 
            {
              if (sym[iSym].st_shndx == SHN_UNDEF)	/* We have an external reference */
                {
                    void *ep;

                    ep = modlib_findglobal(modp, loadinfo, symhdr, &sym[iSym]);
                    if ((ep == NULL) && (ELF32_ST_BIND(sym[iSym].st_info) != STB_WEAK))
                      {
                        berr("ERROR: Unable to resolve address of external reference %s\n",
                             loadinfo->iobuffer);
                        ret = -EINVAL;
                        lib_free(sym);
                        lib_free(rels);
                        lib_free(dyn);
                        return ret;
                      }

                    addr = rel->r_offset + loadinfo->textalloc;
		    *(uintptr_t *)addr = (uintptr_t)ep;
                }
            }
          else
            {
              Elf32_Sym dynSym;

              /*
               * Is it located in .text or .data sections?
               */
              if (rel->r_offset < loadinfo->datasec) 
                {
                  addr = (uintptr_t) loadinfo->textalloc + rel->r_offset;
                }
              else
                {
                  addr = (rel->r_offset - loadinfo->datasec) + (uintptr_t) loadinfo->datastart;
                }
              offset = *(uintptr_t *) addr;

              /*
               * Does this offset live in .text or .data section?
               */
              if (offset < loadinfo->datasec)
                {
                  dynSym.st_value = offset + (uintptr_t) loadinfo->textalloc;
                }
              else
                {
                  dynSym.st_value = (offset - loadinfo->datasec) + (uintptr_t) loadinfo->datastart;
                }
              ret = up_relocate(rel, &dynSym, addr);
            }

          if (ret < 0)
            {
              berr("ERROR: Section %d reloc %d: Relocation failed: %d\n", relidx, i, ret);
              lib_free(sym);
              lib_free(rels);
              lib_free(dyn);
              return ret;
            }
        }
    }    

  /* Iterate through the dynamic symbol table looking for global symbols to put in 
   * our own symbol table for use with dlgetsym()
   */

  /* Relocate the entries in the table */
  for (i = 0; i < (symhdr->sh_size / sizeof(Elf32_Sym)); i++)
    {
      Elf32_Shdr *s = &loadinfo->shdr[sym[i].st_shndx];

      if (sym[i].st_shndx != SHN_UNDEF)
        {
          if (s->sh_addr < loadinfo->datasec)
              sym[i].st_value = sym[i].st_value + loadinfo->textalloc;
          else
              sym[i].st_value = sym[i].st_value - loadinfo->datasec + loadinfo->datastart;
        }
    }

  ret = modlib_insertsymtab(modp, loadinfo, symhdr, sym);

  lib_free(sym);
  lib_free(rels);
  lib_free(dyn);

  return ret;
}


/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: modlib_bind
 *
 * Description:
 *   Bind the imported symbol names in the loaded module described by
 *   'loadinfo' using the exported symbol values provided by modlib_setsymtab().
 *
 * Input Parameters:
 *   modp     - Module state information
 *   loadinfo - Load state information
 *
 * Returned Value:
 *   0 (OK) is returned on success and a negated errno is returned on
 *   failure.
 *
 ****************************************************************************/

int modlib_bind(FAR struct module_s *modp, FAR struct mod_loadinfo_s *loadinfo)
{
  int ret;
  int i;

  /* Find the symbol and string tables */

  ret = modlib_findsymtab(loadinfo);
  if (ret < 0)
    {
      return ret;
    }

  /* Allocate an I/O buffer.  This buffer is used by mod_symname() to
   * accumulate the variable length symbol name.
   */

  ret = modlib_allocbuffer(loadinfo);
  if (ret < 0)
    {
      berr("ERROR: modlib_allocbuffer failed: %d\n", ret);
      return -ENOMEM;
    }

  /* Process relocations in every allocated section */

  for (i = 1; i < loadinfo->ehdr.e_shnum; i++)
    {
      /* Get the index to the relocation section */

      int infosec = loadinfo->shdr[i].sh_info;
      if (infosec >= loadinfo->ehdr.e_shnum)
        {
          continue;
        }

      if (loadinfo->ehdr.e_type == ET_DYN) 
        {
          switch (loadinfo->shdr[i].sh_type) 
            {
              case SHT_DYNAMIC :
                  ret = modlib_relocatedyn(modp, loadinfo, i);
                  break;
              case SHT_DYNSYM :
                  loadinfo->dsymtabidx = i;
                  break;
              case SHT_INIT_ARRAY :
                  loadinfo->initarr = loadinfo->shdr[i].sh_addr - loadinfo->datasec + loadinfo->datastart;
                  loadinfo->ninit = loadinfo->shdr[i].sh_size / sizeof(uintptr_t);
                  break;
              case SHT_FINI_ARRAY :
                  loadinfo->finiarr = loadinfo->shdr[i].sh_addr - loadinfo->datasec + loadinfo->datastart;
                  loadinfo->nfini = loadinfo->shdr[i].sh_size / sizeof(uintptr_t);
                  break;
              case SHT_PREINIT_ARRAY :
                  loadinfo->preiarr = loadinfo->shdr[i].sh_addr - loadinfo->datasec + loadinfo->datastart;
                  loadinfo->nprei = loadinfo->shdr[i].sh_size / sizeof(uintptr_t);
                  break;
            }
        } 
      else
        {
          /* Make sure that the section is allocated.  We can't relocate
           * sections that were not loaded into memory.
           */

          if ((loadinfo->shdr[infosec].sh_flags & SHF_ALLOC) == 0)
                continue;

          /* Process the relocations by type */

          switch (loadinfo->shdr[i].sh_type)
            {
              case SHT_REL :
	          ret = modlib_relocate(modp, loadinfo, i);
                  break;
              case SHT_RELA :
                  ret = modlib_relocateadd(modp, loadinfo, i);
                  break;
            }
        }

      if (ret < 0)
        {
          break;
        }
    }

  /* Ensure that the I and D caches are coherent before starting the newly
   * loaded module by cleaning the D cache (i.e., flushing the D cache
   * contents to memory and invalidating the I cache).
   */
#if 0
  up_coherent_dcache(loadinfo->textalloc, loadinfo->textsize);
  up_coherent_dcache(loadinfo->datastart, loadinfo->datasize);
#endif

  return ret;
}
