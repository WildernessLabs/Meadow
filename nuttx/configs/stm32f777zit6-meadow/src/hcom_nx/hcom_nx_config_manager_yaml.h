#include "../libcyaml/cyaml.h"

extern void hcom_nx_config_manager_logger(cyaml_log_t, void *, const char *, va_list);

/**
 *  Configuration for the CYAML library.
 */
cyaml_config_t cyaml_config =
{
	.log_level = CYAML_LOG_WARNING,             /* Logging errors and warnings only. */
	.log_fn = hcom_nx_config_manager_logger,    /* Use the default logging function. */
	.mem_fn = cyaml_mem,                        /* Use the default memory allocator. */
    .flags = CYAML_CFG_IGNORE_UNKNOWN_KEYS | CYAML_CFG_CASE_INSENSITIVE
};

/**
 *  Schema for string pointer values (used in sequences of strings).
 * 
 *  This is used in the DNS and NTP server sequences. 
 */
static const cyaml_schema_value_t string_ptr_schema =
{
	CYAML_VALUE_STRING(CYAML_FLAG_POINTER, char, 0, CYAML_UNLIMITED),
};

/**
 *  Device configuration options from the YAML file.
 */
struct yaml_device_s
{
    /**
     *  @brief Name of the device.
     */
    char *name;

    /**
     *  @brief Should the system reboot if the .NET application encounter an unhandled exception?
     */
    char *reboot_on_unhandled_exceptions;

    /**
     *  @brief Maximum amount of time the initialisation method in the .NET application can run
     *         before it is assumed to have failed.
     */
    char *initialisation_timeout_seconds;

    /**
     * @brief Does the system have SD card hardware installed (CCM).
     */
    char *sd_storage_supported;

    /**
     * @brief Names of any reserved pins.
     */
    char *reserved_pins;
};
typedef struct yaml_device_s yaml_device_t;

/**
 *  Defintion of the fields in the yaml_device_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_device_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("Name", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_device_t, name, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("InitializationTimeoutSeconds", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_device_t, initialisation_timeout_seconds, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("RebootOnUnhandledException", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_device_t, reboot_on_unhandled_exceptions, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("SdStorageSupported", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_device_t, sd_storage_supported, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("ReservedPins", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_device_t, reserved_pins, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
    
};

/**
 *  Mono startup configuration as defined in the YAML configuration file.
 */
struct yaml_mono_control_s
{
    /**
     *  Pointer to a string containing the command line options that will be
     *  passed to Mono.
     */
    char *options;
};
typedef struct yaml_mono_control_s yaml_mono_control_t;

/**
 *  Defintion of the fields in the yaml_mono_control_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_mono_control_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("Options", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_mono_control_t, options, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/**
 *  Configuration of the coprocessor from the YAML configuration file.
 */
struct yaml_coprocessor_s
{
    /**
     *  @brief Clock speed of the SPI interface between the STM32 and the ESP32.
     */
    char *spi_speed_hz;

    /**
     * Automatically start the WiFi adapter?
     */
    char *automatically_start_network;

    /**
     * Automatically reconnect to access point if the connection is lost.
     */
    char *automatically_reconnect;

    /**
     * Maximum number of retry attempts before the system should return an error condition.
     */
    char *maximum_retry_count;
};
typedef struct yaml_coprocessor_s yaml_coprocessor_t;

/**
 *  Defintion of the fields in the yaml_coprocessor_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_coprocessor_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("SpiSpeedHz", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, spi_speed_hz, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("AutomaticallyStartNetwork", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, automatically_start_network, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("AutomaticallyReconnect", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, automatically_reconnect, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("MaximumRetryCount", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_coprocessor_t, maximum_retry_count, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/**
 *  @brief Network interface information.
 */
struct yaml_network_interface_s
{
    /**
     *  @brief Name of the interface.
     */
    char *name;

    /**
     * @brief Use DHCP or static address?
     */
    char *use_dhcp;

    /**
     *  @brief Static IP address.  DHCP will be used if this parameter is omitted.
     */
    char *ip_address;

    /**
     *  @brief Subnet mask for the interface.
     */
    char *netmask;

    /**
     *  @brief IP address of the gateway.
     */
    char *gateway;
};
typedef struct yaml_network_interface_s yaml_network_interface_t;

static const cyaml_schema_field_t configuration_network_interface_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("Name", CYAML_FLAG_POINTER, yaml_network_interface_t, name, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("UseDHCP", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_interface_t, use_dhcp, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("IPAddress", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_interface_t, ip_address, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("NetMask", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_interface_t, netmask, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Gateway", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_interface_t, gateway, 0, CYAML_UNLIMITED),
    CYAML_FIELD_END
};

/**
 * @brief Value type for the interface entries.
 * 
 * This is used below to allow for a variable length list of interfaces.
 */
static const cyaml_schema_value_t  interface_schema_value =
{
	CYAML_VALUE_MAPPING(CYAML_FLAG_DEFAULT, yaml_network_interface_t, configuration_network_interface_section_schema)
};

/**
 * Network configuration section of the configuration file.
 */
struct yaml_network_s
{
    /**
     *  @brief Indicate if we should get the network time at startup.
     */
    char *get_network_time_at_startup;

    /**
     * @brief Indicate how often the time should be refreshed.
     */
    char *ntp_refresh_period_seconds;

    /**
     *  @brief Name of the network time servers along with the number of NTP servers
     *         in the config file.
     */
    const char **ntp_servers;
    unsigned ntp_servers_count;

    /**
     *  @brief IP addresses of the DNS servers along with the number of DNS servers
     *         in the config file.
     */
    const char **dns_servers;
    unsigned dns_servers_count;

    /**
     * @brief Name of the interface to use.
     */
    char *default_interface;

    /**
     *  @brief Collection of network interfaces.
     */
    yaml_network_interface_t *interfaces;
    unsigned interfaces_count;
};
typedef struct yaml_network_s yaml_network_t;

/**
 *  Defintion of the fields in the yaml_network_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_network_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("GetNetworkTimeAtStartup", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, get_network_time_at_startup, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("NtpRefreshPeriodSeconds", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, ntp_refresh_period_seconds, 0, CYAML_UNLIMITED),
    CYAML_FIELD_SEQUENCE("NtpServers", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, ntp_servers, &string_ptr_schema, 0, CYAML_UNLIMITED),
    CYAML_FIELD_SEQUENCE("DnsServers", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, dns_servers, &string_ptr_schema, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("DefaultInterface", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, default_interface, 0, CYAML_UNLIMITED),
    CYAML_FIELD_SEQUENCE("Interfaces", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_network_t, interfaces, &interface_schema_value, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/**
 *  Debugging (internal) configuration options from the YAML file.
 */
struct yaml_internal_debug_s
{
    /**
     *  Level of trace output to generate.
     */
    char *trace_level;

    /**
     *  Should trace output be diverted to UART1?
     */
    char *uart1_use;

    /**
     *  Is a debugger attached to the ESP32?
     *
     *  The ESP32 should not be reset at startup if a debugger is attached otherwise
     *  the connection between the debugger and the ESP32 will be broken.
     */
    char *debugger_attached_to_esp;
};
typedef struct yaml_internal_debug_s yaml_internal_debug_t;

/**
 *  Defintion of the fields in the yaml_debug_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_debug_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("TraceLevel", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_internal_debug_t, trace_level, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Uart1Use", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_internal_debug_t, uart1_use, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("DebuggerAttachedToEsp", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_internal_debug_t, debugger_attached_to_esp, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/**
 *  This is a local definition of the configuration and it is aimed to be
 *  used by the CYAML library when reading the configuration data from the
 *  meadow.yaml configuration file.
 *
 *  This additional structure is used as some of the configuration
 *  information in the globally available structure is derived from the
 *  chip / board.
 */
struct yaml_configuration_s
{
    /**
     *  Information about the device.
     */
    yaml_device_t *device;
    /**
     *  Debug configuration options.
     */
    yaml_internal_debug_t *internal_debug;

    /**
     *  Coprocessor configuration.
     */
    yaml_coprocessor_t *coprocessor;

    /**
     *  Network configuration
     */
    yaml_network_t *network;

    /**
     *  Mono control configuration.
     */
    yaml_mono_control_t *mono_control;
};
typedef struct yaml_configuration_s yaml_configuration_t;

/**
 *  Definition of the fields in the struct configuration_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t configuration_fields_schema[] =
{
    CYAML_FIELD_MAPPING_PTR("Device", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, device, configuration_device_section_schema),
    CYAML_FIELD_MAPPING_PTR("InternalDebug", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, internal_debug, configuration_debug_section_schema),
    CYAML_FIELD_MAPPING_PTR("Coprocessor", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, coprocessor, configuration_coprocessor_section_schema),
    CYAML_FIELD_MAPPING_PTR("Network", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, network, configuration_network_section_schema),
    CYAML_FIELD_MAPPING_PTR("MonoControl", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_configuration_t, mono_control, configuration_mono_control_section_schema),
	CYAML_FIELD_END
};

/**
 *  Top level schema for the data from the YAML configuration file is a mapping.
 */
static const cyaml_schema_value_t configuration_schema =
{
    CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, yaml_configuration_t, configuration_fields_schema)
};

/**
 *  Device configuration options from the YAML file.
 */
struct yaml_credentials_s
{
    /**
     *  Name of the network access point to connect to.
     */
    char *ssid;

    /**
     *  Password for the network access point.
     */
    char *password;
};
typedef struct yaml_credentials_s yaml_credentials_t;

/**
 *  Defintion of the fields in the yaml_credentials_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t wifi_credentials_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("Ssid", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_credentials_t, ssid, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Password", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_credentials_t, password, 0, CYAML_UNLIMITED),
	CYAML_FIELD_END
};

/**
 *  This is a local definition of the configuration and it is aimed to be
 *  used by the CYAML library when reading the configuration data from the
 *  meadow.yaml configuration file.
 *
 *  This additional structure is used as some of the configuration
 *  information in the globally available structure is derived from the
 *  chip / board.
 */
struct yaml_wifi_credentials_s
{
    /**
     *  Information about the WiFi credentials.
     */
    yaml_credentials_t *credentials;
};
typedef struct yaml_wifi_credentials_s yaml_wifi_credentials_t;

/**
 *  Definition of the fields in the struct yaml_wifi_credentials_t structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t wifi_credentials_fields_schema[] =
{
    CYAML_FIELD_MAPPING_PTR("Credentials", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_wifi_credentials_t, credentials, wifi_credentials_section_schema),
	CYAML_FIELD_END
};

/**
 *  Top level schema for the data from the YAML configuration file is a mapping.
 */
static const cyaml_schema_value_t wifi_credentials_schema =
{
    CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, yaml_wifi_credentials_t, wifi_credentials_fields_schema)
};

/**
 *  Device settings options from the Cell config YAML file.
 */
struct yaml_cell_settings_s
{
    
    /**
     *  Identify which cell module is used.
     */
    char *module;

    /**
     *  User for the access point network (APN).
     */
    char *user;

    /**
     *  Password for the access point network (APN).
     */
    char *password;

    /**
     *  Name for the access point network (APN).
     */
    char *apn;

    /**
     *  Numeric operator code for the access point network (APN).
     */
    char *operator;

    /**
     *  IoT operation mode (NB-IoT = 9 or Cat-M1 = 7)
     */
    char *mode;

    /**
     *  Interface name used to communicate with the cell module.
     */
    char *ttyname;

    /**
     *  Meadow device pin name used to turn on the cell modules.
     */
    char *turn_on_pin_name;

    /**
     *  Cell module response timeout.
     */
    char *timeout;

    /**
     * Cell scan network
    */
    char *scan_mode;
};
typedef struct yaml_cell_settings_s yaml_cell_settings_t;

/**
 *  Defintion of the fields in the yaml_cell_config_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t cell_settings_section_schema[] =
{
    CYAML_FIELD_STRING_PTR("APN", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, apn, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("User", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, user, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Password", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, password, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Operator", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, operator, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Timeout", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, timeout, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Interface", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, ttyname, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("TurnOnPin", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, turn_on_pin_name, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("Mode", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, mode, 0, CYAML_UNLIMITED),	
    CYAML_FIELD_STRING_PTR("Module", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, module, 0, CYAML_UNLIMITED),
    CYAML_FIELD_STRING_PTR("ScanMode", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_settings_t, scan_mode, 0, CYAML_UNLIMITED),
    CYAML_FIELD_END
};

struct yaml_cell_config_s
{
    /**
     *  Information about the Cell settings.
     */
    yaml_cell_settings_t *settings;
};
typedef struct yaml_cell_config_s yaml_cell_config_t;

/**
 *  Definition of the fields in the struct yaml_cell_config_s structure.
 *
 *  This is an array of the field definitions.
 */
static const cyaml_schema_field_t cell_settings_fields_schema[] =
{
    CYAML_FIELD_MAPPING_PTR("Settings", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, yaml_cell_config_t, settings, cell_settings_section_schema),
	CYAML_FIELD_END
};

/**
 *  Top level schema for the data from the YAML configuration file is a mapping.
 */
static const cyaml_schema_value_t cell_settings_schema =
{
    CYAML_VALUE_MAPPING(CYAML_FLAG_POINTER, yaml_cell_config_t, cell_settings_fields_schema)
};