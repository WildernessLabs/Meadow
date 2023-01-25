/*****************************************************************************
 * arch/arm/src/armv7-m/mpu.h
 *
 *   Copyright (C) 2011, 2013 Gregory Nutt. All rights reserved.
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

#ifndef __ARCH_ARM_SRC_ARMV7M_MPU_H
#define __ARCH_ARM_SRC_ARMV7M_MPU_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#ifndef __ASSEMBLY__
#  include <sys/types.h>
#  include <stdint.h>
#  include <stdbool.h>
#  include <assert.h>
#  include <debug.h>

#  include "up_arch.h"
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* MPU Register Addresses */

#define MPU_TYPE                0xe000ed90 /* MPU Type Register */
#define MPU_CTRL                0xe000ed94 /* MPU Control Register */
#define MPU_RNR                 0xe000ed98 /* MPU Region Number Register */
#define MPU_RBAR                0xe000ed9c /* MPU Region Base Address Register */
#define MPU_RASR                0xe000eda0 /* MPU Region Attribute and Size Register */

#define MPU_RBAR_A1             0xe000eda4 /* MPU alias registers */
#define MPU_RASR_A1             0xe000eda8
#define MPU_RBAR_A2             0xe000edac
#define MPU_RASR_A2             0xe000edb0
#define MPU_RBAR_A3             0xe000edb4
#define MPU_RASR_A3             0xe000edb8

/* MPU Type Register Bit Definitions */

#define MPU_TYPE_SEPARATE       (1 << 0) /* Bit 0: 0:unified or 1:separate memory maps */
#define MPU_TYPE_DREGION_SHIFT  (8)      /* Bits 8-15: Number MPU data regions */
#define MPU_TYPE_DREGION_MASK   (0xff << MPU_TYPE_DREGION_SHIFT)
#define MPU_TYPE_IREGION_SHIFT  (16)     /* Bits 16-23: Number MPU instruction regions */
#define MPU_TYPE_IREGION_MASK   (0xff << MPU_TYPE_IREGION_SHIFT)

/* MPU Control Register Bit Definitions */

#define MPU_CTRL_ENABLE         (1 << 0)  /* Bit 0: Enable the MPU */
#define MPU_CTRL_HFNMIENA       (1 << 1)  /* Bit 1: Enable MPU during hard fault, NMI, and FAULTMAS */
#define MPU_CTRL_PRIVDEFENA     (1 << 2)  /* Bit 2: Enable privileged access to default memory map */

/* MPU Region Number Register Bit Definitions */

#if defined(CONFIG_ARM_MPU_NREGIONS)
#  if CONFIG_ARM_MPU_NREGIONS <= 8
#    define MPU_RNR_MASK            (0x00000007)
#  elif CONFIG_ARM_MPU_NREGIONS <= 16
#    define MPU_RNR_MASK            (0x0000000f)
#  elif CONFIG_ARM_MPU_NREGIONS <= 32
#    define MPU_RNR_MASK            (0x0000001f)
#  else
#    error "FIXME: Unsupported number of MPU regions"
#  endif
#endif

/* MPU Region Base Address Register Bit Definitions */

#define MPU_RBAR_REGION_SHIFT   (0)       /* Bits 0-3: MPU region */
#define MPU_RBAR_REGION_MASK    (15 << MPU_RBAR_REGION_SHIFT)
#define MPU_RBAR_VALID          (1 << 4)  /* Bit 4: MPU Region Number valid */
#define MPU_RBAR_ADDR_MASK      0xffffffe0 /* Bits N-31:  Region base addrese */

/* MPU Region Attributes and Size Register Bit Definitions */

#define MPU_RASR_ENABLE         (1 << 0)  /* Bit 0: Region enable */
#define MPU_RASR_SIZE_SHIFT     (1)       /* Bits 1-5: Size of the MPU protection region */
#define MPU_RASR_SIZE_MASK      (31 << MPU_RASR_SIZE_SHIFT)
#  define MPU_RASR_SIZE_LOG2(n) ((n-1) << MPU_RASR_SIZE_SHIFT)
#define MPU_RASR_SRD_SHIFT      (8)       /* Bits 8-15: Subregion disable */
#define MPU_RASR_SRD_MASK       (0xff << MPU_RASR_SRD_SHIFT)
#  define MPU_RASR_SRD_0        (0x01 << MPU_RASR_SRD_SHIFT)
#  define MPU_RASR_SRD_1        (0x02 << MPU_RASR_SRD_SHIFT)
#  define MPU_RASR_SRD_2        (0x04 << MPU_RASR_SRD_SHIFT)
#  define MPU_RASR_SRD_3        (0x08 << MPU_RASR_SRD_SHIFT)
#  define MPU_RASR_SRD_4        (0x10 << MPU_RASR_SRD_SHIFT)
#  define MPU_RASR_SRD_5        (0x20 << MPU_RASR_SRD_SHIFT)
#  define MPU_RASR_SRD_6        (0x40 << MPU_RASR_SRD_SHIFT)
#  define MPU_RASR_SRD_7        (0x80 << MPU_RASR_SRD_SHIFT)
#define MPU_RASR_ATTR_SHIFT     (16)      /* Bits 16-31: MPU Region Attribute field */
#define MPU_RASR_ATTR_MASK      (0xffff << MPU_RASR_ATTR_SHIFT)
#  define MPU_RASR_B            (1 << 16) /* Bit 16: Bufferable */
#  define MPU_RASR_C            (1 << 17) /* Bit 17: Cacheable */
#  define MPU_RASR_S            (1 << 18) /* Bit 18: Shareable */
#  define MPU_RASR_TEX_SHIFT    (19)      /* Bits 19-21: TEX Address Permisson */
#  define MPU_RASR_TEX_MASK     (7 << MPU_RASR_TEX_SHIFT)
#    define MPU_RASR_TEX_SO     (0 << MPU_RASR_TEX_SHIFT) /* Strongly Ordered */
#    define MPU_RASR_TEX_NOR    (1 << MPU_RASR_TEX_SHIFT) /* Normal           */
#    define MPU_RASR_TEX_DEV    (2 << MPU_RASR_TEX_SHIFT) /* Device           */
#    define MPU_RASR_TEX_BB(bb) ((4|(bb)) << MPU_RASR_TEX_SHIFT)
#      define MPU_RASR_CP_NC    (0)                       /* Non-cacheable */
#      define MPU_RASR_CP_WBRA  (1)                       /* Write back, write and Read- Allocate */
#      define MPU_RASR_CP_WT    (2)                       /* Write through, no Write-Allocate */
#      define MPU_RASR_CP_WB    (4)                       /* Write back, no Write-Allocate */
#  define MPU_RASR_AP_SHIFT     (24)      /* Bits 24-26: Access permission */
#  define MPU_RASR_AP_MASK      (7 << MPU_RASR_AP_SHIFT)
#    define MPU_RASR_AP_NONO    (0 << MPU_RASR_AP_SHIFT) /* P:None U:None */
#    define MPU_RASR_AP_RWNO    (1 << MPU_RASR_AP_SHIFT) /* P:RW   U:None */
#    define MPU_RASR_AP_RWRO    (2 << MPU_RASR_AP_SHIFT) /* P:RW   U:RO   */
#    define MPU_RASR_AP_RWRW    (3 << MPU_RASR_AP_SHIFT) /* P:RW   U:RW   */
#    define MPU_RASR_AP_RONO    (5 << MPU_RASR_AP_SHIFT) /* P:RO   U:None */
#    define MPU_RASR_AP_RORO    (6 << MPU_RASR_AP_SHIFT) /* P:RO   U:RO   */
#  define MPU_RASR_XN           (1 << 28) /* Bit 28: Instruction access disable */

#define MPU_N_SUBREGIONS 8

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__
#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: mpu_allocregion
 *
 * Description:
 *  Allocate the next region
 *
 ****************************************************************************/

unsigned int mpu_allocregion(void);

/****************************************************************************
 * Name: mpu_log2regionceil
 *
 * Description:
 *   Determine the smallest value of l2size (log base 2 size) such that the
 *   following is true:
 *
 *   size <= (1 << l2size)
 *
 ****************************************************************************/

uint8_t mpu_log2regionceil(size_t size);

/****************************************************************************
 * Name: mpu_log2regionfloor
 *
 * Description:
 *   Determine the largest value of l2size (log base 2 size) such that the
 *   following is true:
 *
 *   size >= (1 << l2size)
 *
 ****************************************************************************/

uint8_t mpu_log2regionfloor(size_t size);

/****************************************************************************
 * Name: mpu_subregion
 *
 * Description:
 *   Given (1) the offset to the beginning of valid data, (2) the size of the
 *   memory to be mapped and (2) the log2 size of the mapping to use, determine
 *   the minimal sub-region set to span that memory region.
 *
 * Assumption:
 *   l2size has the same properties as the return value from
 *   mpu_log2regionceil()
 *
 ****************************************************************************/

uint32_t mpu_subregion(uintptr_t base, size_t size, uint8_t l2size);

/****************************************************************************
 * Name: mpu_configure_region
 *
 * Description:
 *   Configure a region for privileged, strongly ordered memory
 *
 ****************************************************************************/

void mpu_configure_region(uintptr_t base, size_t size,
                                        uint32_t flags);

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

/****************************************************************************
 * Name: mpu_showtype
 *
 * Description:
 *   Show the characteristics of the MPU
 *
 ****************************************************************************/

static inline void mpu_showtype(void)
{
#ifdef CONFIG_DEBUG_SCHED_INFO
  uint32_t regval = getreg32(MPU_TYPE);

  sinfo("%s MPU Regions: data=%d instr=%d\n",
        (regval & MPU_TYPE_SEPARATE) != 0 ? "Separate" : "Unified",
        (regval & MPU_TYPE_DREGION_MASK) >> MPU_TYPE_DREGION_SHIFT,
        (regval & MPU_TYPE_IREGION_MASK) >> MPU_TYPE_IREGION_SHIFT);
#endif
}

/****************************************************************************
 * Name: mpu_control
 *
 * Description:
 *   Configure and enable (or disable) the MPU
 *
 ****************************************************************************/

static inline void mpu_control(bool enable, bool hfnmiena, bool privdefena)
{
  uint32_t regval = 0;

  if (enable)
    {
      regval |= MPU_CTRL_ENABLE; /* Enable the MPU */

      if (hfnmiena)
        {
           regval |= MPU_CTRL_HFNMIENA; /* Enable MPU during hard fault, NMI, and FAULTMAS */
        }

      if (privdefena)
        {
          regval |= MPU_CTRL_PRIVDEFENA; /* Enable privileged access to default memory map */
        }
    }

  putreg32(regval, MPU_CTRL);
}

#define IS_POWER_OF_TWO(x) (((x) & ((x) - 1)) == 0)
#define IS_ALIGNED_TO(x, n) (((x) & ((1 << (n)) - 1)) == 0)
#define ALIGN_TO(x, n) ((x) & ~((1 << (n)) - 1))

/****************************************************************************
 * Name: mpu_check_alignment
 *
 * Description:
 *   Make sure the base address is aligned to the size of the region
 *
 ****************************************************************************/

static inline uintptr_t mpu_check_alignment(uintptr_t base, size_t size)
{
  uintptr_t alignedbase;
  uintptr_t alignedend;
  size_t subregionsize;

  /* Calculate the minimum power-of-two region size that contains size. */

  uint8_t l2size = mpu_log2regionceil(size);

  /* If the region size is a power-of-two, and base address is aligned to
     the size, then just return, no sub-regions are necessary. */

  if (IS_POWER_OF_TWO(size) && IS_ALIGNED_TO(base, l2size))
    return base;

  /* If the region size is not a power-of-two, or not aligned to the base
     address then we can try re-aligning the base address to the nearest 
     valid alignment for this region size. */

  alignedbase = ALIGN_TO(base, l2size);

  /* Check that the region size is a multiple of the sub-region size, and
    that the new region starting from the aligned base actually contains
    the unaligned region. */

  alignedend = alignedbase + (1 << l2size);
  subregionsize = 1 << mpu_log2regionceil((1 << l2size) / MPU_N_SUBREGIONS);
  if ((size % subregionsize == 0) && (alignedbase <= base) &&
     (alignedend >= base+size))
    return alignedbase;

  /* Else we do not have a valid MPU mapping, alert the user and abort. */

  _alert("Invalid MPU region, please check the address alignment and size\n");
  PANIC();
}

#undef IS_POWER_OF_TWO
#undef IS_ALIGNED_TO
#undef ALIGN_TO

/****************************************************************************
 * Name: mpu_priv_stronglyordered
 *
 * Description:
 *   Configure a region for privileged, strongly ordered memory
 *
 ****************************************************************************/

// static inline void mpu_priv_stronglyordered(uintptr_t base, size_t size)
// {
//   unsigned int region = mpu_allocregion();
//   uint32_t     regval;
//   uint8_t      l2size;
//   uint8_t      subregions;
//   uintptr_t    alignedbase;

//   /* Make sure the base address is aligned to the size of the region */

//   alignedbase = mpu_check_alignment(base, size);

//   /* Select the region */

//   putreg32(region, MPU_RNR);

//   /* Select the region base address */

//   putreg32((alignedbase & MPU_RBAR_ADDR_MASK) | region | MPU_RBAR_VALID, MPU_RBAR);

//   /* Select the region size and the sub-region map */

//   l2size     = mpu_log2regionceil(size);
//   subregions = mpu_subregion(base, size, l2size);

//   /* The configure the region */

//   regval = MPU_RASR_ENABLE                              | /* Enable region  */
//            MPU_RASR_SIZE_LOG2((uint32_t)l2size)         | /* Region size    */
//            ((uint32_t)subregions << MPU_RASR_SRD_SHIFT) | /* Sub-regions    */
//                                                           /* Not Cacheable  */
//                                                           /* Not Bufferable */
//            MPU_RASR_S                                   | /* Shareable      */
//            MPU_RASR_AP_RWNO;                              /* P:RW   U:None  */
//   putreg32(regval, MPU_RASR);
// }

#define mpu_priv_stronglyordered(base, size) \
  do \
    { \
      /* The configure the region */ \
      mpu_configure_region(base, size, \
                           MPU_RASR_TEX_SO   | /* Ordered            */ \
                                               /* Not Cacheable      */ \
                                               /* Not Bufferable     */ \
                           MPU_RASR_S        | /* Shareable          */ \
                           MPU_RASR_AP_RWNO    /* P:RW   U:None      */ \
                                               /* Instruction access */); \
    } while (0)

/****************************************************************************
 * Name: mpu_user_flash
 *
 * Description:
 *   Configure a region for user program flash
 *
 ****************************************************************************/

// static inline void mpu_user_flash(uintptr_t base, size_t size)
// {
//   unsigned int region = mpu_allocregion();
//   uint32_t     regval;
//   uint8_t      l2size;
//   uint8_t      subregions;
//   uintptr_t    alignedbase;

//   /* Make sure the base address is aligned to the size of the region */

//   //<! TODO: This has been temporarily disabled as it fails with the offset build
//   // alignedbase = mpu_check_alignment(base, size);
//   alignedbase = base;

//   /* Select the region */

//   putreg32(region, MPU_RNR);

//   /* Select the region base address */

//   putreg32((alignedbase & MPU_RBAR_ADDR_MASK) | region, MPU_RBAR);

//   /* Select the region size and the sub-region map */

//   l2size     = mpu_log2regionceil(size);
//   subregions = mpu_subregion(base, size, l2size);

//   /* The configure the region */

//   regval = MPU_RASR_ENABLE                              | /* Enable region */
//            MPU_RASR_SIZE_LOG2((uint32_t)l2size)         | /* Region size   */
//            ((uint32_t)subregions << MPU_RASR_SRD_SHIFT) | /* Sub-regions   */
//            MPU_RASR_C                                   | /* Cacheable     */
//            MPU_RASR_AP_RORO;                              /* P:RO   U:RO   */
//   putreg32(regval, MPU_RASR);
// }

#define mpu_user_flash(base, size) \
  do \
    { \
      /* The configure the region */ \
      mpu_configure_region(base, size, \
                           MPU_RASR_TEX_SO   | /* Ordered            */ \
                           MPU_RASR_C        | /* Cacheable          */ \
                                               /* Not Bufferable     */ \
                                               /* Not Shareable      */ \
                           MPU_RASR_AP_RORO    /* P:RO   U:RO        */ \
                                               /* Instruction access */); \
    } while (0)



/****************************************************************************
 * Name: mpu_priv_flash
 *
 * Description:
 *   Configure a region for privileged program flash
 *
 ****************************************************************************/

// static inline void mpu_priv_flash(uintptr_t base, size_t size)
// {
//   unsigned int region = mpu_allocregion();
//   uint32_t     regval;
//   uint8_t      l2size;
//   uint8_t      subregions;
//   uintptr_t    alignedbase;

//   /* Make sure the base address is aligned to the size of the region */

//   alignedbase = mpu_check_alignment(base, size);

//   /* Select the region */

//   putreg32(region, MPU_RNR);

//   /* Select the region base address */

//   putreg32((alignedbase & MPU_RBAR_ADDR_MASK) | region, MPU_RBAR);

//   /* Select the region size and the sub-region map */

//   l2size     = mpu_log2regionceil(size);
//   subregions = mpu_subregion(base, size, l2size);

//   /* The configure the region */

//   regval = MPU_RASR_ENABLE                              | /* Enable region */
//            MPU_RASR_SIZE_LOG2((uint32_t)l2size)         | /* Region size   */
//            ((uint32_t)subregions << MPU_RASR_SRD_SHIFT) | /* Sub-regions   */
//            MPU_RASR_C                                   | /* Cacheable     */
//            MPU_RASR_AP_RONO;                              /* P:RO   U:None */
//   putreg32(regval, MPU_RASR);
// }

/****************************************************************************
 * Name: mpu_priv_flash
 *
 * Description:
 *   Configure a region for privileged program flash
 *
 ****************************************************************************/

#define mpu_priv_flash(base, size) \
  do \
    { \
      /* The configure the region */ \
      mpu_configure_region(base, size, \
                           MPU_RASR_TEX_SO   | /* Ordered            */ \
                           MPU_RASR_C        | /* Cacheable          */ \
                                               /* Not Bufferable     */ \
                                               /* Not Shareable      */ \
                           MPU_RASR_AP_RONO    /* P:RO   U:None      */ \
                                               /* Instruction access */); \
    } while (0)



/****************************************************************************
 * Name: mpu_user_intsram
 *
 * Description:
 *   Configure a region as user internal SRAM
 *
 ****************************************************************************/

// static inline void mpu_user_intsram(uintptr_t base, size_t size)
// {
//   unsigned int region = mpu_allocregion();
//   uint32_t     regval;
//   uint8_t      l2size;
//   uint8_t      subregions;
//   uintptr_t    alignedbase;

//   /* Make sure the base address is aligned to the size of the region */

//   alignedbase = mpu_check_alignment(base, size);

//   /* Select the region */

//   putreg32(region, MPU_RNR);

//   /* Select the region base address */

//   putreg32((alignedbase & MPU_RBAR_ADDR_MASK) | region, MPU_RBAR);

//   /* Select the region size and the sub-region map */

//   l2size     = mpu_log2regionceil(size);
//   subregions = mpu_subregion(base, size, l2size);

//   /* Meadow: According to AN4839 (Level 1 cache on STM32F7 Series and STM32H7 Series)
//      the internal SRAM region is non-shareable for the 0x20000000-0x3FFFFFFF
//      address range.

//      TODO: Make this chip-specific in NuttX and clean this up out of the general
//            MPU code.
//    */

//   /* The configure the region */

//   regval = MPU_RASR_ENABLE                              | /* Enable region */
//            MPU_RASR_SIZE_LOG2((uint32_t)l2size)         | /* Region size   */
//            ((uint32_t)subregions << MPU_RASR_SRD_SHIFT) | /* Sub-regions   */
//            //MPU_RASR_S                                   | /* Shareable     */
//            MPU_RASR_C                                   | /* Cacheable     */
//            MPU_RASR_AP_RWRW;                              /* P:RW   U:RW   */

//   /*  Meadow: According to AN4838 - Managing memory protection unit in STM32 MCUs
//       speculative memory reads can occur for QSPI devices.  We have seen a .NET
//       application fail in the qspi_memory_dma method when the system continuously
//       checks the BUSY flag and finds that it is always set.

//       This behaviour is a known issue, see this support question on the STM32 forums:

//       https://community.st.com/s/question/0D50X00009XkXMHSA3/qspi-flag-qspiflagbusy-sometimes-stays-set

//       The comments towards the end of the thread by Amel Nasri suggest that setting
//       the memory region to be strongly ordered will resolve this issue.  Strongly ordered
//       memory should be sharable and in order to not undo the above change we can just
//       set the QSPI memory region to be sharable.
//   */
//   if (base == 0x90000000)
//   {
//     regval |= MPU_RASR_S;
//   }
//   putreg32(regval, MPU_RASR);
// }

//
//  The following is from release 12.0.  This causes the board to crash.
//
// #define mpu_user_intsram(base, size) \
//   do \
//     { \
//       /* The configure the region */ \
//       mpu_configure_region(base, size, \
//                            MPU_RASR_TEX_SO   | /* Ordered            */ \
//                            MPU_RASR_C        | /* Cacheable          */ \
//                                                /* Not Bufferable     */ \
//                            MPU_RASR_S        | /* Shareable          */ \
//                            MPU_RASR_AP_RWRW    /* P:RW   U:RW        */ \
//                                                /* Instruction access */); \
//     } while (0)

//
//  This is the version that works.
//
#define mpu_user_intsram(base, size) \
  do \
    { \
      /* The configure the region */ \
      mpu_configure_region(base, size, \
                           MPU_RASR_TEX_SO   | /* Ordered            */ \
                           MPU_RASR_C        | /* Cacheable          */ \
                                               /* Not Bufferable     */ \
                                               /* Not Shareable      */ \
                           MPU_RASR_AP_RWRW    /* P:RW   U:RW        */ \
                                               /* Instruction access */); \
    } while (0)

/****************************************************************************
 * Name: mpu_priv_intsram
 *
 * Description:
 *   Configure a region as privileged internal SRAM
 *
 ****************************************************************************/

// static inline void mpu_priv_intsram(uintptr_t base, size_t size)
// {
//   unsigned int region = mpu_allocregion();
//   uint32_t     regval;
//   uint8_t      l2size;
//   uint8_t      subregions;
//   uintptr_t    alignedbase;

//   /* Make sure the base address is aligned to the size of the region */

//   alignedbase = mpu_check_alignment(base, size);

//   /* Select the region */

//   putreg32(region, MPU_RNR);

//   /* Select the region base address */

//   putreg32((alignedbase & MPU_RBAR_ADDR_MASK) | region, MPU_RBAR);

//   /* Select the region size and the sub-region map */

//   l2size     = mpu_log2regionceil(size);
//   subregions = mpu_subregion(base, size, l2size);

//   /* The configure the region */

//   regval = MPU_RASR_ENABLE                              | /* Enable region */
//            MPU_RASR_SIZE_LOG2((uint32_t)l2size)         | /* Region size   */
//            ((uint32_t)subregions << MPU_RASR_SRD_SHIFT) | /* Sub-regions   */
//            MPU_RASR_S                                   | /* Shareable     */
//            MPU_RASR_C                                   | /* Cacheable     */
//            MPU_RASR_AP_RWNO;                              /* P:RW   U:None */
//   putreg32(regval, MPU_RASR);
// }

#define mpu_priv_intsram(base, size) \
  do \
    { \
      /* The configure the region */ \
      mpu_configure_region(base, size,\
                           MPU_RASR_TEX_SO   | /* Ordered            */ \
                           MPU_RASR_C        | /* Cacheable          */ \
                                               /* Not Bufferable     */ \
                           MPU_RASR_S        | /* Shareable          */ \
                           MPU_RASR_AP_RWNO    /* P:RW   U:None      */ \
                                               /* Instruction access */); \
    } while (0)

/****************************************************************************
 * Name: mpu_user_extsram
 *
 * Description:
 *   Configure a region as user external SRAM
 *
 ****************************************************************************/

// static inline void mpu_user_extsram(uintptr_t base, size_t size)
// {
//   unsigned int region = mpu_allocregion();
//   uint32_t     regval;
//   uint8_t      l2size;
//   uint8_t      subregions;
//   uintptr_t    alignedbase;

//   /* Make sure the base address is aligned to the size of the region */

//   alignedbase = mpu_check_alignment(base, size);

//   /* Select the region */

//   putreg32(region, MPU_RNR);

//   /* Select the region base address */

//   putreg32((alignedbase & MPU_RBAR_ADDR_MASK) | region, MPU_RBAR);

//   /* Select the region size and the sub-region map */

//   l2size     = mpu_log2regionceil(size);
//   subregions = mpu_subregion(base, size, l2size);

//   /* The configure the region */

//   /* Meadow: According to AN4839 (Level 1 cache on STM32F7 Series and STM32H7 Series)
//      the external RAM region is non-shareable for the 0x80000000-0x9FFFFFFF
//      address range.

//      TODO: Make this chip-specific in NuttX and clean this up out of the general
//            MPU code.
//    */

//   regval = MPU_RASR_ENABLE                              | /* Enable region */
//            MPU_RASR_SIZE_LOG2((uint32_t)l2size)         | /* Region size   */
//            ((uint32_t)subregions << MPU_RASR_SRD_SHIFT) | /* Sub-regions   */
//            //MPU_RASR_S                                   | /* Shareable     */
//            MPU_RASR_C                                   | /* Cacheable     */
//            //MPU_RASR_B                                   | /* Bufferable    */
//            MPU_RASR_AP_RWRW;                              /* P:RW   U:RW   */
//   putreg32(regval, MPU_RASR);
// }

//
//  The following is from release 12.0.  This causes the board to crash.
//
// #define mpu_user_extsram(base, size) \
//   do \
//     { \
//       /* The configure the region */ \
//       mpu_configure_region(base, size, \
//                            MPU_RASR_TEX_SO   | /* Ordered            */ \
//                            MPU_RASR_C        | /* Cacheable          */ \
//                            MPU_RASR_B        | /* Bufferable         */ \
//                            MPU_RASR_S        | /* Shareable          */ \
//                            MPU_RASR_AP_RWRW    /* P:RW   U:RW        */ \
//                                                /* Instruction access */); \
//     } while (0)

//
//  This is the version that works.
//
#define mpu_user_extsram(base, size) \
  do \
    { \
      /* The configure the region */ \
      mpu_configure_region(base, size, \
                           MPU_RASR_TEX_SO   | /* Ordered            */ \
                           MPU_RASR_C        | /* Cacheable          */ \
                                               /* Not Bufferable     */ \
                                               /* Not Shareable      */ \
                           MPU_RASR_AP_RWRW    /* P:RW   U:RW        */ \
                                               /* Instruction access */); \
    } while (0)

/****************************************************************************
 * Name: mpu_priv_extsram
 *
 * Description:
 *   Configure a region as privileged external SRAM
 *
 ****************************************************************************/

// static inline void mpu_priv_extsram(uintptr_t base, size_t size)
// {
//   unsigned int region = mpu_allocregion();
//   uint32_t     regval;
//   uint8_t      l2size;
//   uint8_t      subregions;
//   uintptr_t    alignedbase;

//   /* Make sure the base address is aligned to the size of the region */

//   alignedbase = mpu_check_alignment(base, size);

//   /* Select the region */

//   putreg32(region, MPU_RNR);

//   /* Select the region base address */

//   putreg32((alignedbase & MPU_RBAR_ADDR_MASK) | region, MPU_RBAR);

//   /* Select the region size and the sub-region map */

//   l2size     = mpu_log2regionceil(size);
//   subregions = mpu_subregion(base, size, l2size);

//   /* The configure the region */

//   regval = MPU_RASR_ENABLE                              | /* Enable region */
//            MPU_RASR_SIZE_LOG2((uint32_t)l2size)         | /* Region size   */
//            ((uint32_t)subregions << MPU_RASR_SRD_SHIFT) | /* Sub-regions   */
//            MPU_RASR_S                                   | /* Shareable     */
//            MPU_RASR_C                                   | /* Cacheable     */
//            MPU_RASR_B                                   | /* Bufferable    */
//            MPU_RASR_AP_RWNO;                              /* P:RW   U:None */
//   putreg32(regval, MPU_RASR);
// }

//
//  The following is from release 12.0.  For user mode this casues the board 
//  to crash, the board does not crash at the moment but this could be because
//  it is not used in the curent implementation.
//
#define mpu_priv_extsram(base, size) \
  do \
    { \
      /* The configure the region */ \
      mpu_configure_region(base, size, \
                           MPU_RASR_TEX_SO   | /* Ordered            */ \
                           MPU_RASR_C        | /* Cacheable          */ \
                           MPU_RASR_B        | /* Bufferable         */ \
                           MPU_RASR_S        | /* Shareable          */ \
                           MPU_RASR_AP_RWNO    /* P:RW   U:None      */ \
                                               /* Instruction access */); \
    } while (0)

/****************************************************************************
 * Name: mpu_peripheral
 *
 * Description:
 *   Configure a region as privileged periperal address space
 *
 ****************************************************************************/

// static inline void mpu_peripheral(uintptr_t base, size_t size)
// {
//   unsigned int region = mpu_allocregion();
//   uint32_t     regval;
//   uint8_t      l2size;
//   uint8_t      subregions;
//   uintptr_t    alignedbase;

//   /* Make sure the base address is aligned to the size of the region */

//   alignedbase = mpu_check_alignment(base, size);

//   /* Select the region */

//   putreg32(region, MPU_RNR);

//   /* Select the region base address */

//   putreg32((alignedbase & MPU_RBAR_ADDR_MASK) | region, MPU_RBAR);

//   /* Select the region size and the sub-region map */

//   l2size     = mpu_log2regionceil(size);
//   subregions = mpu_subregion(base, size, l2size);

//   /* Then configure the region */

//   regval = MPU_RASR_ENABLE                              | /* Enable region */
//            MPU_RASR_SIZE_LOG2((uint32_t)l2size)         | /* Region size   */
//            ((uint32_t)subregions << MPU_RASR_SRD_SHIFT) | /* Sub-regions   */
//            MPU_RASR_S                                   | /* Shareable     */
//            MPU_RASR_B                                   | /* Bufferable    */
//            MPU_RASR_AP_RWNO                             | /* P:RW   U:None */
//            MPU_RASR_XN;                                   /* Instruction access disable */

//   putreg32(regval, MPU_RASR);
// }

#define mpu_peripheral(base, size) \
  do \
    { \
      /* Then configure the region */ \
      mpu_configure_region(base, size, \
                           MPU_RASR_TEX_DEV  | /* Device                */ \
                                               /* Not Cacheable         */ \
                           MPU_RASR_B        | /* Bufferable            */ \
                           MPU_RASR_S        | /* Shareable             */ \
                           MPU_RASR_AP_RWNO  | /* P:RW   U:None         */ \
                           MPU_RASR_XN         /* No Instruction access */); \
    } while (0)

/****************************************************************************
 * Name: mpu_user_peripheral
 *
 * Description:
 *   Configure a region as user peripheral address space
 *
 ****************************************************************************/

#define mpu_user_peripheral(base, size) \
  do \
    { \
      /* Then configure the region */ \
      mpu_configure_region(base, size, \
                           MPU_RASR_TEX_DEV  | /* Device                */ \
                                               /* Not Cacheable         */ \
                           MPU_RASR_B        | /* Bufferable            */ \
                           MPU_RASR_S        | /* Shareable             */ \
                           MPU_RASR_AP_RWRW  | /* P:RW     U:RW         */ \
                           MPU_RASR_XN         /* No Instruction access */); \
    } while (0)

/****************************************************************************
 * Name: mpu_configure_region
 *
 * Description:
 *   Configure a region for privileged, strongly ordered memory
 *
 ****************************************************************************/

void mpu_configure_region(uintptr_t base, size_t size,
                                        uint32_t flags);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif  /* __ASSEMBLY__ */
#endif  /* __ARCH_ARM_SRC_ARMV7M_MPU_H */

