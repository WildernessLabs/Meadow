/****************************************************************************
 * \apps\examples\hcom\cell\hcom_pppd.c
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

#include <meadow/hcom_protocol.h>
#include <meadow/meadow_os.h>

#include <mqueue.h>
#include <string.h>

#include "netutils/chat.h"
#include "netutils/pppd.h"

#include "hcom_pppd.h"
#include "../misc/espcp_utils.c"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// Note:
// These connection scripts are used by PPPD to send AT commands to the 
// module to connect using cell network
#define CONNECT_SCRIPT_MAX_SIZE 1024
#define DISCONNECT_SCRIPT_MAX_SIZE 64
#define AUTHENTICATION_CMD_MAX_SIZE 128
#define OPERATOR_SELECTION_CMD_MAX_SIZE 128

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *thisFile = __FILE__;
static bool cell_connected = false;
static char *cell_at_cmds_output;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int pppd_chardev(int fd)
{
  int flags;

  flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0)
  {
    return flags;
  }

  flags = fcntl(fd, F_SETFL, flags |O_NONBLOCK);
  if (flags < 0)
  {
    return flags;
  }

  return 0;
}

void pppd_create_connect_scripts(cell_settings_t *cell_settings, char *connect_script, char *disconnect_script)
{
  char *authentication_cmd = (char *)malloc(AUTHENTICATION_CMD_MAX_SIZE * sizeof(char));
  char *operator_selection_cmd = (char *)malloc(OPERATOR_SELECTION_CMD_MAX_SIZE * sizeof(char));

  snprintf_chk(authentication_cmd, AUTHENTICATION_CMD_MAX_SIZE,
      cell_settings->pap_user[0] != '\0' && cell_settings->pap_password[0] != '\0'
          ? "AT+CGAUTH=1,1,\\\"%s\\\",\\\"%s\\\" PAUSE 3 OK "
          : "",
      cell_settings->pap_user,
      cell_settings->pap_password
  );

  // If the carrier operator code or the network operator mode is missing, the 
  // automatic network selection will be used (AT+COPS=0)
  snprintf_chk(operator_selection_cmd, OPERATOR_SELECTION_CMD_MAX_SIZE,
      cell_settings->operator[0] != '\0' && cell_settings->mode[0] != '\0'
          ? "AT+COPS=1,2,\\\"%s\\\",%s PAUSE 3 OK "
          : "AT+COPS=0 PAUSE 3 OK ",
      cell_settings->operator,
      cell_settings->mode
  );

  switch(cell_settings->module_id)
  {
    case CELL_BG770A_MODULE:
        snprintf_chk(connect_script, CONNECT_SCRIPT_MAX_SIZE, 
        "ECHO ON " 
        "TIMEOUT %s "
        "\"\" AT+CMEE=2 "
        "PAUSE 3 "
        "OK AT+CEREG=1 "
        "PAUSE 3 "
        "OK AT+CGDCONT=1,\\\"IP\\\",\\\"%s\\\" "
        "PAUSE 3 "
        "OK %s"
        "AT+QCSQ "
        "PAUSE 3 "
        "OK AT+CSQ "
        "PAUSE 3 "
        "OK %s"
        "ATD*99# "
        "CONNECT \\c",
        cell_settings->timeout, 
        cell_settings->apn,
        authentication_cmd,
        operator_selection_cmd
      );
    break;
  
    case CELL_M95_MODULE:
      snprintf_chk(connect_script, CONNECT_SCRIPT_MAX_SIZE, 
        "ECHO ON " 
        "TIMEOUT %s "
        "\"\" AT+QACCM=0,0 "
        "PAUSE 3 "
        "OK AT+CREG? "
        "PAUSE 3 "
        "OK AT+CGDCONT=1,\\\"IP\\\",\\\"%s\\\" "
        "PAUSE 3 "
        "OK AT+CSQ "
        "PAUSE 3 "
        "OK ATD*99# "
        "CONNECT \\c",
        cell_settings->timeout, 
        cell_settings->apn
      );
    break;
    
    case CELL_BG95M3_MODULE:
      snprintf_chk(connect_script, CONNECT_SCRIPT_MAX_SIZE, 
        "ECHO ON " 
        "TIMEOUT %s "
        "\"\" AT+CMEE=2 "
        "PAUSE 3 "
        "OK AT+GSN "
        "PAUSE 3 "
        "OK AT+CGDCONT=1,\\\"IP\\\",\\\"%s\\\" "
        "PAUSE 3 "
        "OK AT+QCSQ "
        "PAUSE 3 "
        "OK AT+CSQ "
        "PAUSE 3 "
        "OK %s"
        "ATD*99# "
        "CONNECT \\c",
        cell_settings->timeout, 
        cell_settings->apn,
        operator_selection_cmd
      );
    break;

    default:
      hcom_logging_syslog(LOG_ERR, "%s-%d-Failed getting connect script\n", thisFile, __LINE__);
      connect_script = NULL;
    break;
  }

  snprintf_chk(disconnect_script, DISCONNECT_SCRIPT_MAX_SIZE,
      "\"\" ATZ "
      "OK \\c"
  );
}

bool meadow_cell_is_connected()
{
    return cell_connected;
}

int meadow_get_cell_at_cmds_output(unsigned char *buf)
{
    size_t len = strlen(cell_at_cmds_output) + 1;
    memcpy(buf, cell_at_cmds_output, len);

    return len;
}

void meadow_cell_connected_event() 
{
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell network has been successfully connected\n", thisFile, __LINE__);
    
    espcp_event_data_t message;

    message.interface = ESPCP_CELL_INTERFACE;
    message.function = ESPCP_CELL_CONNECTED_EVENT;
    message.status_code = ESPCP_COMPLETED_OK_STATUS_CODE;
    message.message_id = ESPCP_SIMPLE_EVENT_MESSAGE_ID;

    uint32_t encodedEventDataSize = ESPCP_EVENT_DATA_SIZE;
    uint8_t *encodedData = (uint8_t *) malloc(encodedEventDataSize);

    cell_connected = true;

    espcp_encode_event_data(&message, encodedData);

    int result = espcp_queue_event_messages(encodedData);
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell connected event message result: %d\n", thisFile, __LINE__, result);
}

void meadow_cell_disconnected_event() 
{
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell network has been disconnected\n", thisFile, __LINE__);

    espcp_event_data_t message;

    message.interface = ESPCP_CELL_INTERFACE;
    message.function = ESPCP_CELL_DISCONNECTED_EVENT;
    message.status_code = ESPCP_FAILURE_STATUS_CODE;
    message.message_id = ESPCP_SIMPLE_EVENT_MESSAGE_ID;

    uint32_t encodedEventDataSize = ESPCP_EVENT_DATA_SIZE;
    uint8_t *encodedData = (uint8_t *) malloc(encodedEventDataSize);

    espcp_encode_event_data(&message, encodedData);

    int result = espcp_queue_event_messages(encodedData);
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell disconnected event message result: %d\n", thisFile, __LINE__, result);

    cell_connected = false;
}

//====================================================================
// This is the PPPD (Point-to-Point Protocol Daemon) thread, which is 
// responsible to send AT commands to the modem, through the chat app, 
// and to manage the PPP connection.
void pppd_thread(void *cell_settings_ptr)
{
    cell_settings_t *cell_settings = (cell_settings_t *) cell_settings_ptr;

    if (cell_settings == NULL)
    {
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed getting cell settings\n", thisFile, __LINE__);
        return;
    }

    char *connect_script = (char*)malloc(CONNECT_SCRIPT_MAX_SIZE * sizeof(char));
    char *disconnect_script = (char *)malloc(DISCONNECT_SCRIPT_MAX_SIZE * sizeof(char));
    cell_at_cmds_output = (char *)malloc(CONNECT_SCRIPT_OUTPUT_MAX_SIZE * sizeof(char));

    pppd_create_connect_scripts(cell_settings, connect_script, disconnect_script);

    if ((connect_script != NULL) && (disconnect_script != NULL) && (cell_at_cmds_output != NULL))
    {
        hcom_logging_syslog(LOG_INFO, "%s-%d-chat scripts created: %s\n %s\n",
                            thisFile, __LINE__, connect_script, disconnect_script);

        const struct pppd_settings_s pppd_settings =
        {
            .disconnect_script = disconnect_script,
            .connect_script = connect_script,
            .ttyname = cell_settings->ttyname,
            .connect_callback = (void*)meadow_cell_connected_event,
            .disconnect_callback = (void*)meadow_cell_disconnected_event,
            .cell_at_cmds_output = cell_at_cmds_output,
#ifdef CONFIG_NETUTILS_PPPD_PAP
            .pap_username = cell_settings->pap_user,
            .pap_password = cell_settings->pap_password,
#endif
        };

        
        hcom_logging_syslog(LOG_INFO, "%s-%d-Starting PPPD\n", thisFile, __LINE__);
        pppd(&pppd_settings);
    }

    hcom_logging_syslog(LOG_INFO, "%s-%d-Failed starting PPPD\n", thisFile, __LINE__);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

//====================================================================
// This function is called by the startup manager to start the PPPD thread,
// which is responsible to establish cell connection, if Cell interface
// is enabled.
int hcom_pppd_start()
{
    meadow_configuration_t *config = meadow_os_deep_copy_config();

    if ((config != NULL) && (config->default_interface != NULL))
    {
        if (config->default_interface->interface_type != MEADOW_IFT_CELL)
        {
            return OK;
        }

        hcom_logging_syslog(LOG_NOTICE, "%s-%d-Attempting to start PPPD\n", thisFile, __LINE__);
        if (config->default_cell_settings == NULL)
        {
            hcom_logging_syslog(LOG_ERR, "%s-%d-Failed getting default cell settings\n", thisFile, __LINE__);
            meadow_os_config_free_resources(config);
            return -ENODATA;
        }

        int ret;
        pthread_t pppd_thread_id;
        cell_settings_t cell_settings = {
            .module_id = config->default_cell_settings->module_id,
            .module = config->default_cell_settings->module,
            .apn = config->default_cell_settings->apn,
            .operator = config->default_cell_settings->operator,
            .ttyname = config->default_cell_settings->ttyname,
            .mode = config->default_cell_settings->mode,
            .timeout = config->default_cell_settings->timeout,
            .pap_user = config->default_cell_settings->pap_user,
            .pap_password = config->default_cell_settings->pap_password,
            .scan_mode = config->default_cell_settings->scan_mode,
        };

        hcom_logging_syslog(LOG_INFO, "%s-%d-cell module id: %u\n", thisFile, __LINE__, cell_settings.module_id);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell module: %s\n", thisFile, __LINE__, cell_settings.module);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell apn: %s\n", thisFile, __LINE__, cell_settings.apn);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell operator: %s\n", thisFile, __LINE__, cell_settings.operator);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell ttyname: %s\n", thisFile, __LINE__, cell_settings.ttyname);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell timeout: %s\n", thisFile, __LINE__, cell_settings.timeout);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell user: %s\n", thisFile, __LINE__, cell_settings.pap_user);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell password: %s\n", thisFile, __LINE__, cell_settings.pap_password);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell operation mode: %s\n", thisFile, __LINE__, cell_settings.mode);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell scan mode: %u\n", thisFile, __LINE__, cell_settings.scan_mode);

        if (cell_settings.module_id == CELL_UNKNOWN_MODULE)
        {
            hcom_logging_syslog(LOG_INFO, "%s-%d-Failed to start PPPD thread, invalid cell module id: %u\n", thisFile, __LINE__, cell_settings.module_id);
            return EINVAL;
        }
        
        if(cell_settings.scan_mode)
        {
          #ifdef HCOM_CELL_DEBUG_LOGS
                  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
                    "Cell: scanning mode on", thisFile, __LINE__);
          #endif
          meadow_os_config_free_resources(config);
          return OK;
        }

        pthread_attr_t attr;
        struct sched_param param;

        // Initialize thread attributes
        pthread_attr_init(&attr);

        // Set the stack size
        size_t stack_size = HCOM_THREAD_STACKSIZE_CELL_PPPD;
        pthread_attr_setstacksize(&attr, stack_size);

        // Set the scheduling policy to SCHED_FIFO (First-In, First-Out)
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

        // Set the priority of the thread
        param.sched_priority = HCOM_THREAD_PRIORITY_CELL_PPPD;
        pthread_attr_setschedparam(&attr, &param);

        ret = pthread_create(&pppd_thread_id, &attr, pppd_thread, (void *) &cell_settings);
        if (ret == OK)
        {
            hcom_logging_syslog(LOG_INFO, "%s@%d-PPPD thread launched\n", thisFile, __LINE__);

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

//====================================================================
// This function is called by a .NET method to start the cell scanner,
// which is responsible for show the available cell networks, including
// its operator code, if the scan mode is enabled.
int meadow_cell_scanner(char *response)
{
  struct chat_ctl ctl;
  meadow_configuration_t *config = meadow_os_deep_copy_config();
  int ret = -1;

  const char script_scanner[] =
    "\"\" AT+COPS=? "
    "PAUSE 3 OK \\c";

  if (config != NULL)
  {
    char *tty = config->default_cell_settings->ttyname;
    char *timeout = config->default_cell_settings->timeout;
    int scan_mode = config->default_cell_settings->scan_mode;
    
    if (!scan_mode)
    {
      hcom_logging_syslog(LOG_ERR, "%s-%d-Scan mode is disabled\n", thisFile, __LINE__);
      meadow_os_config_free_resources(config);
      return ret;
    }

    ctl.echo = false;
    ctl.verbose = false;
    ctl.timeout = (timeout && timeout[0] != '\0') ? atoi(timeout) : atoi(DEFAULT_CELL_PPPD_TIMEOUT);

    memset(response, 0x00, sizeof(response));
  
    ctl.fd = open(tty, O_RDWR);
    if (ctl.fd < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to open the file descriptor\n", thisFile, __LINE__);
      close(ctl.fd);
      meadow_os_config_free_resources(config);
      return ret;
    }
        
    if (pppd_chardev(ctl.fd) < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to config the file descriptor\n", thisFile, __LINE__);
      close(ctl.fd);
      meadow_os_config_free_resources(config);
      return ret;
    }

    // Switch to DATA MODE from AT MODE (required to send AT commands)
    write(ctl.fd,"+++",3);
    sleep(2);
    write(ctl.fd, "ATE1\r\n", 6);
    sleep(2);

    chat(&ctl, script_scanner, response);
    close(ctl.fd);
    
    ret = strlen(response);
    if (ret > 0)
    {
      hcom_logging_syslog(LOG_INFO, "%s-%d-AT commands output: %s\n", thisFile, __LINE__, response);
    }
  }

  meadow_os_config_free_resources(config);
  return ret;
}