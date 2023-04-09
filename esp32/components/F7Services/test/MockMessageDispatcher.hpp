/*
 *  MessageDispatcher.hpp
 *
 *  Message dispatcher layer.  This object separates the need for the
 *  ESP32 interface objects to know about the hardware layer used to
 *  send messages between the ESP32 and the STM32.
 */

#ifndef _MOCK_MESSAGE_DISPATCHER_HPP_
#define _MOCK_MESSAGE_DISPATCHER_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "TransportProviderBase.hpp"
#include "SharedEnums.hpp"
#include "Esp32Messaging.hpp"
#include "IMessageDispatcher.hpp"

/**
 *  @brief Message dispatcher class.
 *
 *  This class acts as a broker between the ESP32 and the STM32
 *  message transmit / receive mechanisms.
 */
class MockMessageDispatcher : public IMessageDispatcher
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
     *  @brief Queue handle for the messages to be sent to the STM32.
     */
    static QueueHandle_t _outboundMessageQueueHandle;

    /**
     *  @brief Hardware transport provider object that is connected to
     *  the STM32.
     */
    TransportProviderBase *_transportProvider = nullptr;

    /**
     *  @brief Maximum length of the message queue.
     */
    const uint32_t MAX_QUEUE_LENGTH = 10;

    /**
     *  @brief Last message ID that was granted by the system.  This always defaults
     *  to 0 as it will be incremented before a new ID is granted.
     */
    uint32_t _lastMessageId = 0;

    /**
     *  @brief Constructor for the Message Dispatcher.
     */
    MockMessageDispatcher();

    /**
     *  @brief Destructor for the Message Dispatcher.
     */
    ~MockMessageDispatcher();

public:
    /**
     *  @brief Name of the message dispatcher task.
     */
    static const char *COMPONENT_NAME;

    /**
     *  @brief Constructor for the Message Dispatcher.
     */
    explicit MockMessageDispatcher(TransportProviderBase *transportProvider);

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

    /**
     *  @brief Get the number of messages in the outbound message queue.
     */
    int OutboundMessageQueueLength();

    /**
     *  @brief Get the next message from the outbound message queue.
     */
    Message *GetOutboundMessage();
};

#endif /* _MOCK_MESSAGE_DISPATCHER_HPP_ */
