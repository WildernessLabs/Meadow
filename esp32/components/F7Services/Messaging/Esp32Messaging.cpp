/*
 *  Esp32Messaging.cpp
 *
 *  Generic messaging constants, structures and definitions used
 *  in the messaging system.
 */
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_log.h"
#include <string.h>

#include "Esp32Messaging.hpp"
#include "Logging.hpp"

/*
 * ---------------------------------------------------------------------------
 *
 *                     Constructors and destructor
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Default constructor for a Message structure.
 */
Message::Message()
{
    MessageType = 0;
    Interface = 0;
    Function = 0;
    StatusCode = 0;
    MessageID = 0;
    PacketOffset = 0;
    PacketLength = 0;
    Payload = nullptr;
    PayloadLength = 0;
}

/**
 *  C@brief onstructor for the Message class.
 *
 *  @param messageType
 *      Type of message to send (see PacketType enum).
 *
 *  @param interface
 *      Interface on the ESP that this message should be routed to.
 *
 *  @param function
 *      Function to be called on the interface.
 *
 *  @param statusCode
 *      Status code to be returned by the function.
 *
 *  @param messageId
 *      Unique ID of thiS message.
 *
 *  @param payload
 *      Data required by the function (or being returned by the function).  Note
 *      that the payload should be on the heap.
 *
 *  @param payloadLength
 *      Size (in bytes) of the payload.
 */
Message::Message(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode, uint32_t messageId, uint8_t *payload, uint32_t payloadLength)
{
    MessageType = messageType;
    Interface = interface;
    Function = function;
    StatusCode = statusCode;
    MessageID = messageId;
    PacketOffset = 0;
    PacketLength = 0;
    Payload = payload;
    PayloadLength = payloadLength;
}

/**
 *  @brief Constructor for the Message class.
 *
 *  @param messageType
 *      Type of message to send (see PacketType enum).
 *
 *  @param interface
 *      Interface on the ESP that this message should be routed to.
 *
 *  @param function
 *      Function to be called on the interface.
 *
 *  @param statusCode
 *      Status code to be returned by the function.
 */
Message::Message(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode)
{
    MessageType = messageType;
    Interface = interface;
    Function = function;
    StatusCode = statusCode;
    MessageID = 0;
    PacketOffset = 0;
    PacketLength = 0;
    Payload = nullptr;
    PayloadLength = 0;
}

/**
 *  @brief Copy constructor
 *
 *  @param original
 *      Message to copy.
 */
Message::Message(const Message &original)
{
    MessageType = original.MessageType;
    Interface = original.Interface;
    Function = original.Function;
    StatusCode = original.StatusCode;
    MessageID = original.MessageID;
    PacketOffset = 0;
    PacketLength = 0;
    Payload = nullptr;
    PayloadLength = 0;
}

/*
 * ---------------------------------------------------------------------------
 *
 *                             Methods
 *
 * ---------------------------------------------------------------------------
 */

/**
 *  @brief Create an instance of a message on the heap.
 *
 *  @param messageType
 *      Type of message to send (see PacketType enum).
 *
 *  @param interface
 *      Interface on the ESP that this message should be routed to.
 *
 *  @param function
 *      Function to be called on the interface.
 *
 *  @param statusCode
 *      Status code to be returned by the function.
 *
 *  @param messageId
 *      Unique ID of thiS message.
 *
 *  @param payload
 *      Data required by the function (or being returned by the function).  Note
 *      that the payload should be on the heap.
 *
 *  @param payloadLength
 *      Size (in bytes) of the payload.
 *
 *  @returns
 *      Pointer to the newly created message.
 */
Message *Message::CreateOnHeap(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode, uint32_t messageId, uint8_t *payload, uint32_t payloadLength)
{
    Message *result = static_cast<Message *>(pvPortMalloc(sizeof(Message)));

    *result = { };
    result->MessageType = messageType;
    result->Interface = interface;
    result->Function = function;
    result->StatusCode = statusCode;
    result->MessageID = messageId;
    result->PacketOffset = 0;
    result->PacketLength = 0;
    result->Payload = payload;
    result->PayloadLength = payloadLength;

    return(result);
}

/**
 *  Create an instance of a message on the heap.
 *
 *  @param messageType
 *      Type of message to send (see PacketType enum).
 *
 *  @param interface
 *      Interface on the ESP that this message should be routed to.
 *
 *  @param function
 *      Function to be called on the interface.
 *
 *  @param statusCode
 *      Status code to be returned by the function.
 *
 *  @returns
 *      Pointer to the newly created message.
 */
Message *Message::CreateOnHeap(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode)
{
    return(CreateOnHeap(messageType, interface, function, statusCode, 0, nullptr, 0));
}

/**
 *  @brief Create a copy of this message on the heap.
 *
 *  @param copyPayload
 *      Indicate if the payload should be copied as well or if we
 *      should just copy the header information.
 *
 *  @returns
 *      New copy of the message.
 */
Message *Message::CreateCopyOnHeap(bool copyPayload)
{
    uint8_t *payload = nullptr;
    uint32_t payloadLength = 0;

    if (copyPayload)
    {
        payloadLength = PayloadLength;
        payload = static_cast<uint8_t *>(pvPortMalloc(payloadLength));
        memcpy(static_cast<void *>(payload), static_cast<void *>(Payload), static_cast<size_t>(payloadLength));
    }
    Message *newMessage = CreateOnHeap(MessageType, Interface, Function, StatusCode, MessageID, payload, payloadLength);
    return(newMessage);
}

/**
 *  @brief Delete the heap storage associated with the payload.
 *
 *  On exit, the Payload will be set to a nullptr and the PayloadLength set to 0.
 */
void Message::DeletePayload()
{
    if (Payload != NULL)
    {
        vPortFree(Payload);
    }
    Payload = nullptr;
    PayloadLength = 0;
}

/**
 *  @brief Release the heap storage allocated for the message and any payload.
 *
 *  @param message
 *      Pointer to the message to be cleared.
 */
void Message::DeleteMessage(Message *message)
{
    if (message != NULL)
    {
        if (message->Payload != NULL)
        {
            vPortFree(message->Payload);
            message->Payload = nullptr;
        }
        vPortFree(message);
    }
}

