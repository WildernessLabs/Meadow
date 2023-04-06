/*
 *  MessageDispatcher.hpp
 *
 *  Message dispatcher layer.  This object separates the need for the
 *  ESP32 interface objects to know about the hardware layer used to
 *  send messages between the ESP32 and the STM32.
 */

#ifndef _MESSAGE_DISPATCHER_HPP_
#define _MESSAGE_DISPATCHER_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "WiFiRequestHandler.hpp"
#include "SystemRequestHandler.hpp"
#include "BluetoothRequestHandler.hpp"
#include "MeshNetworkRequestHandler.hpp"
#include "TransportProviderBase.hpp"
#include "SharedEnums.hpp"
#include "Esp32Messaging.hpp"
#include "IMessageDispatcher.hpp"

/*
 *  Forward declarations required by the compiler.
 */
class WiFiRequestHandler;
class MeshNetworkRequestHandler;
class SystemRequestHandler;
class BluetoothRequestHandler;

/**
 *  @brief Message dispatcher class.
 *
 *  This class acts as a broker between the ESP32 and the STM32
 *  message transmit / receive mechanisms.
 */
class MessageDispatcher : public IMessageDispatcher
{
private:
    /**
     *  @brief Mask for the message IDs for outbound messages.
     *
     *  This mask is applied to message IDs to make sure that they are all
     *  above 0x7fffffff.  This identifies them as originating from the
     *  ESP32 rather than the STM32.
     */
    const uint32_t ESP32_MESSAGE_ID_MASK = 0x80000000;

    /**
     *  @brief WiFi object that will process WiFi messages.
     */
    WiFiRequestHandler *_wifiRequestHandler = nullptr;

    /**
     *  @brief Mesh Network object that will process Mesh Network messages.
     */
    MeshNetworkRequestHandler *_meshNetworkRequestHandler = nullptr;

    /**
     *  @brief System object that will process System messages.
     */
    SystemRequestHandler *_systemRequestHandler = nullptr;

    /**
     *  @brief Bluetooth object that will process Bluetooth messages.
     */
    BluetoothRequestHandler *_bluetoothRequestHandler = nullptr;

    /**
     *  @brief Hardware transport provider object that is connected to
     *  the STM32.
     */
    TransportProviderBase *_transportProvider = nullptr;

    /**
     *  @brief This mutex ensures that one one task can ask for a new message ID
     *  at any time.
     */
    SemaphoreHandle_t _messageIdMutex;

    /**
     *  @brief Last message ID that was granted by the system.  This always defaults
     *  to 0 as it will be incremented before a new ID is granted.
     */
    uint32_t _lastMessageId = 0;

    /**
     *  @brief Constructor for the Message Dispatcher.
     */
    MessageDispatcher();

    /**
     *  @brief Destructor for the Message Dispatcher.
     */
    ~MessageDispatcher();

public:
    /**
     *  @brief Name of the message dispatcher task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Constructor for the Message Dispatcher.
     */
    explicit MessageDispatcher(TransportProviderBase *transportProvider);

    /**
     *  @brief Queue a message for the ESP32.
     */
    bool QueueMessageForEsp32(Message *request);

    /**
     *  @brief Queue a message for the STM32.
     */
    bool QueueMessageForStm32(Message *response);

    /**
     *  @brief Get the next message ID.
     */
    uint32_t GetNextMessageID();
};

#endif /* _MESSAGE_DISPATCHER_HPP_ */
