/*
 *  MessageDispatcher.cpp
 *
 *  Implement the methods that will allow the transmission and reception
 *  of messages between the ESP32 and the STM32.
 */

#include "sdkconfig.h"

#include "Logging.hpp"
#include "MessageDispatcher.hpp"

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
const char *MessageDispatcher::COMPONENT_NAME = "MessageDispatcher";

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
MessageDispatcher::MessageDispatcher() : _messageIdMutex(xSemaphoreCreateMutex())
{
}

/**
 *  @brief Create a new instance of the message dispatcher with an associated transport provider.
 *
 *  @param transportProvider
 *      Hardware level transport provider that will deliver and receive the messages.
 */
MessageDispatcher::MessageDispatcher(TransportProviderBase *transportProvider) : MessageDispatcher()
{
    _transportProvider = transportProvider;
    _transportProvider->Setup(this);
    //
    //  Instantiate all of the request handlers.
    //
    _wifiRequestHandler = new WiFiRequestHandler(this);     // cppcheck-suppress noCopyConstructor
    _wifiRequestHandler->Setup();
    _bluetoothRequestHandler = BluetoothRequestHandler::GetInstance(this);
    _meshNetworkRequestHandler = new MeshNetworkRequestHandler(this);
    _meshNetworkRequestHandler->Setup();
    _systemRequestHandler = new SystemRequestHandler(this);
    _systemRequestHandler->Setup();
}

/**
 *  @brief Default destructor.
 */
MessageDispatcher::~MessageDispatcher()
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
bool MessageDispatcher::QueueMessageForEsp32(Message *request)
{
    TRACE_MESSAGE("QueueMessageForEsp32: Enter");
    RequestHandlerBase *provider = nullptr;

    #if defined(PERFORMANCE_LOGGING)
    request->PerformanceLoggingStart = esp_timer_get_time();
    #endif
    switch (static_cast<Esp32Interfaces::Esp32Interfaces>(request->Interface))
    {
        case Esp32Interfaces::WiFi:
            provider = dynamic_cast<RequestHandlerBase *>(_wifiRequestHandler);
            break;
        case Esp32Interfaces::BlueTooth:
            provider = dynamic_cast<RequestHandlerBase *>(_bluetoothRequestHandler);
            break;
        case Esp32Interfaces::MeshNetwork:
            provider = dynamic_cast<RequestHandlerBase *>(_meshNetworkRequestHandler);
            break;
        case Esp32Interfaces::System:
            provider = dynamic_cast<RequestHandlerBase *>(_systemRequestHandler);
            break;
        default:
            DEBUG_MESSAGE("QueueMessageForEsp32: Unknown interface %d, message will not be processed.", request->Interface);
            Message::DeleteMessage(request);
            break;
    }
    bool result = false;
    if (provider)
    {
        result = provider->QueueEsp32Message(request);
    }
    TRACE_MESSAGE("QueueMessageForEsp32: Exit");
    return(result);
}

/**
 *  @brief Add the response from the system to the hardware transport provider
 *  queue of messages waiting to be sent to the STM32.
 *
 *  @param response
 *      Response from the ESP32 to a request from the STM32.
 *
 *  @return
 *      True if the response was queued correctly, false otherwise.
 */
bool MessageDispatcher::QueueMessageForStm32(Message *response)
{
    return(_transportProvider->QueueMessageForStm32(response));
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
uint32_t MessageDispatcher::GetNextMessageID()
{
    xSemaphoreTake(_messageIdMutex, portMAX_DELAY);
    _lastMessageId++;
    uint32_t messageId = _lastMessageId;
    xSemaphoreGive(_messageIdMutex);
    messageId &= ~ESP32_MESSAGE_ID_MASK;
    return(messageId);
}
