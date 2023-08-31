
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lwip/apps/sntp.h"

#include "NvsManager.hpp"
#include "WiFiRequestHandler.hpp"
#include "Exceptions/MultipleInstancesException.hpp"
#include "Mapping.hpp"
#include "Logging.hpp"

#if defined(CONFIG_HEAP_TRACING_STANDALONE) || defined(CONFIG_HEAP_TRACING_TOHOST)

#include "esp_heap_trace.h"

#endif

#if defined(CONFIG_HEAP_TRACING_STANDALONE)

extern heap_trace_record_t *trace_records;

#endif

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

const char *WiFiRequestHandler::COMPONENT_NAME = "WiFiTask";

/**
 *  @brief Determine if this class is instantiated already (i.e. this should be a singleton).
 */
WiFiRequestHandler *WiFiRequestHandler::_instance = nullptr;

/**
 *  @brief Event group containing 24-bits used to indicate the state of the WiFi connection.
 */
EventGroupHandle_t WiFiRequestHandler::_xWiFiEventGroup = nullptr;

/**
 *  @brief List of active poll requests.
 */
std::map<uint32_t, struct pollfd *> WiFiRequestHandler::_pollRequests;

/**
 *  @brief Mutex for the poll request linked list.
 */
SemaphoreHandle_t WiFiRequestHandler::_pollRequestListMutex = NULL;

/**
 *  @brief Handle for the completed poll requests.
 */
QueueHandle_t WiFiRequestHandler::_completedPollRequestsQueue = NULL;

/**
 *  @brief GPIO Pin connected to the antenna selection switch
 *  which will connect the on board antenna to the ESP32.
 */
const gpio_num_t WiFiRequestHandler::ON_BOARD_ANTENNA_PIN = GPIO_NUM_26;

/**
 *  @brief GPIO Pin connected to the antenna selection switch
 *  which will connect the external antenna to the ESP32.
 */
const gpio_num_t WiFiRequestHandler::EXTERNAL_ANTENNA_PIN = GPIO_NUM_27;

/**
 *  @brief Index of the dummy socket file descriptor in poll request.
 */
const int WiFiRequestHandler::POLL_DUMMY_FD_INDEX = 1;

/**
 *  @brief Index of the actual socket file descriptor in poll request.
 */
const int WiFiRequestHandler::POLL_FD_INDEX = 0;

/**
 *  @brief Network Interface Handle for the board when running in station mode i.e. as a device connected to an access point.
 */
esp_netif_t *WiFiRequestHandler::_stationNetIfHandle;

/**
 *  @brief Network Interface Handle for the board when it is acting as a soft access point.
 */
esp_netif_t *WiFiRequestHandler::_softApNetIfHandle;
/**
 *  @brief Name of the NVS storage holding the current antenna value.
 */
const char *WiFiRequestHandler::ANTENNA_TYPE_NAME = "Antenna";

/**
 *  @brief Should the system automatically reconnect to an access point should there
 *  be a problem and the current connection is dropped.
 */
bool WiFiRequestHandler::_automaticReconnect = false;

/**
 *  @brief Name of the NVS storage holding the _automaticReconnect value.
 */
const char *WiFiRequestHandler::AUTOMATIC_RECONNECT_NAME = "AutoReconnect";

/**
 *  @brief Maximum number of times a failed event should retry before raising an error.
 */
uint32_t WiFiRequestHandler::_maximumRetryCount = 3;

/**
 *  @brief Name of the NVS storage holding the _maximumRetryCount value.
 */
const char *WiFiRequestHandler::MAXIMUM_RETRY_COUNT_NAME = "MaxRetry";

/**
 *  @brief Indicate if the network should start automatically at start up.
 */
bool WiFiRequestHandler:: _automaticallyStartNetwork = false;

/**
 *  @brief Name of the NVS storage holding the _automaticallyStartNetwork value.
 */
const char *WiFiRequestHandler::AUTOMATICALLY_START_NETWORK_NAME = "AutoStartNet";

/**
 *  @brief Indicate if the system should connect to a time server and get the
 *  current time at startup.
 */
bool WiFiRequestHandler::_getTimeAtStartup = false;

/**
 *  @brief Name fo the storage in NVS that holds the value indicating that the
 *  system should get the time at startup.
 */
const char *WiFiRequestHandler::GET_TIME_AT_STARTUP_NAME = "GetTime";

/**
 *  @brief Indicate if we should use DHCP.
 */
bool WiFiRequestHandler::_useDhcp = false;

/**
 *  @brief Name of the storage holding the _useDhcp configuration setting.
 */
const char *WiFiRequestHandler::USE_DHCP_NAME = "UseDHCP";

/**
 *  @brief Defaut access [oint to be used when automatically connecting.
 */
char *WiFiRequestHandler::_defaultAccessPoint = nullptr;

/**
 *  @brief Name of the storage in NVS holding the default access point.
 */
const char *WiFiRequestHandler::DEFAULT_ACCESS_POINT_NAME = "DefaultAP";

/**
 *  @brief Password for the access point.
 */
char *WiFiRequestHandler::_password = nullptr;

/**
 *  @brief Name of the storage in NVS holding the password for the access point.
 */
const char *WiFiRequestHandler::PASSWORD_NAME = "Password";

/**
 *  @brief Name of the NTP server to be used when using NTP to set the time
 *  when the board starts.
 */
char *WiFiRequestHandler::_ntpServer = (char *) "pool.ntp.org";

/**
 *  @brief Name of the storage space in NVS holding the NTP server information.
 */
const char *WiFiRequestHandler::NTP_SERVER_NAME = "NtpServer";

/**
 *  @brief Name of the storage space in NVS holding the timezone information.
 */
const char *WiFiRequestHandler::TIMEZONE_NAME = "Timezone";

/**
 *  @brief Timezone to be used when the ESP32 starts.  Default will be UTC.
 */
char *WiFiRequestHandler::_timezone = (char *) "UTC";

/**
 *  @brief Static IP address when not using DHCP.
 */
uint32_t WiFiRequestHandler::_staticIpAddress = 0;

/**
 *  @brief Name of the storage space holding the static IP address.
 */
const char *WiFiRequestHandler::STATIC_IP_ADDRESS_NAME = "IpAddress";

/**
 *  @brief Static subnet mask when not using DHCP.
 */
uint32_t WiFiRequestHandler::_staticSubNetMask = 0;

/**
 *  @brief Name of the storage space holding the static subnet mask.
 */
const char *WiFiRequestHandler::STATIC_SUBNET_MASK_NAME = "SubnetMask";

/**
 *  @brief DNS server when not using DHCP.
 */
uint32_t WiFiRequestHandler::_dnsServer = 0;

/**
 *  @brief Name of the storage in NVS holding the DNS server IP address.
 */
const char *WiFiRequestHandler::DNS_SERVER_NAME = "DnsServer";

/**
 *  @brief IP address of the default gateway when not using DHCP.
 */
uint32_t WiFiRequestHandler::_defaultGateway = 0;

/**
 *  @brief Name of the storage space holding the IP address of the default gateway.
 */
const char *WiFiRequestHandler::DEFAULT_GATEWAY_NAME = "Gateway";

/**
 *  @brief Default value for the WiFi timeout.
 */
uint32_t WiFiRequestHandler::_wifiTimeout = 30000;

/**
 *  @brief Name of the storage space holding the default WiFi timeout.
 */
const char *WiFiRequestHandler::DEFAULT_WIFI_TIMEOUT_NAME = "WiFiTimeout";

/*
 * ----------------------------------------------------------------------------
 *
 *                  Constructors and destructors.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for the WiFi class.
 */
WiFiRequestHandler::WiFiRequestHandler()
{
    if (_instance != nullptr)
    {
        throw new MultipleInstancesException();
    }
    else
    {
        _instance = this;
    }
    //
    //  Get configuration from NVS.
    //
    _defaultAccessPoint = SystemRequestHandler::GetConfigurationValue(DEFAULT_ACCESS_POINT_NAME, _defaultAccessPoint);
    _password = SystemRequestHandler::GetConfigurationValue(PASSWORD_NAME, _password);
    _stationNetIfHandle = nullptr;
    _softApNetIfHandle = nullptr;
    //
    //  Setup the timezones ready for when we get the time from a timeserver.
    //
    setenv("TZ", _timezone, 1);
    tzset();
    //
    //  Set up the structures need to deal with poll() requests in an asynchronous manner.
    //
    _pollRequestListMutex = xSemaphoreCreateMutex();
    if (_pollRequestListMutex == NULL)
    {
        ERROR_MESSAGE("WiFiRequestHandler: Cannot create poll request list mutex.");
    }
    //
    //  Get ready for nodes connecting when configured as an access point.
    //
    _connectedNodesListMutex = xSemaphoreCreateMutex();
    if (_connectedNodesListMutex == NULL)
    {
        ERROR_MESSAGE("WiFiRequestHandler: Cannot create connected nodes list mutex.");
    }
    _connectedNodes.clear();

    //
    //  Finally set some default values for private fields.
    //
    ClearIpAndDisconnectData();
    bzero((void *) &_apInformation, sizeof(wifi_event_sta_connected_t));
    _lastIdfErrorCode = ESP_OK;
    _ipChanged = false;
}

/**
 *  @brief Create a new object and set the message dispatcher accordingly.
 *
 *  @param messageDispatcher
 *      Message Dispatcher object that can be used to send messages to
 *      the STM32.
 */
WiFiRequestHandler::WiFiRequestHandler(IMessageDispatcher *messageDispatcher) : WiFiRequestHandler()
{
    _messageDispatcher = messageDispatcher;
}

/**
 *  @brief Destructor for the WiFi class.
 *
 *  Free any resources used by the class.
 */
WiFiRequestHandler::~WiFiRequestHandler()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                      Getters and setters
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Get the automatic maximum retry count value.
 */
uint32_t WiFiRequestHandler::GetMaximumRetryCount()
{
    return(_maximumRetryCount == 0 ? 3 : _maximumRetryCount);
}

/**
 *  @brief Set the maximum retry count value.
 *
 *  @param value
 *      New maximum retry count value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetMaximumRetryCount(uint32_t value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetMaximumRetryCount: %d", value);
    _maximumRetryCount = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the automatic reconnect value.
 */
bool WiFiRequestHandler::GetAutomaticReconnect()
{
    return(_automaticReconnect);
}

/**
 *  @brief Set the automatic reconnect value.
 *
 *  @param value
 *      New automatic reconnection value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetAutomaticReconnect(bool value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetAutomaticReconnect: %d", value ? 1 : 0);
    _automaticReconnect = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the automatically start network value.
 */
bool WiFiRequestHandler::GetAutomaticallyStartNetwork()
{
    return(_automaticallyStartNetwork);
}

/**
 *  @brief Set the automatic start network value.
 *
 *  @param value
 *      New automatic start network value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetAutomaticallyStartNetwork(bool value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetAutomaticallyStartNetwork %d", value ? 1 : 0);
    _automaticallyStartNetwork = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the use DHCP value.
 */
bool WiFiRequestHandler::GetUseDhcp()
{
    return(_useDhcp);
}

/**
 *  @brief Set the use DHCP value.
 *
 *  @param value
 *      New use DHCP value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetUseDhcp(bool value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetUseDhcp %d", value ? 1 : 0);
    _useDhcp = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the default access point.
 */
char *WiFiRequestHandler::GetDefaultAccessPoint()
{
    return(_defaultAccessPoint);
}

/**
 *  @brief Set default access point.
 *
 *  @param value
 *      New default access point value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetDefaultAccessPoint(char *value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetDefaultAccessPoint %s", (value == NULL) ? "NULL" : value);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    char emptyAp[1];

    *emptyAp = 0;
    if (value == NULL)
    {
        value = emptyAp;
    }
    if (!_defaultAccessPoint)
    {
        _defaultAccessPoint = strdup(emptyAp);
    }

    if (strcmp(value, _defaultAccessPoint) != 0)
    {
        if (SystemRequestHandler::SetConfigurationValue(DEFAULT_ACCESS_POINT_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            free(_defaultAccessPoint);
            _defaultAccessPoint = strdup(value);
        }
    }
    return(result);
}

/**
 *  @brief Get the password for the default access point.
 */
char *WiFiRequestHandler::GetPassword()
{
    return(_password);
}

/**
 *  @brief Set password for the default access point.
 *
 *  @param value
 *      New password value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetPassword(char *value)
{
    TRACE_MESSAGE("WiFiRequestHandler::SetPassword %s", (value == NULL) ? "NULL" : value);
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    char emptyPassword = '\0';

    if (value == NULL)
    {
        value = &emptyPassword;
    }
    if (!_password)
    {
        _password = strdup(&emptyPassword);
    }

    if (strcmp(value, _password) != 0)
    {
        if (SystemRequestHandler::SetConfigurationValue(PASSWORD_NAME, value) == NvsManager::ErrorCodes::Ok)
        {
            free(_password);
            _password = strdup(value);
        }
    }
    return(result);
}

/**
 *  @brief Get the static IP address value.
 */
uint32_t WiFiRequestHandler::GetIpAddress()
{
    return(_staticIpAddress);
}

/**
 *  @brief Set the static IP address value.
 *
 *  @param value
 *      New static IP address value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetIpAddress(uint32_t value)
{
    _staticIpAddress = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the subnet mask when static addressing is used..
 */
uint32_t WiFiRequestHandler::GetSubNetMask()
{
    return(_staticSubNetMask);
}

/**
 *  @brief Set the static IP address value.
 *
 *  @param value
 *      New static IP address value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetSubNetMask(uint32_t value)
{
    _staticSubNetMask = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the static DNS server value.
 */
uint32_t WiFiRequestHandler::GetDnsServer()
{
    return(_dnsServer);
}

/**
 *  @brief Set the static DNS address value.
 *
 *  @param value
 *      New static DNS address value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetDnsServer(uint32_t value)
{
    _dnsServer = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the static default gateway value.
 */
uint32_t WiFiRequestHandler::GetDefaultGateway()
{
    return(_defaultGateway);
}

/**
 *  @brief Set the static default gateway value.
 *
 *  @param value
 *      New static DNS address value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetDefaultGateway(uint32_t value)
{
    _defaultGateway = value;
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the MAC address of the board.
 *
 *  @param macAddress
 *      Pointer to the buffer to hold the MAC address.
 *
 *  @param interface
 *      ESP interface to get the MAC address.  Valid values:
 *          WIFI_IF_STA     Board MAC address
 *          WIFI_IF_AP      Software access point MAC address
 */
void WiFiRequestHandler::GetMacAddress(uint8_t *macAddress, wifi_interface_t interface)
{
    if (interface == WIFI_IF_STA)
    {
        if (esp_read_mac(macAddress, ESP_MAC_WIFI_STA) != ESP_OK)
        {
            bzero(macAddress, 6);
        }
    }
    else
    {
        if (esp_wifi_get_mac(WIFI_IF_AP, macAddress) != ESP_OK)
        {
            bzero(macAddress, 6);
        }
    }
}

/**
 *  @brief Set the MAC address of the board.
 *
 *  @param macAddress
 *      New MAC address.
 *
 *  @param interface
 *      Interface the MAC address refers to.  Valid values:
 *          WIFI_IF_STA     Board MAC address
 *          WIFI_IF_AP      Software access point MAC address
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetMacAddress(uint8_t *macAddress, wifi_interface_t interface)
{
    return(StatusCodes::CompletedOk);
}

/**
 *  @brief Get the currently selected antenna.
 *
 *  @return
 *      Current Antenna in use as an unsigned 8-bit integer.
 */
uint8_t WiFiRequestHandler::GetAntenna()
{
    return(SystemRequestHandler::GetConfigurationValue(ANTENNA_TYPE_NAME, static_cast<uint8_t>(AntennaTypes::OnBoard)));
}

/**
 *  @brief Set the value for the current Antenna.
 *
 *  @param antenna
 *      New antenna to use.
 *
 *  TODO: Do we need to turn the WiFi off before switching the antenna?
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetAntenna(uint8_t antenna)
{
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (antenna > static_cast<uint8_t>(AntennaTypes::Max))
    {
        result = StatusCodes::InvalidAntennaValue;
    }
    else
    {
        SwitchAntenna(static_cast<AntennaTypes::AntennaTypes>(antenna));
        SystemRequestHandler::SetConfigurationValue(ANTENNA_TYPE_NAME, antenna);
    }

    return(result);
}

/**
 *  @brief Get the WiFi timeout value (milliseconds).
 */
uint32_t WiFiRequestHandler::GetWiFiTimeout()
{
    return(_wifiTimeout);
}

/**
 *  @brief Set the WiFi timeout value (milliseconds).
 *
 *  @param value
 *      New WiFi timeout value.
 *
 *  @returns
 *      StatusCodes::Completed on success, StatusCodes::Failure if there is a problem.
 */
StatusCodes::StatusCodes WiFiRequestHandler::SetWiFiTimeout(uint32_t value)
{
    _wifiTimeout = value;
    return(StatusCodes::CompletedOk);
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Get a pointer to this instance of the WiFiRequest Handler object.
 * 
 *  Note: There should be only one instance of this object in the system.
 * 
 * @return Pointer the this instance of the WiFiRequest handler.
 */
WiFiRequestHandler *WiFiRequestHandler::GetInstance()
{
    return(_instance);
}

/**
 *  @brief Clear the IP and disconnect data.
 */
void WiFiRequestHandler::ClearIpAndDisconnectData()
{
    bzero((void *) &_instance->_disconnectData, sizeof(_disconnectData));
    bzero((void *) &_instance->_ipInformation, sizeof(_ipInformation));
}

/**
 *  @brief Map the errno variable from an ESP to a STM32 value.
 */
int32_t WiFiRequestHandler::MapErrno(int e)
{
    int32_t result = 0;
    if (Mapping::GetStmValue(Mapping::Errno, e, &result) != 0)
    {
        Mapping::GetStmValue(Mapping::Errno, EFAULT, &result);
    }
    return(result);
}

/**
 *  @brief Callback to be executed when the NTP time server has provided updated time information.
 *
 *  @param tv
 *      New time information from the time server.
 */
void WiFiRequestHandler::NtpServerCallback(struct timeval *tv)
{
    TRACE_MESSAGE("NtpServerCallback: Enter");

    TRACE_MESSAGE("Setting time to %d", (int) tv->tv_sec);

    settimeofday(tv, NULL);

    TRACE_MESSAGE("NtpServerCallback: Exit");
}

/**
 *  @brief Convert the antenna enum into a GPIO pin number
 *
 *  @param antenna
 *      Enum representing the antenna to be converted.
 *
 *  @returns
 *      GPIO pin number connected to the appropriate antenna.
 */
gpio_num_t WiFiRequestHandler::ConvertAntennaToPin(AntennaTypes::AntennaTypes antenna)
{
    TRACE_MESSAGE("ConvertAntennaToPin: Enter");
    TRACE_MESSAGE("ConvertAntennaToPin: Exit");

    return((antenna == AntennaTypes::OnBoard) ? WiFiRequestHandler::ON_BOARD_ANTENNA_PIN : WiFiRequestHandler::EXTERNAL_ANTENNA_PIN);
}

/**
 *  @brief Change the current antenna in use.
 *
 *  Note that this does not change the actual default antenna in use, it just switches to a new one.
 *
 *  @param newAntenna
 *      New antenna to use.
 */
void WiFiRequestHandler::SwitchAntenna(AntennaTypes::AntennaTypes newAntenna)
{
    TRACE_MESSAGE("SwitchAntenna: Enter");
    //
    // TODO: What happens to any pending requests and do we disconnect WiFi first?
    //
    gpio_num_t newAntennaPin = ConvertAntennaToPin(newAntenna);
    gpio_num_t oldAntennaPin = ConvertAntennaToPin(static_cast<AntennaTypes::AntennaTypes>(GetAntenna()));
    //
    //  Make sure the old antenna is switched off before the new one is turned on.
    //
    TRACE_MESSAGE("Switching from antenna %d to %d", oldAntennaPin, newAntennaPin);
    gpio_set_level(oldAntennaPin, 0);
    gpio_set_level(newAntennaPin, 1);
    TRACE_MESSAGE("SwitchAntenna; Exit");
}

/**
 *  @brief Setup the WiFi network class.
 */
void WiFiRequestHandler::Setup()
{
    TRACE_MESSAGE("Setup: Enter");

    RequestHandlerBase::Setup();
    gpio_set_direction(WiFiRequestHandler::EXTERNAL_ANTENNA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(WiFiRequestHandler::EXTERNAL_ANTENNA_PIN, 0);
    gpio_set_direction(WiFiRequestHandler::ON_BOARD_ANTENNA_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(WiFiRequestHandler::ON_BOARD_ANTENNA_PIN, 1);

    SwitchAntenna(static_cast<AntennaTypes::AntennaTypes>(GetAntenna()));

    esp_wifi_restore();

    /* 
     *  Now create the objects necessary for the WiFi tasks.
     */
    _xWiFiEventGroup = xEventGroupCreate();
    xEventGroupClearBits(_xWiFiEventGroup, 0x008fffff);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
    xTaskCreate(Task, COMPONENT_NAME, 2048 * 2, this, configMAX_PRIORITIES - 1, &_taskHandle);
    TRACE_MESSAGE("Setup: Exit");
}

/**
 *  @brief Copy the IP and access point structure into the connection event data structure.
 *
 *  @param ed
 *    Pointer to the connection event data structure to be populated.
 */
void WiFiRequestHandler::PopulateConnectionEventDataStructure(Esp32Messaging::ConnectEventData *ed)
{
    TRACE_MESSAGE("PopulateConnectionEventDataStructure: Enter");

    *ed = { };
    // bzero((void *) ed, sizeof(Esp32Messaging::ConnectEventData));
    //
    //  IP information.
    //
    memcpy((void *) &ed->IpAddress, (void *) &_ipInformation.ip.addr, sizeof(ed->IpAddress));
    memcpy((void *) &ed->SubnetMask, (void *) &_ipInformation.netmask.addr, sizeof(ed->SubnetMask));
    memcpy((void *) &ed->Gateway, (void *) &_ipInformation.gw.addr, sizeof(ed->Gateway));
    //
    //  Access point information.
    //
    memcpy((void *) &ed->Ssid, _apInformation.ssid, _apInformation.ssid_len);
    memcpy((void *) &ed->Bssid, _apInformation.bssid, sizeof(ed->Bssid));
    ed->Channel = _apInformation.channel;
    Mapping::GetStmValue(Mapping::NetworkAuthenticationType, _apInformation.authmode, (int8_t *) &ed->AuthenticationMode);

    TRACE_MESSAGE("PopulateConnectionEventDataStructure: Exit");
}

/**
 *  @brief Raise the ConnectToAccessPointEvent for the managed code.
 *
 *  @param status
 *     Status code to send back to the STM32.
 */
void WiFiRequestHandler::RaiseConnectToAccessPointEvent(StatusCodes::StatusCodes status)
{
    TRACE_MESSAGE("RaiseConnectToAccessPointEvent: Enter");

    Esp32Messaging::ConnectEventData *ed = static_cast<Esp32Messaging::ConnectEventData *>(pvPortMalloc(sizeof(Esp32Messaging::ConnectEventData)));
    if (ed != NULL)
    {
        PopulateConnectionEventDataStructure(ed);
        uint32_t payloadLength = Encoders::EncodedConnectEventDataBufferSize(ed);
        uint8_t *payload = static_cast<uint8_t *> (pvPortMalloc(payloadLength));
        if (payload != NULL)
        {
            Encoders::EncodeConnectEventData(ed, payload);
            vPortFree(ed);
            RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::NetworkConnectedEvent, status, payload, payloadLength);
        }
        else
        {
            ERROR_MESSAGE("Failed to allocate memory for ConnectEventData payload.");
        }
    }
    else
    {
        ERROR_MESSAGE("Failed to allocate memory for ConnectEventData");
    }

    TRACE_MESSAGE("RaiseConnectToAccessPointEvent: Exit");
}

/**
 * @brief Raise an error event for the WiFi interface.
 * 
 * @param statusCode Status code
 * 
 * @param payload Payload for the error event.
 * 
 * @param payloadLength Length of the payload.
 */
void WiFiRequestHandler::RaiseErrorEvent(StatusCodes::StatusCodes statusCode, uint8_t *payload, uint32_t payloadLength)
{
    TRACE_MESSAGE("RaiseErrorEvent: Enter");

    if (!payload || (payloadLength == 0))
    {
        payload = NULL;
        payloadLength = 0;
    }

    Message *message = Message::CreateOnHeap(MessageTypes::Event, Esp32Interfaces::WiFi, WiFiFunction::ErrorEvent, statusCode, _messageDispatcher->GetNextMessageID(), payload, payloadLength);
    if (message != NULL)
    {
        _messageDispatcher->QueueMessageForStm32(message);
    }

    TRACE_MESSAGE("RaiseErrorEvent: Exit");
}

/**
 *  @brief Event loop for the IDF WiFi system.
 *
 *  Perform the actual event loop handling on behalf of the static method set up for the
 *  IDF event handler.
 *
 *  See the notes for the WiFiRequestHandler::EventHandler for more information as to why this
 *  method is necessary.
 *
 *  @param eventBase
 *      Subsystem that created the event.  This will be one of WIFI_EVENT or IP_EVENT.
 *
 *  @param eventId
 *      Type of event that has occurred.  This will vary depending upon the subsystem
 *      that generated the event.
 *
 *  @param eventData
 *      Data specific to the event that has occurred.
 */
void WiFiRequestHandler::EventHandlerHelper(esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    TRACE_MESSAGE("EventHandlerHelper: Enter");
    if (eventBase == WIFI_EVENT)
    {
        switch (eventId)
        {
            case WIFI_EVENT_WIFI_READY:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_WIFI_READY");
                break;
            case WIFI_EVENT_SCAN_DONE:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_SCAN_DONE");
                xEventGroupSetBits(_xWiFiEventGroup, WIFI_SCAN_COMPLETE_BIT);
                break;
            case WIFI_EVENT_STA_START:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_START");
                StationStartedEvent();
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_STOP:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_STOP");
                StationStoppedEvent();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_CONNECTED");
                StationConnectedEvent(static_cast<wifi_event_sta_connected_t *>(eventData));
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_DISCONNECTED");
                StationDisconnectedEvent(static_cast<wifi_event_sta_disconnected_t *>(eventData));
                break;
            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_AUTHMODE_CHANGE");
                break;
            case WIFI_EVENT_STA_WPS_ER_SUCCESS:
                TRACE_MESSAGE("EventHandlerHelper:  WIFI_EVENT_STA_WPS_ER_SUCCESS");
                break;
            case WIFI_EVENT_STA_WPS_ER_FAILED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_WPS_ER_FAILED");
                break;
            case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_WPS_ER_TIMEOUT");
                break;
            case WIFI_EVENT_STA_WPS_ER_PIN:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_WPS_ER_PIN");
                break;
            case WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP");
                break;
            case WIFI_EVENT_AP_START:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_START");
                AccessPointStartedEvent();
                break;
            case WIFI_EVENT_AP_STOP:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_STOP");
                AccessPointStoppedEvent();
                break;
            case WIFI_EVENT_AP_STACONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_STACONNECTED");
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_STADISCONNECTED");
                NodeDisconnectedEvent(static_cast<wifi_event_ap_stadisconnected_t *>(eventData));
                break;
            case WIFI_EVENT_AP_PROBEREQRECVED:
                TRACE_MESSAGE("EventHandlerHelper: WIFI_EVENT_AP_PROBEREQRECVED");
                break;
            default:
                TRACE_MESSAGE("EventHandlerHelper: Unhandled WiFi Event ID %d", eventId);
                break;
        }
    }
    else
    {
        if (eventBase == IP_EVENT)
        {
            switch (eventId)
            {
                case IP_EVENT_STA_GOT_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_STA_GOT_IP");
                    StationGotIpEvent(static_cast<ip_event_got_ip_t *>(eventData));
                    break;
                case IP_EVENT_STA_LOST_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_STA_LOST_IP");
                    break;
                case IP_EVENT_AP_STAIPASSIGNED:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_AP_STAIPASSIGNED");
                    NodeConnectedEvent();
                    break;
                case IP_EVENT_GOT_IP6:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_GOT_IP6");
                    break;
                case IP_EVENT_ETH_GOT_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_ETH_GOT_IP");
                    break;
                case IP_EVENT_PPP_GOT_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_PPP_GOT_IP");
                    break;
                case IP_EVENT_PPP_LOST_IP:
                    TRACE_MESSAGE("EventHandlerHelper: IP_EVENT_PPP_LOST_IP");
                    break;
                default:
                    TRACE_MESSAGE("EventHandlerHelper: Unhandled IP Event ID %d", eventId);
                    break;
            }
        }
    }
    TRACE_MESSAGE("EventHandlerHelper: Exit");
}

/**
 *  @brief Event loop for the IDF WiFi system.
 *
 *  Event loop (handler) for the WiFi and IP events as discussed in the following ESP-IDF document:
 *
 *  https://docs.espressif.com/projects/esp-idf/en/release-v4.1/api-reference/system/esp_event.html
 *
 *  This method passes the data on to an instance helper method.  This is done for convenience as it
 *  allows access to the member variables and methods without having to use the sender context or
 *  convert methods / variables / constants to static members.
 *
 *  @param arg
 *      Argument data specified when the event loop was first registered with
 *      esp_event_handler_register.  arg should point to the instance of the WiFiRequestHandler
 *      that registered the event loop.
 *
 *  @param eventBase
 *      Subsystem that created the event.  This will be one of WIFI_EVENT or IP_EVENT.
 *
 *  @param eventId
 *      Type of event that has occurred.  This will vary depending upon the subsystem
 *      that generated the event.
 *
 *  @param eventData
 *      Data specific to the event that has occurred.
 */
void WiFiRequestHandler::EventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData)
{
    TRACE_MESSAGE("EventHandler: Enter");
    WiFiRequestHandler *sender = static_cast<WiFiRequestHandler *>(arg);
    sender->EventHandlerHelper(eventBase, eventId, eventData);
    TRACE_MESSAGE("EventHandler: Exit");
}

/**
 *  @brief Dispatch the WiFi request to the appropriate method in the class.
 *
 *  Some of the method calls in the switch statement below are followed by a call to
 *  xEventGroupSetBits and some are not.  The basic principle is that the POSIX calls
 *  that are executed through the thread pool need to have the
 *  WIFI_READY_FOR_NEXT_COMMAND_BIT set here as the thread pool does not know about
 *  the WiFi event group.  The other methods (in this class) have this set in the
 *  method call itself.
 *
 *  @param request
 *      Data structure containing the request information from the sender.
 */
void WiFiRequestHandler::DispatchRequest(Message *request)
{
    TRACE_MESSAGE("DispatchRequest: Enter");
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT | WIFI_ERROR_BIT);
    switch (request->Function)
    {
        case WiFiFunction::ConnectToAccessPoint:
            ConnectToAccessPoint(request);
            break;
        case WiFiFunction::ConnectToDefaultAccessPoint:
            ConnectToDefaultAccessPoint(request);
            break;
        case WiFiFunction::DisconnectFromAccessPoint:
            DisconnectFromAccessPoint(request);
            break;
        case WiFiFunction::GetAccessPoints:
            ScanForAccessPoints(request);
            break;
        case WiFiFunction::ClearDefaultAccessPoint:
            ClearDefaultAccessPointAndPassword(request);
            break;
        case WiFiFunction::StartAccessPoint:
            StartAccessPoint(request);
            break;
        case WiFiFunction::StopAccessPoint:
            StopAccessPoint(request);
            break;
        case WiFiFunction::SetAntenna:
            SetAntennaRequest(request);
            break;
        case WiFiFunction::Close:
            SystemRequestHandler::SharedThreadPool()->Execute(Close, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Read:
            SystemRequestHandler::SharedThreadPool()->Execute(Read, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::SetSockOpt:
            SystemRequestHandler::SharedThreadPool()->Execute(SetSockOpt, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::GetSockOpt:
            SystemRequestHandler::SharedThreadPool()->Execute(GetSockOpt, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Write:
            SystemRequestHandler::SharedThreadPool()->Execute(Write, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::FreeAddrInfo:
            SystemRequestHandler::SharedThreadPool()->Execute(FreeAddrInfo, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Connect:
            SystemRequestHandler::SharedThreadPool()->Execute(Connect, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Socket:
            SystemRequestHandler::SharedThreadPool()->Execute(Socket,static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::GetAddrInfo:
            SystemRequestHandler::SharedThreadPool()->Execute(GetAddrInfo, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::SendTo:
            SystemRequestHandler::SharedThreadPool()->Execute(SendTo, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::RecvFrom:
            SystemRequestHandler::SharedThreadPool()->Execute(RecvFrom, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Poll:
            Poll((void *) request);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Send:
            SystemRequestHandler::SharedThreadPool()->Execute(Send, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Accept:
            SystemRequestHandler::SharedThreadPool()->Execute(Accept, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Bind:
            SystemRequestHandler::SharedThreadPool()->Execute(Bind, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Listen:
            SystemRequestHandler::SharedThreadPool()->Execute(Listen, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::Ioctl:
            SystemRequestHandler::SharedThreadPool()->Execute(Ioctl, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::GetSockName:
            SystemRequestHandler::SharedThreadPool()->Execute(GetSockName, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        case WiFiFunction::GetPeerName:
            SystemRequestHandler::SharedThreadPool()->Execute(GetPeerName, static_cast<void *>(request));
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
            break;
        default:
            TRACE_MESSAGE("DispatchRequest: Unknown WiFi message received, function %d", request->Function);
            if ((request->Payload != NULL) && (request->PayloadLength > 0))
            {
                TRACE_HEX_BUFFER(request->Payload, request->PayloadLength);
            }
            break;
    }
    xEventGroupWaitBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    TRACE_MESSAGE("DispatchRequest: Exit");
}

/**
 * @brief Prepare the WiFi interface for deep sleep.
 */
StatusCodes::StatusCodes WiFiRequestHandler::PrepareForDeepSleep()
{
    TRACE_MESSAGE("PrepareForDeepSleep(): Enter");

    WiFiRequestHandler *wifiHandler = WiFiRequestHandler::GetInstance();
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    if (wifiHandler != NULL)
    {
        result = wifiHandler->StopWiFIAdapter();
    }
    return(result);

    TRACE_MESSAGE("PrepareForDeepSleep(): Enter");
}

/**
 *  @brief Stop the WiFi adapter.
 */
StatusCodes::StatusCodes WiFiRequestHandler::StopWiFIAdapter()
{
    TRACE_MESSAGE("StopWiFIAdapter(): Enter");

    esp_err_t result = ESP_OK;

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_CONNECTED_BIT))
    {
        result = esp_wifi_disconnect();
    }

    if (result == ESP_OK)
    {
        result = esp_wifi_stop();
    }

    //
    //  Strictly speaking we do not need to unregister the event handlers as registering
    //  the handlers again will overwrite the previous handlers.
    //
    if (result == ESP_OK)
    {
        result = esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiRequestHandler::EventHandler);
    }

    if (result == ESP_OK)
    {
        result = esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &WiFiRequestHandler::EventHandler);
    }

    if (result == ESP_OK)
    {
        result = esp_wifi_deinit();
    }

    if (result != ESP_OK)
    {
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
    }

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_INTERFACE_STARTING_BIT);

    TRACE_MESSAGE("StopWiFIAdapter(): Exit, ESP error code %d (%s)", result, esp_err_to_name(result));
    return(Mapping::GetStatusCode(result));
}

/**
 *  @brief Disconnect from any access points.
 */
StatusCodes::StatusCodes WiFiRequestHandler::DisconnectFromAccessPoint()
{
    TRACE_MESSAGE("DisconnectFromAccessPoint(): Enter");
    StatusCodes::StatusCodes sc = StatusCodes::CompletedOk;

    esp_err_t result = esp_wifi_disconnect();
    if ((result == ESP_OK) || (result == ESP_ERR_WIFI_NOT_STARTED) || (result == ESP_ERR_WIFI_NOT_INIT))
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);
    }
    else
    {
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
        sc = StatusCodes::Failure;
    }
    ClearIpAndDisconnectData();

    TRACE_MESSAGE("DisconnectFromAccessPoint(): Exit");
    return(sc);
}

/**
 *  @brief Disconnect from the current access point.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::DisconnectFromAccessPoint(Message *message)
{
    TRACE_MESSAGE("DisconnectFromAccessPoint(Message): Enter");

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_DISCONNECT_REQUESTED_BIT);

    StatusCodes::StatusCodes sc = StatusCodes::CompletedOk;
    if (IsConnected())
    {
        sc = DisconnectFromAccessPoint();
    }
    if (sc == StatusCodes::CompletedOk)
    {
        Esp32Messaging::DisconnectFromAccessPointRequest *request = Encoders::ExtractDisconnectFromAccessPointRequest(message->Payload);

        if (request->TurnOffWiFiInterface == 1)
        {
            sc = StopWiFIAdapter();
        }
        vPortFree(request);
    }

    vPortFree(message->Payload);
    message->Payload = NULL;
    message->PayloadLength = 0;

    message->StatusCode = sc;
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::NetworkDisconnectedEvent, sc);

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_DISCONNECT_REQUESTED_BIT);

    TRACE_MESSAGE("DisconnectFromAccessPoint(Message): Exit");
}

/**
 *  @brief Change the antenna.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::SetAntennaRequest(Message *message)
{
    TRACE_MESSAGE("SetAntennaRequest: Enter");

    Esp32Messaging::SetAntennaRequest *request = Encoders::ExtractSetAntennaRequest(message->Payload);
    vPortFree(message->Payload);
    message->Payload = nullptr;
    message->PayloadLength = 0;

    TRACE_MESSAGE("Switching antenna to %d with persist set to %d", request->Antenna, request->Persist);

    if (request->Persist)
    {
        SetAntenna(request->Antenna);   // This method also switches the antenna.
    }
    else
    {
        SwitchAntenna(static_cast<AntennaTypes::AntennaTypes>(request->Antenna));
    }

    vPortFree(request);
    message->StatusCode = StatusCodes::CompletedOk;
    message->MessageType = MessageTypes::Response;
    _messageDispatcher->QueueMessageForStm32(message);

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    TRACE_MESSAGE("SetAntennaRequest: Exit");
}

/**
 *  @brief Indicate if we have a valid WiFi connection.
 *
 *  @returns
 *      True if the WiFi connection is active, false otherwise.
 */
bool WiFiRequestHandler::IsConnected()
{
    EventBits_t bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, 10 / portTICK_PERIOD_MS);

    return((bits & WIFI_CONNECTED_BIT) == WIFI_CONNECTED_BIT);
}

/**
 *  @brief Connect to a specified access point.
 *
 *  @param ssid
 *      SSID of the access point.
 *
 *  @param password
 *      Password for the access point.
 */
StatusCodes::StatusCodes WiFiRequestHandler::ConnectToAccessPoint(const Esp32Messaging::AccessPointInformation *accessPointInformation)
{
    char *password = accessPointInformation->Password;

    TRACE_MESSAGE("ConnectToAccessPoint: Access point %s, password '%s'", accessPointInformation->NetworkName, password == nullptr ? "" : password);
    TRACE_MESSAGE("IP information, IP: 0x%08x, subnet: 0x%08x, gateway 0x%08x", accessPointInformation->IpAddress, accessPointInformation->SubnetMask, accessPointInformation->Gateway);

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (bits & WIFI_HAVE_IP_BIT)
    {
        return(StatusCodes::WiFiAlreadyStarted);
    }

    if ((accessPointInformation->NetworkName == nullptr) || (strlen(accessPointInformation->NetworkName) == 0))
    {
        return (StatusCodes::InvalidWiFiCredentials);
    }

    _currentRetryCount = 0;
    _lastIdfErrorCode = ESP_OK;
    ClearIpAndDisconnectData();
    if (!(bits & WIFI_INTERFACE_STARTED_BIT))
    {
        if (StartWiFiInterface(WIFI_MODE_STA, accessPointInformation) != ESP_OK)
        {
            result = StatusCodes::CannotStartNetworkInterface;
        }
        else
        {
            TRACE_MESSAGE("WiFi interface started OK.");
        }
    }

    if (result == StatusCodes::CompletedOk)
    {
        _currentRetryCount = 0;
        char zeroLengthString[1] = { 0 };
        if (password == nullptr)
        {
            password = zeroLengthString;
        }
        if ((strlen(accessPointInformation->NetworkName) > MAXIMUM_SSID_LENGTH) || (strlen(password) > MAXIMUM_PASSWORD_LENGTH))
        {
            result = StatusCodes::InvalidWiFiCredentials;
            TRACE_MESSAGE("ConnectToAccessPoint: Invalid credentials");
        }
        else
        {
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
            wifi_config_t wifi_config = { };
            wifi_config.sta.pmf_cfg =
            {
                .capable = true,
                .required = false
            };
            memcpy(wifi_config.sta.ssid, accessPointInformation->NetworkName, strlen(accessPointInformation->NetworkName));
            memcpy(wifi_config.sta.password, password, strlen(password));
            // _lastIdfErrorCode = esp_wifi_set_mode(WIFI_MODE_STA);
            if (_lastIdfErrorCode == ESP_OK)
            {
                _lastIdfErrorCode = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            }
            // if (_lastIdfErrorCode == ESP_OK)
            // {
            //     _lastIdfErrorCode = esp_wifi_start();
            // }
            if (_lastIdfErrorCode == ESP_OK)
            {
                _lastIdfErrorCode = esp_wifi_connect();
            }
            if (_lastIdfErrorCode == ESP_OK)
            {
                bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_HAVE_IP_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
                if (bits & WIFI_ERROR_BIT)
                {
                    result = StatusCodes::CannotConnectToAccessPoint;
                }
                else
                {
                    if (bits & WIFI_HAVE_IP_BIT)
                    {
                        result = StatusCodes::CompletedOk;
                    }
                    else
                    {
                        result = StatusCodes::Timeout;
                    }
                }
            }
            else
            {
                int32_t found;
                result = (StatusCodes::StatusCodes) Mapping::GetStmValue(Mapping::EspErrorCodes, (int32_t) _lastIdfErrorCode, &found);
            }
        }
    }
    TRACE_MESSAGE("%s: result %d", __func__, result);
    return(result);
}

/**
 *  @brief Connect to a specified access point.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::ConnectToAccessPoint(Message *message)
{
    TRACE_MESSAGE("ConnectToAccessPoint: Enter");
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    Esp32Messaging::AccessPointInformation *accessPointInformation = Encoders::ExtractAccessPointInformation(message->Payload);
    message->DeletePayload();

    //
    //  Check if the IP address is non-zero, in this case the application is configured for a static IP.
    //  Static IP addresses must have a valid subnet mask and gateway address set.
    //
    if (accessPointInformation->IpAddress != 0)
    {
        if ((accessPointInformation->SubnetMask == 0) || (accessPointInformation->Gateway == 0))
        {
            result = StatusCodes::InvalidIp;
            RaiseErrorEvent(result, nullptr, 0);
        }
    }

    result = ConnectToAccessPoint(accessPointInformation);
    vPortFree(accessPointInformation);

    Esp32Messaging::ConnectEventData data = { };

    if (result == StatusCodes::CompletedOk)
    {
        PopulateConnectionEventDataStructure(&data);
    }
    else
    {
        memcpy(static_cast<void *>(data.Ssid), static_cast<void *>(_disconnectData.ssid), _disconnectData.ssid_len);
        memcpy(static_cast<void *>(data.Bssid), static_cast<void *>(_disconnectData.bssid), sizeof(data.Bssid));
        TRACE_MESSAGE("Failed to connect to access point, code: %d", _disconnectData.reason);
        data.Reason = _disconnectData.reason;
        ClearIpAndDisconnectData();
        _lastIdfErrorCode = ESP_OK;
    }
    message->PayloadLength = Encoders::EncodedConnectEventDataBufferSize(&data);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeConnectEventData(&data, message->Payload);

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    Logging::DumpMessage(COMPONENT_NAME, message);

    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    if (result == StatusCodes::CompletedOk)
    {
        RaiseConnectToAccessPointEvent(result);
    }
    
    TRACE_MESSAGE("ConnectToAccessPoint: Exit");
}

/**
 *  @brief Connect to the default access point.
 *
 *  Note: This is a non-blocking call and will generate the connection event
 *        with the status code set accordingly.
 *
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::ConnectToDefaultAccessPoint(Message *message)
{
    TRACE_MESSAGE("ConnectToDefaultAccessPoint: Enter");
    StatusCodes::StatusCodes result = StatusCodes::DefaultAccessPointNotConfigured;

    char *access_point = GetDefaultAccessPoint();
    if (access_point != NULL)
    {
        Esp32Messaging::AccessPointInformation accessPointInformation = { };
        accessPointInformation.NetworkName = access_point;
        accessPointInformation.Password = GetPassword();
        if (!_useDhcp)
        {
            accessPointInformation.IpAddress = GetIpAddress();
            accessPointInformation.SubnetMask = GetSubNetMask();
            accessPointInformation.Gateway = GetDefaultGateway();
        }
        result = ConnectToAccessPoint(&accessPointInformation);
    }

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    if (result == StatusCodes::CompletedOk)
    {
        RaiseConnectToAccessPointEvent(result);
    }

    TRACE_MESSAGE("ConnectToDefaultAccessPoint: Exit");
}

/**
 *  @brief Clear the default access point and password from NVS.
 * 
 *  @param message
 *      Message containing the encoded request.
 */
void WiFiRequestHandler::ClearDefaultAccessPointAndPassword(Message *message)
{
    TRACE_MESSAGE("ClearDefaultAccessPointAndPassword: Enter");

    message->MessageType = MessageTypes::Response;
    StatusCodes::StatusCodes sc = SetDefaultAccessPoint(NULL);
    StatusCodes::StatusCodes sc1 = SetPassword(NULL);
    message->StatusCode = (sc == StatusCodes::CompletedOk) && (sc1 == StatusCodes::CompletedOk) ? StatusCodes::CompletedOk : StatusCodes::Failure;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    TRACE_MESSAGE("ClearDefaultAccessPointAndPassword: Exit");
}

/**
 *  @brief Start the network interface.
 *
 *  Start the WiFi network interface ready for the device to be configured
 *  as either an access point (softAP) or as a network device (station).
 * 
 *  @param mode
 *      WiFi mode of the ESP32 (Station, Access Point or both).
 * 
 *  @param accessPointInformation
 *      Information about the configuration of the access point to connect to or the
 *      IP information for a station.
 *
 *  @return
 *      ESP_OK if the interface is initialised correctly, an error code otherwise.
 */
esp_err_t WiFiRequestHandler::StartWiFiInterface(const wifi_mode_t mode, const Esp32Messaging::AccessPointInformation *accessPointInformation)
{
    TRACE_MESSAGE("StartWiFiInterface: Enter");

    esp_err_t result = ESP_OK;

    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_INTERFACE_STARTED_BIT))
    {
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTING_BIT);
        result = esp_netif_init();
        if (result == ESP_OK)
        {
            result = esp_event_loop_create_default();
        }

        if (result == ESP_OK)
        {
            _stationNetIfHandle = NULL;
            _softApNetIfHandle = NULL;
            if ((mode == WIFI_MODE_STA) || (mode == WIFI_MODE_APSTA))
            {
                _stationNetIfHandle = esp_netif_create_default_wifi_sta();
            }

            // if ((mode == WIFI_MODE_AP) || (mode == WIFI_MODE_APSTA))
            // {
            //     _softApNetIfHandle = esp_netif_create_default_wifi_ap();
            // }
            if (_stationNetIfHandle == NULL)
            {
                result = ESP_FAIL;
            }
        }

        //
        //  Set IP address if static address has been selected otherwise DHCP will be used.
        //
        if ((result == ESP_OK) && ((accessPointInformation) && (accessPointInformation->IpAddress != 0) && (accessPointInformation->Gateway != 0) && (accessPointInformation->SubnetMask != 0)))
        {
            result = esp_netif_dhcpc_stop(_stationNetIfHandle);
            if (result == ESP_OK)
            {
                char ip[INET_ADDRSTRLEN], subnet[INET_ADDRSTRLEN], gateway[INET_ADDRSTRLEN];
                strcpy(ip, inet_ntoa(accessPointInformation->IpAddress));
                strcpy(subnet, inet_ntoa(accessPointInformation->SubnetMask));
                strcpy(gateway, inet_ntoa(accessPointInformation->Gateway));
                TRACE_MESSAGE("Configuring static IP address, IP: %s, subnet mask %s, default gateway %s", ip, subnet, gateway);

                esp_netif_ip_info_t ip_info;
                ip_info.ip.addr = accessPointInformation->IpAddress;
                ip_info.gw.addr = accessPointInformation->Gateway;
                ip_info.netmask.addr = accessPointInformation->SubnetMask;
                result = esp_netif_set_ip_info(_stationNetIfHandle, &ip_info);
            }
        }

        if (result == ESP_OK)
        {
            wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
            result = esp_wifi_init(&cfg);
        }

        if (result == ESP_OK)
        {
            result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiRequestHandler::EventHandler, this);
        }

        if (result == ESP_OK)
        {
            result = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &WiFiRequestHandler::EventHandler, this);
        }

        if (result == ESP_OK)
        {
            result = esp_wifi_set_mode(mode);
        }

        if (result == ESP_OK)
        {
            result = esp_wifi_start();
        }

        if (result != ESP_OK)
        {
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTING_BIT);
        }
    }
    ESP_ERROR_CHECK(result);

    TRACE_MESSAGE("StartWiFiInterface: Exit");
    return(result);
}

/**
 *  @brief Get the full list of available networks.
 *
 *  @pre
 *      The network interface must be started before this method is called.
 *      It is not necessary to connected to an access pint but the interface
 *      must be started.
 *
 *  @param message
 *      Request sent from the STM32.
 */
void WiFiRequestHandler::ScanForAccessPoints(Message *message)
{
    TRACE_MESSAGE("ScanForAccessPoints: Enter");
    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    _currentRetryCount = 0;
    EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    if (!(bits & WIFI_CONNECTED_BIT))
    {
        TRACE_MESSAGE("Starting network");
        if (StartWiFiInterface(WIFI_MODE_STA, nullptr) != ESP_OK)
        {
            result = StatusCodes::CannotStartNetworkInterface;
        }
    }

    if (result == StatusCodes::CompletedOk)
    {
        uint16_t number = CONFIG_WIFI_PROV_SCAN_MAX_ENTRIES;
        wifi_ap_record_t *ap_info = static_cast<wifi_ap_record_t *>(pvPortMalloc(CONFIG_WIFI_PROV_SCAN_MAX_ENTRIES * sizeof(wifi_ap_record_t)));
        if (ap_info == NULL)
        {
            result = StatusCodes::Failure;
        }
        else
        {
            bzero(ap_info, CONFIG_WIFI_PROV_SCAN_MAX_ENTRIES * sizeof(wifi_ap_record_t));

            bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
            if (bits & WIFI_INTERFACE_STARTED_BIT)
            {
                ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));

                bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_SCAN_COMPLETE_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
                if (bits & WIFI_SCAN_COMPLETE_BIT)
                {
                    uint16_t apCount = 0;
                    esp_wifi_scan_get_ap_num(&apCount);
                    TRACE_MESSAGE("ScanForAccessPoints: Number of access points found: %d", apCount);
                    if (apCount != 0)
                    {
                        Esp32Messaging::AccessPointList *accessPointList = static_cast<Esp32Messaging::AccessPointList *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPointList)));
                        accessPointList->NumberOfAccessPoints = apCount;
                        //
                        //  Now we get the access point information.
                        //
                        wifi_ap_record_t *accessPoints = static_cast<wifi_ap_record_t *>(pvPortMalloc(apCount * sizeof(wifi_ap_record_t)));
                        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, accessPoints));

                        Esp32Messaging::AccessPoint *accessPoint = static_cast<Esp32Messaging::AccessPoint *>(pvPortMalloc(sizeof(Esp32Messaging::AccessPoint)));
                        int singleAccessPointSize = Encoders::EncodedAccessPointBufferSize(accessPoint);
                        accessPointList->AccessPointsLength = singleAccessPointSize * apCount;
                        accessPointList->AccessPoints = static_cast<uint8_t *>(pvPortMalloc(accessPointList->AccessPointsLength));
                        uint8_t *currentLocation = accessPointList->AccessPoints;
                        for (int index = 0; index < apCount; index++)
                        {
                            bzero(accessPoint, sizeof(Esp32Messaging::AccessPoint));
                            memcpy(accessPoint->Ssid, &accessPoints[index].ssid, 33);
                            memcpy(accessPoint->Bssid, &accessPoints[index].bssid, 6);
                            accessPoint->PrimaryChannel = accessPoints[index].primary;
                            accessPoint->SecondaryChannel = accessPoints[index].second;
                            accessPoint->Rssi = accessPoints[index].rssi;
                            accessPoint->AuthenticationMode = accessPoints[index].authmode;
                            accessPoint->Protocols = accessPoints[index].phy_11b | accessPoints[index].phy_11g | accessPoints[index].phy_11n | accessPoints[index].phy_lr | accessPoints[index].wps;
                            Encoders::EncodeAccessPoint(accessPoint, currentLocation);
                            currentLocation += singleAccessPointSize;
                        }
                        vPortFree(accessPoints);
                        vPortFree(accessPoint);

                        message->PayloadLength = Encoders::EncodedAccessPointListBufferSize(accessPointList);
                        message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
                        Encoders::EncodeAccessPointList(accessPointList, message->Payload);
                        vPortFree(accessPointList);
                    }
                    result = StatusCodes::CompletedOk;
                }
                else
                {
                    TRACE_MESSAGE("ScanForAccessPoints: Scan timeout.");
                    result = StatusCodes::Timeout;
                }
                vPortFree(ap_info);
            }
            else
            {
                TRACE_MESSAGE("ScanForAccessPoints: Scan timeout.");
                result = StatusCodes::Timeout;
            }
        }
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = result;
    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);
    TRACE_MESSAGE("ScanForAccessPoints: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Event Handlers
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief The access point has just given the ESP32 an IP address.
 *
 *  This event indicates that we have successfully obtained an IP address.  It is at this
 *  point that we really consider the device to be connected to the access point as we can
 *  start to perform network access operations.
 *
 *  @param eventData
 *      Data for the IP event.
 */
void WiFiRequestHandler::StationGotIpEvent(ip_event_got_ip_t *eventData)
{
    TRACE_MESSAGE("StationGotIpEvent: Enter");
    _currentRetryCount = 0;
    memcpy((void *) &_ipInformation, (void *) &eventData->ip_info, sizeof(esp_netif_ip_info_t));
    _ipChanged = eventData->ip_changed;

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT | WIFI_INTERFACE_STARTING_BIT);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_HAVE_IP_BIT);
    TRACE_MESSAGE("StationGotIpEvent: Exit");
}

/**
 *  @brief Process the station started event.
 */
void WiFiRequestHandler::StationStartedEvent()
{
    TRACE_MESSAGE("StationStartedEvent: Enter");
    _lastIdfErrorCode = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, SystemRequestHandler::GetDeviceName());
    if (_lastIdfErrorCode != ESP_OK)
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_INTERFACE_STARTING_BIT | WIFI_CONNECTING_BIT);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
    }
    else
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTING_BIT | WIFI_ERROR_BIT);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT);
    }
    TRACE_MESSAGE("StationStartedEvent: Exit");
}

/**
 *  @brief Process the station stopped event.
 */
void WiFiRequestHandler::StationStoppedEvent()
{
    TRACE_MESSAGE("StationStoppedEvent: Enter");

    _stationNetIfHandle = nullptr;
    xEventGroupClearBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_INTERFACE_STARTING_BIT);

    TRACE_MESSAGE("StationStoppedEvent: Exit");
}

/**
 *  @brief Process the WiFi station connect event.
 *
 *  This event will fire when the ESP has successfully connected to an access point.
 *
 *  @param eventData
 *      Data for the connection event.
 */
void WiFiRequestHandler::StationConnectedEvent(wifi_event_sta_connected_t *eventData)
{
    TRACE_MESSAGE("StationConnectedEvent: Enter");
    memcpy((void *) &_apInformation, (void *) eventData, sizeof(wifi_event_sta_connected_t));

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);

    TRACE_MESSAGE("StationConnectedEvent: Exit");
}

/**
 *  @brief Process the WiFi disconnected event.
 *
 *  @param eventData
 *      Data for the disconnected event.
 */
void WiFiRequestHandler::StationDisconnectedEvent(wifi_event_sta_disconnected_t *eventData)
{
    TRACE_MESSAGE("StationDisconnectedEvent: Enter");

    bool raiseEvent = true;
    EventBits_t bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_DISCONNECT_REQUESTED_BIT, pdFALSE, pdFALSE, 0);
    if (bits & WIFI_DISCONNECT_REQUESTED_BIT)
    {
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT | WIFI_DISCONNECT_REQUESTED_BIT);
    }
    else
    {
        uint32_t maximumRetryCount = GetMaximumRetryCount();
        if (bits & WIFI_CONNECTED_BIT)
        {
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT);
            xEventGroupSetBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
            _currentRetryCount = 0;
        }
        if (_automaticReconnect && (_currentRetryCount < maximumRetryCount))
        {
            esp_wifi_connect();
            _currentRetryCount++;
            raiseEvent = false;
        }
        else
        {
            xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTING_BIT);
        }
    }

    if (raiseEvent)
    {
        ClearIpAndDisconnectData();
        bzero((void *) &_apInformation, sizeof(wifi_event_sta_connected_t));
        RaiseEvent(Esp32Interfaces::WiFi, WiFiFunction::NetworkDisconnectedEvent);
        xEventGroupClearBits(_xWiFiEventGroup, WIFI_CONNECTED_BIT | WIFI_CONNECTING_BIT);
        xEventGroupSetBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
    }

    TRACE_MESSAGE("StationDisconnectedEvent: Exit");
}