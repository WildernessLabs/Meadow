/****************************************************************************
 * arch/arm/src/armv7-m/meadow_blackbox.h
 *
 * Meadow fault black-box recorder.
 *
 * Rationale (captured on hardware): a heap corruption took a HardFault
 * whose HANDLER then wedged mid-_alert -- the syslog chain touches
 * corrupted state, jumped through a bad pointer, and the device sat dark
 * with VECTACTIVE=3 until power-cycled.  Everything printed by the handler
 * was lost.  This recorder snapshots the fault BEFORE any complex code
 * runs, using only plain stores to a fixed DTCM address (DTCM is unused by
 * the Meadow image, is not touched by .bss/.data init on warm reset, and
 * survives the IWDG reset that now follows a wedged handler).
 *
 * Read side: after any incident, dump 256 bytes at 0x2001FF00 via the
 * ST-Link/GDB, or from firmware on the next boot before DTCM reuse.
 ****************************************************************************/

#ifndef __ARCH_ARM_SRC_ARMV7M_MEADOW_BLACKBOX_H
#define __ARCH_ARM_SRC_ARMV7M_MEADOW_BLACKBOX_H

#include <stdint.h>
#include "up_arch.h"
#include "nvic.h"

#define MEADOW_BLACKBOX_ADDR   0x2001ff00u   /* last 256B of 128K DTCM */
#define MEADOW_BLACKBOX_MAGIC  0x0DDBA11Fu

struct meadow_blackbox_s
{
  uint32_t magic;        /* MEADOW_BLACKBOX_MAGIC when valid */
  uint32_t seq;          /* increments every capture (wraps) */
  uint32_t kind;         /* 'H'ardfault / 'M'emfault / 'B'usfault /
                          * 'U'sagefault / 'A'ssert */
  uint32_t irq;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t dfsr;
  uint32_t bfar;
  uint32_t afsr;
  uint32_t ipsr;
  uint32_t nregs;        /* number of context words captured */
  uint32_t regs[49];     /* raw xcp context copy (bounded) */
};

/* Plain stores only: no calls, no heap, no locks.  Safe from any fault
 * context including a fault-within-fault.
 */

static inline void meadow_blackbox_capture(uint32_t kind, int irq,
                                           uint32_t *context)
{
  volatile struct meadow_blackbox_s *bb =
    (volatile struct meadow_blackbox_s *)MEADOW_BLACKBOX_ADDR;
  uint32_t seq = (bb->magic == MEADOW_BLACKBOX_MAGIC) ? bb->seq + 1 : 0;
  int i;

  bb->magic = 0;                    /* invalidate while writing */
  bb->seq   = seq;
  bb->kind  = kind;
  bb->irq   = (uint32_t)irq;
  bb->cfsr  = getreg32(NVIC_CFAULTS);
  bb->hfsr  = getreg32(NVIC_HFAULTS);
  bb->dfsr  = getreg32(NVIC_DFAULTS);
  bb->bfar  = getreg32(NVIC_BFAULT_ADDR);
  bb->afsr  = getreg32(NVIC_AFAULTS);
  bb->ipsr  = getipsr();

  bb->nregs = 0;
  if (context != 0)
    {
      for (i = 0; i < 49; i++)
        {
          bb->regs[i] = context[i];
        }

      bb->nregs = 49;
    }

  bb->magic = MEADOW_BLACKBOX_MAGIC;
}

#endif /* __ARCH_ARM_SRC_ARMV7M_MEADOW_BLACKBOX_H */
