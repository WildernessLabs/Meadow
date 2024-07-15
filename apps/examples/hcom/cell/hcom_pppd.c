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
#include "netutils/ntpclient.h"

#include "hcom_pppd.h"
#include "../misc/espcp_utils.h"

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
#define GPS_AT_CMD_TIMEOUT 600
#define NETWORK_SCAN_AT_CMD_TIMEOUT 600
#define GET_CSQ_AT_CMD_TIMEOUT 120

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *thisFile = __FILE__;
static bool cell_connected = false;
static char *cell_at_cmds_output;
static hcom_pppd_handler_t hcom_cell_handler;
static hcom_cell_err_t cell_err;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

//====================================================================
// This function is used to generate the connection and disconnection script
// based on cell settings and is later passed to the pppd() function
static int pppd_create_connect_scripts(cell_settings_t *cell_settings, char **connect_script, char **disconnect_script)
{
  char *authentication_cmd = (char *)malloc(AUTHENTICATION_CMD_MAX_SIZE * sizeof(char));
  if (authentication_cmd == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate authentication\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  char *operator_selection_cmd = (char *)malloc(OPERATOR_SELECTION_CMD_MAX_SIZE * sizeof(char));
  if (operator_selection_cmd == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate operator\n", thisFile, __LINE__);
    free(authentication_cmd);
    return -ENOMEM;
  }

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

  switch (cell_settings->module_id)
  {
    case CELL_BG770A_MODULE:
        snprintf_chk(*connect_script, CONNECT_SCRIPT_MAX_SIZE, 
          "ECHO ON " 
          "TIMEOUT %s "
          "\"\" AT+CMEE=2 "
          "PAUSE 3 "
          "OK AT+GSN "
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
      snprintf_chk(*connect_script, CONNECT_SCRIPT_MAX_SIZE, 
        "ECHO ON " 
        "TIMEOUT %s "
        "\"\" AT+QACCM=0,0 "
        "PAUSE 3 "
        "OK AT+GSN "
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
      snprintf_chk(*connect_script, CONNECT_SCRIPT_MAX_SIZE, 
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
      free(authentication_cmd);
      free(operator_selection_cmd);
      return -EINVAL;
    break;
  }

  snprintf_chk(*disconnect_script, DISCONNECT_SCRIPT_MAX_SIZE,
    "\"\" ATZ "
    "OK \\c"
  );

  free(authentication_cmd);
  free(operator_selection_cmd);

  return OK;
}

bool meadow_cell_is_connected(void)
{
    return cell_connected;
}

//====================================================================
// This function is to get the script according to the state (GPS, Signal Quality
// or Scan). After the selected script will be performed in PPPD thread.
static void hcom_pppd_get_script(int state, char *script)
{
  // TODO: Add GPS timeout to cell config yaml
  // TODO: Add a parameter to specify the desired NMEA sentences
  switch (state)
  {
    case CELL_AT_CMD_GPS:
      hcom_logging_syslog(LOG_INFO, "%s-%d-Cell GPS/GNSS\n", thisFile, __LINE__);
      snprintf_chk(hcom_cell_handler.script, CONNECT_SCRIPT_MAX_SIZE,
        "TIMEOUT %d \"\" "
        "AT+QGPS=1,2,180,1 PAUSE 3 OK " 
        "AT+QCFG=\\\"gpio\\\",1,64,1,0,0,1 PAUSE 3 OK "
        "AT+QCFG=\\\"gpio\\\",3,64,1,1 PAUSE 3 OK "
        "AT+QGPSCFG=\\\"nmeasrc\\\",1 PAUSE 120 OK "
        "AT+QGPSGNMEA=\\\"GSV\\\" PAUSE 3 OK "
        "AT+QGPSGNMEA=\\\"GGA\\\" PAUSE 3 OK "
        "AT+QGPSGNMEA=\\\"RMC\\\" PAUSE 3 OK "
        "AT+QGPSGNMEA=\\\"GSA\\\" PAUSE 3 OK "
        "AT+QGPSGNMEA=\\\"VTG\\\" PAUSE 3 OK "
        "AT+QGPSEND PAUSE 3 OK "
        "AT+QCFG=\\\"gpio\\\",1,64,1,0,0,1 PAUSE 3 OK "
        "AT+QCFG=\\\"gpio\\\",3,64,0,1 PAUSE 3 OK "
        "\\c", GPS_AT_CMD_TIMEOUT);
      break;

    case CELL_AT_CMD_SIGNAL_QUALITY:
      hcom_logging_syslog(LOG_INFO, "%s-%d-Cell Signal Quality\n", thisFile, __LINE__);
      snprintf_chk(hcom_cell_handler.script, CONNECT_SCRIPT_MAX_SIZE,
        "TIMEOUT %d \"\" AT+CSQ PAUSE 3 OK \\c",
        GET_CSQ_AT_CMD_TIMEOUT);
      break;

    case CELL_AT_CMD_SCAN:
      hcom_logging_syslog(LOG_INFO, "%s-%d-Cell Scan Network\n", thisFile, __LINE__);
      snprintf_chk(hcom_cell_handler.script, CONNECT_SCRIPT_MAX_SIZE,
        "TIMEOUT %d \"\" AT+COPS=? PAUSE 3 OK \\c",
        NETWORK_SCAN_AT_CMD_TIMEOUT);
      break;

    default:
      break;
  }
}

void meadow_cell_change_state(int state)
{
  if (hcom_cell_handler.script != NULL)
    {
      memset(hcom_cell_handler.script, 0x00, sizeof(hcom_cell_handler.script));
      hcom_pppd_get_script(state, hcom_cell_handler.script);

      if (state != 0)
        {
          if (strlen(hcom_cell_handler.script) > 0)
            {
              hcom_logging_syslog(LOG_INFO, "%s-%d-Cell script: %s\n", thisFile, __LINE__, hcom_cell_handler.script);
              pppd_set_state(&hcom_cell_handler, CELL_AT_CMD);
            }
          pppd_set_state(&hcom_cell_handler, CELL_PAUSED);
        }
      else
        {
          // Waiting until script performed.
          // Do this, we protect the early changed state.
          while (hcom_cell_handler.state == (CELL_AT_CMD | CELL_PAUSED))
            {
              usleep(100);
            }
          hcom_cell_handler.state  = CELL_RESUMED;
        }
    }
  hcom_logging_syslog(LOG_INFO, "%s-%d-Cell current state: %d\n", thisFile, __LINE__, hcom_cell_handler.state);
}

void pppd_set_state(hcom_pppd_handler_t *handler, int state)
{
  handler->state = handler->state | state;
}

void pppd_clear_state(hcom_pppd_handler_t *handler, int state)
{
  handler->state = handler->state ^ state;
}

int meadow_get_cell_at_cmds_output(unsigned char *buf)
{
    size_t len = strlen(cell_at_cmds_output) + 1;
    memcpy(buf, cell_at_cmds_output, len);

    return len;
}

int meadow_get_cell_error (void)
{
  return (int)cell_err;
}

void meadow_cell_connected_event(void) 
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

    meadow_configuration_t *config = meadow_os_deep_copy_config();
    bool get_time = config->get_network_time_at_startup;

    if (get_time)
    {
        ntpc_start();
    }

    meadow_os_config_free_resources(config);

    espcp_encode_event_data(&message, encodedData);

    int result = espcp_queue_event_messages(encodedData);
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell connected event message result: %d\n", thisFile, __LINE__, result);
}

void meadow_cell_disconnected_event(int err_base) 
{
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell network has been disconnected, error: %d\n", thisFile, __LINE__, err_base);

    espcp_event_data_t message;

    message.interface = ESPCP_CELL_INTERFACE;
    
    if (err_base == CELL_PPPD_LOST_CONNECTION_ERR)
    {
      message.function = ESPCP_CELL_DISCONNECTED_EVENT;
    }
    else
    {
      message.function = ESPCP_CELL_ERROR_EVENT;
    }
 
    message.status_code = ESPCP_FAILURE_STATUS_CODE;
    message.message_id = ESPCP_SIMPLE_EVENT_MESSAGE_ID;

    uint32_t encodedEventDataSize = ESPCP_EVENT_DATA_SIZE;
    uint8_t *encodedData = (uint8_t *) malloc(encodedEventDataSize);

    espcp_encode_event_data(&message, encodedData);

    int result = espcp_queue_event_messages(encodedData);
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell disconnected event message result: %d\n", thisFile, __LINE__, result);

    cell_connected = false;
    cell_err = err_base;
}

void meadow_cell_at_cmd_event(int ret)
{
  espcp_event_data_t message;

  if (ret < 0)
  {
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell chat failed: %d\n", thisFile, __LINE__, ret);
    return;
  }

  if (strlen(cell_at_cmds_output))
  {
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell: %s \n", thisFile, __LINE__, cell_at_cmds_output);
    message.interface = ESPCP_CELL_INTERFACE;
    message.function = ESPCP_CELL_AT_CMD_EVENT;
    message.status_code = ESPCP_COMPLETED_OK_STATUS_CODE;
    message.message_id = ESPCP_SIMPLE_EVENT_MESSAGE_ID;

    uint32_t encodedEventDataSize = ESPCP_EVENT_DATA_SIZE;
    uint8_t *encodedData = (uint8_t *) malloc(encodedEventDataSize);

    espcp_encode_event_data(&message, encodedData);

    int result = espcp_queue_event_messages(encodedData);
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell event message result: %d\n", thisFile, __LINE__, result);
  }
}

static int pppd_create_handler(void)
{
  hcom_cell_handler.state = CELL_RESUMED;
  hcom_cell_handler.callback = (void *)meadow_cell_at_cmd_event;
  hcom_cell_handler.script = (char *)malloc(CONNECT_SCRIPT_MAX_SIZE);

  if (hcom_cell_handler.script == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate cell handler script\n", thisFile, __LINE__);
    return -ENOMEM;
  }

  return OK;
}

//====================================================================
// This is the PPPD (Point-to-Point Protocol Daemon) thread, which is 
// responsible to send AT commands to the modem, through the chat app, 
// and to manage the PPP connection.
static void *pppd_thread(void *cell_settings_ptr)
{
    cell_settings_t *cell_settings = (cell_settings_t *) cell_settings_ptr;

    if (cell_settings == NULL)
    {
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed getting cell settings\n", thisFile, __LINE__);
        sleep(20);
        hcom_logging_syslog(LOG_INFO, "%s-%d-Failed starting PPPD\n", thisFile, __LINE__);
        cell_err = CELL_INVALID_SETTING_ERR;
        meadow_cell_disconnected_event(cell_err);
        return NULL;
    }

    if (cell_settings->module_id == CELL_UNKNOWN_MODULE)
    {
        hcom_logging_syslog(LOG_INFO, "%s-%d-Invalid cell module id: %u\n", thisFile, __LINE__, cell_settings->module_id);
        sleep(20);
        hcom_logging_syslog(LOG_INFO, "%s-%d-Failed starting PPPD\n", thisFile, __LINE__);
        cell_err = CELL_INVALID_MODEM_ERR;
        meadow_cell_disconnected_event(cell_err);
        return NULL;
    }

    char *connect_script = (char *)malloc(CONNECT_SCRIPT_MAX_SIZE * sizeof(char));
    if (connect_script == NULL)
    {
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate memory for connect script\n", thisFile, __LINE__);
        return NULL;
    }

    char *disconnect_script = (char *)malloc(DISCONNECT_SCRIPT_MAX_SIZE * sizeof(char));
    if (disconnect_script == NULL)
    {
        free(connect_script);
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate memory for disconnect script\n", thisFile, __LINE__);
        return NULL;
    }

    cell_at_cmds_output = (char *)malloc(CONNECT_SCRIPT_OUTPUT_MAX_SIZE * sizeof(char));
    if (cell_at_cmds_output == NULL)
    {
        free(connect_script);
        free(disconnect_script);
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate memory for cell AT commands output\n", thisFile, __LINE__);
        return NULL;
    }

    int ret;
    ret = pppd_create_connect_scripts(cell_settings, &connect_script, &disconnect_script);
    if (ret < 0)
    {
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to generate connect scripts, ret=%d\n", thisFile, __LINE__, ret);
        free(connect_script);
        free(disconnect_script);
        free(cell_at_cmds_output);
        return NULL;
    }

    ret = pppd_create_handler();
    if (ret < 0)
    {
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to create pppd handler, ret=%d\n", thisFile, __LINE__, ret);
        free(connect_script);
        free(disconnect_script);
        free(cell_at_cmds_output);
        return NULL;
    }

    hcom_logging_syslog(LOG_INFO, "%s-%d-Chat scripts created: %s\n %s\n",
                        thisFile, __LINE__, connect_script, disconnect_script);

    const struct pppd_settings_s pppd_settings =
    {
        .disconnect_script = disconnect_script,
        .connect_script = connect_script,
        .ttyname = cell_settings->ttyname,
        .connect_callback = (void*)meadow_cell_connected_event,
        .disconnect_callback = (void*)meadow_cell_disconnected_event,
        .cell_at_cmds_output = cell_at_cmds_output,
        .cell_handler = &hcom_cell_handler,
#ifdef CONFIG_NETUTILS_PPPD_PAP
            .pap_username = cell_settings->pap_user,
            .pap_password = cell_settings->pap_password,
#endif
    };

    hcom_logging_syslog(LOG_INFO, "%s-%d-Starting PPPD\n", thisFile, __LINE__);
    pppd(&pppd_settings);

    sleep(20);
    hcom_logging_syslog(LOG_INFO, "%s-%d-Failed after starting PPPD\n", thisFile, __LINE__);
    meadow_cell_disconnected_event(cell_err);

    return NULL;
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
    int ret;
    pthread_t pppd_thread_id;
    meadow_configuration_t *config = meadow_os_deep_copy_config();

    if ((config != NULL) && (config->default_interface != NULL))
    {
        if (config->default_interface->interface_type != MEADOW_IFT_CELL)
        {
            return OK;
        }

        hcom_logging_syslog(LOG_INFO, "%s-%d-Attempting to start PPPD\n", thisFile, __LINE__);
        if (config->default_cell_settings == NULL)
        {
            hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to get PPPD settings\n", thisFile, __LINE__);

            // At this moment, the ESP32 is not ready for sending event messages
#ifdef HCOM_CELL_DEBUG_LOGS
            hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
                "Invalid cell settings. Please check your cell configuration file.", thisFile, __LINE__);
#endif

            meadow_os_config_free_resources(config);
            return -ENOENT;
        }

        cell_settings_t *cell_settings = malloc(sizeof(cell_settings_t));
        if (cell_settings == NULL) {
            hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate cell settings struct\n", thisFile, __LINE__);
            return -ENOMEM;
        }

        memcpy(cell_settings, config->default_cell_settings, sizeof(cell_settings_t));

        hcom_logging_syslog(LOG_INFO, "%s-%d-cell module id: %u\n", thisFile, __LINE__, cell_settings->module_id);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell module: %s\n", thisFile, __LINE__, cell_settings->module);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell apn: %s\n", thisFile, __LINE__, cell_settings->apn);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell operator: %s\n", thisFile, __LINE__, cell_settings->operator);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell ttyname: %s\n", thisFile, __LINE__, cell_settings->ttyname);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell timeout: %s\n", thisFile, __LINE__, cell_settings->timeout);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell user: %s\n", thisFile, __LINE__, cell_settings->pap_user);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell password: %s\n", thisFile, __LINE__, cell_settings->pap_password);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell operation mode: %s\n", thisFile, __LINE__, cell_settings->mode);
        hcom_logging_syslog(LOG_INFO, "%s-%d-cell scan mode: %u\n", thisFile, __LINE__, cell_settings->scan_mode);

        if (cell_settings->scan_mode)
        {
#ifdef HCOM_CELL_DEBUG_LOGS
            hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
                "ScanMode config has been deprecated! Consult how to use the network scanner on Meadow cellular docs.", thisFile, __LINE__);
#endif
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

        ret = pthread_create(&pppd_thread_id, &attr, pppd_thread, (void *) cell_settings);
        if (ret == OK)
        {
            hcom_logging_syslog(LOG_INFO, "%s@%d-PPPD thread launched\n", thisFile, __LINE__);

            meadow_os_config_free_resources(config);
            return ret;
        }

        hcom_logging_syslog(LOG_ERR, "%s@%d-The task to run PPPD failed in create\n",
                            thisFile, __LINE__);
        
        cell_err = CELL_PPPD_THREAD_ERR;
        meadow_cell_disconnected_event(cell_err);

        meadow_os_config_free_resources(config);
        return -ret;
    }

  meadow_os_config_free_resources(config);
  return -ENODATA;
}