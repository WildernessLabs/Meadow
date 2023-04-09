/*
 *  WiFi.hpp
 *
 *  Encapsulate the properties and methods required to provide the
 *  ability to communicate with the WiFi network.
 */

#ifndef _WIFI_REQUEST_HANDLER_HPP_
#define _WIFI_REQUEST_HANDLER_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "errno.h"
#include <string.h>
#include <stdlib.h>
#include <map>
#include <iterator>

#include "MessageDispatcher.hpp"
#include "Esp32Messaging.hpp"
#include "RequestHandlerBase.hpp"
#include "SharedEnums.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"

/**
 *  The following definitions are copied from the NuttX system as they
 *  have no analogy in ESP-IDF and hence no mapping is needed but we need
 *  a copy of these defintions in order to implement some of the networking
 *  functionality.
 */

/* Interface flag bits (for ioctl) */

#define IFF_DOWN           (1 << 0) /* Interface is down */
#define IFF_UP             (1 << 1) /* Interface is up */
#define IFF_RUNNING        (1 << 2) /* Carrier is available */
#define IFF_IPv6           (1 << 3) /* Configured for IPv6 packet (vs ARP or IPv4) */
#define IFF_NOARP          (1 << 7) /* ARP is not required for this packet */

/* Socket definitions (for ioctl) */

#define _SIOCBASE           (0x0700)            /* Socket ioctl commands */
#define _IOC(type,nr)       ((type)|(nr))
#define _SIOC(nr)           _IOC(_SIOCBASE,nr)

#define SIOCGIFADDR         _SIOC(0x0001)       /* Get IP address */
#define SIOCGIFDSTADDR      _SIOC(0x0003)       /* Get P-to-P address */
#define SIOCGIFBRDADDR      _SIOC(0x0005)       /* Get broadcast IP address */
#define SIOCGIFNETMASK      _SIOC(0x0007)       /* Get network mask */
#define SIOCGIFHWADDR       _SIOC(0x0014)       /* Get hardware address */

#define SIOCGIFCONF         _SIOC(0x0018)       /* Return an interface list (IPv4) */
#define SIOCGIFFLAGS        _SIOC(0x001b)       /* Gets the interface flags */
#define SIOCGIFNAME         _SIOC(0x002A)       /* Get interface name string */

/**
 *  @brief WiFi class
 *
 *  Encapsulate the properties and methods required to provide the
 *  ability to communicate with the WiFi network.
 */
class WiFiRequestHandler : public RequestHandlerBase
{
private:
    /**
     *  @brief Hold the information about the originating poll request and the pollfd's.
     */
    struct poll_request_s
    {
        struct pollfd *fd;      /* Pointer to the pollfd structure of the original request. */
        uint32_t request_id;    /* ID of the message sent to the ESP32. */
    };
    typedef struct poll_request_s poll_request_t;

    /*
     *  Event group bits.
     */

    /**
     *  @brief Indicate if the WiFi interface can accept a new request.
     * 
     *  The WIFI_READY_FOR_NEXT_COMMAND_BIT indicates if this current request has completed successfully.  It
     *  is used to block the Task from making a second request when one is already
     *  running.
     *
     *  This flag is necessary as some tasks are actually multi-stage processes.  For
     *  instance, Connecting to a network requires a start request which will generate
     *  two events, network start and got IP.  The request only ends when the IP address
     *  has been granted.
     */
    static const int WIFI_READY_FOR_NEXT_COMMAND_BIT = BIT0;

    /**
     *  @brief Indicate if the system is in the process of connecting to an access point.
     */
    static const int WIFI_CONNECTING_BIT = BIT1;

    /**
     *  @brief When set, this bit indicates that we have an active WiFi connection.
     */
    static const int WIFI_CONNECTED_BIT = BIT2;

    /**
     *  @brief When set, this indicates that the system has lost the WiFi connection
     *  and is actively trying to re-establish the connection.
     */
    static const int WIFI_HAVE_IP_BIT = BIT3;

    /**
     *  @brief Indicate if an explicit disconnect has been requested.
     */
    static const int WIFI_DISCONNECT_REQUESTED_BIT = BIT4;

    /**
     *  @brief Indicate if the WiFi system has been initialised OK.
     */
    static const int WIFI_INTERFACE_STARTING_BIT = BIT5;

    /**
     *  @brief Indicate if the WiFi system has been initialised OK.
     */
    static const int WIFI_INTERFACE_STARTED_BIT = BIT6;

    /**
     *  @brief Indicate if the a request to stop the WiFi interface has been made.
     */
    static const int WIFI_INTERFACE_STOP_REQUESTED_BIT = BIT7;

    /**
     *  @brief Indicate if the scan for access points has completed.
     */
    static const int WIFI_SCAN_COMPLETE_BIT = BIT8;

    /**
     *  @brief Indicate a reset of the WiFi interface has been requested.
     */
    static const int WIFI_RESET_BIT = BIT9;

    /**
     * @brief Indicated if an access point is running on this device.
     */
    static const int WIFI_ACCESS_POINT_STARTED_BIT = BIT10;

    /**
     * @brief USed to indicate that the access point is starting.
     */
    static const int WIFI_STARTING_ACCESS_POINT_BIT = BIT11;

    /**
     *  @brief Indicate that some sort of error has occurred.
     * 
     *  This will usually be problems with connections, getting the system started etc.
     */
    static const int WIFI_ERROR_BIT = BIT23;

    /**
     * @brief All of the bits that could be set in an event group.
     */
    static const int WIFI_ALL_BITS = 0x008fffff;

    /**
     *  @brief Default scan method for WiFi networks.
     */
    const wifi_scan_method_t DEFAULT_SCAN_METHOD = WIFI_FAST_SCAN;

    /**
     *  @brief Default sort method for the WiFi networks when scanned.
     */
    const wifi_sort_method_t DEFAULT_SORT_METHOD = WIFI_CONNECT_AP_BY_SIGNAL;

    /**
     *  @brief Default RSSI when scanning for networks.
     */
    const int DEFAULT_RSSI = -127;

    /**
     *  @brief Default authorisation mode.
     */
    const wifi_auth_mode_t DEFAULT_AUTH_MODE = WIFI_AUTH_OPEN;

    /**
     *  @brief Maximum length of the string containing the SSID name.
     */
    const int MAXIMUM_SSID_LENGTH = 32;

    /**
     *  @brief Maximum length of the password for the access point.
     */
    const int MAXIMUM_PASSWORD_LENGTH = 64;

    /**
     *  @brief GPIO Pin connected to the antenna selection switch
     *  which will connect the on board antenna to the ESP32.
     */
    static const gpio_num_t ON_BOARD_ANTENNA_PIN;

    /**
     *  @brief GPIO Pin connected to the antenna selection switch
     *  which will connect the external antenna to the ESP32.
     */
    static const gpio_num_t EXTERNAL_ANTENNA_PIN;

    /**
     *  @brief Index of the dummy socket file descriptor in poll request.
     */
    static const int POLL_DUMMY_FD_INDEX;

    /**
     *  @brief Index of the actual socket file descriptor in poll request.
     */
    static const int POLL_FD_INDEX;

    /**
     *  @brief Pointer to the single instance of the WiFiRequestHandler.
     */
    static WiFiRequestHandler *_instance;

    /**
     *  @brief Event group containing 24-bits used to indicate the state of the WiFi connection.
     */
    static EventGroupHandle_t _xWiFiEventGroup;

    /**
     *  @brief Configuration of the WiFi network.
     */
    wifi_config_t *_wifiConfig = nullptr;

    /**
     *  @brief Current retry count.
     */
    int _currentRetryCount = 0;

    /**
     *  @brief Last error code from a call into the ESP IDF.
     * 
     *  This can be used to track errors when using events.
     */
    esp_err_t _lastIdfErrorCode;

    /**
     *  @brief Information about the access point that we have connected to.
     * 
     *  This will be filled with 0 at startup and when the device disconnects from an access point.
     */
    wifi_event_sta_connected_t _apInformation;

    /**
     *  @brief Information about the disconnect reason.
     * 
     *  This will be filled with 0 at startup and should only be populated following a disconnect event.
     */
    wifi_event_sta_disconnected_t _disconnectData;

    /**
     *  @brief Current IP information (IP address, subnet mask and gateway).
     * 
     *  This will be filled with 0 at startup and when the device disconnects from an access point.
     */
    esp_netif_ip_info_t _ipInformation;

    /**
     *  @brief Indicate if the IP address information has changed.
     */
    bool _ipChanged;

    /**
     *  @brief Network Interface Handle for the board when running in station mode
     *  i.e. as a device connected to an access point.
     */
    static esp_netif_t *_stationNetIfHandle;

    /**
     *  @brief Network Interface Handle for the board when it is acting as a soft
     *  access point.
     */
    static esp_netif_t *_softApNetIfHandle;

    /**
     *  @brief Current antenna in use.  Default to the on board antenna at startup.
     */
    static AntennaTypes::AntennaTypes _currentAntenna;

    /**
     *  @brief Name of the NVS storage holding the _currentAntenna value.
     */
    static const char *ANTENNA_TYPE_NAME;

    /**
     *  @brief Should the system automatically reconnect to an access point should there
     *  be a problem and the current connection is dropped.
     */
    static bool _automaticReconnect;

    /**
     *  @brief Name of the NVS storage holding the _automaticReconnect value.
     */
    static const char *AUTOMATIC_RECONNECT_NAME;

    /**
     *  @brief Indicate if the network should start automatically on reboot.
     */
    static bool _automaticallyStartNetwork;

    /**
     *  @brief Name of the NVS storage holding the _automaticallyStartNetwork value.
     */
    static const char *AUTOMATICALLY_START_NETWORK_NAME;

    /**
     *  Maximum number of times a failed event should retry before raising an error.
     */
    static uint32_t _maximumRetryCount;

    /**
     *  @brief Name of the NVS storage holding the _maximumRetryCount value.
     */
    static const char *MAXIMUM_RETRY_COUNT_NAME;

    /**
     *  @brief Indicate if the system should connect to a time server and get the
     *  current time at startup.
     */
    static bool _getTimeAtStartup;

    /**
     *  @brief Name fo the storage in NVS that holds the value indicating that the
     *  system should get the time at startup. 
     */
    static const char *GET_TIME_AT_STARTUP_NAME;

    /**
     *  @brief Indicate if we should use DHCP.
     */
    static bool _useDhcp;

    /**
     *  @brief Name of the storage holding the _useDhcp configuration setting.
     */
    static const char *USE_DHCP_NAME;

    /**
     *  @brief Defaut access [oint to be used when automatically connecting.
     */
    static char *_defaultAccessPoint;

    /**
     *  @brief Name of the storage in NVS holding the default access point.
     */
    static const char *DEFAULT_ACCESS_POINT_NAME;

    /**
     *  @brief Password for the access point.
     */
    static char *_password;

    /**
     *  @brief Name of the storage in NVS holding the password for the access point.
     */
    static const char *PASSWORD_NAME;

    /**
     *  @brief Name of the NTP server to be used when using NTP to set the time
     *  when the board starts.
     */
    static char *_ntpServer;

    /**
     *  @brief Name of the storage space in NVS holding the timezone information.
     */
    static const char *TIMEZONE_NAME;

    /**
     *  @brief Timezone to be used when the ESP32 starts.  Default will be UTC.
     */
    static char *_timezone;

    /**
     *  @brief Name of the storage space in NVS holding the NTP server information.
     */
    static const char *NTP_SERVER_NAME;

    /**
     *  @brief Static IP address when not using DHCP.
     */
    static uint32_t _staticIpAddress;

    /**
     *  @brief Name of the storage space holding the static IP address.
     */
    static const char *STATIC_IP_ADDRESS_NAME;

    /**
     *  @brief DNS server when not using DHCP.
     */
    static uint32_t _dnsServer;

    /**
     *  @brief Name of the storage in NVS holding the DNS server IP address.
     */
    static const char *DNS_SERVER_NAME;

    /**
     *  @brief IP address of the default gateway when not using DHCP.
     */
    static uint32_t _defaultGateway;

    /**
     *  @brief Name of the storage space holding the IP address of the default gateway.
     */
    static const char *DEFAULT_GATEWAY_NAME;

    /**
     *  @brief Default value for the WiFi timeout.
     */
    static uint32_t _wifiTimeout;

    /**
     *  @brief Name of the storage space holding the default WiFi timeout.
     */
    static const char *DEFAULT_WIFI_TIMEOUT_NAME;

    /**
     *  @brief List of active poll requests.
     */
    static std::map<uint32_t, struct pollfd *> _pollRequests;

    /**
     *  @brief Mutex for the poll request linked list.
     */
    static SemaphoreHandle_t _pollRequestListMutex;

    /**
     *  @brief Handle for the completed poll requests.
     */
    static QueueHandle_t _completedPollRequestsQueue;

    /**
     *  @brief Method that will deal with completed poll requests.
     */
    TaskHandle_t _completedPollRequestsTaskHandle;

    /**
     * @brief Mutex for the list of connected nodes.
     */
    SemaphoreHandle_t _connectedNodesListMutex;

    /**
     * @brief List of the nodes connected to this device in configured as an access point.
     */
    std::map<uint64_t, uint32_t> _connectedNodes;

    /*
     *  Private methods.
     */

    /**
     * @brief Get the _instance object
     * 
     * @return WiFiRequestHandler* 
     */
    static WiFiRequestHandler *GetInstance();

    /**
     *  @brief This method will dispatch incoming messages to the appropriate request handler.
     */
    void DispatchRequest(Message *request) override;

    /**
     *  @brief Instance helper method for the static EventHandler method.
     */
    void EventHandlerHelper(esp_event_base_t eventBase, int32_t eventId, void *eventData);

    /**
     *  @brief Lock the _pollRequests object.
     */
    static inline void LockPollRequests();

    /**
     *  @brief Unlock the _pollRequests object.
     */
    static inline void UnlockPollRequests();

    /**
     *  @brief Clear the _ipInformation and _disconnectData structures.
     */
    static void ClearIpAndDisconnectData();

    /**
     *  Process the request from the managed code to change the antenna.
     */
    void SetAntennaRequest(Message *);

    /**
     *  @brief Copy the IP and access point structure into the connection event data structure.
     */
    void PopulateConnectionEventDataStructure(Esp32Messaging::ConnectEventData *ed);

    /**
     *  @brief Raise the ConnectToAccessPointEvent for the managed code.
     */
    void RaiseConnectToAccessPointEvent(StatusCodes::StatusCodes);

    /**
     *  @brief Raise the ErrorEvent for the managed code.
     */
    void RaiseErrorEvent(StatusCodes::StatusCodes statusCode, uint8_t *payload, uint32_t payloadLength);

    /**
     *  @brief Start the network interface ready for the device to be configured
     *  as either an access point (softAP) or as a network device (station).
     */
    esp_err_t StartWiFiInterface(const wifi_mode_t mode, const Esp32Messaging::AccessPointInformation *accessPointInformation);

    /**
     *  @brief Stop the WiFi Adapter.
     * 
     *  This request could come from an explicit stop request of from a disconnect request.
     */
    StatusCodes::StatusCodes StopWiFIAdapter();

    /**
     *  @brief Disconnect from any access points.
     */
    StatusCodes::StatusCodes DisconnectFromAccessPoint();

    /**
     *  @brief Disconnect from the current access point.
     */
    void DisconnectFromAccessPoint(Message *message);

    /**
     *  @brief Connect to the specified access point starting the network interface if necessary.
     */
    StatusCodes::StatusCodes ConnectToAccessPoint(const Esp32Messaging::AccessPointInformation *accessPointInformation);

    /**
     *  @brief Connect to an access point.
     */
    void ConnectToAccessPoint(Message *message);

    /**
     *  @brief Connect to the default access point.
     */
    void ConnectToDefaultAccessPoint(Message *message);

    /**
     *  @brief Process the result of the connect to access point request.
     */
    void ProcessConnectionResult(Message *, StatusCodes::StatusCodes, bool);

    /**
     *  @brief Start the access point.
     */
    void StartAccessPoint(Message *message);

    /**
     *  @brief Stop the access point if it is running.
     */
    void StopAccessPoint(Message *message);

    /**
     * @brief Extract and validate the access point information.
     * 
     * @return Esp32Messaging::AccessPointInformation* 
     */
    Esp32Messaging::AccessPointInformation *ExtractAndValidateAccessPointInformation(uint8_t *);

    /**
     * @brief Raise a node connect / disconnect event for the C# code.
     */
    void RaiseNodeConnectionChangeEvent(WiFiFunction::WiFiFunction event_type, Esp32Messaging::NodeConnectionChangeEventData *data);

    /**
     *  @brief Get a list of available access points.
     */
    void ScanForAccessPoints(Message *message);

    /**
     *  @brief Clear the default access point and password from NVS.
     */
    void ClearDefaultAccessPointAndPassword(Message *message);

    /**
     *  @brief Convert the Antenna enum representation of an antenna into a GPIO pin number.
     */
    static gpio_num_t ConvertAntennaToPin(AntennaTypes::AntennaTypes antenna);

    /**
     *  @brief Change the current antenna and activate it.
     */
    static void SwitchAntenna(AntennaTypes::AntennaTypes newAntenna);

    /**
     *  @brief Private constructor for the class.
     */
    WiFiRequestHandler();

    /**
     *  @brief Private destructor for the class.
     */
    ~WiFiRequestHandler();

    /**
     *  @brief Process the station started event (WIFI_EVENT_STA_START).
     */
    void StationStartedEvent();

    /**
     *  @brief Process the station stopped event (WIFI_EVENT_STA_STOP).
     */
    void StationStoppedEvent();

    /**
     *  @brief Process the access point stopped event (WIFI_EVENT_AP_START).
     */
    void  AccessPointStartedEvent();

    /**
     *  @brief Process the access point stopped event (WIFI_EVENT_AP_STOP).
     */
    void AccessPointStoppedEvent();

    /**
     *  @brief Process the WiFi connecting and getting an IP address event
     */
    void StationGotIpEvent(ip_event_got_ip_t *);

    /**
     *  @brief Process the station connected to access point event (WIFI_EVENT_STA_CONNECTED).
     */
    void StationConnectedEvent(wifi_event_sta_connected_t *);

    /**
     *  @brief Processes the station disconnected event (WIFI_EVENT_STA_DISCONNECTED).
     */
    void StationDisconnectedEvent(wifi_event_sta_disconnected_t *);

    /**
     *  @brief Process the change in authentication mode event (WIFI_EVENT_STA_AUTHMODE_CHANGE).
     */
    void AuthenticationModeChangedEvent(wifi_event_sta_authmode_change_t *);

    /**
     * @brief Process the node connecting to access point event.
     */
    void NodeConnectedEvent();

    /**
     * @brief Processes the node disconnecting from the access point event.
     */
    void NodeDisconnectedEvent(wifi_event_ap_stadisconnected_t *);

    /**
     * @brief Lock the _connectedNodes object.
     */
    void LockConnectedNodesMap();

    /**
     * @brief Unlock the _connectedNodes object.
     */
    void UnlockConnectedNodesMap();

    /**
     * @brief Extract an encoded integer setsockopt option value.
     */
    static uint8_t *ExtractIntegerSocketOptionValue(uint8_t *, socklen_t *);

    /**
     * @brief Setup the socket value and length for calls to SetSockOpt.
     */
    static uint8_t *GetSocketOptionValue(int, uint8_t *, socklen_t *);

    /**
     * @brief Setup the TCP value and length for calls to SetSockOpt.
     */
    static uint8_t *GetTcpOptionValue(int, uint8_t *, socklen_t *);

    //----------------------------------------------------------------------
    //
    //                      POSIX Calls start here.
    
    /**
     *  @brief Provide a mechanism for the STM32 to call the getaddrinfo method.
     */
    static void GetAddrInfo(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the socket method.
     */
    static void Socket(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the connect method.
     */
    static void Connect(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the freeaddrinfo method.
     */
    static void FreeAddrInfo(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the write method.
     */
    static void Write(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the setsockopt method.
     */
    static void SetSockOpt(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the getsockopt method.
     */
    static void GetSockOpt(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the read method.
     */
    static void Read(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the close method.
     */
    static void Close(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the sendto method.
     */
    static void SendTo(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the send method.
     */
    static void Send(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the recvfrom method.
     */
    static void RecvFrom(void *vpMessage);

    /**
     *  @brief Setup a poll request an assign it to a thread in the thread pool.
     */
    static int PollSetup(void *vpMessage);

    /**
     *  @brief Teardown a poll request that has previously been setup by PollSetup.
     */
    static int PollTeardown(void *pvMessage);

    /**
     *  @brief Execute the actual request to poll a socket.
     */
    static void PollSocket(void *pvMessage);

    /**
     *  @brief Open a dummy socket to be used by the Poll methods to terminate a poll request.
     */
    static int OpenDummySocket();

    /**
     *  @brief Debug method to dump all of the active poll requests.
     */
    static void DumpActivePollRequests();

    /**
     *  @brief Provide a mechanism for the STM32 to call the poll method.
     */
    static void Poll(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the bind method.
     */
    static void Bind(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the listen method.
     */
    static void Listen(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the accept method.
     */
    static void Accept(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the simulated ioctl method.
     */
    static void Ioctl(void *vpMessage);

    /**
     *  @brief Provide a mechanism to call the getsockname or getpeername.
     */
    static void GetSockPeerName(void *vpMessage, WiFiFunction::WiFiFunction request);

    /**
     *  @brief Provide a mechanism for the STM32 to call the simulated getsockname method.
     */
    static void GetSockName(void *vpMessage);

    /**
     *  @brief Provide a mechanism for the STM32 to call the simulated getpeername method.
     */
    static void GetPeerName(void *vpMessage);

    /**
     *  @brief Method called when the time server has supplied updated time information.
     */
    static void NtpServerCallback(struct timeval *tv);

    /**
     *  @brief Indicate if we have a valid WiFi connection.
     */
    static bool IsConnected();

    /**
     *  @brief Map the errno variable from an ESP to a STM32 value.
     */
    static int32_t MapErrno(int32_t);

public:
    /**
     *  @brief Name of the component / task for the WiFi system.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Constructor for the WiFi object.
     */
    explicit WiFiRequestHandler(IMessageDispatcher *messageDispatcher);

    /**
     *  @brief Perform and class level setup required before an instance can be used.
     */
    void Setup();

    /**
     *  @brief This is the event handler called by FreeRTOS.
     * 
     *  It merely passes the event on to an internal private event handler
     *  within the class.
     */
    static void EventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData);

    /**
     * @brief Prepare the WiFi interface for deep sleep.
     */
    static StatusCodes::StatusCodes PrepareForDeepSleep();

    /**
     *  @brief Get / Set the maximum number of retry attempts.
     */
    static uint32_t GetMaximumRetryCount();
    static StatusCodes::StatusCodes SetMaximumRetryCount(uint32_t value);

    /**
     *  @brief Get / Set the automatic reconnect property.
     */
    static bool GetAutomaticReconnect();
    static StatusCodes::StatusCodes SetAutomaticReconnect(bool value);

    /**
     *  @brief Get / Set the automatically start network property.
     */
    static bool GetAutomaticallyStartNetwork();
    static StatusCodes::StatusCodes SetAutomaticallyStartNetwork(bool value);

    /**
     *  @brief Get / Set the get time a startup configuration value.
     */
    static bool GetUseDhcp();
    static StatusCodes::StatusCodes SetUseDhcp(bool value);

    /**
     *  @brief Get / Set the name of the default access point to be used when
     *  the board starts.
     */
    static char *GetDefaultAccessPoint();
    static StatusCodes::StatusCodes SetDefaultAccessPoint(char *value);

    /**
     *  @brief Get / Set the password for the default access point.
     */
    static char *GetPassword();
    static StatusCodes::StatusCodes SetPassword(char *value);

    /**
     *  @brief Get / set the default IP address when not using DHCP.
     */
    static uint32_t GetIpAddress();
    static StatusCodes::StatusCodes SetIpAddress(uint32_t value);

    /**
     *  @brief Get / set the IP address of the DNS server to be used when not using DHCP.
     */
    static uint32_t GetDnsServer();
    static StatusCodes::StatusCodes SetDnsServer(uint32_t value);

    /**
     *  @brief Get / set the IP address of the default gateway.
     */
    static uint32_t GetDefaultGateway();
    static StatusCodes::StatusCodes SetDefaultGateway(uint32_t value);

    /**
     *  @brief Get / Set the MAC address of the interface.
     */
    static void GetMacAddress(uint8_t *macAddress, wifi_interface_t interface);
    static StatusCodes::StatusCodes SetMacAddress(uint8_t *macAddress, wifi_interface_t interface);

    /**
     *  @brief Get / Set the current antenna in use.
     */
    static uint8_t GetAntenna();
    static StatusCodes::StatusCodes SetAntenna(uint8_t antenna);

    /**
     *  @brief Get / Set the WiFi timeout value.
     */
    static uint32_t GetWiFiTimeout();
    static StatusCodes::StatusCodes SetWiFiTimeout(uint32_t value);
};

#endif /* _WIFI_REQUEST_HANDLER_HPP_ */
