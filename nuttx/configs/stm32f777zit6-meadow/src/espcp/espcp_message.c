 /****************************************************************************
 * Esp32Messaging.c
 *
 *   Generic messaging constants, structures and definitions used
 *   in the messaging system.
 *  
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *   Author: Mark Stevens
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/config.h>

#include "espcp_message.h"
#include "espcp_shared_enums.h"

// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Name: espcp_create_message_on_heap
 *
 * Description:
 *  Create an instance of a message on the heap.
 * 
 * Input Parameters:
 *  message_type - Type of the message.
 *  interface - Interface the message is destined for.
 *  function - Function on the interface to be executed.
 *  status_code - Status code for any function that has been executed.
 *  message_id - ID of this message.
 *  payload - Binary payload for the message.
 *  payload_length - Size of the binary data (payload)
 *
 * Returned Value:
 *  Pointer to a new message.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_message_t *espcp_create_message_on_heap(uint8_t message_type, uint8_t interface, uint32_t function, uint32_t status_code, uint32_t message_id, uint8_t *payload, uint32_t payload_length)
{
    espcp_message_t *new_message = (espcp_message_t *) zalloc(sizeof(espcp_message_t));
    if (new_message != NULL)
    {
        new_message->message_type = message_type;
        new_message->interface = interface;
        new_message->function = function;
        new_message->status_code = status_code;
        new_message->message_id = message_id;
        new_message->payload = payload;
        new_message->payload_length = payload_length;
        new_message->semaphore = NULL;
        new_message->message_sent = NULL;
    }
    return(new_message);
}

/****************************************************************************
 * Name: espcp_create_copy_of_message_on_heap
 *
 * Description:
 *  Create a copy of the specified message on the heap.
 * 
 * Input Parameters:
 *  message - pointer to an ESP32 Message
 *  copy_payload - Boolean indicating if we want to copy the message and the
 *                 payload or just the message header.
 *
 * Returned Value:
 *  Pointer to a copy of the original message.  Note that any semaphores in
 *  in the message will not be copied nor will a  new semaphore be created,
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_message_t *espcp_create_copy_of_message_on_heap(espcp_message_t *message, bool copy_payload)
{
    uint8_t *payload = NULL;
    uint32_t payload_length = 0;

    if (copy_payload)
    {
        payload_length = message->payload_length;
        payload = (uint8_t *) malloc(payload_length);
        if (payload == NULL)
        {
            return(NULL);
        }
        memcpy((void *) payload, (void *) message->payload, (size_t) payload_length);
    }
    espcp_message_t *new_message = espcp_create_message_on_heap(message->message_type,
        message->interface, message->function, message->status_code, message->message_id, payload, payload_length);
    return(new_message);
}

/****************************************************************************
 * Name: espcp_delete_message_payload
 *
 * Description:
 *  Delete the heap storage associated with the payload.
 * 
 *  On exit, the payload will be set to a NULL and the payload length
 *  set to 0.
 *
 * Input Parameters:
 *  message - pointer to an ESP32 Message
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_delete_message_payload(espcp_message_t *message)
{
    if ((message != NULL) && (message->payload != NULL))
    {
        free(message->payload);
        message->payload = NULL;
        message->payload_length = 0;
    }
}

/****************************************************************************
 * Name: espcp_delete_message_and_payload
 *
 * Description:
 *  Delete the heap storage associated with the message and any payload.
 * 
 *  The memory associated with the pointer will be invalid on exit.
 *
 * Input Parameters:
 *  message - pointer to an ESP32 Message
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_delete_message_and_payload(espcp_message_t *message)
{
    if (message != NULL)
    {
        if (message->semaphore != NULL)
        {
            sem_destroy(message->semaphore);
        }
        if (message->message_sent != NULL)
        {
            sem_destroy(message->message_sent);
        }
        espcp_delete_message_payload(message);
        free(message);
    }
}


#if defined(USE_MEADOW_DEBUG_HELPERS)

/**
 *  @brief Allow the mapping of enums etc to text for debugging.
 */
struct meadow_debug_mapping_s
{
    /**
     *  @brief Enum value.
     */
    uint32_t value;

    /**
     *  @brief Name associated with the value.
     */
    char *name;
};
typedef struct meadow_debug_mapping_s meadow_debug_mapping_t;

/**
 *  @brief Mapping of the status code enum to the a text representation of the status code.
 */
static meadow_debug_mapping_t status_codes[] =
{
    { espcp_status_codes_completed_ok, "espcp_status_codes_completed_ok" },
    { espcp_status_codes_crc_error, "espcp_status_codes_crc_error" },
    { espcp_status_codes_restart, "espcp_status_codes_restart" },
    { espcp_status_codes_failure, "espcp_status_codes_invalid_interface" },
    { espcp_status_codes_queue_error, "espcp_status_codes_queue_error" },
    { espcp_status_codes_timeout, "espcp_status_codes_timeout" },
    { espcp_status_codes_invalid_packet, "espcp_status_codes_invalid_packet" },
    { espcp_status_codes_invalid_header, "espcp_status_codes_invalid_header" },
    { espcp_status_codes_unexpected_data, "espcp_status_codes_unexpected_data" },
    { espcp_status_codes_missing_end_of_frame_marker, "espcp_status_codes_missing_end_of_frame_marker" },
    { espcp_status_codes_header_body_field_mismatch, "espcp_status_codes_header_body_field_mismatch" },
    { espcp_status_codes_wi_fi_already_started, "espcp_status_codes_wi_fi_already_started" },
    { espcp_status_codes_invalid_wi_fi_credentials, "espcp_status_codes_invalid_wi_fi_credentials" },
    { espcp_status_codes_wi_fi_disconnected, "espcp_status_codes_wi_fi_disconnected" },
    { espcp_status_codes_cannot_start_network_interface, "espcp_status_codes_cannot_start_network_interface" },
    { espcp_status_codes_cannot_connect_to_access_point, "espcp_status_codes_cannot_connect_to_access_point" },
    { espcp_status_codes_default_access_point_not_configured, "espcp_status_codes_default_access_point_not_configured" },
    { espcp_status_codes_invalid_antenna_data, "espcp_status_codes_invalid_antenna_data" },
    { espcp_status_codes_invalid_antenna_value, "espcp_status_codes_invalid_antenna_value" },
    { espcp_status_codes_no_messages_waiting, "espcp_status_codes_no_messages_waiting" },
    { espcp_status_codes_coprocessor_not_responding, "espcp_status_codes_coprocessor_not_responding" },
    { espcp_status_codes_esp_wi_fi_not_started, "espcp_status_codes_esp_wi_fi_not_started" },
    { espcp_status_codes_esp_out_of_memory, "espcp_status_codes_esp_out_of_memory" },
    { espcp_status_codes_esp_wi_fi_invalid_ssid, "espcp_status_codes_esp_wi_fi_invalid_ssid" },
    { espcp_status_codes_access_point_not_found, "espcp_status_codes_access_point_not_found" },
    { espcp_status_codes_beacon_timeout, "espcp_status_codes_beacon_timeout" },
    { espcp_status_codes_authentication_failed, "espcp_status_codes_authentication_failed" },
    { espcp_status_codes_association_failed, "espcp_status_codes_association_failed" },
    { espcp_status_codes_handshake_timeout, "espcp_status_codes_handshake_timeout" },
    { espcp_status_codes_connection_failed, "espcp_status_codes_connection_failed" },
    { espcp_status_codes_ap_tsf_reset, "espcp_status_codes_ap_tsf_reset" },
    { espcp_status_codes_unmapped_error_code, "espcp_status_codes_unmapped_error_code" },
    { espcp_status_codes_unknown_configuration_item, "espcp_status_codes_unknown_configuration_item" },
    { espcp_status_codes_cannot_start_access_point, "espcp_status_codes_cannot_start_access_point" },
    { espcp_status_codes_dhcp_configuration_error, "espcp_status_codes_dhcp_configuration_error" },
    { espcp_status_codes_access_point_not_started, "espcp_status_codes_access_point_not_started" },
    { espcp_status_codes_access_point_already_started, "espcp_status_codes_access_point_already_started" },
    { espcp_status_codes_not_implemented, "espcp_status_codes_not_implemented" },
    { espcp_status_codes_file_not_found, "espcp_status_codes_file_not_found" },
    { espcp_status_codes_thread_pool_is_full, "espcp_status_codes_thread_pool_is_full" },
    { espcp_status_codes_unexpected_coprocessor_restart, "espcp_status_codes_unexpected_coprocessor_restart" },
    { espcp_status_codes_invalid_configuration_file, "espcp_status_codes_invalid_configuration_file" },
    { espcp_status_codes_invalid_WiFi_configuration_file, "espcp_status_codes_invalid_WiFi_configuration_file" },
    { espcp_status_codes_invalid_cell_configuration_file, "espcp_status_codes_invalid_cell_configuration_file" },
    { espcp_status_codes_network_deadlock, "espcp_status_codes_network_deadlock" },
    { espcp_status_codes_esp_reset_unknown, "espcp_status_codes_esp_reset_unknown" },
    { espcp_status_codes_esp_reset_power_on, "espcp_status_codes_esp_reset_power_on" },
    { espcp_status_codes_esp_reset_external_gpio, "espcp_status_codes_esp_reset_external_gpio"}, 
    { espcp_status_codes_esp_reset_software, "espcp_status_codes_esp_reset_software" },
    { espcp_status_codes_esp_reset_panic, "espcp_status_codes_esp_reset_panic" },
    { espcp_status_codes_esp_reset_interrupt_watchdog, "espcp_status_codes_esp_reset_interrupt_watchdog" },
    { espcp_status_codes_esp_reset_task_watchdog, "espcp_status_codes_esp_reset_task_watchdog" },
    { espcp_status_codes_esp_reset_other_watchdog, "espcp_status_codes_esp_reset_other_watchdog" },
    { espcp_status_codes_esp_reset_deep_sleep, "espcp_status_codes_esp_reset_deep_sleep" },
    { espcp_status_codes_esp_reset_brownout, "espcp_status_codes_esp_reset_brownout" },
    { espcp_status_codes_esp_reset_sdio, "espcp_status_codes_esp_reset_sdio" },
};

/**
 *  @brief Mapping of the ESP32 interfaces enum to the a text representation of the ESP32 interfaces.
 */
static meadow_debug_mapping_t interfaces[] =
{
    { espcp_esp32_interfaces_none, "espcp_esp32_interfaces_none" },
    { espcp_esp32_interfaces_wi_fi, "espcp_esp32_interfaces_wi_fi" },
    { espcp_esp32_interfaces_blue_tooth, "espcp_esp32_interfaces_blue_tooth" },
    { espcp_esp32_interfaces_mesh_network, "espcp_esp32_interfaces_mesh_network" },
    { espcp_esp32_interfaces_system, "espcp_esp32_interfaces_system" },
    { espcp_esp32_interfaces_transport, "espcp_esp32_interfaces_transport" },
    { espcp_esp32_interfaces_wired_ethernet, "espcp_esp32_interfaces_wired_ethernet" },
};

/**
 *  @brief Mapping of the WiFi function enum to the a text representation of the WiFi function.
 */
static meadow_debug_mapping_t wifi_functions[] =
{
    { espcp_wi_fi_function_connect_to_access_point, "espcp_wi_fi_function_connect_to_access_point" },
    { espcp_wi_fi_function_connect_to_default_access_point, "espcp_wi_fi_function_connect_to_default_access_point" },
    { espcp_wi_fi_function_clear_default_access_point, "espcp_wi_fi_function_clear_default_access_point" },
    { espcp_wi_fi_function_disconnect_from_access_point, "espcp_wi_fi_function_disconnect_from_access_point" },
    { espcp_wi_fi_function_get_access_points, "espcp_wi_fi_function_get_access_points" },
    { espcp_wi_fi_function_set_antenna, "espcp_wi_fi_function_set_antenna" },
    { espcp_wi_fi_function_socket, "espcp_wi_fi_function_socket" },
    { espcp_wi_fi_function_connect, "espcp_wi_fi_function_socket" },
    { espcp_wi_fi_function_write, "espcp_wi_fi_function_write" },
    { espcp_wi_fi_function_set_sock_opt, "espcp_wi_fi_function_set_sock_opt" },
    { espcp_wi_fi_function_get_sock_opt, "espcp_wi_fi_function_get_sock_opt" },
    { espcp_wi_fi_function_read, "espcp_wi_fi_function_read" },
    { espcp_wi_fi_function_close, "espcp_wi_fi_function_close" },
    { espcp_wi_fi_function_send_to, "espcp_wi_fi_function_send_to" },
    { espcp_wi_fi_function_recv_from, "espcp_wi_fi_function_recv_from" },
    { espcp_wi_fi_function_poll, "espcp_wi_fi_function_poll" },
    { espcp_wi_fi_function_interrupt_poll_response, "espcp_wi_fi_function_interrupt_poll_response" },
    { espcp_wi_fi_function_send, "espcp_wi_fi_function_send" },
    { espcp_wi_fi_function_bind, "espcp_wi_fi_function_bind" },
    { espcp_wi_fi_function_listen, "espcp_wi_fi_function_listen" },
    { espcp_wi_fi_function_accept, "espcp_wi_fi_function_accept" },
    { espcp_wi_fi_function_ioctl, "espcp_wi_fi_function_ioctl" },
    { espcp_wi_fi_function_get_sock_name, "espcp_wi_fi_function_get_sock_name" },
    { espcp_wi_fi_function_get_peer_name, "espcp_wi_fi_function_get_peer_name" },
    { espcp_wi_fi_function_free_addr_info, "espcp_wi_fi_function_free_addr_info" },
    { espcp_wi_fi_function_get_addr_info, "espcp_wi_fi_function_get_addr_info" },
    { espcp_wi_fi_function_recv_msg, "espcp_wi_fi_function_recv_msg" },
    { espcp_wi_fi_function_shutdown, "espcp_wi_fi_function_shutdown" },
    { espcp_wi_fi_function_send_msg, "espcp_wi_fi_function_send_msg" },
    { espcp_wi_fi_function_dup2, "espcp_wi_fi_function_dup2" },
    { espcp_wi_fi_function_add_ref, "espcp_wi_fi_function_add_ref" },
    { espcp_wi_fi_function_sock_caps, "espcp_wi_fi_function_sock_caps" },
    { espcp_wi_fi_function_network_connected_event, "espcp_wi_fi_function_network_connected_event" },
    { espcp_wi_fi_function_network_disconnected_event, "espcp_wi_fi_function_network_disconnected_event" },
    { espcp_wi_fi_function_ntp_update_event, "espcp_wi_fi_function_ntp_update_event" },
    { espcp_wi_fi_function_error_event, "espcp_wi_fi_function_error_event" },
    { espcp_wi_fi_function_start_access_point, "espcp_wi_fi_function_start_access_point" },
    { espcp_wi_fi_function_stop_access_point, "espcp_wi_fi_function_stop_access_point" },
    { espcp_wi_fi_function_access_point_started_event, "espcp_wi_fi_function_access_point_started_event" },
    { espcp_wi_fi_function_access_point_stopped_event, "espcp_wi_fi_function_access_point_stopped_event" },
    { espcp_wi_fi_function_node_connected_event, "espcp_wi_fi_function_node_connected_event" },
    { espcp_wi_fi_function_node_disconnected_event, "espcp_wi_fi_function_node_disconnected_event" },
    { espcp_wi_fi_function_network_connection_retry_count_exceeded_event, "espcp_wi_fi_function_network_connection_retry_count_exceeded_event" },
    { espcp_wi_fi_function_network_connecting_event, "espcp_wi_fi_function_network_connecting_event" },
};

/**
 *  @brief Mapping of the system function enum to the a text representation of the system function.
 */
static meadow_debug_mapping_t system_functions[] =
{
    { espcp_system_function_get_configuration, "espcp_system_function_get_configuration" },
    { espcp_system_function_set_configuration_item, "espcp_system_function_set_configuration_item" },
    { espcp_system_function_deep_sleep, "espcp_system_function_deep_sleep" },
    { espcp_system_function_get_battery_charge_level, "espcp_system_function_get_battery_charge_level" },
    { espcp_system_function_error_event, "espcp_system_function_error_event" },
    { espcp_system_function_start_heap_trace, "espcp_system_function_start_heap_trace" },
    { espcp_system_function_stop_heap_trace, "espcp_system_function_stop_heap_trace" }
};

/**
 *  @brief Mapping of the transport function enum to the a text representation of the transport function.
 */
static meadow_debug_mapping_t transport_functions[] =
{
    { espcp_transport_function_response_ready, "espcp_transport_function_response_ready" },
    { espcp_transport_function_send_response, "espcp_transport_function_send_response" },
    { espcp_transport_function_kill_nuttx_thread, "espcp_transport_function_kill_nuttx_thread" },
    { espcp_transport_function_reset_esp32, "espcp_transport_function_reset_esp32" }
};

/**
 *  @brief Mapping of the Bluetooth function enum to the a text representation of the Bluetooth function.
 */
static meadow_debug_mapping_t bluetooth_functions[] =
{
    { espcp_bluetooth_function_start, "espcp_bluetooth_function_start" },
    { espcp_bluetooth_function_stop, "espcp_bluetooth_function_stop" },
    { espcp_bluetooth_function_get_handles, "espcp_bluetooth_function_get_handles" },
    { espcp_bluetooth_function_server_data_set, "espcp_bluetooth_function_server_data_set" },
    { espcp_bluetooth_function_client_write_request_event, "espcp_bluetooth_function_client_write_request_event" }
};

/**
 *  @brief Mapping of the message type enum to the a text representation of the message type.
 */
static meadow_debug_mapping_t message_types[] =
{
    { espcp_message_types_ack, "espcp_message_types_ack" },
    { espcp_message_types_nak, "espcp_message_types_nak" },
    { espcp_message_types_reset, "espcp_message_types_reset" },
    { espcp_message_types_event, "espcp_message_types_event" },
    { espcp_message_types_response, "espcp_message_types_response" },
    { espcp_message_types_transport, "espcp_message_types_transport" },
    { espcp_message_types_header, "espcp_message_types_header (request)" },
    { espcp_message_types_data, "espcp_message_types_data" }
};

/**
 *  @brief Size of the static message buffer used for dynamic messages.
 */
#define MEADOW_DEBUG_MESSAGE_BUFFER_LENGTH      128

/**
 *  @brief, Somewhere to store dynamic messages.
 */
static char meadow_debug_message_buffer[MEADOW_DEBUG_MESSAGE_BUFFER_LENGTH];

/**
 *  @brief Lookup the uint32_t value and convert it to a text representation using the mapping table.
 */
static char *lookup_value(uint32_t value, meadow_debug_mapping_t table[], uint32_t length)
{
    char *result = NULL;

    if (length > 0)
    {
        for (int index = 0; index < length; index++)
        {
            if (value == table[index].value)
            {
                result = table[index].name;
                break;
            }
        }
    }
    else
    {
        result = "Invalid table length (0)";
    }
    if (result == NULL)
    {
        snprintf(meadow_debug_message_buffer, MEADOW_DEBUG_MESSAGE_BUFFER_LENGTH, "Unknown value %u", value);
    }
    return(result);
}

/**
 *  @brief Dump the given message to the debug output.
 */
void espcp_dump_message(espcp_message_t *message)
{
    MEADOW_TRACE_INFORMATION("\n");
    MEADOW_TRACE_INFORMATION("********************** Message Details **********************\n");
    MEADOW_TRACE_INFORMATION("\n");
    MEADOW_TRACE_INFORMATION("Message type: 0x%02x (%s)\n", message->message_type, lookup_value(message->message_type, message_types, sizeof(message_types) / sizeof(meadow_debug_mapping_t)));
    MEADOW_TRACE_INFORMATION("ESP32 Interface: 0x%02x (%s)\n", message->interface, lookup_value(message->interface, interfaces, sizeof(interfaces) / sizeof(meadow_debug_mapping_t)));
    MEADOW_TRACE_INFORMATION("Message ID: 0x%08x\n", message->message_id);
    MEADOW_TRACE_INFORMATION("Packet offset (length): %d (%d) bytes\n", message->packet_offset, message->packet_length);
    meadow_debug_mapping_t *mapping = NULL;
    uint32_t mapping_length = 0;
    switch (message->interface)
    {
        case espcp_esp32_interfaces_wired_ethernet:
        case espcp_esp32_interfaces_wi_fi:
            mapping = wifi_functions;
            mapping_length = sizeof(wifi_functions);
            break;
        case espcp_esp32_interfaces_system:
            mapping = system_functions;
            mapping_length = sizeof(system_functions);
            break;
        case espcp_esp32_interfaces_transport:
            mapping = transport_functions;
            mapping_length = sizeof(transport_functions);
            break;
        case espcp_esp32_interfaces_blue_tooth:
            mapping = bluetooth_functions;
            mapping_length = sizeof(bluetooth_functions);
            break;
    }
    mapping_length /= sizeof(meadow_debug_mapping_t);
    MEADOW_TRACE_INFORMATION("Function: 0x%08x (%s)\n", message->function, lookup_value(message->function, mapping, mapping_length));
    MEADOW_TRACE_INFORMATION("Status code: 0x%08x (%s)\n", message->status_code, lookup_value(message->status_code, status_codes, sizeof(status_codes) / sizeof(meadow_debug_mapping_t)));
    MEADOW_TRACE_INFORMATION("Payload length: %d\n", message->payload_length);
    MEADOW_TRACE_INFORMATION("\n");
    MEADOW_TRACE_INFORMATION("************************************************************\n");
}

#else

/**
 *  @brief Dump the given message to the debug output.
 * 
 * This is intentionally empty as the debug helpers are not enabled and is here to ensure that the system will link
 * correctly if the debug helpers are not enabled in this file but are enabled elsewhere.
 */
void espcp_dump_message(espcp_message_t *message)
{

}

#endif // USE_MEADOW_DEBUG_HELPERS