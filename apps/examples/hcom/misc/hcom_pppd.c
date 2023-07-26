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

#include <mqueue.h>
#include <string.h>

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

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#define ESPCP_MAXIMUM_MESSAGE_QUEUE_LENGTH 10
#define ESPCP_EVENT_MESSAGE_QUEUE_NAME      "/Esp32Events"
#define ESPCP_REQUEST_MESSAGE_QUEUE_NAME    "/Esp32Requests"
#define ESPCP_EVENT_HANDLER_MESSAGE_QUEUE_NAME    "/IncomingEvents"
#define ESPCP_DEFAULT_MESSAGE_PRIORITY 1
struct espcp_event_data_s
{
    uint8_t interface;
    uint32_t function;
    uint32_t status_code;
    uint32_t message_id;
};
typedef struct espcp_event_data_s espcp_event_data_t;

int queue_cell_event_messages(const espcp_event_data_t *message) {
    mqd_t event_queue_id;
    struct mq_attr queue_attributes;
    int result;

    queue_attributes.mq_maxmsg = ESPCP_MAXIMUM_MESSAGE_QUEUE_LENGTH;
    queue_attributes.mq_msgsize = 13;
    queue_attributes.mq_flags = 0;

    // Open the message queue
    event_queue_id = mq_open(ESPCP_EVENT_MESSAGE_QUEUE_NAME, O_RDWR | O_CREAT, 0666, &queue_attributes);
    if (event_queue_id == (mqd_t)-1) {
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to open queue for sending cell event messages\n", thisFile, __LINE__);
        return -1;
    }

    // Send the message to the queue
    result = mq_send(event_queue_id, (const char *)message, 13, ESPCP_DEFAULT_MESSAGE_PRIORITY);
    if (result == -1) {
        hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to queue the cell event message\n", thisFile, __LINE__);
        mq_close(event_queue_id);
        return -1;
    }

    // Close the message queue when done
    mq_close(event_queue_id);

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

void espcp_encode_uint32(uint32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

void espcp_encode_event_data(espcp_event_data_t *event_data, uint8_t *buffer)
{
    *buffer = event_data->interface;
    buffer += 1;
    espcp_encode_uint32(event_data->function, buffer);
    buffer += 4;
    espcp_encode_uint32(event_data->status_code, buffer);
    buffer += 4;
    espcp_encode_uint32(event_data->message_id, buffer);
}

void meadow_cell_connected_event() 
{
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell network has been successfully connected\n", thisFile, __LINE__);
    
    // Create an instance of espcp_event_data_t
    espcp_event_data_t message;

    // Populate the fields with sample data
    message.interface = 0x07;
    message.function = 0x00;
    message.status_code = 0x00;
    message.message_id = 0x00;

    uint32_t encodedEventDataSize = 13;
    uint8_t *encodedData = (uint8_t *) malloc(encodedEventDataSize);

    espcp_encode_event_data(&message, encodedData);

    // Call the function to send the message
    int result = queue_cell_event_messages(encodedData);
    // hcom_logging_syslog(LOG_INFO, "%s-%d-Cell connected event message result: \n", thisFile, __LINE__, result);

    cell_connected = true;
}

void meadow_cell_disconnected_event() 
{
    hcom_logging_syslog(LOG_INFO, "%s-%d-Cell network has been disconnected\n", thisFile, __LINE__);

    // Create an instance of espcp_event_data_t
    espcp_event_data_t message;

    // Populate the fields with sample data
    message.interface = 0x07;
    message.function = 0x01;
    message.status_code = 0x16U;
    message.message_id = 0x00;

    uint32_t encodedEventDataSize = 13;
    uint8_t *encodedData = (uint8_t *) malloc(encodedEventDataSize);

    espcp_encode_event_data(&message, encodedData);

    // Call the function to send the message
    int result = queue_cell_event_messages(encodedData);
    // hcom_logging_syslog(LOG_INFO, "%s-%d-Cell disconnected event message result: \n", thisFile, __LINE__, result);

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

    pppd_create_connect_scripts(cell_settings, connect_script, disconnect_script);

    if ((connect_script != NULL) && (disconnect_script != NULL))
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


//====================================================================
// This function is called by the startup manager to start the PPPD thread,
// which is responsible to establish cell connection, if BG770A interface
// is desired and enabled.
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

        if (cell_settings.module_id == CELL_UNKNOWN_MODULE)
        {
            hcom_logging_syslog(LOG_INFO, "%s-%d-Failed to start PPPD thread, invalid cell module id: %u\n", thisFile, __LINE__, cell_settings.module_id);
            return EINVAL;
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