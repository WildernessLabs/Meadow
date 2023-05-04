/****************************************************************************
 * \apps\examples\hcom\misc\hcom_pppd.c
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
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_os.h>

#include "netutils/pppd.h"

#include <string.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Note:
// These connection scripts are used by PPPD to send AT commands to the 
// modem to connect using cell network
#define CONNECT_SCRIPT_MAX_SIZE 1024
#define DISCONNECT_SCRIPT_MAX_SIZE 64

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *thisFile = __FILE__;

//====================================================================
// This is the PPPD (Point-to-Point Protocol Daemon) thread, which is 
// responsible to send AT commands to the modem, through the chat app, 
// and to manage the PPP connection.
void pppd_thread(void *cell_settings_ptr)
{
    cell_settings_t *cell_settings = (cell_settings_t *) cell_settings_ptr;

    if(cell_settings == NULL){
      hcom_logging_syslog(LOG_ERR, "%s-%d-Failed getting cell settings\n", thisFile, __LINE__);
      return;
    }

    char *connect_script = (char*)malloc(CONNECT_SCRIPT_MAX_SIZE * sizeof(char));
    char *disconnect_script = (char *)malloc(DISCONNECT_SCRIPT_MAX_SIZE * sizeof(char));

    snprintf_chk(connect_script, HCOM_MAX_HOST_STRING_BUFF_LENGTH, 
        "ECHO ON " 
        "TIMEOUT %s "
        "\"\" AT+CMEE=2 "
        "PAUSE 3 "
        "OK AT+CEREG=1 "
        "PAUSE 3 "
        "OK AT+CGDCONT=1,\\\"IP\\\",\\\"%s\\\" "
        "PAUSE 3 "
        "OK AT+CGAUTH=1,1,\\\"%s\\\",\\\"%s\\\" "
        "PAUSE 3 "
        "OK AT+CSQ "
        "PAUSE 3 "
        "OK AT+COPS=1,2,\\\"%s\\\",7 "
        "PAUSE 3 "
        "OK ATD*99# "
        "CONNECT \\c",
        cell_settings->timeout, 
        cell_settings->apn,
        cell_settings->pap_user, 
        cell_settings->pap_password,
        cell_settings->operator
    );
    
    snprintf_chk(disconnect_script, HCOM_MED_SHORT_HOST_STRING_BUFF_LENGTH,
        "\"\" ATZ "
        "OK \\r\\c"
    );

    hcom_logging_syslog(LOG_INFO, "%s-%d-chat scripts created: %s\n %s\n",
                          thisFile, __LINE__, connect_script, disconnect_script);

    const struct pppd_settings_s pppd_settings =
    {
        .disconnect_script = disconnect_script,
        .connect_script = connect_script,
        .ttyname = cell_settings->ttyname,
#ifdef CONFIG_NETUTILS_PPPD_PAP
        .pap_username = cell_settings->pap_user,
        .pap_password = cell_settings->pap_password,
#endif
    };  

    hcom_logging_syslog(LOG_INFO, "%s-%d-Starting PPPD\n", thisFile, __LINE__);
    pppd(&pppd_settings);
}

//====================================================================
// This function is called by the startup manager to start the PPPD thread,
// which is responsible to establish cell connection, if BG770A interface
// is desired and enabled.
int hcom_pppd_start()
{  
  meadow_configuration_t *config = meadow_os_deep_copy_config();

  if (config != NULL && config->default_interface != NULL)
  {
    if (config->default_interface->interface_type != MEADOW_IFT_BG770A)
    {
      return OK;
    }

    hcom_logging_syslog(LOG_NOTICE, "%s-%d-Attempting to start PPPD\n", thisFile, __LINE__);

    if (config->default_cell_settings == NULL) {
      hcom_logging_syslog(LOG_ERR, "%s-%d-Failed getting default cell settings\n", thisFile, __LINE__);
      meadow_os_config_free_resources(config);
      return -ENODATA;
    }
    
    int ret;
    pthread_t pppd_thread_id;
    cell_settings_t cell_settings = {
      .apn = config->default_cell_settings->apn,
      .operator = config->default_cell_settings->operator,
      .ttyname = config->default_cell_settings->ttyname,
      .timeout = config->default_cell_settings->timeout,
      .pap_user = config->default_cell_settings->pap_user,
      .pap_password = config->default_cell_settings->pap_password,  
   };

    hcom_logging_syslog(LOG_INFO, "%s-%d-cell apn: %s\n", thisFile, __LINE__, cell_settings.apn);
    hcom_logging_syslog(LOG_INFO, "%s-%d-cell operator: %s\n", thisFile, __LINE__, cell_settings.operator);
    hcom_logging_syslog(LOG_INFO, "%s-%d-cell ttyname: %s\n", thisFile, __LINE__, cell_settings.ttyname);
    hcom_logging_syslog(LOG_INFO, "%s-%d-cell timeout: %s\n", thisFile, __LINE__, cell_settings.timeout);
    hcom_logging_syslog(LOG_INFO, "%s-%d-cell user: %s\n", thisFile, __LINE__, cell_settings.pap_user);
    hcom_logging_syslog(LOG_INFO, "%s-%d-cell password: %s\n", thisFile, __LINE__, cell_settings.pap_password);

    ret = pthread_create(&pppd_thread_id, NULL, pppd_thread, (void *) &cell_settings);
    if (ret == OK)
    {
      hcom_logging_syslog(LOG_INFO, "%s@%d-PPPD launched\n", thisFile, __LINE__);

      meadow_os_config_free_resources(config);
      return ret;
    }

    hcom_logging_syslog(LOG_ERR, "%s@%d-The task to run PPPD failed in create\n",
                        thisFile, __LINE__);

    meadow_os_config_free_resources(config);
    return -ret;
  }

  meadow_os_config_free_resources(config);
  return -ENODATA;
}
