/****************************************************************************
 * \apps\examples\hcom\tests\developer_tests.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_upd_shared.h>

#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/
// static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
void hcom_developer_tests_developer_1(uint32_t userData)
{
#if HCOM_INCLUDE_ESPCP_TESTS > 0
  #warning "ESP32 Coprocessor Tests are enabled."
  hcom_via_nx_execute_espcp_tests();
#endif

#if HCOM_INCLUDE_SNPRINTF_ON_NUTTX_TESTS_IN_BUILD > 0
  #warning "snprintf Tests are enabled."
  diag_misc_tests_snprintf_on_nuttx(userData);
#endif

#if HCOM_INCLUDE_GPIO_DIAG_TESTS_IN_BUILD > 0
  #warning "GPIO Tests are enabled."
  hcom_meadow_diag_gpio_tests(userData);
#endif

}

//==============================================================
void hcom_developer_tests_developer_2(uint32_t userData)
{

#if defined(CONFIG_EXAMPLES_SQLITE_TESTS)
  hcom_meadow_sqlite_tests(userData);
#endif

}

//==============================================================
void hcom_developer_tests_developer_3(uint32_t userData)
{
#if HCOM_INCLUDE_INI_CFG_TESTS_IN_BUILD > 0
  hcom_tests_ini_cfg_execute_selected(userData);
#endif

#if HCOM_INCLUDE_BATTERY_BACKED_REG_TEST > 0
  if(userData == 0)
    hcom_bbr_tests();
#endif
}

//==============================================================
void hcom_developer_tests_developer_4(uint32_t userData)
{
  // Shows all devices within Nuttx system on cli
  hcom_file_lists_all_dev_dir_and_files_start(userData);
}

