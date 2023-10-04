/*
 *  Esp32Messaging.hpp
 *
 *  Generic messaging constants, structures and definitions used
 *  in the messaging system.
 */

#ifndef _ESP32_MESSAGING_HPP_
#define _ESP32_MESSAGING_HPP_

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"

/*
 *  Messages between the ESP32 and the STM32 will be encoded and
 *  extracted through the message structure blow.
 */
struct Message
{
    /**
     *  Type of message.
     */
    uint8_t MessageType;

    /**
     *  Interface that this message is destined for.
     */
    uint8_t Interface;

    /**
     *  Function (on the interface) to be executed.
     */
    uint32_t Function;

    /**
     *  Status code (for returning messages) from the function.
     */
    uint32_t StatusCode;

    /**
     *  Unique ID of this message.
     */
    uint32_t MessageID;

    /**
     *  @brief Offset of the packet in the full message.
     */
    uint16_t PacketOffset;

    /**
     *  @brief Number of bytes in the packet.
     */
    uint16_t PacketLength;

    /**
     *  Pointer to the payload data to be processed (or returned from) the function.
     */
    uint8_t *Payload;

    /**
     *  Number of bytes in the payload.
     */
    uint32_t PayloadLength;

#if defined(PERFORMANCE_LOGGING)
    int64_t PerformanceLoggingStart;
#endif

    /**
     *  @brief Size of an encoded message header (in bytes).
     */
    static const uint32_t HEADER_SIZE = 27;

    /**
     *  @brief Offset of the CRC in an encoded message header.
     */
    static const uint32_t CRC_OFFSET = 1;

    /*
     *  @brief Current protocol number.
     */
    static const uint8_t PROTOCOL_NUMBER = 1;

    /**
     *  @brief Maximum number of bytes in an SPI frame.
     * 
     *  This is determined by a bug in the ESP where the maximum number of bytes in a SPI
     *  data transmission is 4092 bytes.
     */
    static const uint16_t MAXIMUM_SPI_FRAME_SIZE = 4092;

    /**
     * @brief SPI overhead in bytes.
     * 
     *  The buffer should always be 4 bytes longer than needed.  During development it
     *  was found that the last four bytes of any transmission were being discarded.
     *  Empirical tests proved this for 24, 32 and 40 byte packets.
     * 
     *  The work around is to increase the packet size by 4 and have dummy data in the
     *  last four bytes and discard the bytes.
     * 
     * See support post: https://esp32.com/viewtopic.php?f=13&t=10117
     */
    static const uint16_t SPI_MESSAGE_OVERHEAD = 4;

    /**
     *  @brief Maximum number of payload bytes in a payload frame (packet).
     */
    static const uint16_t MAXIMUM_PACKET_SIZE = (MAXIMUM_SPI_FRAME_SIZE - HEADER_SIZE - SPI_MESSAGE_OVERHEAD);

    /**
     *  @brief Message ID used to indicate an invalid (or unknown) message ID.
     */
    static const uint32_t INVALID_MESSAGE_ID = 0xffffffff;

    /**
     *  @brief Constructor for the Message class.
     */
    Message(uint8_t, uint8_t, uint32_t, uint32_t , uint32_t, uint8_t *, uint32_t);

    /**
     *  @brief Constructor for the Message class.
     */
    Message(uint8_t, uint8_t, uint32_t, uint32_t);

    /**
     *  @brief Default constructor for a Message structure.
     */
    Message();

    /**
     *  @brief Copy constructor.
     */
    Message(const Message &);

    /**
     * @brief Provide a default implementation for the assignment operator.
     */
    Message& operator=(const Message&) = default;

    /**
     *  @brief Make a copy of the message.
     */
    Message *CreateCopyOnHeap(bool);

    /**
     *  @brief Create a new message on the heap with the specified values.
     */
    static Message *CreateOnHeap(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode, uint32_t messageId, uint8_t *payload, uint32_t payloadLength);

    /**
     *  @brief Create a new message on the heap with the specified values.
     */
    static Message *CreateOnHeap(uint8_t messageType, uint8_t interface, uint32_t function, uint32_t statusCode);

    /**
     *  @brief Delete the payload and free up heap storage.
     */
    void DeletePayload();

    /**
     *  @brief Delete the message and payload releassing heap storage.
     */
    static void DeleteMessage(Message *);
};

#endif /* _ESP32_MESSAGING_HPP_ */
