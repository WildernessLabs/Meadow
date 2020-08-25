/****************************************************************************
 * \configs\stm32f777zit6-meadow\src\hcom_nx\hcom_nx_bbreg_access.c
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

// This module contains code to support access to the STM32F7 battery backed registers

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <ctype.h>
#include <stdint.h>

#include "hcom_nx_common.h"
#include <meadow/hcom_bbreg_defn.h>

#include <arch/board/board.h>
#include "stm32_gpio.h"

//===================================================================
// This function takes a value (0-31) for the battery backed reg,
// reads the value then based on the mask clears all other bits and
// right justifies the remaining bit(s).
uint32_t hcom_bb_reg_acc_read_bbr_and_right_justify(uint32_t bitMask)
{
  uint32_t regValue;
 
  if(bitMask == 0)
    return 0;
  
  regValue = getreg32(HCOM_MEADOW_BATTERY_BACKED_REGISTER);
  if(regValue == 0)
    return 0;

  regValue &= bitMask;    // Save only bit in mask

  // We have the 32-bit value, shift right based on the mask
  uint32_t tempMask = bitMask;
  while((tempMask & 0x00000001) == 0)
  {
    tempMask >>= 1;
    regValue >>= 1;
  }

  return regValue;
}

//===================================================================
// Reads any of the 32 battery backed registers
uint32_t hcom_bb_reg_acc_read_bbr()
{
  uint32_t regValue = getreg32(HCOM_MEADOW_BATTERY_BACKED_REGISTER);
  return regValue;
}

//===================================================================
// Writes any of the 32 battery backed registers
void hcom_bb_reg_acc_write_bbr(uint32_t value)
{
  putreg32(value, HCOM_MEADOW_BATTERY_BACKED_REGISTER);
}

//===================================================================
// Reads the state of a bit then clears that bit and returns it's
// original state.
bool hcom_bb_reg_acc_is_bbr_bits_set_n_clear(uint32_t value)
{
  uint32_t regValue;
  
  regValue = getreg32(HCOM_MEADOW_BATTERY_BACKED_REGISTER);
  putreg32(regValue & (~value), HCOM_MEADOW_BATTERY_BACKED_REGISTER);
  return (value & regValue) != 0;
}

//===================================================================
// Reads bits in register and returns their state
bool hcom_bb_reg_acc_is_bbr_bit_set(uint32_t value)
{
  uint32_t regValue;
  regValue = getreg32(HCOM_MEADOW_BATTERY_BACKED_REGISTER);
  return (value & regValue) != 0;
}

//===================================================================
// Set bit(s) in any bit in battery backed register
void hcom_bb_reg_acc_set_bbr_bits(uint32_t value)
{
  modifyreg32(HCOM_MEADOW_BATTERY_BACKED_REGISTER, 0, value);
}

//===================================================================
// Clears the specified bits
void hcom_bb_reg_acc_clear_bbr_bits(uint32_t value)
{
  modifyreg32(HCOM_MEADOW_BATTERY_BACKED_REGISTER, value, 0);
}

//===================================================================
// Clears then sets the specified bits
void hcom_bb_reg_acc_clear_then_set_bbr_bits(uint32_t clearBits, uint32_t setBits)
{
  modifyreg32(HCOM_MEADOW_BATTERY_BACKED_REGISTER, clearBits, setBits);
}
