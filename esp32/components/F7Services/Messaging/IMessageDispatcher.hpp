/*
 *  IMessageDispatcher.hpp
 *
 *  Message dispatcher layer.  This object separates the need for the
 *  ESP32 interface objects to know about the hardware layer used to
 *  send messages between the ESP32 and the STM32.
 */

#ifndef _IMESSAGE_DISPATCHER_HPP_
#define _IMESSAGE_DISPATCHER_HPP_

#include "sdkconfig.h"

#include "Esp32Messaging.hpp"

/**
 *  @brief Message dispatcher class.
 *
 *  This class acts as a broker between the ESP32 and the STM32
 *  message transmit / receive mechanisms.
 */
class IMessageDispatcher
{
public:
    /**
     *  @brief Queue a message for the ESP32.
     */
    virtual bool QueueMessageForEsp32(Message *request) = 0;

    /**
     *  @brief Queue a message for the STM32.
     */
    virtual bool QueueMessageForStm32(Message *response) = 0;

    /**
     *  @brief Get the next message ID.
     */
    virtual uint32_t GetNextMessageID() = 0;
};

#endif /* _IMESSAGE_DISPATCHER_HPP_ */
