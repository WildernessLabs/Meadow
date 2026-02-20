/****************************************************************************
 * \apps\examples\hcom\cell\hcom_cmux.c
 *
 *   Copyright (C) 2025 Wilderness Labs. All rights reserved.
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
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/select.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pty.h>

#include <meadow/hcom_protocol.h>
#include <meadow/meadow_os.h>

#include "netutils/cmux.h"
#include "hcom_cmux.h"
#include "cell/hcom_pppd.h"
#include "netutils/pppd.h"

#define HCOM_CMUX_NUMBER_OF_PORTS   (3)
#define HCOM_CMUX_PPP_VIRTUAL_CHANNEL "/dev/pts/2"

static char *thisFile = __FILE__;

static char  g_cmux_script[] = 
    "ECHO ON "
    "TIMEOUT 30 "
    "\"\" ATE0 PAUSE 3 OK "
    "AT+IFC=2,2 PAUSE 3 OK "
    "AT+IPR=115200 PAUSE 3 OK "
    "AT+CMUX=0,0,5,127,10,3,30,10,2 PAUSE 3 OK "
    "\\c";

static FAR const char connect_script[] =
  "ECHO ON " 
        "TIMEOUT 30 "
        "\"\" AT+CMEE=2 "
        "PAUSE 3 "
        "OK AT+GSN "
        "PAUSE 3 "
        "OK AT+CGDCONT=1,\\\"IP\\\",\\\"teal\\\" "
        "PAUSE 3 "
        "OK AT+QCSQ "
        "PAUSE 3 "
        "OK AT+CSQ "
        "PAUSE 3 "
        "OK AT+COPS=0 "
        "PAUSE 3 "
        "OK ATD*99# "
        "CONNECT \\c";

static FAR const char disconnect_script[] =
  "\"\" ATZ "
  "OK \\c";

static void *hcom_mux_thread(void *parameters)
{
    int ret = 0;
    struct cmux_settings_s hcom_cmux;
    cell_settings_t *cell_settings = NULL;
    meadow_configuration_t *config = meadow_os_deep_copy_config();

    cell_settings = malloc(sizeof(cell_settings_t));
    if (cell_settings == NULL)
    {
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate cell settings struct\n", thisFile, __LINE__);
        ret = -ENOMEM;
        goto exit;
    }
    memcpy(cell_settings, config->default_cell_settings, sizeof(cell_settings_t));
    

    hcom_cmux.total_channels = (int) 2;
    hcom_cmux.script = g_cmux_script;
    hcom_cmux.tty_name = cell_settings->ttyname;

    ret = cmux_create(&hcom_cmux);
    if (ret < 0)
    {
        syslog(1, "%s-%d-Failed to start CMUX service.\n", thisFile, __LINE__);
        goto exit;
    }

    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell CMUX created: %d\n", thisFile, __LINE__, ret);

    meadow_os_config_free_resources(config);
    // cell_settings->ppp_ptsname = (char *)HCOM_CMUX_PPP_VIRTUAL_CHANNEL;

    // const struct pppd_settings_s pppd_settings =
    // {
    //     .disconnect_script = disconnect_script,
    //     .connect_script = connect_script,
    //     .ttyname = "/dev/pts/1",
    // #ifdef CONFIG_NETUTILS_PPPD_PAP
    //     .pap_username = "user",
    //     .pap_password = "pass",
    // #endif

    // };
    
    // ret = pppd(&pppd_settings);

    exit:

    if (cell_settings)
    {
        free(cell_settings);
    }

    syslog(1, "%s-%d-Falied to start CMUX service: %d\n", thisFile, __LINE__, ret);

    return NULL;

}

int hcom_cmux_start(void)
{
    int ret = OK;
    meadow_configuration_t *config = meadow_os_deep_copy_config();
    
    if ((config != NULL) &&
    (config->default_interface != NULL))
    {
        
        if (config->default_interface->interface_type != MEADOW_IFT_CELL)
        {
            return ret;
        }
        syslog(1, "%s-%d-Starting CMUX.\n", thisFile, __LINE__);

        // pthread_attr_t attr;
        // struct sched_param param;
        // pthread_t hcom_thread_id;
        // size_t stack_size = 4098;

        // pthread_attr_init(&attr);
        // pthread_attr_setstacksize(&attr, stack_size);
        // param.sched_priority = 190;
        // pthread_attr_setschedparam(&attr, &param);

        // ret = pthread_create(&hcom_thread_id, &attr, hcom_mux_thread, NULL);
        // if (ret == OK)
        // {
        //     syslog(1, "%s@%d-Hcom Mux thread launched\n", thisFile, __LINE__);
        // }

        struct cmux_settings_s hcom_cmux;
        cell_settings_t *cell_settings = NULL;
        meadow_configuration_t *config = meadow_os_deep_copy_config();

        cell_settings = malloc(sizeof(cell_settings_t));
        if (cell_settings == NULL)
        {
            hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate cell settings struct\n", thisFile, __LINE__);
            ret = -ENOMEM;
            goto exit;
        }
        memcpy(cell_settings, config->default_cell_settings, sizeof(cell_settings_t));
        

        hcom_cmux.total_channels = (int) 2;
        hcom_cmux.script = g_cmux_script;
        hcom_cmux.tty_name = cell_settings->ttyname;

        ret = cmux_create(&hcom_cmux);
        if (ret < 0)
        {
            syslog(1, "%s-%d-Failed to start CMUX service.\n", thisFile, __LINE__);
            goto exit;
        }

        hcom_logging_syslog(LOG_INFO, "%s-%d-Cell CMUX created: %d\n", thisFile, __LINE__, ret);
        meadow_os_config_free_resources(config);

        const struct pppd_settings_s pppd_settings =
        {
            .disconnect_script = disconnect_script,
            .connect_script = connect_script,
            .ttyname = "/dev/pts/1",
        #ifdef CONFIG_NETUTILS_PPPD_PAP
            .pap_username = "user",
            .pap_password = "pass",
        #endif

        };

        // //ret = hcom_pppd_start(cell_settings);
        return pppd(&pppd_settings);

    }

    meadow_os_config_free_resources(config);
    return ret;

    exit:

    // if (cell_settings)
    // {
    //     free(cell_settings);
    // }

    hcom_logging_syslog(LOG_ERR, "%s-%d-Falied to start CMUX service: %d\n", thisFile, __LINE__, ret);

    return ret;
}