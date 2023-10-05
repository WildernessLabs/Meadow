/*
 *  AccessPoint.cpp
 *
 *  Implement the access point functionality (the board acting as an access point).
 */
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

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Lock the _connectedNodes object.
 */
void WiFiRequestHandler::LockConnectedNodesMap()
{
     xSemaphoreTake(_connectedNodesListMutex, portMAX_DELAY);
}

/**
 *  @brief Unlock the _connectedNodes object.
 */
void WiFiRequestHandler::UnlockConnectedNodesMap()
{
     xSemaphoreGive(_connectedNodesListMutex);
}

/**
 * @brief Extract and validate the access point information.
 * 
 * @param data
 *      Pointer to a block of memory holding the encoded access point information. 
 * 
 * @return Esp32Messaging::AccessPointInformation* 
 *      Pointer to validated access point information, nullptr if there is a problem.
 */
Esp32Messaging::AccessPointInformation *WiFiRequestHandler::ExtractAndValidateAccessPointInformation(uint8_t *data)
{
    Esp32Messaging::AccessPointInformation *result = nullptr;

    if (data)
    {
        result = Encoders::ExtractAccessPointInformation(data);
        if (result)
        {
            bool error = (!result->NetworkName) || (strlen(result->NetworkName) == 0) || (strlen(result->NetworkName) > MAXIMUM_SSID_LENGTH);
            error |= (!result->Password) || (strlen(result->Password) > MAXIMUM_PASSWORD_LENGTH);
            // error |= (result->IpAddress == 0) || (result->SubnetMask == 0) || (result->Gateway == 0);
            if (error)
            {
                vPortFree(result);
                result = nullptr;
            }
        }
    }
    
    return(result);
}

/**
 *  @brief Start the access point.
 * 
 *  @param message
 *      Message from the C# application containing information about the access point configuration.
 */
void WiFiRequestHandler::StartAccessPoint(Message *message)
{
    TRACE_MESSAGE("StartAccessPoint: Enter");

    // StatusCodes::StatusCodes result = StatusCodes::CompletedOk;

    // Esp32Messaging::AccessPointInformation *accessPointInformation = ExtractAndValidateAccessPointInformation(message->Payload);
    // message->DeletePayload();

    // if (accessPointInformation)
    // {
    //     xEventGroupClearBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT | WIFI_ERROR_BIT);
    //     EventBits_t bits = xEventGroupGetBits(_xWiFiEventGroup);
    //     uint32_t ipAddress = accessPointInformation->IpAddress;
    //     uint32_t subnetMask = accessPointInformation->SubnetMask;
    //     uint32_t gateway = accessPointInformation->Gateway;
    //     if (!(bits & WIFI_INTERFACE_STARTED_BIT))
    //     {
    //         accessPointInformation->IpAddress = 0;
    //         accessPointInformation->SubnetMask = 0;
    //         accessPointInformation->Gateway = 0;
    //         if (StartWiFiInterface(WIFI_MODE_APSTA, accessPointInformation) != ESP_OK)
    //         {
    //             result = StatusCodes::CannotStartNetworkInterface;
    //         }
    //         else
    //         {
    //             bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_INTERFACE_STARTED_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
    //             if ((bits & WIFI_INTERFACE_STARTED_BIT) == 0)
    //             {
    //                 result = StatusCodes::CannotStartNetworkInterface;
    //             }
    //             else
    //             {
    //                 TRACE_MESSAGE("StartAccessPoint: WiFi interface started OK.");
    //             }
    //         }
    //     }
    //     if (result == StatusCodes::CompletedOk)
    //     {
    //         TRACE_MESSAGE("StartAccessPoint: Creating access point: %s, password %s", accessPointInformation->NetworkName, accessPointInformation->Password);
    //         wifi_config_t config = { };
    //         config.ap.ssid_len = strlen(accessPointInformation->NetworkName);
    //         memcpy(config.ap.ssid, accessPointInformation->NetworkName, strlen(accessPointInformation->NetworkName));
    //         memcpy(config.ap.password, accessPointInformation->Password, strlen(accessPointInformation->Password));
    //         config.ap.max_connection = ESP_WIFI_MAX_CONN_NUM;
    //         config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    //         esp_err_t error = esp_wifi_set_config(ESP_IF_WIFI_AP, &config);
    //         if (error != ESP_OK)
    //         {
    //             TRACE_MESSAGE("StartAccessPoint: Setting access point configuration returned 0x%x (%s)", error, esp_err_to_name(error));
    //             result = StatusCodes::CannotStartAccessPoint;
    //         }
    //         else
    //         {
    //             if ((ipAddress != 0) && (gateway != 0) && (subnetMask != 0))
    //             {
    //                 if (esp_netif_dhcps_stop(_softApNetIfHandle) == ESP_OK)
    //                 {
    //                     char ip[INET_ADDRSTRLEN], sn[INET_ADDRSTRLEN], gw[INET_ADDRSTRLEN];
    //                     strcpy(ip, inet_ntoa(ipAddress));
    //                     strcpy(sn, inet_ntoa(subnetMask));
    //                     strcpy(gw, inet_ntoa(gateway));
    //                     TRACE_MESSAGE("StartAccessPoint: Configuring access point DHCP server, IP: %s, subnet mask %s, default gateway %s", ip, sn, gw);

    //                     esp_netif_ip_info_t ip_info;
    //                     ip_info.ip.addr = ipAddress;
    //                     ip_info.gw.addr = gateway;
    //                     ip_info.netmask.addr = subnetMask;
    //                     if (esp_netif_set_ip_info(_softApNetIfHandle, &ip_info) != ESP_OK)
    //                     {
    //                         result = StatusCodes::DhcpConfigurationError;
    //                     }
    //                     else
    //                     {
    //                         if (esp_netif_dhcps_start(_softApNetIfHandle) != ESP_OK)
    //                         {
    //                             result = StatusCodes::DhcpConfigurationError;
    //                         }
    //                     }
    //                 }
    //                 else
    //                 {
    //                     result = StatusCodes::DhcpConfigurationError;
    //                 }
    //             }
    //             bits = xEventGroupWaitBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT | WIFI_ERROR_BIT, pdFALSE, pdFALSE, GetWiFiTimeout() / portTICK_PERIOD_MS);
    //             if ((bits & WIFI_ACCESS_POINT_STARTED_BIT) == 0)
    //             {
    //                 result = StatusCodes::Timeout;
    //                 xEventGroupClearBits(_xWiFiEventGroup, WIFI_ERROR_BIT);
    //             }
    //         }
    //     }
    //     vPortFree(accessPointInformation);
    // }
    // else
    // {
    //     result = StatusCodes::CannotStartAccessPoint;
    // }

    message->DeletePayload();

    message->PayloadLength = 0;
    message->Payload = nullptr;
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::NotImplemented;
    Logging::DumpMessage(COMPONENT_NAME, message);

    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    TRACE_MESSAGE("StartAccessPoint: Exit");
}

/**
 *  @brief Stop the access point if it is running.
 *  
 *  @param message
 *      Message from the C# application requesting that the access point is stopped.
  */
void WiFiRequestHandler::StopAccessPoint(Message *message)
{
    TRACE_MESSAGE("StopAccessPoint: Enter");

    // wifi_config_t config = { };
    // StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    // if (esp_wifi_set_config(ESP_IF_WIFI_AP, &config) != ESP_OK)
    // {
    //     result = StatusCodes::Failure;
    // }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::NotImplemented;
    Logging::DumpMessage(COMPONENT_NAME, message);

    _messageDispatcher->QueueMessageForStm32(message);
    xEventGroupSetBits(_xWiFiEventGroup, WIFI_READY_FOR_NEXT_COMMAND_BIT);

    TRACE_MESSAGE("StopAccessPoint: Exit");
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Event Handlers
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Process the access point started event.
 */
void WiFiRequestHandler::AccessPointStartedEvent()
{
    TRACE_MESSAGE("AccessPointStartedEvent: Enter");

    StatusCodes::StatusCodes result = StatusCodes::CompletedOk;
    uint8_t *payload = nullptr;
    uint32_t payloadLength = 0;

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(_softApNetIfHandle, &ip_info) == ESP_OK)
    {
        Esp32Messaging::AccessPointInformation info = { };
        info.IpAddress = ip_info.ip.addr;
        info.SubnetMask = ip_info.netmask.addr;
        info.Gateway = ip_info.gw.addr;
        payloadLength = Encoders::EncodedAccessPointInformationBufferSize(&info);
        payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        if (payload)
        {
            Encoders::EncodeAccessPointInformation(&info, payload);
        }
        else
        {
            payload = nullptr;
            payloadLength = 0;
            result = StatusCodes::Failure;
        }
    }

    Message *message = Message::CreateOnHeap(MessageTypes::Event, Esp32Interfaces::WiFi, WiFiFunction::AccessPointStartedEvent, result, _messageDispatcher->GetNextMessageID(), payload, payloadLength);
    _messageDispatcher->QueueMessageForStm32(message);

    xEventGroupSetBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT);

    TRACE_MESSAGE("AccessPointStartedEvent: Exit");
}

/**
 *  @brief Process the access point stopped event.
 */
void WiFiRequestHandler::AccessPointStoppedEvent()
{
    TRACE_MESSAGE("AccessPointStoppedEvent: Enter");

    LockConnectedNodesMap();
    _connectedNodes.clear();
    UnlockConnectedNodesMap();

    Message *message = Message::CreateOnHeap(MessageTypes::Event, Esp32Interfaces::WiFi, WiFiFunction::AccessPointStoppedEvent, StatusCodes::CompletedOk, _messageDispatcher->GetNextMessageID(), nullptr, 0);
    _messageDispatcher->QueueMessageForStm32(message);

    xEventGroupClearBits(_xWiFiEventGroup, WIFI_ACCESS_POINT_STARTED_BIT);

    TRACE_MESSAGE("AccessPointStoppedEvent: Exit");
}

/**
 * @brief Raise an event for a node connecting ot disconnecting.
 * 
 * @param event_type 
 *      Type of event to raise, NodeConnectedEvent or NodeDisconnectedEvent
 * 
 * @param data 
 *      Information about the node (MAC and possibly IP address).
 */
void WiFiRequestHandler::RaiseNodeConnectionChangeEvent(WiFiFunction::WiFiFunction event_type, Esp32Messaging::NodeConnectionChangeEventData *data)
{
    uint32_t payloadLength = Encoders::EncodedNodeConnectionChangeEventDataBufferSize(data);
    uint8_t *payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
    if (payload)
    {
        Encoders::EncodeNodeConnectionChangeEventData(data, payload);
        Message *message = Message::CreateOnHeap(MessageTypes::Event, Esp32Interfaces::WiFi, event_type, StatusCodes::CompletedOk, _messageDispatcher->GetNextMessageID(), payload, payloadLength);
        _messageDispatcher->QueueMessageForStm32(message);
    }
}

/**
 * @brief Process the node connecting to access point event.
 */
void WiFiRequestHandler::NodeConnectedEvent()
{
    TRACE_MESSAGE("NodeConnectedEvent: Enter");

    // wifi_sta_list_t wifi_sta_list;
    // tcpip_adapter_sta_list_t adapter_sta_list;
    // esp_wifi_ap_get_sta_list(&wifi_sta_list);
    // tcpip_adapter_get_sta_list(&wifi_sta_list, &adapter_sta_list);
    // if (adapter_sta_list.num > 0)
    // {
    //     LockConnectedNodesMap();
    //     for (uint8_t i = 0; i < adapter_sta_list.num; i++)
    //     {
    //         tcpip_adapter_sta_info_t station = adapter_sta_list.sta[i];
    //         uint64_t key = 0;
    //         memcpy(&key, &station.mac, 6);
    //         std::map<uint64_t, uint32_t>::iterator it = _connectedNodes.find(key);
    //         if (it == _connectedNodes.end())
    //         {
    //             _connectedNodes[key] = 0;
    //         }
    //         if (station.ip.addr != _connectedNodes[key])
    //         {
    //             TRACE_MESSAGE("NodeConnectedEvent: Updating IP address for node: MAC " MACSTR " IP Address " IPSTR, MAC2STR(station.mac), IP2STR(&station.ip));
    //             Esp32Messaging::NodeConnectionChangeEventData data;
    //             memcpy(&data.MacAddress, &station.mac, 6);
    //             memcpy(&data.IpAddress, &station.ip, sizeof(uint32_t));
    //             _connectedNodes[key] = data.IpAddress;
    //             RaiseNodeConnectionChangeEvent(WiFiFunction::NodeConnectedEvent, &data);
    //         }
    //     }
    //     UnlockConnectedNodesMap();
    // }
    // else
    // {
    //     TRACE_MESSAGE("NodeConnectedEvent raised but there are no nodes connected to this device.");
    // }

    TRACE_MESSAGE("NodeConnectedEvent: Exit");
}

/**
 * @brief Processes the node disconnecting from the access point event.
 * 
 * @param eventData
 *      Information about the node that has disconnected from the access point.
 */
void WiFiRequestHandler::NodeDisconnectedEvent(wifi_event_ap_stadisconnected_t *eventData)
{
    TRACE_MESSAGE("NodeDisconnectedEvent: Enter");

    uint64_t key = 0;
    memcpy(&key, &eventData->mac, 6);

    LockConnectedNodesMap();
    std::map<uint64_t, uint32_t>::iterator it = _connectedNodes.find(key);
    if (it != _connectedNodes.end())
    {
        // TRACE_MESSAGE("NodeDisconnectedEvent: Found MAC address " MACSTR, MAC2STR(eventData->mac));
        _connectedNodes.erase(it);
        Esp32Messaging::NodeConnectionChangeEventData data = { };
        memcpy(&data.MacAddress, &eventData->mac, 6);
        RaiseNodeConnectionChangeEvent(WiFiFunction::NodeDisconnectedEvent, &data);
    }
    else
    {
        TRACE_MESSAGE("NodeDisconnectedEvent raised but the node could not be found in the connected nodes list.");
    }
    UnlockConnectedNodesMap();

    TRACE_MESSAGE("NodeDisconnectedEvent: Exit");
}
