/*
 *  PosixAPI.cpp
 *
 *  Implement POSIX APIs.
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
 *  @brief Expose the POSIX getaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getaddrinfo method.
 */
void WiFiRequestHandler::GetAddrInfo(void *vpMessage)
{
    TRACE_MESSAGE("GetAddrInfo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::GetAddrInfoResponse *response = static_cast<Esp32Messaging::GetAddrInfoResponse *>(pvPortMalloc(sizeof(Esp32Messaging::GetAddrInfoResponse)));

    if (IsConnected())
    {
        Esp32Messaging::GetAddrInfoRequest *request = Encoders::ExtractGetAddrInfoRequest(message->Payload);
        //
        Esp32Messaging::AddrInfo *extractedHints = Encoders::ExtractAddrInfo(request->Hints);
        vPortFree(request->Hints);

        struct addrinfo hints = { };
        hints.ai_family = extractedHints->Family;
        hints.ai_socktype = extractedHints->SocketType;
        vPortFree(extractedHints);
        //
        //  This will hold a pointer to a chain of one or more addrinfo structure.
        //
        //  It is the responsibility of the calling .NET application to ensure
        //  that FreeAddrInfo is called.  FreeAddrInfo will free the allocated
        //  memory.
        //
        struct addrinfo *addressInformation;
        int errorCode = getaddrinfo(request->NodeName, request->ServName, &hints, &addressInformation);
        vPortFree(request);

        response->AddrInfoResponseErrno = MapErrno(errorCode);
        if ((errorCode == 0) || (addressInformation != NULL))
        {
            Esp32Messaging::SockAddr sa = { };
            Esp32Messaging::AddrInfo a = { };

            int32_t family;
            Mapping::GetStmValue(Mapping::AddressFamily, addressInformation->ai_addr->sa_family, &family);
            sa.Family = (unsigned char) (family & 0xff);
            Encoders::EncodeSockAddr(&sa, a.Addr);

            a.CanonName = strdup(addressInformation->ai_canonname);
            a.Family = addressInformation->ai_family;
            a.Flags = addressInformation->ai_flags;
            a.MyHeapAddress = reinterpret_cast<uint32_t>(addressInformation);
            a.Next = addressInformation->ai_next;
            a.Protocol = addressInformation->ai_protocol;
            a.SocketType = addressInformation->ai_socktype;
            a.AddrLen = addressInformation->ai_addrlen;

            response->ResLength = Encoders::EncodedAddrInfoBufferSize(&a);
            response->Res = static_cast<uint8_t *>(pvPortMalloc(response->ResLength));
            Encoders::EncodeAddrInfo(&a, response->Res);
        }
        else
        {
            response->AddrInfoResponseErrno = MapErrno(errno);
            vPortFree(addressInformation);
        }
    }
    else
    {
        response->AddrInfoResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedGetAddrInfoResponseBufferSize(response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeGetAddrInfoResponse(response, message->Payload);
    vPortFree(response);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("GetAddrInfo: Exit");
}

/**
 *  @brief Expose the POSIX socket method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the socket method.
 */
void WiFiRequestHandler::Socket(void *vpMessage)
{
    TRACE_MESSAGE("Socket: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::SocketRequest *request = Encoders::ExtractSocketRequest(message->Payload);
        TRACE_MESSAGE("Socket: Creating socket on domain %d, type %d and protocol %d", (int) request->Domain, (int) request->Type, (int)request->Protocol);

        response.Result = socket(request->Domain, request->Type, request->Protocol);
        if (response.Result < 0)
        {
            TRACE_MESSAGE("Socket: Error code: %d - %s", errno, strerror(errno));
            response.ResponseErrno = MapErrno(errno);
        }
        else
        {
            TRACE_MESSAGE("Socket: Created socket %d", (int) response.Result);
            response.ResponseErrno = 0;
        }

        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Socket: Exit");
}

/**
 *  @brief Expose the POSIX connect method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the connect method.
 */
void WiFiRequestHandler::Connect(void *vpMessage)
{
    TRACE_MESSAGE("Connect: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::ConnectRequest *request = Encoders::ExtractConnectRequest(message->Payload);
        Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->Addr);
        if (request->AddrLength > 0)
        {
            if (request->Addr)
            {
                vPortFree(request->Addr);
            }
        }

        struct sockaddr_in sa = { };
        int32_t family;
        Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
        errno = 0;
        if (family == AF_INET)
        {
            sa.sin_family = (uint8_t) (family & 0xff);
            sa.sin_port = socketAddress->Port;
            memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(sa.sin_addr));
            sa.sin_len = sizeof(struct sockaddr_in);
            TRACE_MESSAGE("Connect: Connecting socket: %d", (int) request->SocketHandle);
            TRACE_MESSAGE("Connect: Family: %hhd", sa.sin_family);
            TRACE_MESSAGE("Connect: IP address: %s", inet_ntoa(sa.sin_addr));
            TRACE_MESSAGE("Connect: Port: %d", ntohs(sa.sin_port));
            response.Result = connect(request->SocketHandle, (struct sockaddr *) &sa, sizeof(sa));
            if (response.Result != 0)
            {
                TRACE_MESSAGE("Connect: Error code: %d - %s", errno, strerror(errno));
                response.ResponseErrno = MapErrno(errno);
            }
        }
        else
        {
            response.Result = -1;
            response.ResponseErrno = MapErrno(EFAULT);
        }

        vPortFree(request);
        vPortFree(socketAddress);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Connect: Exit");
}

/**
 *  @brief Expose the POSIX freeaddrinfo method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the freeaddrinfo method.
 */
void WiFiRequestHandler::FreeAddrInfo(void  *vpMessage)
{
    TRACE_MESSAGE("FreeAddrInfo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    TRACE_MESSAGE("FreeAddrInfo: Releasing addrinfo storage.");
    Esp32Messaging::FreeAddrInfoRequest *request = Encoders::ExtractFreeAddrInfoRequest(message->Payload);

    freeaddrinfo(reinterpret_cast<struct addrinfo *>(request->AddrInfoAddress));

    vPortFree(request);
    vPortFree(message->Payload);

    message->PayloadLength = 0;
    message->Payload = nullptr;
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;

    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("FreeAddrInfo: Exit");
}

/**
 *  @brief Expose the POSIX getsockopt method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockopt method.
 */
void WiFiRequestHandler::GetSockOpt(void *vpMessage)
{
    TRACE_MESSAGE("GetSockOpt: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::GetSockOptRequest *request = Encoders::ExtractGetSockOptRequest(message->Payload);
    Esp32Messaging::GetSockOptResponse response = { };

    if (IsConnected())
    {
        struct timeval timeOut = { };
        struct linger lingerOut = { };
        int intVal = 0;
        int32_t optionName = 0;
        void *optionValue = nullptr;
        socklen_t optionLength = 0;

        if (Mapping::GetEspValue(Mapping::SocketOptions, request->OptionName, &optionName) < 0)
        {
            TRACE_MESSAGE("GetSockOpt: Unknown option: %d", (int) request->OptionName);
            response.Result = -1;
            response.ResponseErrno = MapErrno(ENOPROTOOPT);
        }
        else
        {
            switch (optionName)
            {
                case SO_SNDTIMEO:       /* Get the send timeout */
                case SO_RCVTIMEO:       /* Get the receive timeout */
                    optionValue = &timeOut;
                    optionLength = sizeof(struct timeval);
                    break;
                case SO_LINGER:         /* Linger on close if data present */
                    optionValue = &lingerOut;
                    optionLength = sizeof(struct linger);
                    break;
                case SO_RCVBUF:         /* Get the receive buffer size */
                    optionValue = &intVal;
                    optionLength = sizeof(intVal);
                    break;
                case SO_ACCEPTCONN:     /* Socket has had listen() */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_ACCEPTCONN");
                    break;
                case SO_DONTLINGER:     /* Don't linger on close if data present */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_DONTLINGER");
                    break;
                case SO_ERROR:          /* Get error status and clear */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_ERROR");
                    break;
                case SO_TYPE:           /* Get socket type */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_TYPE");
                    break;
                case SO_NO_CHECK:       /* Don't create UDP checksum */
                    TRACE_MESSAGE("GetSockOpt: Implement SO_NO_CHECK");
                    break;
                default:
                    response.Result = -1;
                    response.ResponseErrno = MapErrno(ENOPROTOOPT);
                    break;
            }
            if (response.Result == 0)
            {
                int32_t level = 0;
                Mapping::GetEspValue(Mapping::SocketLevel, request->Level, &level);
                TRACE_MESSAGE("GetSockOpt: Getting socket option level 0x%x, option name 0x%x for socket %d", (unsigned int) level, (unsigned int) optionName, (int) request->SocketHandle);
                response.Result = getsockopt(request->SocketHandle, level, optionName, static_cast<void *>(optionValue), &optionLength);
            }
        }
        if (response.Result == 0)
        {
            Esp32Messaging::TimeVal esp_tv = { };
            Esp32Messaging::Linger esp_lv = { };
            Esp32Messaging::IntegerResponse esp_iv = { };
            switch (optionName)
            {
                case SO_SNDTIMEO:       /* Set the send timeout */
                case SO_RCVTIMEO:       /* Set the receive timeout */
                    esp_tv.TvSec = timeOut.tv_sec;
                    esp_tv.TvUsec = timeOut.tv_usec;
                    response.OptionValueLength = Encoders::EncodedTimeValBufferSize(&esp_tv);
                    response.OptionValue = static_cast<uint8_t *>(malloc(response.OptionValueLength));
                    if (response.OptionValue == NULL)
                    {
                        response.ResponseErrno = MapErrno(ENOMEM);
                        response.Result = -1;
                        response.OptionValueLength = 0;
                    }
                    else
                    {
                        Encoders::EncodeTimeVal(&esp_tv, response.OptionValue);
                    }
                    break;
                case SO_LINGER:         /* Linger on close if data present */
                    esp_lv.LLinger = lingerOut.l_linger;
                    esp_lv.LOnOff = lingerOut.l_onoff;
                    response.OptionValueLength = Encoders::EncodedLingerBufferSize(&esp_lv);
                    response.OptionValue = static_cast<uint8_t *>(malloc(response.OptionValueLength));
                    if (response.OptionValue == NULL)
                    {
                        response.ResponseErrno = MapErrno(ENOMEM);
                        response.Result = -1;
                        response.OptionValueLength = 0;
                    }
                    else
                    {
                        Encoders::EncodeLinger(&esp_lv, response.OptionValue);
                    }
                    break;
                case SO_RCVBUF:
                    esp_iv.Result = *(static_cast<int *>(optionValue));
                    response.OptionValueLength = Encoders::EncodedIntegerResponseBufferSize(&esp_iv);
                    response.OptionValue = static_cast<uint8_t *>(malloc(response.OptionValueLength));
                    if (response.OptionValue == NULL)
                    {
                        response.ResponseErrno = MapErrno(ENOMEM);
                        response.Result = -1;
                        response.OptionValueLength = 0;
                    }
                    else
                    {
                        Encoders::EncodeIntegerResponse(&esp_iv, response.OptionValue);
                    }
                    break;
            }
        }
        else
        {
            response.ResponseErrno = MapErrno(errno);
        }
        TRACE_MESSAGE("GetSockOpt: Result: %d, errno: %d - %s", (int) response.Result, (int) response.ResponseErrno, strerror(response.ResponseErrno));

        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    message->PayloadLength = Encoders::EncodedGetSockOptResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeGetSockOptResponse(&response, message->Payload);
    if (response.OptionValue != NULL)
    {
        free(response.OptionValue);
    }
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("GetSockOpt: Exit");
}

/**
 * @brief Extract an encoded integer setsockopt option value.
 * 
 * @param data 
 *      Source of the socket option data (the option value).
 * 
 * @param length
 *      Pointer to a socklen_t object to take the length of the extracted option value.
 * 
 * @return uint8_t* 
 *      Pointer to the extracted option value (integer) or NULL if there was a problem.
 */
uint8_t *WiFiRequestHandler::ExtractIntegerSocketOptionValue(uint8_t *data, socklen_t *length)
{
    uint8_t *result = NULL;

    if (length != NULL)
    {
        *length = 0;
        if (data != NULL)
        {
            int *intVal = reinterpret_cast<int *>(pvPortMalloc(sizeof(int)));
            if (intVal != NULL)
            {
                *intVal = Encoders::ExtractInt32(data);
                *length = sizeof(intVal);
            }
            result = reinterpret_cast<uint8_t *>(intVal);
        }
    }

    return(result);
}

/**
 * @brief Setup the socket value and length for calls to SetSockOpt.
 * 
 * @param name
 *      Name of the option to decode.
 * 
 * @param optionValue
 *      Pointer to the source data.
 * 
 * @param length
 *      Pointer to a socklen_t object to take the length of the option value.
 * 
 * @return uint8_t *
 *      Pointer the the decoded option value.
 */
uint8_t *WiFiRequestHandler::GetSocketOptionValue(int name, uint8_t *optionValue, socklen_t *length)
{
    TRACE_MESSAGE("GetSocketOptionValue: Enter");

    uint8_t *result = NULL;
    if (length != NULL)
    {
        switch (name)
        {
            case SO_SNDTIMEO:       /* Set the send timeout */
            case SO_RCVTIMEO:       /* Set the receive timeout */
                {
                    struct timeval *value = reinterpret_cast<struct timeval *>(pvPortMalloc(sizeof(struct timeval)));
                    if (value != NULL)
                    {
                        Esp32Messaging::TimeVal *tv = Encoders::ExtractTimeVal(optionValue);
                        value->tv_sec = tv->TvSec;
                        value->tv_usec = tv->TvUsec;
                        vPortFree(tv);
                        result = reinterpret_cast<uint8_t *>(value);
                        *length = sizeof(struct timeval);
                    }
                }
                break;
            case SO_REUSEADDR:      /* Reuse local addresses when binding */
            case SO_RCVBUF:         /* Set the receive buffer size */
                result = ExtractIntegerSocketOptionValue(optionValue, length);
                break;
            case SO_LINGER:         /* Linger on close if data present */
                TRACE_MESSAGE("SetSockOpt: Implement SO_LINGER");
                break;
            case SO_ACCEPTCONN:     /* Socket has had listen() */
                TRACE_MESSAGE("SetSockOpt: Implement SO_ACCEPTCONN");
                break;
            case SO_DONTLINGER:     /* Don't linger on close if data present */
                TRACE_MESSAGE("SetSockOpt: Implement SO_DONTLINGER");
                break;
            case SO_ERROR:          /* Get error status and clear */
                TRACE_MESSAGE("SetSockOpt: Implement SO_ERROR");
                break;
            case SO_TYPE:           /* Get socket type */
                TRACE_MESSAGE("SetSockOpt: Implement SO_TYPE");
                break;
            case SO_NO_CHECK:       /* Don't create UDP checksum */
                TRACE_MESSAGE("SetSockOpt: Implement SO_NO_CHECK");
                break;
            case SO_BROADCAST:
                TRACE_MESSAGE("SetSockOpt: Implement SO_BROADCAST");
                break;
            case SO_KEEPALIVE:
                TRACE_MESSAGE("SetSockOpt: Implement SO_KEEPALIVE");
                break;
            default:
                break;
        }
    }

    TRACE_MESSAGE("GetSocketOptionValue: Exit");

    return(result);
}

/**
 * @brief Setup the TCP value and length for calls to SetSockOpt.
 * 
 * @param name
 *      Name of the option to decode.
 * 
 * @param optionValue
 *      Pointer to the source data.
 * 
 * @param length
 *      Pointer to a socklen_t object to take the length of the option value.
 * 
 * @return uint8_t *
 *      Pointer the the decoded option value.
 */
uint8_t *WiFiRequestHandler::GetTcpOptionValue(int name, uint8_t *optionValue, socklen_t *length)
{
    TRACE_MESSAGE("GetTcpOptionValue: Enter");

    uint8_t *result = NULL;

    if (length != NULL)
    {
        switch (name)
        {
            case TCP_NODELAY:
                result = ExtractIntegerSocketOptionValue(optionValue, length);
                break;
            default:
                *length = 0;
                break;
        }
    }

    TRACE_MESSAGE("GetTcpOptionValue: Exit");

    return(result);
}

/**
 *  @brief Expose the POSIX setsockopt method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the setsockopt method.
 */
void WiFiRequestHandler::SetSockOpt(void *vpMessage)
{
    TRACE_MESSAGE("SetSockOpt: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::SetSockOptRequest *request = Encoders::ExtractSetSockOptRequest(message->Payload);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        void *optionValue = nullptr;

        socklen_t optionLength = 0;
        int32_t level = 0;

        if (Mapping::GetEspValue(Mapping::SocketLevel, request->Level, &level) < 0)
        {
            TRACE_MESSAGE("SetSockOpt: Unknown level: 0x%x", (unsigned int) request->Level);
        }
        else
        {
            int32_t optionName = 0;
            if (Mapping::GetEspValue(level == SOL_SOCKET ? Mapping::SocketOptions : Mapping::TCPOptions, request->OptionName, &optionName) < 0)
            {
                TRACE_MESSAGE("SetSockOpt: Unknown option name: 0x%x", (unsigned int) request->OptionName);
            }

            switch (level)
            {
                case SOL_SOCKET:
                    optionValue = GetSocketOptionValue(optionName, request->OptionValue, &optionLength);
                    break;
                case IPPROTO_TCP:
                    optionValue = GetTcpOptionValue(optionName, request->OptionValue, &optionLength);
                    break;   
                default:
                    TRACE_MESSAGE("SetSockOpt: Cannot process option level: 0x%x", (unsigned int) request->Level);
                    break;
            }
            if (optionValue != NULL)
            {
                TRACE_MESSAGE("SetSockOpt: Setting socket option level to 0x%x, option name 0x%x for socket %d", (unsigned int) level, (unsigned int) optionName, (int) request->SocketHandle);
                response.Result = setsockopt(request->SocketHandle, level, optionName, optionValue, optionLength);
                vPortFree(optionValue);
            }
        }

        if (response.Result != 0)
        {
            response.ResponseErrno = MapErrno(errno);
        }
        TRACE_MESSAGE("SetSockOpt: Result: %d, errno: %d - %s", (int) response.Result, (int) response.ResponseErrno, strerror(response.ResponseErrno));

        if (request->OptionValueLength > 0)
        {
            if (request->OptionValue)
            {
                vPortFree(request->OptionValue);
            }
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("SetSockOpt: Exit");
}

/**
 *  @brief Expose the POSIX write method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the write method.
 */
void WiFiRequestHandler::Write(void *vpMessage)
{
    TRACE_MESSAGE("Write: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::WriteRequest *request = Encoders::ExtractWriteRequest(message->Payload);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        response.Result = write(request->SocketHandle, request->Buffer, request->Count);
        if (response.Result < 0)
        {
            response.ResponseErrno = MapErrno(errno);;
        }

        if (request->BufferLength > 0)
        {
            if (request->Buffer)
            {
                vPortFree(request->Buffer);
            }
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Write: Exit");
}

/**
 *  @brief Expose the POSIX read method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the read method.
 */
void WiFiRequestHandler::Read(void *vpMessage)
{
    TRACE_MESSAGE("Read: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::ReadResponse response = { };
    response.Buffer = nullptr;

    if (IsConnected())
    {
        Esp32Messaging::ReadRequest *request = Encoders::ExtractReadRequest(message->Payload);
        response.Buffer = static_cast<uint8_t *>(pvPortMalloc(request->Count));
        bzero(response.Buffer, request->Count);
        response.BufferLength = request->Count;
        response.ReadResponseResult = read(request->SocketHandle, response.Buffer, request->Count);
        if (response.ReadResponseResult < 0)
        {
            response.ReadResponseErrno = MapErrno(errno);
        }
        vPortFree(request);
    }
    else
    {
        response.ReadResponseResult = -1;
        response.ReadResponseErrno = MapErrno(ENETDOWN)        ;
    }

    vPortFree(message->Payload);

    //
    //  TODO: Optimise this by not sending an empty buffer in the case of an error.
    //
    message->PayloadLength = Encoders::EncodedReadResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeReadResponse(&response, message->Payload);

    if (response.Buffer)
    {
        vPortFree(response.Buffer);
    }

    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Read: Exit");
}

/**
 *  @brief Expose the POSIX close method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the close method.
 */
void WiFiRequestHandler::Close(void *vpMessage)
{
    TRACE_MESSAGE("Close: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::CloseRequest *request = Encoders::ExtractCloseRequest(message->Payload);

        response.Result = close(request->SocketHandle);
        if (response.Result < 0)
        {
            response.ResponseErrno = MapErrno(errno);
        }

        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Close: Exit");
}

/**
 *  @brief Expose the POSIX send method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the send method.
 */
void WiFiRequestHandler::Send(void *vpMessage)
{
    TRACE_MESSAGE("Send: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::SendRequest *request = Encoders::ExtractSendRequest(message->Payload);
        int32_t mappedFlags = 0;
        response.ResponseErrno = 0;
        response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
        if (response.Result == 0)
        {
            response.Result = send(request->SocketHandle, request->Buffer, request->Length, mappedFlags);
            if (response.Result < 0)
            {
                response.ResponseErrno = MapErrno(errno);
            }
        }
        else
        {
            response.ResponseErrno = MapErrno(EFAULT);
        }

        TRACE_MESSAGE("Send: Sent %d bytes.", (int) request->Length);
        if (request->Buffer && (request->Length > 0))
        {
            TRACE_HEX_BUFFER(request->Buffer, request->Length);
        }
        TRACE_MESSAGE("Send: Result: %d, errno: %d - %s", (int) response.Result, (int) response.ResponseErrno, strerror(response.ResponseErrno));

        vPortFree(request->Buffer);
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Send: Exit");
}

/**
 *  @brief Expose the POSIX sendto method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the sendto method.
 */
void WiFiRequestHandler::SendTo(void *vpMessage)
{
    TRACE_MESSAGE("SendTo: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::SendToRequest *request = Encoders::ExtractSendToRequest(message->Payload);

        int32_t mappedFlags = 0;
        response.ResponseErrno = 0;
        response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
        if (response.Result == 0)
        {
            Esp32Messaging::SockAddr *socketAddress = NULL;
            int32_t family = 0;
            if (request->DestinationAddress)
            {
                socketAddress = Encoders::ExtractSockAddr(request->DestinationAddress);
                Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
                TRACE_MESSAGE("SendTo: Mapping request family %hhd to ESP family %d", socketAddress->Family, (int) family);
            }
            
            struct sockaddr_in sa = { };
            struct sockaddr *psa = NULL;
            size_t lenSa = 0;
            if (request->DestinationAddressLength != 0)
            {
                if (family == AF_INET)
                {
                    if (request->DestinationAddress)
                    {
                        sa.sin_family = (unsigned char) (family & 0xff);

                        sa.sin_port = socketAddress->Port;
                        memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(sa.sin_addr));
                        sa.sin_len = sizeof(sa);
                        psa = (struct sockaddr *) &sa;
                        lenSa = sizeof(sa);
                    }
                    TRACE_MESSAGE("SendTo: Connecting socket: %d", (int) request->SocketHandle);
                    TRACE_MESSAGE("SendTo: Family: %hhd", (psa == NULL) ? 0 : sa.sin_family);
                    TRACE_MESSAGE("SendTo: IP address: %s", (psa == NULL) ? "NULL" : inet_ntoa(sa.sin_addr));
                    TRACE_MESSAGE("SendTo: Port: %d", (psa == NULL) ? 0 : ntohs(sa.sin_port));
                }
            }
            response.Result = sendto(request->SocketHandle, request->Buffer, request->Length, mappedFlags, psa, lenSa);

            if (response.Result < 0)
            {
                response.ResponseErrno = MapErrno(errno);
            }

            if (socketAddress != NULL)
            {
                vPortFree(socketAddress);
            }
        }
        else
        {
            response.ResponseErrno = MapErrno(EFAULT);
        }

        TRACE_MESSAGE("SendTo: Sent %d bytes.", (int) response.Result);
        if (request->Buffer && (request->Length > 0))
        {
            TRACE_HEX_BUFFER(request->Buffer, request->Length);
        }
        TRACE_MESSAGE("SendTo: Result: %d, errno: %d - %s", (int) response.Result, (int) response.ResponseErrno, strerror(response.ResponseErrno));

        if (request->DestinationAddress)
        {
            vPortFree(request->DestinationAddress);
        }
        vPortFree(request->Buffer);
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("SendTo: Exit");
}

/**
 *  @brief Expose the POSIX recvfrom method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the recvfrom method.
 */
void WiFiRequestHandler::RecvFrom(void *vpMessage)
{
    TRACE_MESSAGE("RecvFrom: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::RecvFromResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::RecvFromRequest *request = Encoders::ExtractRecvFromRequest(message->Payload);
        int32_t mappedFlags = 0;
        response.Result = Mapping::GetEspFlagValue(Mapping::MessageFlags, request->Flags, &mappedFlags);
        if (response.Result == 0)
        {
            TRACE_MESSAGE("Requesting %d bytes from socket handle %d", (int) request->Length, (int) request->SocketHandle);
            struct sockaddr_in *sa = NULL;
            socklen_t len;
            if (request->GetSourceAddress != 0)
            {
                sa = static_cast<struct sockaddr_in *>(pvPortMalloc(sizeof(struct sockaddr_in)));
                bzero(sa, sizeof(struct sockaddr_in));
                len = sizeof(struct sockaddr_in);
            }
            response.Buffer = static_cast<uint8_t *>(pvPortMalloc(request->Length));
            if (response.Buffer == NULL)
            {
                response.Buffer = nullptr;
                response.Result = -1;
                response.ResponseErrno = ENOMEM;
            }
            else
            {
                response.Result = recvfrom(request->SocketHandle, response.Buffer, request->Length, mappedFlags, reinterpret_cast<struct sockaddr *>(sa), (sa == NULL) ? NULL : &len);
            }

            if (response.Result < 0)
            {
                response.ResponseErrno = errno;
            }
            else
            {
                if (request->GetSourceAddress != 0)
                {
                    Esp32Messaging::SockAddr socketAddress = { };
                    int32_t family;
                    Mapping::GetStmValue(Mapping::AddressFamily, sa->sin_family, &family);
                    socketAddress.Family = (uint8_t) (family & 0xff);
                    memcpy(&socketAddress.Ip4Address, &sa->sin_addr, sizeof(sa->sin_addr));
                    socketAddress.Port = sa->sin_port;
                    response.SourceAddressLength = Encoders::EncodedSockAddrBufferSize(&socketAddress);
                    response.SourceAddress = static_cast<uint8_t *>(pvPortMalloc(response.SourceAddressLength));
                    if (response.SourceAddress == NULL)
                    {
                        response.SourceAddressLength = 0;
                        response.SourceAddressLen = 0;
                        response.Result = -1;
                        response.ResponseErrno = ENOMEM;
                    }
                    else
                    {
                        Encoders::EncodeSockAddr(&socketAddress, response.SourceAddress);
                    }
                }
                else
                {
                    response.SourceAddress = nullptr;
                    response.SourceAddressLength = 0;
                    response.SourceAddressLen = 0;
                }
                response.BufferLength = response.Result;
            }

            if (sa != NULL)
            {
                vPortFree(sa);
            }
        }
        else
        {
            response.ResponseErrno = EFAULT;
        }
        TRACE_MESSAGE("RecvFrom: Received %d bytes.", (int) response.Result);
        if (response.Buffer && (response.BufferLength > 0))
        {
            TRACE_HEX_BUFFER(response.Buffer, response.Result);
        }
        TRACE_MESSAGE("RecvFrom: Result: %d, errno: %d - %s", (int) response.Result, (int) response.ResponseErrno, strerror(response.ResponseErrno));
        if (response.Result < 0)
        {
            response.ResponseErrno = MapErrno(response.ResponseErrno);
        }

        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedRecvFromResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeRecvFromResponse(&response, message->Payload);
    if (response.Buffer)
    {
        vPortFree(response.Buffer);
    }
    if (response.SourceAddress)
    {
        vPortFree(response.SourceAddress);
    }
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("RecvFrom: Exit");
}

/**
 *  @brief Expose the POSIX bind method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the bind method.
 */
void WiFiRequestHandler::Bind(void *vpMessage)
{
    TRACE_MESSAGE("Bind: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response =  { };

    if (IsConnected())
    {
        Esp32Messaging::BindRequest *request = Encoders::ExtractBindRequest(message->Payload);
        Esp32Messaging::SockAddr *socketAddress = Encoders::ExtractSockAddr(request->Addr);
        if (request->AddrLength > 0)
        {
            if (request->Addr)
            {
                vPortFree(request->Addr);
                request->Addr = nullptr;
            }
        }

        struct sockaddr_in sa = { };
        int32_t family;
        Mapping::GetEspValue(Mapping::AddressFamily, socketAddress->Family, &family);
        errno = 0;
        if (family == AF_INET)
        {
            sa.sin_family = socketAddress->Family;
            sa.sin_port = socketAddress->Port;
            memcpy(&sa.sin_addr, &socketAddress->Ip4Address, sizeof(socketAddress->Ip4Address));
            TRACE_MESSAGE("Bind: Socket handle: %d", (int) request->SocketHandle);
            TRACE_MESSAGE("Bind: Family: %hhd", sa.sin_family);
            TRACE_MESSAGE("Bind: IP address: %s", inet_ntoa(sa.sin_addr));
            TRACE_MESSAGE("Bind: Port: %d", ntohs(sa.sin_port));
            response.Result = bind(request->SocketHandle, (struct sockaddr *) &sa, sizeof(sa));
            if (response.Result != 0)
            {
                response.ResponseErrno = MapErrno(errno);
            }
        }
        else
        {
            response.Result = -1;
            response.ResponseErrno = MapErrno(EFAULT);
        }

        TRACE_MESSAGE("Bind: Error code: %d - %s", errno, strerror(errno));
        vPortFree(request);
        vPortFree(socketAddress);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Bind: Exit");
}

/**
 *  @brief Expose the POSIX listen method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the listen method.
 */
void WiFiRequestHandler::Listen(void *vpMessage)
{
    TRACE_MESSAGE("Listen: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::ListenRequest *request = Encoders::ExtractListenRequest(message->Payload);
        response.Result = listen(request->SocketHandle, request->BackLog);
        response.ResponseErrno = (response.Result == 0) ? 0 : MapErrno(errno);
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    PERFORMANCE_LOGGING_DUMP_RESULTS();
    TRACE_MESSAGE("Listen: Exit");
}

/**
 *  @brief Expose the POSIX accept method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the accept method.
 */
void WiFiRequestHandler::Accept(void *vpMessage)
{
    TRACE_MESSAGE("Accept: Enter");
    PERFORMANCE_LOGGING_ENTER();
    Esp32Messaging::AcceptResponse response = { };
    Message *message = static_cast<Message *>(vpMessage);

    if (IsConnected())
    {
        Esp32Messaging::AcceptRequest *request = Encoders::ExtractAcceptRequest(message->Payload);

        struct sockaddr sa = { };
        socklen_t saLen = sizeof(sa);
        response.Result = accept(request->SocketHandle, &sa, &saLen);
        if (response.Result > 0)
        {
            Esp32Messaging::SockAddr *sockAddr = static_cast<Esp32Messaging::SockAddr *>(pvPortMalloc(sizeof(Esp32Messaging::SockAddr)));
            bzero(sockAddr, sizeof(Esp32Messaging::SockAddr));
            int32_t family;
            Mapping::GetStmValue(Mapping::AddressFamily, sa.sa_family, &family);
            sockAddr->Family = (uint8_t) (family & 0xff);
            struct sockaddr_in *sin = (struct sockaddr_in *) &sa;
            sockAddr->Port = sin->sin_port;
            memcpy(&sockAddr->Ip4Address, &sin->sin_addr, sizeof(sin->sin_addr));
            response.AddrLength = Encoders::EncodedSockAddrBufferSize(sockAddr);
            response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
            Encoders::EncodeSockAddr(sockAddr, response.Addr);
            vPortFree(sockAddr);
            response.ResponseErrno = 0;
            TRACE_MESSAGE("Accept: Socket handle: %d", (int) response.Result);
            TRACE_MESSAGE("Accept: Family: %hhd", sin->sin_family);
            TRACE_MESSAGE("Accept: IP address: %s", inet_ntoa(sin->sin_addr));
            TRACE_MESSAGE("Accept: Port: %d", ntohs(sin->sin_port));
        }
        else
        {
            response.AddrLength = 0;
            response.Addr = nullptr;
            response.ResponseErrno = MapErrno(errno);
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedAcceptResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeAcceptResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Accept: Exit");
}

/**
 *  @brief Expose the simulation of the POSIX ioctl method to the STM32.
 *
 *  This is a simulation of the ioctl method as this is not supported by the
 *  ESP-IDF framework.
 *
 *  @param message
 *      Message structure containing the parameters for the ioctl method.
 */
void WiFiRequestHandler::Ioctl(void *vpMessage)
{
    TRACE_MESSAGE("Ioctl: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IoctlResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::IoctlRequest *request = Encoders::ExtractIoctlRequest(message->Payload);

        esp_netif_ip_info_t ipInfo;
        Esp32Messaging::SockAddr sa = { };
        int32_t family;

        Mapping::GetStmValue(Mapping::AddressFamily, AF_INET, &family);
        sa.Family = (unsigned char) (family & 0xff);
        response.AddrLength = 0;
        response.Addr = nullptr;

        switch (request->Command)
        {
            case SIOCGIFNETMASK:    /* Get network mask */
                if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
                {
                    memcpy(&sa.Ip4Address, &ipInfo.netmask, sizeof(ipInfo.netmask));
                    response.AddrLength = Encoders::EncodedSockAddrBufferSize(&sa);
                    response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
                    Encoders::EncodeSockAddr(&sa, response.Addr);
                }
                break;
            case SIOCGIFHWADDR:     /* Get hardware address */
                response.AddrLength = 6;
                response.Addr = static_cast<uint8_t *>(pvPortMalloc(6));
                GetMacAddress(response.Addr, WIFI_IF_STA);
                break;
            case SIOCGIFADDR:       /* Get IP address */
            case SIOCGIFCONF:       /* Return an interface list (IPv4) */
                if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
                {
                    memcpy(&sa.Ip4Address, &ipInfo.ip, sizeof(ipInfo.ip));
                    response.AddrLength = Encoders::EncodedSockAddrBufferSize(&sa);
                    response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
                    Encoders::EncodeSockAddr(&sa, response.Addr);
                }
                break;
            case SIOCGIFFLAGS:      /* Gets the interface flags */
                if (esp_netif_get_ip_info(_stationNetIfHandle, &ipInfo) == ESP_OK)
                {
                    response.Flags = IFF_UP;
                }
                else
                {
                    response.Flags = IFF_DOWN;
                }
                break;
            default:
                response.Result = -1;
                response.ResponseErrno = MapErrno(EINVAL);
                TRACE_MESSAGE("Unknown ioctl request %04x", (int) request->Command);
                break;
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIoctlResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIoctlResponse(&response, message->Payload);
    if (response.Addr)
    {
        vPortFree(response.Addr);
    }
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("Ioctl: Exit");
}

/**
 *  @brief Expose the POSIX getsockname method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockname method.
 *
 *  @param methodToCall
 *      Which method should be called, getsockname or getpeername?
 */
void WiFiRequestHandler::GetSockPeerName(void *vpMessage, WiFiFunction::WiFiFunction methodToCall)
{
    TRACE_MESSAGE("GetSockPeerName: Enter");
    PERFORMANCE_LOGGING_ENTER();

    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::GetSockPeerNameResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::GetSockPeerNameRequest *request = Encoders::ExtractGetSockPeerNameRequest(message->Payload);

        struct sockaddr sa = { };
        socklen_t saLen = sizeof(sa);
        if (methodToCall == WiFiFunction::GetSockName)
        {
            response.Result = getsockname(request->SocketHandle, &sa, &saLen);
        }
        else
        {
            response.Result = getpeername(request->SocketHandle, &sa, &saLen);
        }
        if (response.Result == 0)
        {
            Esp32Messaging::SockAddr *sockAddr = static_cast<Esp32Messaging::SockAddr *>(pvPortMalloc(sizeof(Esp32Messaging::SockAddr)));
            bzero(sockAddr, sizeof(Esp32Messaging::SockAddr));
            int32_t family;
            Mapping::GetStmValue(Mapping::AddressFamily, sa.sa_family, &family);
            sockAddr->Family = (uint8_t) (family & 0xff);
            struct sockaddr_in *sin = (struct sockaddr_in *) &sa;
            sockAddr->Port = sin->sin_port;
            memcpy(&sockAddr->Ip4Address, &sin->sin_addr, sizeof(sin->sin_addr));
            response.AddrLength = Encoders::EncodedSockAddrBufferSize(sockAddr);
            response.Addr = static_cast<uint8_t *>(pvPortMalloc(response.AddrLength));
            Encoders::EncodeSockAddr(sockAddr, response.Addr);
            vPortFree(sockAddr);
            response.ResponseErrno = 0;
            TRACE_MESSAGE("GetSockPeerName: Socket handle: %d", (int) request->SocketHandle);
            TRACE_MESSAGE("GetSockPeerName: Family: %hhd", sin->sin_family);
            TRACE_MESSAGE("GetSockPeerName: IP address: %s", inet_ntoa(sin->sin_addr));
            TRACE_MESSAGE("GetSockPeerName: Port: %d", ntohs(sin->sin_port));
        }
        else
        {
            response.AddrLength = 0;
            response.Addr = nullptr;
            response.ResponseErrno = MapErrno(errno);
            ERROR_MESSAGE("Failure: Result code %d (%s), errno %d (%s)", (int) response.Result, esp_err_to_name(response.Result), errno, strerror(errno));
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }

    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedGetSockPeerNameResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeGetSockPeerNameResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    PERFORMANCE_LOGGING_EXIT();
    PERFORMANCE_LOGGING_ADD_ENTRY();
    TRACE_MESSAGE("GetSockPeerName: Exit");
}

/**
 *  @brief Expose the POSIX getsockname method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getsockname method.
 */
void WiFiRequestHandler::GetSockName(void *vpMessage)
{
    TRACE_MESSAGE("GetSockName: Enter");
    GetSockPeerName(vpMessage, WiFiFunction::GetSockName);
    TRACE_MESSAGE("GetSockName: Exit");
}

/**
 *  @brief Expose the POSIX getpeername method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the getpeername method.
 */
void WiFiRequestHandler::GetPeerName(void *vpMessage)
{
    TRACE_MESSAGE("GetPeerName: Enter");
    GetSockPeerName(vpMessage, WiFiFunction::GetPeerName);
    TRACE_MESSAGE("GetPeerName: Exit");
}

/**
 *  @brief Lock the _pollRequests object.
 */
void WiFiRequestHandler::LockPollRequests()
{
     xSemaphoreTake(_pollRequestListMutex, portMAX_DELAY);
}

/**
 *  @brief Unlock the _pollRequests object.
 */
void WiFiRequestHandler::UnlockPollRequests()
{
     xSemaphoreGive(_pollRequestListMutex);
}

/**
 *  @brief Open a dummy socket to be used by the Poll methods to terminate a poll request.
 * 
 *  @returns
 *      Handle to the socket that has just been opened of -1 if there is a problem.
 */
int WiFiRequestHandler::OpenDummySocket()
{
    TRACE_MESSAGE("OpenDummySocket: Enter");

    struct addrinfo hints = { };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo *res = NULL;

    int dummy_socket_fd = -1;
    if (getaddrinfo("localhost", "80", &hints, &res) >= 0)
    {
        dummy_socket_fd = socket(res->ai_family, res->ai_socktype, 0);
        freeaddrinfo(res);
    }
    TRACE_MESSAGE("OpenDummySocket: Exit, result %d", dummy_socket_fd);

    return(dummy_socket_fd);
}

/**
 *  @brief Debug method to dump all of the active poll requests.
 */
void WiFiRequestHandler::DumpActivePollRequests()
{
    TRACE_MESSAGE("DumpActivePollRequests: Enter");
    LockPollRequests();
    std::map<uint32_t, struct pollfd *>::iterator it = _pollRequests.begin();
    while (it != _pollRequests.end())
    {
        //
        //  fds is marked as unused as in debug mode the TRACE_MESSAGE results in the usage being thrown away.
        //  Basically, we are trying to keep the compiler happy and not generate misleading warnings.
        //
        // cppcheck-suppress unreadVariable symbolName=fds
        __attribute__ ((unused)) struct pollfd *fds = static_cast<struct pollfd *>(it->second);
        TRACE_MESSAGE("DumpActiveRequests: Key %08x, socket %d, dummy socket %d", (unsigned int) it->first, fds[POLL_FD_INDEX].fd, fds[POLL_DUMMY_FD_INDEX].fd);
        it++;
    }
    UnlockPollRequests();
    TRACE_MESSAGE("DumpActivePollRequests: Exit");
}

/**
 *  @brief Setup a poll request an assign it to a thread in the thread pool.
 * 
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
int WiFiRequestHandler::PollSetup(void *vpMessage)
{
    TRACE_MESSAGE("PollSetup: Enter");

    int result = 0;
    if (vpMessage == NULL)
    {
        return(-1);
    }
    struct pollfd *fds = static_cast<struct pollfd *>(pvPortMalloc(2 * sizeof(struct pollfd)));
    if (fds == NULL)
    {
        return(-1);
    }

    int dummy_socket_handle = OpenDummySocket();
    if (dummy_socket_handle < 0)
    {
        vPortFree(fds);
        return(-1);
    }

    Esp32Messaging::PollRequest *request = static_cast<Esp32Messaging::PollRequest *>(vpMessage);
    memset(fds, 0, 2 * sizeof(struct pollfd));
    int32_t events;
    Mapping::GetEspFlagValue(Mapping::PollEvents, request->Events, &events);

    fds[WiFiRequestHandler::POLL_FD_INDEX].fd = request->SocketHandle;
    fds[WiFiRequestHandler::POLL_FD_INDEX].events = (uint16_t) (events & 0xffff);
    fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd = dummy_socket_handle;
    fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].events = POLLIN;

    TRACE_MESSAGE("Poll: Requested Nuttx events 0x%04hx maps to ESP events 0x%04x", (unsigned short) request->Events, (int) events);

    LockPollRequests();
    _pollRequests[request->SetupMessageId] = fds;
    UnlockPollRequests();

    #if defined(WIFI_DEBUG)
        DumpActivePollRequests();
    #endif

    SystemRequestHandler::SharedThreadPool()->Execute(PollSocket, reinterpret_cast<void *>(request->SetupMessageId));

    TRACE_MESSAGE("PollSetup: Exit, result %d", result);

    return(result);
}

/**
 *  @brief Teardown a poll request that has previously been setup by PollSetup.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
int WiFiRequestHandler::PollTeardown(void *vpMessage)
{
    TRACE_MESSAGE("PollTeardown: Enter");
    int result = 0;

    Esp32Messaging::PollRequest *request = (Esp32Messaging::PollRequest *) vpMessage;

    #if defined(WIFI_DEBUG)
        DumpActivePollRequests();
    #endif

    LockPollRequests();
    std::map<uint32_t, struct pollfd *>::iterator it = _pollRequests.find(request->SetupMessageId);
    if (it != _pollRequests.end())
    {
        TRACE_MESSAGE("PollTeardown: Found socket handle for request 0x%08x", (unsigned int) request->SetupMessageId);
        struct pollfd *fds = static_cast<struct pollfd *>(it->second);
        result = write(fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd, "E", 1);      // Writing to the dummy file descriptor should end the poll request.
        if (close(fds[WiFiRequestHandler::POLL_DUMMY_FD_INDEX].fd) < 0)
        {
            result = -1;
        }
        vPortFree(fds);
        _pollRequests.erase(it);
    }
    else
    {
        //
        //  We do not treat this as an error as the request information may have been removed by the 
        //  interrupt handler.
        //
        TRACE_MESSAGE("PollTeardown: Cannot find request %08x", (unsigned int) request->SetupMessageId);
    }
    UnlockPollRequests();

    #if defined(WIFI_DEBUG)
        DumpActivePollRequests();
    #endif

    TRACE_MESSAGE("PollTeardown: Exit, result %d", result);

    return(result);
}

/**
 *  @brief Execute the actual request to poll a socket.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
void WiFiRequestHandler::PollSocket(void *requestId)
{
    TRACE_MESSAGE("PollSocket: Enter");

    Esp32Messaging::InterruptPollResponse response = { };

    LockPollRequests();
    std::map<uint32_t, struct pollfd *>::iterator it = _pollRequests.find((uint32_t) requestId);
    UnlockPollRequests();
    if (it != _pollRequests.end())
    {
        struct pollfd *fds = static_cast<struct pollfd *>(it->second);
        response.Result = poll(fds, 2, -1);
        if (response.Result < 0)
        {
            response.ResponseErrno = MapErrno(errno);
        }
        else
        {
            LockPollRequests();
            it = _pollRequests.find((uint32_t) requestId);      // Get again in case anything has changed.
            if (it != _pollRequests.end())
            {
                int32_t events;
                Mapping::GetStmFlagValue(Mapping::PollEvents, fds[POLL_FD_INDEX].revents, &events);
                response.ReturnedEvents = (uint16_t) (events & 0xffff);
                if (response.ReturnedEvents != 0)
                {
                    response.Result = 1;
                }
                TRACE_MESSAGE("Poll: Returned ESP32 events 0x%04hx maps to NuttX events 0x%04x", fds[POLL_FD_INDEX].revents, (unsigned int) events);
                response.SetupMessageId = (uint32_t) requestId;
                close(fds[POLL_DUMMY_FD_INDEX].fd);
                vPortFree(fds);
                _pollRequests.erase(it);
            }
            else
            {
                response.Result = -1;
                response.ResponseErrno = MapErrno(EFAULT);
            }
            UnlockPollRequests();
        }
    }
    TRACE_MESSAGE("PollSocket: Return value from poll: %d", (int) response.Result);

    Message *message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));
    *message = { };
    message->Interface = Esp32Interfaces::WiFi;
    message->Function = WiFiFunction::InterruptPollResponse;
    message->MessageID = _messageDispatcher->GetNextMessageID();
    message->PayloadLength = Encoders::EncodedInterruptPollResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));

    Encoders::EncodeInterruptPollResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Event;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("PollSocket: Exit");
}

/**
 *  @brief Expose the poll method to the STM32.
 *
 *  @param message
 *      Message structure containing the parameters for the poll method.
 */
void WiFiRequestHandler::Poll(void *vpMessage)
{
    TRACE_MESSAGE("Poll: Enter");
    Message *message = static_cast<Message *>(vpMessage);
    Esp32Messaging::IntegerAndErrnoResponse response = { };

    if (IsConnected())
    {
        Esp32Messaging::PollRequest *request = Encoders::ExtractPollRequest(message->Payload);
        if (request->Setup == 1)
        {
            response.Result = PollSetup(static_cast<void *>(request));
        }
        else
        {
            response.Result = PollTeardown(static_cast<void *>(request));
        }

        if (response.Result < 0)
        {
            response.ResponseErrno = MapErrno(errno);
        }
        vPortFree(request);
    }
    else
    {
        response.Result = -1;
        response.ResponseErrno = MapErrno(ENETDOWN);
    }   
    vPortFree(message->Payload);

    message->PayloadLength = Encoders::EncodedIntegerAndErrnoResponseBufferSize(&response);
    message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PayloadLength));
    Encoders::EncodeIntegerAndErrnoResponse(&response, message->Payload);
    message->MessageType = MessageTypes::Response;
    message->StatusCode = StatusCodes::CompletedOk;
    _messageDispatcher->QueueMessageForStm32(message);

    TRACE_MESSAGE("Poll: Exit");
}
