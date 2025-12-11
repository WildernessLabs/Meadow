/****************************************************************************
 * \apps\examples\hcom\tests\bbreg_tests.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>

#include "../hcom_common.h"
#include <meadow/hcom_bbreg_defn.h>

/*******************************************************************
* TESTS OF REGISTER ACCESS ON THE NUTTX SIDE
 *******************************************************************/

// Called from one of the Developer CLI commands
void hcom_bbr_tests(uint32_t userData)
{
  #define BBR_TEST_VALUE_DB (0xdeadbeef)
  #define BBR_TEST_VALUE_0x00010000 (0x00010000)
  #define BBR_TEST_VALUE_0 (0)

  uint32_t testValue;

  // Verify that 0 won't be broken is handled correctly
  hcom_bbreg_write_bbr(BBR_TEST_VALUE_0);
  uint32_t returnedValue = hcom_bbreg_read_bbr_and_right_justify(0xffffffff);
  syslog(LOG_MTEST, "Register Tests a: Expected 0x%08x and got 0x%08x\n", BBR_TEST_VALUE_0, returnedValue);

  // Verify that clearing bits is handled correctly
  hcom_bbreg_write_bbr(0xffffffff);
  hcom_bbreg_clear_bbr_bits(0x55555555);
  returnedValue = hcom_bbreg_read_bbr();
  syslog(LOG_MTEST, "Register Tests b: Expected 0x%08x and got 0x%08x\n", 0xaaaaaaaa, returnedValue);

  // Verify that a single bit can be read from 0xffffffff 
  hcom_bbreg_write_bbr(0xffffffff);
  returnedValue = hcom_bbreg_read_bbr_and_right_justify(0x00000100);
  syslog(LOG_MTEST, "Register Tests c: Expected 0x%08x and got 0x%08x\n", 0x00000001, returnedValue);

  // Verify that value not shifted or modified
  hcom_bbreg_set_bbr_bits(0x0000000f);
  returnedValue = hcom_bbreg_read_bbr_and_right_justify(0x0000000f);
  syslog(LOG_MTEST, "Register Tests d: Expected 0x%08x and got 0x%08x\n", 0x0000000f, returnedValue);

  // Verify that value not shifted or modified
  hcom_bbreg_set_bbr_bits(0x0000f000);
  returnedValue = hcom_bbreg_read_bbr_and_right_justify(0x0000f000);
  syslog(LOG_MTEST, "Register Tests e: Expected 0x%08x and got 0x%08x\n", 0x0000000f, returnedValue);

  // Verify that a single bit can be set
  hcom_bbreg_write_bbr(0x0000000f);
  hcom_bbreg_set_bbr_bits(0x00000100);
  returnedValue = hcom_bbreg_read_bbr();
  syslog(LOG_MTEST, "Register Tests f: Expected 0x%08x and got 0x%08x\n", 0x0000010f, returnedValue);

  hcom_bbreg_write_bbr(0x0000010f);
  hcom_bbreg_clear_bbr_bits(0x00000100);
  returnedValue = hcom_bbreg_read_bbr();
  syslog(LOG_MTEST, "Register Tests g: Expected 0x%08x and got 0x%08x\n", 0x0000000f, returnedValue);

  // Write and read the battery backed register
  hcom_bbreg_write_bbr(0);
  hcom_bbreg_write_bbr(BBR_TEST_VALUE_DB);
  testValue = hcom_bbreg_read_bbr();
  syslog(LOG_MTEST, "Register Tests 1: Expected 0x%08x and got 0x%08x\n", BBR_TEST_VALUE_DB, testValue);

  // Set a bit and read value
  hcom_bbreg_write_bbr(0);
  hcom_bbreg_set_bbr_bits(BBR_TEST_VALUE_0x00010000);
  testValue = hcom_bbreg_read_bbr();
  syslog(LOG_MTEST, "Register Tests 2: Expected 0x%08x and got 0x%08x\n", BBR_TEST_VALUE_0x00010000, testValue);

  // Test if expected bit is set
  hcom_bbreg_write_bbr(BBR_TEST_VALUE_0x00010000);
  bool bitsettest = hcom_bbreg_is_bbr_bit_set(BBR_TEST_VALUE_0x00010000);
  syslog(LOG_MTEST, "Register Tests 3: Expected 1 and got %d\n", bitsettest);

  // Clear bit and insure it's clear
  hcom_bbreg_write_bbr(BBR_TEST_VALUE_0x00010000);
  hcom_bbreg_clear_bbr_bits(BBR_TEST_VALUE_0x00010000);
  testValue = hcom_bbreg_read_bbr();
  syslog(LOG_MTEST, "Register Tests 4: Expected 0x%08x and got 0x%08x\n", BBR_TEST_VALUE_0, testValue);

  // Verify we can read and clear the same bit
  hcom_bbreg_write_bbr(0);
  hcom_bbreg_set_bbr_bits(BBR_TEST_VALUE_0x00010000);
  bitsettest = hcom_bbreg_is_bbr_bits_set_n_clear(BBR_TEST_VALUE_0x00010000);
  syslog(LOG_MTEST, "Register Tests 5: Expected 1 and got %d\n", bitsettest);

  testValue = hcom_bbreg_read_bbr();
  syslog(LOG_MTEST, "Register Tests 6: Expected 0x%08x and got 0x%08x\n", BBR_TEST_VALUE_0, testValue);

  hcom_bbreg_write_bbr(0);
}

