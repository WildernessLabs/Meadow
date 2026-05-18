/*
 *  @file SharedEnums.h
 *
 */

#ifndef _SHARED_ENUMS_H
#define _SHARED_ENUMS_H

#include <meadow/hcom_shared_common.h>
#include "syslog.h"

/*
 *    Status and error codes for the various function calls.
 */
enum espcp_status_codes
{
    espcp_status_codes_completed_ok = 0,
    espcp_status_codes_crc_error = 1,
    espcp_status_codes_restart = 2,
    espcp_status_codes_failure = 3,
    espcp_status_codes_invalid_interface = 4,
    espcp_status_codes_queue_error = 5,
    espcp_status_codes_timeout = 6,
    espcp_status_codes_invalid_packet = 7,
    espcp_status_codes_invalid_header = 8,
    espcp_status_codes_unexpected_data = 9,
    espcp_status_codes_missing_end_of_frame_marker = 10,
    espcp_status_codes_header_body_field_mismatch = 11,
    espcp_status_codes_wi_fi_already_started = 12,
    espcp_status_codes_invalid_wi_fi_credentials = 13,
    espcp_status_codes_wi_fi_disconnected = 14,
    espcp_status_codes_cannot_start_network_interface = 15,
    espcp_status_codes_cannot_connect_to_access_point = 16,
    espcp_status_codes_default_access_point_not_configured = 17,
    espcp_status_codes_invalid_antenna_data = 18,
    espcp_status_codes_invalid_antenna_value = 19,
    espcp_status_codes_invalid_ip = 20,
    espcp_status_codes_no_messages_waiting = 21,
    espcp_status_codes_coprocessor_not_responding = 22,
    espcp_status_codes_esp_wi_fi_not_started = 23,
    espcp_status_codes_esp_out_of_memory = 24,
    espcp_status_codes_esp_wi_fi_invalid_ssid = 25,
    espcp_status_codes_access_point_not_found = 26,
    espcp_status_codes_beacon_timeout = 27,
    espcp_status_codes_authentication_failed = 28,
    espcp_status_codes_association_failed = 29,
    espcp_status_codes_handshake_timeout = 30,
    espcp_status_codes_connection_failed = 31,
    espcp_status_codes_ap_tsf_reset = 32,
    espcp_status_codes_unmapped_error_code = 33,
    espcp_status_codes_unknown_configuration_item = 34,
    espcp_status_codes_cannot_start_access_point = 35,
    espcp_status_codes_dhcp_configuration_error = 36,
    espcp_status_codes_access_point_not_started = 37,
    espcp_status_codes_access_point_already_started = 38,
    espcp_status_codes_not_implemented = 39,
    espcp_status_codes_file_not_found = 40,
    espcp_status_codes_thread_pool_is_full = 41,
    espcp_status_codes_unexpected_coprocessor_restart = 42,
    espcp_status_codes_invalid_configuration_file = 43,
    espcp_status_codes_invalid_WiFi_configuration_file = 44,
    espcp_status_codes_invalid_cell_configuration_file = 45,
    espcp_status_codes_network_deadlock = 46,
    espcp_status_codes_close_deadlock = 47,
    espcp_status_codes_poll_deadlock = 48,
    espcp_status_codes_socket_deadlock = 49,

    //
    //  Keep the ESP reset codes consecutive as they are subject to arithmetic operations
    //  to determine the exact reset reason.
    //
    /**
     * @brief Unknown reset reason.
     */
    espcp_status_codes_esp_reset_unknown = 50,

    /**
     * @brief Power-on or EN line pulled low and released.
     */
    espcp_status_codes_esp_reset_power_on = 51,

    /**
     * @brief Reset using an external GPIO (n/a for the ESP32).
     */
    espcp_status_codes_esp_reset_external_gpio = 52,

    /**
     * @brief esp_restart has used to reset the ESP32.
     */
    espcp_status_codes_esp_reset_software = 53,

    /**
     * @brief Reset due to software exception or panic.
     */
    espcp_status_codes_esp_reset_panic = 54,

    /**
     * @brief Reset due to interrupt watchdog.
     */
    espcp_status_codes_esp_reset_interrupt_watchdog = 55,

    /**
     * @brief Reset due to task watchdog.
     */
    espcp_status_codes_esp_reset_task_watchdog = 56,

    /**
     * @brief Reset due to watchdog other than task or interrupt watchdogs.
     */
    espcp_status_codes_esp_reset_other_watchdog = 57,

    /**
     * @brief Reset after exiting deep sleep.
     */
    espcp_status_codes_esp_reset_deep_sleep = 58,

    /**
     * @brief Hardware or software brownout reset.
     */
    espcp_status_codes_esp_reset_brownout = 59,

    /**
     * @brief Reset over SDIO.
     */
    espcp_status_codes_esp_reset_sdio = 60,

    /**
     * @brief Invalid request.
     */
    espcp_status_codes_invalid_request = 61,

    /**
     * @brief Invalid WiFi country code.
     */
    espcp_status_codes_invalid_wifi_country_code = 62,
};
typedef enum espcp_status_codes espcp_status_codes_t;

/*
 *    System interfaces available on the ESP32 for the various function calls.
 */
enum espcp_esp32_interfaces
{
    espcp_esp32_interfaces_none = 0,
    espcp_esp32_interfaces_wi_fi = 1,
    espcp_esp32_interfaces_blue_tooth = 2,
    espcp_esp32_interfaces_mesh_network = 3,
    espcp_esp32_interfaces_system = 4,
    espcp_esp32_interfaces_transport = 5,
    espcp_esp32_interfaces_wired_ethernet = 6
};
typedef enum espcp_esp32_interfaces espcp_esp32_interfaces_t;

/*
 *    System functions available on the ESP32
 */
enum espcp_system_function
{
    espcp_system_function_get_configuration = 0,
    espcp_system_function_set_configuration_item = 1,
    espcp_system_function_deep_sleep = 2,
    espcp_system_function_get_battery_charge_level = 3,
    espcp_system_function_error_event = 4,
    espcp_system_function_start_heap_trace = 5,
    espcp_system_function_stop_heap_trace = 6,
    espcp_system_function_file_system_format = 7,
    espcp_system_function_file_system_list_files = 8,
    espcp_system_function_file_system_write_file = 9,
    espcp_system_function_file_system_read_file = 10,
    espcp_system_function_file_system_delete_file = 11,
    espcp_system_function_os_exception = 12,
    espcp_system_function_logging_configuration = 13,
    espcp_system_function_log_message = 14,

    /**
     * @brief Get core dump information.
     */
    espcp_system_function_get_core_dump_information = 15,

    /**
     * @brief Get core dump fragment.
     */
    espcp_system_function_core_dump_fragment = 16,

    /**
     * @brief Erase core dump.
     */
    espcp_system_function_core_dump_erase = 17,
};
typedef enum espcp_system_function espcp_system_function_t;

/*
 *    WiFi functions available on the ESP32
 */
enum espcp_wi_fi_function
{
    espcp_wi_fi_function_start_wi_fi_interface = 0,
    espcp_wi_fi_function_stop_wi_fi_interface = 1,
    espcp_wi_fi_function_connect_to_access_point = 2,
    espcp_wi_fi_function_connect_to_default_access_point = 3,
    espcp_wi_fi_function_clear_default_access_point = 4,
    espcp_wi_fi_function_disconnect_from_access_point = 5,
    espcp_wi_fi_function_get_access_points = 6,
    espcp_wi_fi_function_set_antenna = 7,
    espcp_wi_fi_function_socket = 8,
    espcp_wi_fi_function_connect = 9,
    espcp_wi_fi_function_write = 10,
    espcp_wi_fi_function_set_sock_opt = 11,
    espcp_wi_fi_function_get_sock_opt = 12,
    espcp_wi_fi_function_read = 13,
    espcp_wi_fi_function_close = 14,
    espcp_wi_fi_function_send_to = 15,
    espcp_wi_fi_function_recv_from = 16,
    espcp_wi_fi_function_poll = 17,
    espcp_wi_fi_function_interrupt_poll_response = 18,
    espcp_wi_fi_function_send = 19,
    espcp_wi_fi_function_bind = 20,
    espcp_wi_fi_function_listen = 21,
    espcp_wi_fi_function_accept = 22,
    espcp_wi_fi_function_ioctl = 23,
    espcp_wi_fi_function_get_sock_name = 24,
    espcp_wi_fi_function_get_peer_name = 25,
    espcp_wi_fi_function_free_addr_info = 26,
    espcp_wi_fi_function_get_addr_info = 27,
    espcp_wi_fi_function_recv_msg = 28,
    espcp_wi_fi_function_shutdown = 29,
    espcp_wi_fi_function_send_msg = 30,
    espcp_wi_fi_function_dup2 = 31,
    espcp_wi_fi_function_add_ref = 32,
    espcp_wi_fi_function_sock_caps = 33,
    espcp_wi_fi_function_start_wi_fi_interface_event = 34,
    espcp_wi_fi_function_stop_wi_fi_interface_event = 35,
    espcp_wi_fi_function_network_connected_event = 36,
    espcp_wi_fi_function_network_disconnected_event = 37,
    espcp_wi_fi_function_ntp_update_event = 38,
    espcp_wi_fi_function_error_event = 39,
    espcp_wi_fi_function_start_access_point = 40,
    espcp_wi_fi_function_stop_access_point = 41,
    espcp_wi_fi_function_access_point_started_event = 42,
    espcp_wi_fi_function_access_point_stopped_event = 43,
    espcp_wi_fi_function_node_connected_event = 44,
    espcp_wi_fi_function_node_disconnected_event = 45,
    espcp_wi_fi_function_network_connection_retry_count_exceeded_event = 46,
    espcp_wi_fi_function_network_connecting_event = 47,
    espcp_wi_fi_function_network_got_ip_event = 48,
};
typedef enum espcp_wi_fi_function espcp_wi_fi_function_t;

/*
 *    Bluetooth functions available on the ESP32
 */
enum espcp_bluetooth_function
{
    /**
     * @brief Start the Bluetooth service.
     */
    espcp_bluetooth_function_start = 0,
    /**
     * @brief Stop the Bluetooth service.
     */
    espcp_bluetooth_function_stop = 1,
    /**
     * @brief Get the handles for the service and characteristics.
     */
    espcp_bluetooth_function_get_handles = 2,
    /**
     * @brief Set the value of a characteristic.
     */
    espcp_bluetooth_function_server_data_set = 3,
    /**
     * @brief Client has written to a characteristic value.
     */
    espcp_bluetooth_function_client_write_request_event = 4,
    /**
     * @brief Indicate that the Bluetooth service is starting.
     */
    espcp_bluetooth_starting_event = 5,
    /**
     * @brief Indicate the the Bluetooth service has started.
     */
    espcp_bluetooth_started_event = 6,
    /**
     * @brief Indicate the the Bluetooth service is stopping.
     */
    espcp_bluetooth_stopping_event = 7,
    /**
     * @brief Indicate the the Bluetooth service has stopped.
     */
    espcp_bluetooth_stopped_event = 8,
    /**
     * @brief Indicate that a client has connected to the Bluetooth service.
     */
    espcp_client_connected_event = 9,
    /**
     * @brief Indicate that a client has disconnected from the Bluetooth service.
     */
    espcp_client_disconnected_event = 10,
};
typedef enum espcp_bluetooth_function espcp_bluetooth_function_t;

/*
 *    Transport functions available on the ESP32
 */
enum espcp_transport_function
{
    espcp_transport_function_response_ready = 0,
    espcp_transport_function_send_response = 1,
    espcp_transport_function_kill_nuttx_thread = 2,
    espcp_transport_function_reset_esp32 = 3
};
typedef enum espcp_transport_function espcp_transport_function_t;

/*
 *    Possible transport packet types.
 */
enum espcp_message_types
{
    espcp_message_types_ack = 0x00,
    espcp_message_types_nak = 0x01,
    espcp_message_types_reset = 0x02,
    espcp_message_types_event = 0x04,
    espcp_message_types_response = 0x10,
    espcp_message_types_transport = 0x20,
    espcp_message_types_header = 0x40,
    espcp_message_types_data = 0x80
};
typedef enum espcp_message_types espcp_message_types_t;

/*
 *    Name of items that can be configured (changed by the code on the STM32) on the ESP32.
 */
enum espcp_configuration_items
{
    espcp_configuration_items_maximum_message_queue_length = 0,
    espcp_configuration_items_automatically_start_network = 1,
    espcp_configuration_items_automatically_reconnect = 2,
    espcp_configuration_items_maximum_retry_count = 3,
    espcp_configuration_items_device_name = 4,
    espcp_configuration_items_default_ap_and_password = 5,
    espcp_configuration_items_ntp_server = 6,
    espcp_configuration_items_get_time_at_startup = 7,
    espcp_configuration_items_use_dhcp = 8,
    espcp_configuration_items_static_ip_address = 9,
    espcp_configuration_items_dns_server = 10,
    espcp_configuration_items_default_gateway = 11,
    espcp_configuration_items_antenna = 12,
    espcp_configuration_items_board_mac_address = 13,
    espcp_configuration_items_soft_ap_mac_address = 14,
    espcp_configuration_items_subnet_mask = 15,
    espcp_configuration_items_bluetooth_mac_address = 16,
};
typedef enum espcp_configuration_items espcp_configuration_items_t;

/*
 *    WiFi reason codes
 */
enum espcp_wi_fi_reasons
{
    espcp_wi_fi_reasons_unspecified = 1,
    espcp_wi_fi_reasons_authentication_expired = 2,
    espcp_wi_fi_reasons_authentication_leave = 3,
    espcp_wi_fi_reasons_association_expired = 4,
    espcp_wi_fi_reasons_association_too_many = 5,
    espcp_wi_fi_reasons_not_authenticated = 6,
    espcp_wi_fi_reasons_not_associated = 7,
    espcp_wi_fi_reasons_association_leave = 8,
    espcp_wi_fi_reasons_association_not_authorized = 9,
    espcp_wi_fi_reasons_disassociated_power_capability_bad = 10,
    espcp_wi_fi_reasons_disassociated_supplementary_channel_bad = 11,
    espcp_wi_fi_reasons_invalid_element = 13,
    espcp_wi_fi_reasons_message_integrity_code_failure = 14,
    espcp_wi_fi_reasons_four_way_handshake_timeout = 15,
    espcp_wi_fi_reasons_group_key_update_timeout = 16,
    espcp_wi_fi_reasons_invalid_element_in_four_way_handshake = 17,
    espcp_wi_fi_reasons_invalid_group_cipher = 18,
    espcp_wi_fi_reasons_invalid_pairwise_cipher = 19,
    espcp_wi_fi_reasons_invalid_akmp = 20,
    espcp_wi_fi_reasons_unsupported_rsne_version = 21,
    espcp_wi_fi_reasons_invalid_rsne_capabilities = 22,
    espcp_wi_fi_reasons_authentication801_failed = 23,
    espcp_wi_fi_reasons_cipher_suite_rejected = 24,
    espcp_wi_fi_reasons_invalid_pmkid = 53,
    espcp_wi_fi_reasons_beacon_timeout = 200,
    espcp_wi_fi_reasons_no_access_point_found = 201,
    espcp_wi_fi_reasons_authentication_failed = 202,
    espcp_wi_fi_reasons_association_failed = 203,
    espcp_wi_fi_reasons_handshake_timeout = 204,
    espcp_wi_fi_reasons_connection_failed = 205,
    espcp_wi_fi_reasons_tsf_reset = 206
};
typedef enum espcp_wi_fi_reasons espcp_wi_fi_reasons_t;

/*
 *    Access point authentication method.
 */
enum espcp_wi_fi_authentication_mode
{
    espcp_wi_fi_authentication_mode_open = 0,
    espcp_wi_fi_authentication_mode_wep = 1,
    espcp_wi_fi_authentication_mode_wpa_psk = 2,
    espcp_wi_fi_authentication_mode_wpa2_psk = 3,
    espcp_wi_fi_authentication_mode_wpa_wpa2_psk = 4,
    espcp_wi_fi_authentication_mode_wpa2_enterprise = 5,
    espcp_wi_fi_authentication_mode_wpa3_psk = 6,
    espcp_wi_fi_authentication_mode_wpa2_wpa3_psk = 7
};
typedef enum espcp_wi_fi_authentication_mode espcp_wi_fi_authentication_mode_t;

/*
 *    WiFi Country Policy.
 */
enum espcp_wi_fi_country_policy
{
    espcp_wi_fi_country_policy_automatic = 0,
    espcp_wi_fi_country_policy_manual = 1
};
typedef enum espcp_wi_fi_country_policy espcp_wi_fi_country_policy_t;

/*
 *    Location of the secondary channel in respect to the primary channel.
 */
enum espcp_wi_fi_second_channel
{
    espcp_wi_fi_second_channel_none = 0,
    espcp_wi_fi_second_channel_above = 1,
    espcp_wi_fi_second_channel_below = 2
};
typedef enum espcp_wi_fi_second_channel espcp_wi_fi_second_channel_t;

/*
 *    WiFiScanType
 */
enum espcp_wi_fi_scan_type
{
    espcp_wi_fi_scan_type_active = 0,
    espcp_wi_fi_scan_type_passive = 1
};
typedef enum espcp_wi_fi_scan_type espcp_wi_fi_scan_type_t;

/*
 *    Types of antennas that can be selected.
 */
enum espcp_antenna_types
{
    espcp_antenna_types_on_board = 0,
    espcp_antenna_types_external = 1,
    espcp_antenna_types_max = 1
};
typedef enum espcp_antenna_types espcp_antenna_types_t;

/*
 *    Encryption method used by the access point.
 */
enum espcp_wi_fi_cipher_type
{
    espcp_wi_fi_cipher_type_none = 0,
    espcp_wi_fi_cipher_type_wep40 = 1,
    espcp_wi_fi_cipher_type_wep104 = 2,
    espcp_wi_fi_cipher_type_tkip = 3,
    espcp_wi_fi_cipher_type_ccmp = 4,
    espcp_wi_fi_cipher_type_tkip_ccmp = 5,
    espcp_wi_fi_cipher_type_unknown = 6
};
typedef enum espcp_wi_fi_cipher_type espcp_wi_fi_cipher_type_t;

/*
 *    Address family types for the low level sockets..
 */
enum espcp_address_family_type
{
    espcp_address_family_type_af_unspec = 0,
    espcp_address_family_type_af_inet = 2,
    espcp_address_family_type_af_inet6 = 10
};
typedef enum espcp_address_family_type espcp_address_family_type_t;

/*
 *    Protocol family types for the low level sockets..
 */
enum espcp_protocol_family_type
{
    espcp_protocol_family_type_pf_unspec = 0,
    espcp_protocol_family_type_pf_inet = 2,
    espcp_protocol_family_type_pf_inet6 = 10
};
typedef enum espcp_protocol_family_type espcp_protocol_family_type_t;

/*
 *    Protocol types for the low level sockets.
 */
enum espcp_ip_protocol_type
{
    espcp_ip_protocol_type_ip_proto_i_p = 0,
    espcp_ip_protocol_type_ip_proto_icmp = 1,
    espcp_ip_protocol_type_ip_proto_tcp = 6,
    espcp_ip_protocol_type_ip_proto_udp = 17,
    espcp_ip_protocol_type_ip_proto_ip_v6 = 41,
    espcp_ip_protocol_type_ip_proto_icmp_v6 = 58,
    espcp_ip_protocol_type_ip_proto_udp_lite = 136,
    espcp_ip_protocol_type_ip_proto_new = 255
};
typedef enum espcp_ip_protocol_type espcp_ip_protocol_type_t;

/*
 *    Socket protocol types for the low level sockets.
 */
enum espcp_socket_protocol_type
{
    espcp_socket_protocol_type_sock_stream = 1,
    espcp_socket_protocol_type_sock_dgram = 2,
    espcp_socket_protocol_type_sock_raw = 3
};
typedef enum espcp_socket_protocol_type espcp_socket_protocol_type_t;

/*
 *    Socket level types for the low level sockets.
 */
enum espcp_socket_level_type
{
    espcp_socket_level_type_sol_socket = 0xfff
};
typedef enum espcp_socket_level_type espcp_socket_level_type_t;

/*
 *    Socket option types for the low level sockets.
 */
enum espcp_socket_option_type
{
    espcp_socket_option_type_so_debug = 0x0001,
    espcp_socket_option_type_so_accept_conn = 0x0002,
    espcp_socket_option_type_so_dont_route = 0x0010,
    espcp_socket_option_type_use_loopback = 0x0040,
    espcp_socket_option_type_so_linger = 0x0080,
    espcp_socket_option_type_so_dont_linger = 0xff7f,
    espcp_socket_option_type_so_oob_inline = 0x0100,
    espcp_socket_option_type_so_reuse_port = 0x0200,
    espcp_socket_option_type_so_snd_buf = 0x1001,
    espcp_socket_option_type_so_rcv_buf = 0x1002,
    espcp_socket_option_type_so_snd_lo_wat = 0x1003,
    espcp_socket_option_type_so_s_rcv_lo_wat = 0x1004,
    espcp_socket_option_type_so_snd_time_o = 0x1005,
    espcp_socket_option_type_so_rcv_time_o = 0x1006,
    espcp_socket_option_type_so_error = 0x1007,
    espcp_socket_option_type_so_type = 0x1008,
    espcp_socket_option_type_so_con_time_o = 0x1009,
    espcp_socket_option_type_so_no_check = 0x100a
};
typedef enum espcp_socket_option_type espcp_socket_option_type_t;

/*
 *    ESP32 Error codes (errno).
 */
enum espcp_esp32_error_codes
{
    espcp_esp32_error_codes_perm = 1,
    espcp_esp32_error_codes_no_ent = 2,
    espcp_esp32_error_codes_srch = 3,
    espcp_esp32_error_codes_intr = 4,
    espcp_esp32_error_codes_i_o = 5,
    espcp_esp32_error_codes_nx_i_o = 6,
    espcp_esp32_error_codes_too_big = 7,
    espcp_esp32_error_codes_no_exec = 8,
    espcp_esp32_error_codes_bad_f = 9,
    espcp_esp32_error_codes_child = 10,
    espcp_esp32_error_codes_again = 11,
    espcp_esp32_error_codes_no_mem = 12,
    espcp_esp32_error_codes_acces = 13,
    espcp_esp32_error_codes_fault = 14,
    espcp_esp32_error_codes_busy = 16,
    espcp_esp32_error_codes_exist = 17,
    espcp_esp32_error_codes_x_dev = 18,
    espcp_esp32_error_codes_no_dev = 19,
    espcp_esp32_error_codes_not_dir = 20,
    espcp_esp32_error_codes_is_dir = 21,
    espcp_esp32_error_codes_in_val = 22,
    espcp_esp32_error_codes_n_file = 23,
    espcp_esp32_error_codes_m_file = 24,
    espcp_esp32_error_codes_no_tty = 25,
    espcp_esp32_error_codes_txt_bsy = 26,
    espcp_esp32_error_codes_f_big = 27,
    espcp_esp32_error_codes_no_spc = 28,
    espcp_esp32_error_codes_s_pipe = 29,
    espcp_esp32_error_codes_ro_fs = 30,
    espcp_esp32_error_codes_m_link = 31,
    espcp_esp32_error_codes_pipe = 32,
    espcp_esp32_error_codes_dom = 33,
    espcp_esp32_error_codes_range = 34,
    espcp_esp32_error_codes_no_msg = 35,
    espcp_esp32_error_codes_id_rm = 36,
    espcp_esp32_error_codes_dead_lck = 45,
    espcp_esp32_error_codes_no_lock = 46,
    espcp_esp32_error_codes_no_str = 60,
    espcp_esp32_error_codes_no_data = 61,
    espcp_esp32_error_codes_time = 62,
    espcp_esp32_error_codes_no_sr = 63,
    espcp_esp32_error_codes_no_link = 67,
    espcp_esp32_error_codes_proto = 71,
    espcp_esp32_error_codes_multi_hop = 74,
    espcp_esp32_error_codes_bad_msg = 77,
    espcp_esp32_error_codes_f_type = 79,
    espcp_esp32_error_codes_no_sys = 88,
    espcp_esp32_error_codes_not_empty = 90,
    espcp_esp32_error_codes_name_too_long = 91,
    espcp_esp32_error_codes_loop = 92,
    espcp_esp32_error_codes_op_not_support = 95,
    espcp_esp32_error_codes_p_no_support = 96,
    espcp_esp32_error_codes_conn_reset = 104,
    espcp_esp32_error_codes_no_bufs = 105,
    espcp_esp32_error_codes_af_no_support = 106,
    espcp_esp32_error_codes_proto_type = 107,
    espcp_esp32_error_codes_not_sock = 108,
    espcp_esp32_error_codes_no_proto_opt = 109,
    espcp_esp32_error_codes_conn_refused = 111,
    espcp_esp32_error_codes_addr_in_use = 112,
    espcp_esp32_error_codes_conn_aborted = 113,
    espcp_esp32_error_codes_net_unreach = 114,
    espcp_esp32_error_codes_net_down = 115,
    espcp_esp32_error_codes_timed_out = 116,
    espcp_esp32_error_codes_host_down = 117,
    espcp_esp32_error_codes_host_unreach = 118,
    espcp_esp32_error_codes_in_progress = 119,
    espcp_esp32_error_codes_already = 120,
    espcp_esp32_error_codes_dest_addr_req = 121,
    espcp_esp32_error_codes_msg_size = 122,
    espcp_esp32_error_codes_proto_no_support = 123,
    espcp_esp32_error_codes_addr_not_avail = 125,
    espcp_esp32_error_codes_net_reset = 126,
    espcp_esp32_error_codes_not_conn = 128,
    espcp_esp32_error_codes_too_many_refs = 129,
    espcp_esp32_error_codes_d_quot = 132,
    espcp_esp32_error_codes_stale = 133,
    espcp_esp32_error_codes_not_sup = 134,
    espcp_esp32_error_codes_il_seq = 138,
    espcp_esp32_error_codes_overflow = 139,
    espcp_esp32_error_codes_cancelled = 140,
    espcp_esp32_error_codes_not_recoverable = 141,
    espcp_esp32_error_codes_owner_dead = 142,
    espcp_esp32_error_codes_would_block = 11
};
typedef enum espcp_esp32_error_codes espcp_esp32_error_codes_t;

/*
 *    ESP32 Error codes (errno).
 */
enum espcp_esp32_reset_codes
{
    espcp_esp32_reset_codes_unknown = 0,
    espcp_esp32_reset_codes_power_on = 1,
    espcp_esp32_reset_codes_external_gpio = 2,
    espcp_esp32_reset_codes_software = 3,
    espcp_esp32_reset_codes_panic = 4,
    espcp_esp32_reset_codes_interrupt_watchdog = 5,
    espcp_esp32_reset_codes_task_watchdog = 6,
    espcp_esp32_reset_codes_other_watchdog = 7,
    espcp_esp32_reset_codes_deep_sleep = 8,
    espcp_esp32_reset_codes_brownout = 9,
    espcp_esp32_reset_codes_s_d_i_o = 10
};
typedef enum espcp_esp32_reset_codes espcp_esp32_reset_codes_t;

/**
 * @brief Where should the ESP log messages be sent?
 * 
 * Definitions here can be found in hcom_shared_common.h
 */
typedef enum espcp_log_destination
{
    /**
     * @brief Noe logging destination.
     */
    espcp_log_destination_none = esp_log_destination_none,

    /**
     * @brief Send log messages to the UART.
     */
    espcp_log_destination_uart = esp_log_destination_uart,

    /**
     * @brief Send log messages to the JTAG.
     */
    espcp_log_destination_jtag = esp_log_destination_jtag,

    /**
     * @brief Send log messages to the UDP.
     */
    espcp_log_destination_udp = esp_log_destination_udp,
} espcp_log_destination_t;

/**
 * @brief Log levels (taken from syslog.h)
 */
typedef enum espcp_log_level
{
    /**
     * @brief No logging, not really taken from syslog.h but here to represent no logging.
     */
    espcp_log_level_none = 255,

    /**
     * @brief Emergency log level.
     */
    espcp_log_level_emergency = LOG_EMERG,

    /**
     * @brief Alert log level.
     */
    espcp_log_level_alert = LOG_ALERT,

    /**
     * @brief Critical log level.
     */
    espcp_log_level_critical = LOG_CRIT,

    /**
     * @brief Error log level.
     */
    espcp_log_level_error = LOG_ERR,

    /**
     * @brief Warning log level.
     */
    espcp_log_level_warning = LOG_WARNING,

    /**
     * @brief Notice log level.
     */
    espcp_log_level_notice = LOG_NOTICE,

    /**
     * @brief Information log level.
     */
    espcp_log_level_information = LOG_INFO,

    /**
     * @brief Debug log level.
     */
    espcp_log_level_debug = LOG_DEBUG,
} espcp_log_level_t;

#endif /* _SHARED_ENUMS_H */
