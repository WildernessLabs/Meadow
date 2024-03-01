/****************************************************************************
 * \apps\examples\hcom\os_rqsts\hcom_bbreg_access.c
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

// This module exists to provide access to the STM32F7 battery backed registers.
// Currently only one BBR is used.

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <ctype.h>
#include "../hcom_common.h"
#include <meadow/hcom_bbreg_defn.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// This function takes a value (0-31) for the battery backed reg,
// reads the value then based on the mask clears all other bits and
// right justifies the remaining bit(s).
uint32_t hcom_bbreg_read_bbr_and_right_justify(uint32_t bitMask)
{
  uint32_t regValue;
 
  if(bitMask == 0)
    return 0;
  
  int ret = hcom_via_nx_get_bbr(&regValue);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() Failed, ret:%d, errno:%d\n",
              thisFile, __LINE__, __func__, ret, errno);
  }

  if(regValue == 0)
    return 0;

  regValue &= bitMask;    // Save only bit in mask

  // We have the 32-bit value, shift right based on the mask
  while((bitMask & 0x00000001) == 0)
  {
    bitMask >>= 1;
    regValue >>= 1;
  }

  return regValue;
}

//===================================================================
// Reads the defined battery backed register
uint32_t hcom_bbreg_read_bbr()
{
  uint32_t regValue;
  int ret = hcom_via_nx_get_bbr(&regValue);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
  }
  return regValue;
}

//===================================================================
// Writes the defined battery backed register
void hcom_bbreg_write_bbr(uint32_t value)
{
  int ret = hcom_via_nx_set_bbr(value);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
  }
}

//===================================================================
// Reads the state of a bit then clears that bit and returns it's
// original state.
bool hcom_bbreg_is_bbr_bits_set_n_clear(uint32_t value)
{
  uint32_t regValue;
  int ret = hcom_via_nx_get_bbr(&regValue);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() #1 Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
  }

  ret = hcom_via_nx_set_bbr(regValue & (~value));
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() #2 Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
  }
  return (value & regValue) != 0;
}

//===================================================================
// Reads bits in register and returns their state
bool hcom_bbreg_is_bbr_bit_set(uint32_t value)
{
  uint32_t regValue;
  int ret = hcom_via_nx_get_bbr(&regValue);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
  }
  return (value & regValue) != 0;
}

//===================================================================
// Set bit(s) in the defined battery backed register
void hcom_bbreg_set_bbr_bits(uint32_t value)
{
  int ret = hcom_via_nx_update_bbr(0, value);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
  }
}

//===================================================================
// Clears the specified bits
void hcom_bbreg_clear_bbr_bits(uint32_t value)
{
  int ret = hcom_via_nx_update_bbr(value, 0);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
  }
}

//-------------------------------------------------------------------
// Special version that does not assume that the nx access fd is from
// the HCOM task list of file descriptors
void hcom_bbreg_clear_bbr_bits_alt(int nx_access_fd, uint32_t value)
{
  int ret = hcom_via_nx_update_bbr_alt(nx_access_fd, value, 0);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
  }
}

//===================================================================
// Clears then sets the specified bits
void hcom_bbreg_clear_then_set_bbr_bits(uint32_t clearBits, uint32_t setBits)
{
  int ret = hcom_via_nx_update_bbr(clearBits, setBits);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-%s() Failed, ret:%d, errno:%d\n",
            thisFile, __LINE__, __func__, ret, errno);
  }
}
