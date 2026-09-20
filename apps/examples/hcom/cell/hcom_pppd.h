/****************************************************************************
 * \apps\examples\hcom\cell\hcom_pppd.h
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
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

#ifndef __CONFIGS_MEADOW_SRC_HCOM_CELL_PPPD__H
#define __CONFIGS_MEADOW_SRC_HCOM_CELL_PPPD__H

#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Shared Definitions
 ****************************************************************************/

#define CONNECT_SCRIPT_MAX_SIZE         (512)
#define CHAT_SCRIPT_MAX_SIZE            (1024)
#define DISCONNECT_SCRIPT_MAX_SIZE      (50)
#define RESET_SCRIPT_MAX_SIZE           (50)

#define CELL_RESUMED                0x00
#define CELL_PAUSED                 (1 << 0)
#define CELL_AT_CMD_GPS             (1 << 1)
#define CELL_AT_CMD_SIGNAL_QUALITY  (1 << 2)
#define CELL_AT_CMD_SCAN            (1 << 3)
#define CELL_AT_CMD                 (1 << 4)
#define CELL_CHAT_DONE              (1 << 5)
#define CELL_CHAT_FAILED            (1 << 6)

/****************************************************************************
 * Public Data
 ****************************************************************************/

struct hcom_pppd_handler_s
{
    int state;
    char *script;
    void (* callback)(int ret);
};
typedef struct hcom_pppd_handler_s hcom_pppd_handler_t;

/****************************************************************************
 * Public Data
 ****************************************************************************/

enum hcom_cell_err_e
{
  CELL_INVALID_SETTING_ERR,
  CELL_INVALID_MODEM_ERR,
  CELL_PPPD_LOST_CONNECTION_ERR,
  CELL_PPPD_TIMEOUT_ERR,
  CELL_PPPD_THREAD_ERR,
};
typedef enum hcom_cell_err_e hcom_cell_err_t;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

void pppd_set_state(hcom_pppd_handler_t *handler, int state);
void pppd_clear_state(hcom_pppd_handler_t *handler, int state);
int hcom_pppd_start(void);

#endif //__CONFIGS_MEADOW_SRC_HCOM_CELL_PPPD__H
