/*
 *  MockMessageDispatcher.cpp
 *
 *  Implement the methods that will allow the transmission and reception
 *  of messages between the ESP32 and the STM32.
 */

#include "sdkconfig.h"

#include "Logging.hpp"
#include "MockMessageDispatcher.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of the message dispatcher task.
 */
const char *MockMessageDispatcher::COMPONENT_NAME = "MockMessageDispatcher";

/**
 * @brief Handle for the outbound (simulated STM32) message queue.
 */
QueueHandle_t MockMessageDispatcher::_outboundMessageQueueHandle = nullptr;

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor.
 */
MockMessageDispatcher::MockMessageDispatcher()
{
    _outboundMessageQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, sizeof(Message *));
    if (_outboundMessageQueueHandle == NULL)
    {
        ERROR_MESSAGE("Cannot allocate hardware transport queue.");
    }
}

/**
 *  @brief Create a new instance of the message dispatcher with an associated transport provider.
 *
 *  @param transportProvider
 *      Hardware level transport provider that will deliver and receive the messages.
 */
MockMessageDispatcher::MockMessageDispatcher(TransportProviderBase *transportProvider) : MockMessageDispatcher()
{
}

/**
 *  @brief Default destructor.
 */
MockMessageDispatcher::~MockMessageDispatcher()
{
}

/* ---------------------------------------------------------------------------
 *
 *                              Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Add the specified message (request) to the appropriate interface message queue.
 *
 *  @param request
 *      Request from the STM32 to the ESP32.
 *
 *  @return
 *      True if the request could be queue, false otherwise.
 */
bool MockMessageDispatcher::QueueMessageForEsp32(Message *request)
{
    TRACE_MESSAGE("QueueMessageForEsp32: Enter");

    TRACE_MESSAGE("QueueMessageForEsp32: Exit");
    return(true);
}

/**
 *  @brief Add the response from the system to the hardware transport provider
 *  queue of messages waiting to be sent to the STM32.
 *
 *  @param message
 *      Response from the ESP32 to a request from the STM32.
 *
 *  @return
 *      True if the message was queued correctly, false otherwise.
 */
bool MockMessageDispatcher::QueueMessageForStm32(Message *message)
{
    bool result = true;

    if (xQueueSendToBack(_outboundMessageQueueHandle, &message, 0) != pdPASS)
    {
        ERROR_MESSAGE("Cannot queue message.");
        result = false;
    }

    return(result);
}

/**
 *  @brief Get the next message ID that can be used by the ESP32 system to send a
 *  message to the STM32.
 *
 *  This method will increment the last used message ID, apply the message
 *  ID mask and then return the Message ID.
 *
 *  @returns
 *      Next message ID that can be used.
 */
uint32_t MockMessageDispatcher::GetNextMessageID()
{
    _lastMessageId++;
    uint32_t messageId = _lastMessageId;
    messageId &= ~ESP32_MESSAGE_ID_MASK;
    return(messageId);
}


/**
 *  @brief Get the number of messages in the outbound message queue.
 *
 *  @returns
 *      Number of messages in the outbound message queue.  This
 *      method will return -1 if the number of messages cannot be
 *      obtained.
 */
int MockMessageDispatcher::OutboundMessageQueueLength()
{
    int result = -1;

    UBaseType_t messageCount = uxQueueMessagesWaiting(_outboundMessageQueueHandle);
    result = (int) messageCount;

    return(result);
}

/**
 *  @brief Get the next message from the message queue (if there is one available).
 *
 *  @return
 *  Pointer to the message that has been retrieved or nullptr if there
 *  are no messages in the queue or there is a problem getting the next
 *  message.
 */
Message *MockMessageDispatcher::GetOutboundMessage()
{
    Message *result = nullptr;

    if (uxQueueMessagesWaiting(_outboundMessageQueueHandle) > 0)
    {
        if (xQueueReceive(_outboundMessageQueueHandle, (void *) &result, portMAX_DELAY) != pdTRUE)
        {
            result = nullptr;
        }
    }

    return(result);
}
