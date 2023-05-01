/*
 *  TransportProviderBase.hpp
 *
 *  This class is a base class for the hardware level data transmit / receive
 *  hardware used to connect the ESP32 and the STM32 chips.
 */

#ifndef _TRANSPORT_PROVIDER_BASE_HPP_
#define _TRANSPORT_PROVIDER_BASE_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Esp32Messaging.hpp"

/*
 *  Forward reference for the MessageDispatcher class.  This is needed
 *  here to get around compiler errors if the MessageDispatcher.hpp
 *  file is included.
 */
class MessageDispatcher;

/**
 *  @brief Base class for the message system transport provider.
 */
class TransportProviderBase
{
public:
    /**
     *  @brief Setup the transport provider.
     */
    void Setup(MessageDispatcher *messageDispatcher);

    /**
     *  @brief Add the message given to the queue of messages waiting
     *  for transmission to the STM32.
     */
    bool QueueMessageForStm32(Message *message);

    /**
     *  @brief Bit that will be used to indicate that a the STM32 has just
     *  finished sending a message to the ESP32.
     */
    static const uint32_t STM32_INTERRUPT_NOTIFICATION_BIT = 0x00000001;

private:
    /**
     *  @brief Maximum length of the message queue.
     */
    const uint32_t MAX_QUEUE_LENGTH = 10;

protected:
    /**
     *  @brief Message dispatcher object used to send messages to the
     *  various interfaces on the ESP32.
     */
    MessageDispatcher *_messageDispatcher = nullptr;

    /**
     *  @brief Queue handle for the messages to be sent to the STM32.
     */
    static QueueHandle_t _outboundMessageQueueHandle;

    /**
     *  @brief Mutex for the outbound message queue.
     */
    static SemaphoreHandle_t _outboundMessageQueueMutex;

    /**
     *  @brief Task handle for the FreeRTOS task that will deal with the
     *  process of sending and receiving messages between the ESP32
     *  and the STM32 chips.
     */
    static TaskHandle_t _transportProviderTaskHandle;

    /**
     *  @brief Default constructor.
     */
    TransportProviderBase();

    /**
     *  @brief Default destructor.
     */
    ~TransportProviderBase();

    /**
     *  @brief Get the number of messages in the outbound message queue.
     */
    int OutboundMessageQueueLength();

    /**
     *  @brief Get the next message from the outbound message queue.
     */
    Message *GetOutboundMessage();

    /**
     *  @brief Toggle the GPIO pin that is connected to the STM32.
     */
    void ToggleEsp32MessageWaitingPin();

    /**
     *  @brief Method used for debugging, this will dump the message at the head
     *  of the message queue as LOGI informaiton.
     */
    void DumpMessageAtHeadOfQueue();

public:
    /**
     *  @brief Name of this component / task.
     */
    static const char *COMPONENT_NAME;
};

#endif /* _TRANSPORT_PROVIDER_BASE_HPP_ */
