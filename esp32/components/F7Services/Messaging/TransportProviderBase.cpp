/*
 *  TransportProviderBase.cpp
 *
 *  Base class for any hardware transport provider which facilitates
 *  communication between the ESP32 and STM32 chips.
 */

#include "sdkconfig.h"

#include "driver/gpio.h"

#include "TransportProviderBase.hpp"
#include "Encoders.hpp"
#include "Logging.hpp"

/*
 * ----------------------------------------------------------------------------
 *
 *               Initialise class level static variables.
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Name of this component / task.
 */
const char *TransportProviderBase::COMPONENT_NAME = "TransportProviderBase";

QueueHandle_t TransportProviderBase::_outboundMessageQueueMutex = nullptr;
QueueHandle_t TransportProviderBase::_outboundMessageQueueHandle = nullptr;
TaskHandle_t TransportProviderBase::_transportProviderTaskHandle = nullptr;

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
TransportProviderBase::TransportProviderBase()
{
    _outboundMessageQueueMutex = xSemaphoreCreateMutex();
    if (!_outboundMessageQueueMutex)
    {
        ERROR_MESSAGE("Cannot create outbound message queue mutex.");
    }
}

/**
 *  @brief Default destructor.
 */
TransportProviderBase::~TransportProviderBase()
{
}

/*
 * ----------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ----------------------------------------------------------------------------
 */

/**
 *  @brief Setup the hardware transport provider.
 */
void TransportProviderBase::Setup(MessageDispatcher *messageDispatcher)
{
    _messageDispatcher = messageDispatcher;
    _outboundMessageQueueHandle = xQueueCreate(MAX_QUEUE_LENGTH, sizeof(Message *));
    if (_outboundMessageQueueHandle == NULL)
    {
        ERROR_MESSAGE("Cannot allocate hardware transport queue.");
    }
}

/**
 *  @brief Get the number of messages in the outbound message queue.
 *
 *  @returns
 *      Number of messages in the outbound message queue.  This
 *      method will return -1 if the number of messages cannot be
 *      obtained.
 */
int TransportProviderBase::OutboundMessageQueueLength()
{
    int result = -1;
    if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
    {
        UBaseType_t messageCount = uxQueueMessagesWaiting(_outboundMessageQueueHandle);
        xSemaphoreGive(_outboundMessageQueueMutex);
        result = (int) messageCount;
    }
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
Message *TransportProviderBase::GetOutboundMessage()
{
    Message *result = nullptr;
    // UBaseType_t messageCount = OutboundMessageQueueLength();
    if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
    {
        if (uxQueueMessagesWaiting(_outboundMessageQueueHandle) > 0)
        {
            if (xQueueReceive(_outboundMessageQueueHandle, (void *) &result, portMAX_DELAY) != pdTRUE)
            {
                result = nullptr;
            }
        }
        xSemaphoreGive(_outboundMessageQueueMutex);
    }

    return(result);
}

/**
 *  @brief Dump the message at the head of the message queue (if there is one)
 *  to as LOGI informaiton.
 *
 *  This method is intended for debugging.
 *
 *  @return
 *      None.
 */
void TransportProviderBase::DumpMessageAtHeadOfQueue()
{
    Message *message;
    if (xQueuePeek( _outboundMessageQueueHandle, &( message ), ( TickType_t ) 10 ))
    {
        Logging::DumpMessage(COMPONENT_NAME, message);
    }
    else
    {
        TRACE_MESSAGE("Message queue is empty.");
    }
}

/**
 *  @brief Add the message to the queue of messages waiting to be sent to
 *  the STM32.
 *
 *  @param message
 *      Message to be sent to the STM32.
 *
 *  @return
 *      true if the message was queued, false otherwise.
 */
bool TransportProviderBase::QueueMessageForStm32(Message *message)
{
    bool result = true;

    if (xSemaphoreTake(_outboundMessageQueueMutex, portMAX_DELAY))
    {
        if (xQueueSendToBack(_outboundMessageQueueHandle, &message, 0) != pdPASS)
        {
            ERROR_MESSAGE("Cannot queue message.");
            result = false;
        }
        else
        {
            Logging::DumpMessage(COMPONENT_NAME, message);
            ToggleEsp32MessageWaitingPin();
        }
        xSemaphoreGive(_outboundMessageQueueMutex);
    }
    return(result);
}

/**
 *  @brief Toggle the "Message waiting" GPIO pin connected to the STM32.
 *
 *  There may be a problem with this method if the clock frequency on the ESP32
 *  is increased to 240 MHz.  It appears that the messages going to the STM32
 *  stop being received.
 */
void TransportProviderBase::ToggleEsp32MessageWaitingPin()
{
    gpio_set_level(Gpio::ESP32_MESSAGE_WAITING_PIN, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(Gpio::ESP32_MESSAGE_WAITING_PIN, 0);
}
