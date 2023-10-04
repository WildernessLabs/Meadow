
#include "Encoders.hpp"


#include "Encoders.hpp"
#include "Logging.hpp"
#include "SpiTransportProvider.hpp"

/**
 *  Calculate the amount of memory that should be allocated for
 *  the receive buffer.
 *
 *  SPI reception on the ESP32 should be on a 32-bit boundary and
 *  also a multiple of 4 bytes long (See the article linked below).
 *
 *  https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/spi_slave.html#restrictions-and-known-issues
 *
 *  @param requestedSize
 *      Actual amount of data requested / to be received.
 *
 *  @returns
 *      Number of bytes that should be allocated.
 */
uint32_t Encoders::CalculateSpiBufferSize(uint32_t requestedSize)
{
    uint32_t result = requestedSize;

    if (result < 8)
    {
        result = 8;
    }
    if ((requestedSize & 3) != 0)
    {
        result = (requestedSize & 0xfffffffc) + 4;
    }
    //
    //  There is an issue with the SPI interface that can corrupt the last
    //  few bytes of a message to pad this message out and we will ignore
    //  the last few bytes when encoding / decoding.
    //
    result += Message::SPI_MESSAGE_OVERHEAD;
    return (result);
}

/**
 *  Calculate the minimum amount of space needed to store an encoded message.
 * 
 *  Assumptions:
 *      1 - The packet information has been set up correctly.
 * 
 *  @param message
 *      Message to be encoded.
 * 
 *  @param headerOnly
 *      Are we just sending the header?
 * 
 *  @returns
 *      Number of bytes needed to contain the message.
 */
uint32_t Encoders::EncodedPacketSize(Message *message, bool headerOnly)
{
    uint32_t result = Message::HEADER_SIZE;

    if (!headerOnly)
    {
        result += message->PacketLength;
    }

    return(result);
}

/**
 *  Take the first two bytes from the buffer and encode them as a 16 bit integer.
 *
 *  Note that the data should be encoded as LSB first.
 *
 *  @param buffer
 *      Pointer to the buffer holding the data that should be used to created the
 *      16-bit unsigned integer.
 *
 *  @returns
 *      16-bit unsigned integer value from the first two byte in the buffer.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractUInt16
uint16_t Encoders::ExtractUInt16(uint8_t *buffer)
{
    uint16_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    return (result);
}

/**
 *  Encode a 16-bit integer as two bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 *  @param value
 *      16-bit value to encode in the first two bytes of the buffer.
 *
 *  @param buffer
 *      Pointer to the buffer where the two bytes will be inserted
 *      from the 16-bit unsigned integer (LSB first).
 */
// cppcheck-suppress unusedFunction symbolName=EncodeUInt16
void Encoders::EncodeUInt16(uint16_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
}

/**
 *  Take the first four bytes from the buffer and encode them as a 32 bit unsigned integer.
 *
 *  Note that the data should be encoded as LSB first.
 *
 *  @param buffer
 *      Pointer to the buffer where the next four bytes should be extracted
 *      and a 32-bit unsigned integer created (LSB first).
 *
 *  @returns
 *      32-bit unsigned integer extracted from the first 4 bytes in the buffer.
 */
uint32_t Encoders::ExtractUInt32(uint8_t *buffer)
{
    uint32_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    result |= (buffer[2] << 16);
    result |= (buffer[3] << 24);
    return (result);
}

/**
 *  Encode a 32-bit unsigned integer as four bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 *  @param value
 *      Unsigned 32-bit integer value to write into the buffer.
 *
 *  @param buffer
 *      Pointer to the buffer where 4 bytes will be replaced with the four
 *      bytes representing unsigned integer (LSB first).
 */
void Encoders::EncodeUInt32(uint32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/**
 *  Take the first four bytes from the buffer and encode them as a 32 bit integer.
 *
 *  Note that the data should be encoded as LSB first.
 *
 *  @param buffer
 *      Pointer to the buffer where the next four bytes should be extracted
 *      and a 32-bit unsigned integer created (LSB first).
 *
 *  @returns
 *      32-bit unsigned integer extracted from the first 4 bytes in the buffer.
 */
uint32_t Encoders::ExtractInt32(uint8_t *buffer)
{
    int32_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    result |= (buffer[2] << 16);
    result |= (buffer[3] << 24);
    return (result);
}

/**
 *  Encode a 32-bit  integer as four bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 *  @param value
 *      Unsigned 32-bit integer value to write into the buffer.
 *
 *  @param buffer
 *      Pointer to the buffer where 4 bytes will be replaced with the four
 *      bytes representing unsigned integer (LSB first).
 */
void Encoders::EncodeInt32(int32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/**
 *  Extract a string (terminated by 0) from a block of memory.
 *
 *  @param buffer
 *      Pointer to the block of memory containing the string.
 *
 *  @returns
 *      Pointer to the extracted string.
 */
char *Encoders::ExtractString(uint8_t *buffer)
{
    uint32_t length = 0;
    uint8_t *ptr = buffer;

    while (*ptr != 0)
    {
        ptr++;
        length++;
    }

    if (length == 0)
    {
        ptr = nullptr;
    }
    else
    {
        ptr = (uint8_t *) pvPortMalloc(length + 1);
        if (ptr != NULL)
        {
            strcpy((char *) ptr, (char *) buffer);
        }
        else
        {
            ptr = nullptr;
        }
    }
    return((char *) ptr);
}

/**
 *  Copy the string into the buffer.
 *
 *  @param source
 *      Block of memory containing the string.
 *
 *  @param buffer
 *      Pointer to a block of memory to take the string.
 */
void Encoders::EncodeString(char *source, uint8_t *buffer)
{
    if ((source == nullptr) || (source == NULL))
    {
        *buffer = 0;
    }
    else
    {
        strcpy((char *) buffer, (char *) source);
    }
}

/**
 *  Get the length of a string taking into consideration the
 *  fact that the string could be nullptr or NULL.
 *
 *  @param str
 *      Block of memory containing the string.
 *  
 *  @returns
 *      Length of the string.
 */
uint32_t Encoders::StringLength(char *str)
{
    uint32_t length = 0;

    if ((str != nullptr) || (str != NULL))
    {
        length = strlen(str);
    }
    return(length);
}

/**
 *  Calculate the 8-bit CRC value for the specified data buffer.
 *
 *  This algorithm is loosely based upon the Dallas 1-Wire algorithm.
 *
 *  @param data
 *      Pointer to the buffer containing the data for which the CRC should be calculated.
 *
 *  @param len
 *      Number of bytes in the data buffer.
 *
 *  @returns
 *      8-bit checksum of the bytes in the buffer.
 */
// cppcheck-suppress unusedFunction symbolName=CRC8
uint8_t Encoders::CRC8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        uint8_t byte = data[byteCounter];
        for (uint8_t bitCounter = 8; bitCounter; bitCounter--)
        {
            uint8_t sum = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (sum)
            {
                crc ^= 0x8C;
            }
            byte >>= 1;
        }
    }
    return(crc);
}

/**
 *  Calculate the 32-bit CRC of the buffer of data.
 *
 *  @param data
 *      Pointer to the buffer containing the data for which the CRC should be calculated.
 *
 *  @param len
 *      Number of bytes in the data buffer.
 *
 *  @returns
 *      32-bit checksum of the bytes in the buffer.
 */
uint32_t Encoders::CRC32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = CRC32_SEED;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        crc = ProgressiveCRC32(crc, data[byteCounter]);
    }
    return (crc);
}

/**
 *  Calculate the CRC32 value change for the specified byte.
 *
 *  @param currentChecksum
 *      Current value of the checksum.
 *
 *  @param byte
 *      Next byte to use in the checksum calculation.
 *
 *  @returns
 *      Next checksum value.
 */
uint32_t Encoders::ProgressiveCRC32(uint32_t currentChecksum, uint8_t byte)
{
    uint32_t crc = currentChecksum;
    crc ^= byte;
    for (uint32_t index = 0; index < 8; index++)
    {
        uint32_t mask = (uint32_t) -(crc & 0x01);
        crc = (crc >> 1) ^ (0xedb88320 & mask);
    }
    return (crc);
}

/**
 *  @brief Extract a message from a block of memory and create a new Message object.
 *
 *  Note:
 *  There are four bytes at the end of the message that may be corrupted so
 *  we will pad out the message with four additional bytes and ignore these
 *  four bytes when calculating CRCs (we do not know what they will contain
 *  post transmission).
 * 
 *  @param buffer
 *      Byte (uint8_t) array containing the encoded message.
 *
 *  @param bufferLength
 *      Size of the buffer.
 *
 *  @param headerOnly
 *      Indicate if we should extract the header information only
 *      (i.e. do not copy the payload).
 *
 *  @returns
 *      New Message object extracted from the buffer.  The Message object
 *     Will be placed on the FreeRTOS heap (use vPortFree to release the
 *     storage).
 */
Message *Encoders::ExtractMessage(uint8_t *buffer, uint32_t bufferLength, bool headerOnly)
{
    uint32_t crc = ExtractUInt32(buffer + Message::CRC_OFFSET);
    EncodeUInt32(0, buffer + Message::CRC_OFFSET);
    
    Message *message = nullptr;
    if (crc == CRC32(buffer, bufferLength - 4))             // See note in comment above.
    {
        message = static_cast<Message *>(pvPortMalloc(sizeof(Message)));

        memset(static_cast<void *>(message), 0, sizeof(Message));
        buffer += 5;                                        // Skip the protocol and CRC.
        message->PacketOffset = ExtractUInt16(buffer);
        buffer += 2;
        message->PacketLength = ExtractUInt16(buffer);
        buffer += 2;
        message->MessageType = *buffer;
        buffer++;
        message->Interface = *buffer;
        buffer++;
        message->Function = ExtractUInt32(buffer);
        buffer += 4;
        message->StatusCode = ExtractUInt32(buffer);
        buffer += 4;
        message->MessageID = ExtractUInt32(buffer);
        buffer += 4;
        message->PayloadLength = ExtractUInt32(buffer);
        buffer += 4;
        if (!headerOnly && (message->PayloadLength > 0))
        {
            message->Payload = static_cast<uint8_t *>(pvPortMalloc(message->PacketLength));
            if (message->Payload == NULL)
            {
                vPortFree(message);
                message = nullptr;
            }
            else
            {
                memcpy(message->Payload, buffer, message->PacketLength);
            }
        }
        else
        {
            message->Payload = nullptr;
        }
    }
    return(message);
}

/**
 *  @brief Encode a message into a block of memory.
 *
 *  @param message
 *      Message structure to encode in to the byte buffer.
 * 
 *  @param buffer
 *      Pointer to the block of memory that will contain the encoded message.
 *
 *  @param headerOnly
 *      Copy this as a header only (i.e. do not copy the payload).
 *
 *  @returns
 *     Byte buffer containing the encoded message.  The storage
 *     Will be placed on the FreeRTOS heap (use vPortFree to release the
 *     storage).
 */
void Encoders::EncodeMessage(Message *message, uint8_t *buffer, bool headerOnly)
{
    uint32_t messageSize = EncodedPacketSize(message, headerOnly);
    uint32_t bufferSize = Encoders::CalculateSpiBufferSize(messageSize);
    TRACE_MESSAGE_SPECIFY_COMPONENT(SpiTransportProvider::COMPONENT_NAME, "Encoding %u bytes as a %u byte packet", (unsigned int) messageSize, (unsigned int) bufferSize);

    if (buffer)
    {
        uint8_t *next_location = buffer;
        
        memset(buffer, 0, bufferSize);
        *next_location = Message::PROTOCOL_NUMBER;
        next_location++;
        EncodeUInt32(0, next_location);
        next_location += 4;
        EncodeUInt16(message->PacketOffset, next_location);
        next_location += 2;
        EncodeUInt16(message->PacketLength, next_location);
        next_location += 2;
        *next_location = message->MessageType;
        next_location++;
        *next_location = message->Interface;
        next_location++;
        EncodeUInt32(message->Function, next_location);
        next_location += 4;
        EncodeUInt32(message->StatusCode, next_location);
        next_location += 4;
        EncodeUInt32(message->MessageID, next_location);
        next_location += 4;
        EncodeUInt32(message->PayloadLength, next_location);
        next_location += 4;
        //
        //  The byte after the PayloadLength is the CRC.  This is filled in later
        //  and so a 0 is written into the message temporarily.
        //
        if (!headerOnly && (message->PacketLength > 0))
        {
            memcpy(next_location, message->Payload + message->PacketOffset, message->PacketLength);
        }
        uint32_t crc = CRC32(buffer, bufferSize - 4);
        EncodeUInt32(crc, buffer + Message::CRC_OFFSET);
    }
}

/*
 *******************************************************************************

       THE METHODS BELOW HAVE BEEN AUTOMATICALLY GENERATED BASED UPON THE
       Messages.json METADATA FILE.

 *******************************************************************************
*/

/**
 *  Convert the SystemConfiguration object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSystemConfigurationBufferSize method.
 *  
 *  @param systemConfiguration
 *      SystemConfiguration object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SystemConfiguration object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSystemConfiguration
void Encoders::EncodeSystemConfiguration(Esp32Messaging::SystemConfiguration *systemConfiguration, uint8_t *buffer)
{
    *buffer = systemConfiguration->MaximumMessageQueueLength;
    buffer += 1;
    EncodeInt32(systemConfiguration->MaximumRetryCount, buffer);
    buffer += 4;
    *buffer = systemConfiguration->Antenna;
    buffer += 1;
    memcpy((void *) buffer, (void *) systemConfiguration->BoardMacAddress, 6);
    buffer += 6;
    memcpy((void *) buffer, (void *) systemConfiguration->SoftApMacAddress, 6);
    buffer += 6;
    memcpy((void *) buffer, (void *) systemConfiguration->BluetoothMacAddress, 6);
    buffer += 6;
    EncodeString(systemConfiguration->DeviceName, buffer);
    buffer += StringLength(systemConfiguration->DeviceName) + 1;
    EncodeString(systemConfiguration->DefaultAccessPoint, buffer);
    buffer += StringLength(systemConfiguration->DefaultAccessPoint) + 1;
    *buffer = systemConfiguration->ResetReason;
    buffer += 1;
    EncodeUInt32(systemConfiguration->VersionMajor, buffer);
    buffer += 4;
    EncodeUInt32(systemConfiguration->VersionMinor, buffer);
    buffer += 4;
    EncodeUInt32(systemConfiguration->VersionRevision, buffer);
    buffer += 4;
    EncodeUInt32(systemConfiguration->VersionBuild, buffer);
    buffer += 4;
    *buffer = systemConfiguration->BuildDay;
    buffer += 1;
    *buffer = systemConfiguration->BuildMonth;
    buffer += 1;
    *buffer = systemConfiguration->BuildYear;
    buffer += 1;
    *buffer = systemConfiguration->BuildHour;
    buffer += 1;
    *buffer = systemConfiguration->BuildMinute;
    buffer += 1;
    *buffer = systemConfiguration->BuildSecond;
    buffer += 1;
    EncodeUInt32(systemConfiguration->BuildHash, buffer);
    buffer += 4;
    EncodeString(systemConfiguration->BuildBranchName, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SystemConfiguration object.
 *  
 *  @param systemConfiguration
 *      SystemConfiguration object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SystemConfiguration object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSystemConfigurationBufferSize
int Encoders::EncodedSystemConfigurationBufferSize(Esp32Messaging::SystemConfiguration *systemConfiguration)
{
    int result = 0;
    result += Encoders::StringLength(systemConfiguration->DeviceName);
    result += Encoders::StringLength(systemConfiguration->DefaultAccessPoint);
    result += Encoders::StringLength(systemConfiguration->BuildBranchName);
    return(result + 54);
}

/**
 *  Extract the SystemConfiguration object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SystemConfiguration object.
 *  
 *  @returns
 *      Pointer to a SystemConfiguration object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSystemConfiguration
Esp32Messaging::SystemConfiguration *Encoders::ExtractSystemConfiguration(uint8_t *buffer)
{
    Esp32Messaging::SystemConfiguration *systemConfiguration = (Esp32Messaging::SystemConfiguration *) pvPortMalloc(sizeof(Esp32Messaging::SystemConfiguration));

    systemConfiguration->MaximumMessageQueueLength = *buffer;
    buffer += 1;
    systemConfiguration->MaximumRetryCount = ExtractInt32(buffer);
    buffer += 4;
    systemConfiguration->Antenna = *buffer;
    buffer += 1;
    memcpy((void *) systemConfiguration->BoardMacAddress, (void *) buffer, 6);
    buffer += 6;
    memcpy((void *) systemConfiguration->SoftApMacAddress, (void *) buffer, 6);
    buffer += 6;
    memcpy((void *) systemConfiguration->BluetoothMacAddress, (void *) buffer, 6);
    buffer += 6;
    systemConfiguration->DeviceName = ExtractString(buffer);
    buffer += Encoders::StringLength(systemConfiguration->DeviceName) + 1;
    systemConfiguration->DefaultAccessPoint = ExtractString(buffer);
    buffer += Encoders::StringLength(systemConfiguration->DefaultAccessPoint) + 1;
    systemConfiguration->ResetReason = *buffer;
    buffer += 1;
    systemConfiguration->VersionMajor = ExtractUInt32(buffer);
    buffer += 4;
    systemConfiguration->VersionMinor = ExtractUInt32(buffer);
    buffer += 4;
    systemConfiguration->VersionRevision = ExtractUInt32(buffer);
    buffer += 4;
    systemConfiguration->VersionBuild = ExtractUInt32(buffer);
    buffer += 4;
    systemConfiguration->BuildDay = *buffer;
    buffer += 1;
    systemConfiguration->BuildMonth = *buffer;
    buffer += 1;
    systemConfiguration->BuildYear = *buffer;
    buffer += 1;
    systemConfiguration->BuildHour = *buffer;
    buffer += 1;
    systemConfiguration->BuildMinute = *buffer;
    buffer += 1;
    systemConfiguration->BuildSecond = *buffer;
    buffer += 1;
    systemConfiguration->BuildHash = ExtractUInt32(buffer);
    buffer += 4;
    systemConfiguration->BuildBranchName = ExtractString(buffer);
    return(systemConfiguration);
}

/**
 *  Convert the ConfigurationValue object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedConfigurationValueBufferSize method.
 *  
 *  @param configurationValue
 *      ConfigurationValue object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ConfigurationValue object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeConfigurationValue
void Encoders::EncodeConfigurationValue(Esp32Messaging::ConfigurationValue *configurationValue, uint8_t *buffer)
{
    EncodeUInt32(configurationValue->Item, buffer);
    buffer += 4;
    EncodeUInt32(configurationValue->ValueLength, buffer);
    buffer += 4;
    if (configurationValue->ValueLength > 0)
    {
        memcpy((void *) buffer, (void *) configurationValue->Value, configurationValue->ValueLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ConfigurationValue object.
 *  
 *  @param configurationValue
 *      ConfigurationValue object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded ConfigurationValue object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedConfigurationValueBufferSize
int Encoders::EncodedConfigurationValueBufferSize(Esp32Messaging::ConfigurationValue *configurationValue)
{
    int result = 0;
    result += configurationValue->ValueLength;
    return(result + 8);
}

/**
 *  Extract the ConfigurationValue object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ConfigurationValue object.
 *  
 *  @returns
 *      Pointer to a ConfigurationValue object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractConfigurationValue
Esp32Messaging::ConfigurationValue *Encoders::ExtractConfigurationValue(uint8_t *buffer)
{
    Esp32Messaging::ConfigurationValue *configurationValue = (Esp32Messaging::ConfigurationValue *) pvPortMalloc(sizeof(Esp32Messaging::ConfigurationValue));

    configurationValue->Item = ExtractUInt32(buffer);
    buffer += 4;
    configurationValue->ValueLength = ExtractUInt32(buffer);
    buffer += 4;
    if (configurationValue->ValueLength > 0)
    {
        configurationValue->Value = (uint8_t *) pvPortMalloc(configurationValue->ValueLength);
        memcpy(configurationValue->Value, buffer, configurationValue->ValueLength);
        buffer += configurationValue->ValueLength;
    }
    else
    {
        configurationValue->Value = nullptr;
    }
    return(configurationValue);
}

/**
 *  Convert the ErrorEvent object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedErrorEventBufferSize method.
 *  
 *  @param errorEvent
 *      ErrorEvent object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ErrorEvent object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeErrorEvent
void Encoders::EncodeErrorEvent(Esp32Messaging::ErrorEvent *errorEvent, uint8_t *buffer)
{
    EncodeUInt32(errorEvent->ErrorCode, buffer);
    buffer += 4;
    *buffer = errorEvent->Interface;
    buffer += 1;
    EncodeUInt32(errorEvent->ErrorDataLength, buffer);
    buffer += 4;
    if (errorEvent->ErrorDataLength > 0)
    {
        memcpy((void *) buffer, (void *) errorEvent->ErrorData, errorEvent->ErrorDataLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ErrorEvent object.
 *  
 *  @param errorEvent
 *      ErrorEvent object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded ErrorEvent object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedErrorEventBufferSize
int Encoders::EncodedErrorEventBufferSize(Esp32Messaging::ErrorEvent *errorEvent)
{
    int result = 0;
    result += errorEvent->ErrorDataLength;
    return(result + 9);
}

/**
 *  Extract the ErrorEvent object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ErrorEvent object.
 *  
 *  @returns
 *      Pointer to a ErrorEvent object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractErrorEvent
Esp32Messaging::ErrorEvent *Encoders::ExtractErrorEvent(uint8_t *buffer)
{
    Esp32Messaging::ErrorEvent *errorEvent = (Esp32Messaging::ErrorEvent *) pvPortMalloc(sizeof(Esp32Messaging::ErrorEvent));

    errorEvent->ErrorCode = ExtractUInt32(buffer);
    buffer += 4;
    errorEvent->Interface = *buffer;
    buffer += 1;
    errorEvent->ErrorDataLength = ExtractUInt32(buffer);
    buffer += 4;
    if (errorEvent->ErrorDataLength > 0)
    {
        errorEvent->ErrorData = (uint8_t *) pvPortMalloc(errorEvent->ErrorDataLength);
        memcpy(errorEvent->ErrorData, buffer, errorEvent->ErrorDataLength);
        buffer += errorEvent->ErrorDataLength;
    }
    else
    {
        errorEvent->ErrorData = nullptr;
    }
    return(errorEvent);
}

/**
 *  Convert the AccessPointInformation object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedAccessPointInformationBufferSize method.
 *  
 *  @param accessPointInformation
 *      AccessPointInformation object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AccessPointInformation object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeAccessPointInformation
void Encoders::EncodeAccessPointInformation(Esp32Messaging::AccessPointInformation *accessPointInformation, uint8_t *buffer)
{
    EncodeString(accessPointInformation->NetworkName, buffer);
    buffer += StringLength(accessPointInformation->NetworkName) + 1;
    EncodeString(accessPointInformation->Password, buffer);
    buffer += StringLength(accessPointInformation->Password) + 1;
    EncodeUInt32(accessPointInformation->IpAddress, buffer);
    buffer += 4;
    EncodeUInt32(accessPointInformation->SubnetMask, buffer);
    buffer += 4;
    EncodeUInt32(accessPointInformation->Gateway, buffer);
    buffer += 4;
    *buffer = accessPointInformation->WiFiAuthenticationMode;
    buffer += 1;
    *buffer = accessPointInformation->Channel;
    buffer += 1;
    *buffer = accessPointInformation->Hidden;
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AccessPointInformation object.
 *  
 *  @param accessPointInformation
 *      AccessPointInformation object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded AccessPointInformation object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedAccessPointInformationBufferSize
int Encoders::EncodedAccessPointInformationBufferSize(Esp32Messaging::AccessPointInformation *accessPointInformation)
{
    int result = 0;
    result += Encoders::StringLength(accessPointInformation->NetworkName);
    result += Encoders::StringLength(accessPointInformation->Password);
    return(result + 17);
}

/**
 *  Extract the AccessPointInformation object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPointInformation object.
 *  
 *  @returns
 *      Pointer to a AccessPointInformation object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAccessPointInformation
Esp32Messaging::AccessPointInformation *Encoders::ExtractAccessPointInformation(uint8_t *buffer)
{
    Esp32Messaging::AccessPointInformation *accessPointInformation = (Esp32Messaging::AccessPointInformation *) pvPortMalloc(sizeof(Esp32Messaging::AccessPointInformation));

    accessPointInformation->NetworkName = ExtractString(buffer);
    buffer += Encoders::StringLength(accessPointInformation->NetworkName) + 1;
    accessPointInformation->Password = ExtractString(buffer);
    buffer += Encoders::StringLength(accessPointInformation->Password) + 1;
    accessPointInformation->IpAddress = ExtractUInt32(buffer);
    buffer += 4;
    accessPointInformation->SubnetMask = ExtractUInt32(buffer);
    buffer += 4;
    accessPointInformation->Gateway = ExtractUInt32(buffer);
    buffer += 4;
    accessPointInformation->WiFiAuthenticationMode = *buffer;
    buffer += 1;
    accessPointInformation->Channel = *buffer;
    buffer += 1;
    accessPointInformation->Hidden = *buffer;
    return(accessPointInformation);
}

/**
 *  Convert the DisconnectFromAccessPointRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedDisconnectFromAccessPointRequestBufferSize method.
 *  
 *  @param disconnectFromAccessPointRequest
 *      DisconnectFromAccessPointRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded DisconnectFromAccessPointRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeDisconnectFromAccessPointRequest
void Encoders::EncodeDisconnectFromAccessPointRequest(Esp32Messaging::DisconnectFromAccessPointRequest *disconnectFromAccessPointRequest, uint8_t *buffer)
{
    *buffer = disconnectFromAccessPointRequest->TurnOffWiFiInterface;
}

/**
 *  Calculate the amount of memory required to hold the given instance of the DisconnectFromAccessPointRequest object.
 *  
 *  @param disconnectFromAccessPointRequest
 *      DisconnectFromAccessPointRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded DisconnectFromAccessPointRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedDisconnectFromAccessPointRequestBufferSize
int Encoders::EncodedDisconnectFromAccessPointRequestBufferSize(Esp32Messaging::DisconnectFromAccessPointRequest *disconnectFromAccessPointRequest)
{
    return(1);
}

/**
 *  Extract the DisconnectFromAccessPointRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded DisconnectFromAccessPointRequest object.
 *  
 *  @returns
 *      Pointer to a DisconnectFromAccessPointRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractDisconnectFromAccessPointRequest
Esp32Messaging::DisconnectFromAccessPointRequest *Encoders::ExtractDisconnectFromAccessPointRequest(uint8_t *buffer)
{
    Esp32Messaging::DisconnectFromAccessPointRequest *disconnectFromAccessPointRequest = (Esp32Messaging::DisconnectFromAccessPointRequest *) pvPortMalloc(sizeof(Esp32Messaging::DisconnectFromAccessPointRequest));

    disconnectFromAccessPointRequest->TurnOffWiFiInterface = *buffer;
    return(disconnectFromAccessPointRequest);
}

/**
 *  Convert the ConnectEventData object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedConnectEventDataBufferSize method.
 *  
 *  @param connectEventData
 *      ConnectEventData object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ConnectEventData object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeConnectEventData
void Encoders::EncodeConnectEventData(Esp32Messaging::ConnectEventData *connectEventData, uint8_t *buffer)
{
    EncodeUInt32(connectEventData->IpAddress, buffer);
    buffer += 4;
    EncodeUInt32(connectEventData->SubnetMask, buffer);
    buffer += 4;
    EncodeUInt32(connectEventData->Gateway, buffer);
    buffer += 4;
    memcpy((void *) buffer, (void *) connectEventData->Ssid, 33);
    buffer += 33;
    memcpy((void *) buffer, (void *) connectEventData->Bssid, 6);
    buffer += 6;
    *buffer = connectEventData->Channel;
    buffer += 1;
    *buffer = connectEventData->AuthenticationMode;
    buffer += 1;
    EncodeUInt32(connectEventData->Reason, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ConnectEventData object.
 *  
 *  @param connectEventData
 *      ConnectEventData object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded ConnectEventData object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedConnectEventDataBufferSize
int Encoders::EncodedConnectEventDataBufferSize(Esp32Messaging::ConnectEventData *connectEventData)
{
    return(57);
}

/**
 *  Extract the ConnectEventData object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ConnectEventData object.
 *  
 *  @returns
 *      Pointer to a ConnectEventData object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractConnectEventData
Esp32Messaging::ConnectEventData *Encoders::ExtractConnectEventData(uint8_t *buffer)
{
    Esp32Messaging::ConnectEventData *connectEventData = (Esp32Messaging::ConnectEventData *) pvPortMalloc(sizeof(Esp32Messaging::ConnectEventData));

    connectEventData->IpAddress = ExtractUInt32(buffer);
    buffer += 4;
    connectEventData->SubnetMask = ExtractUInt32(buffer);
    buffer += 4;
    connectEventData->Gateway = ExtractUInt32(buffer);
    buffer += 4;
    memcpy((void *) connectEventData->Ssid, (void *) buffer, 33);
    buffer += 33;
    memcpy((void *) connectEventData->Bssid, (void *) buffer, 6);
    buffer += 6;
    connectEventData->Channel = *buffer;
    buffer += 1;
    connectEventData->AuthenticationMode = *buffer;
    buffer += 1;
    connectEventData->Reason = ExtractUInt32(buffer);
    return(connectEventData);
}

/**
 *  Convert the NodeConnectionChangeEventData object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedNodeConnectionChangeEventDataBufferSize method.
 *  
 *  @param nodeConnectionChangeEventData
 *      NodeConnectionChangeEventData object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded NodeConnectionChangeEventData object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeNodeConnectionChangeEventData
void Encoders::EncodeNodeConnectionChangeEventData(Esp32Messaging::NodeConnectionChangeEventData *nodeConnectionChangeEventData, uint8_t *buffer)
{
    EncodeUInt32(nodeConnectionChangeEventData->IpAddress, buffer);
    buffer += 4;
    memcpy((void *) buffer, (void *) nodeConnectionChangeEventData->MacAddress, 6);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the NodeConnectionChangeEventData object.
 *  
 *  @param nodeConnectionChangeEventData
 *      NodeConnectionChangeEventData object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded NodeConnectionChangeEventData object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedNodeConnectionChangeEventDataBufferSize
int Encoders::EncodedNodeConnectionChangeEventDataBufferSize(Esp32Messaging::NodeConnectionChangeEventData *nodeConnectionChangeEventData)
{
    return(10);
}

/**
 *  Extract the NodeConnectionChangeEventData object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded NodeConnectionChangeEventData object.
 *  
 *  @returns
 *      Pointer to a NodeConnectionChangeEventData object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractNodeConnectionChangeEventData
Esp32Messaging::NodeConnectionChangeEventData *Encoders::ExtractNodeConnectionChangeEventData(uint8_t *buffer)
{
    Esp32Messaging::NodeConnectionChangeEventData *nodeConnectionChangeEventData = (Esp32Messaging::NodeConnectionChangeEventData *) pvPortMalloc(sizeof(Esp32Messaging::NodeConnectionChangeEventData));

    nodeConnectionChangeEventData->IpAddress = ExtractUInt32(buffer);
    buffer += 4;
    memcpy((void *) nodeConnectionChangeEventData->MacAddress, (void *) buffer, 6);
    return(nodeConnectionChangeEventData);
}

/**
 *  Convert the DisconnectEventData object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedDisconnectEventDataBufferSize method.
 *  
 *  @param disconnectEventData
 *      DisconnectEventData object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded DisconnectEventData object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeDisconnectEventData
void Encoders::EncodeDisconnectEventData(Esp32Messaging::DisconnectEventData *disconnectEventData, uint8_t *buffer)
{
    *buffer = disconnectEventData->Retrying;
    buffer += 1;
    EncodeInt32(disconnectEventData->RetriesRemaining, buffer);
    buffer += 4;
    EncodeUInt32(disconnectEventData->Reason, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the DisconnectEventData object.
 *  
 *  @param disconnectEventData
 *      DisconnectEventData object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded DisconnectEventData object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedDisconnectEventDataBufferSize
int Encoders::EncodedDisconnectEventDataBufferSize(Esp32Messaging::DisconnectEventData *disconnectEventData)
{
    return(9);
}

/**
 *  Extract the DisconnectEventData object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded DisconnectEventData object.
 *  
 *  @returns
 *      Pointer to a DisconnectEventData object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractDisconnectEventData
Esp32Messaging::DisconnectEventData *Encoders::ExtractDisconnectEventData(uint8_t *buffer)
{
    Esp32Messaging::DisconnectEventData *disconnectEventData = (Esp32Messaging::DisconnectEventData *) pvPortMalloc(sizeof(Esp32Messaging::DisconnectEventData));

    disconnectEventData->Retrying = *buffer;
    buffer += 1;
    disconnectEventData->RetriesRemaining = ExtractInt32(buffer);
    buffer += 4;
    disconnectEventData->Reason = ExtractUInt32(buffer);
    return(disconnectEventData);
}

/**
 *  Convert the AccessPoint object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedAccessPointBufferSize method.
 *  
 *  @param accessPoint
 *      AccessPoint object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AccessPoint object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeAccessPoint
void Encoders::EncodeAccessPoint(Esp32Messaging::AccessPoint *accessPoint, uint8_t *buffer)
{
    memcpy((void *) buffer, (void *) accessPoint->Ssid, 33);
    buffer += 33;
    memcpy((void *) buffer, (void *) accessPoint->Bssid, 6);
    buffer += 6;
    *buffer = accessPoint->PrimaryChannel;
    buffer += 1;
    *buffer = accessPoint->SecondaryChannel;
    buffer += 1;
    *buffer = (uint8_t) accessPoint->Rssi;
    buffer += 1;
    *buffer = accessPoint->AuthenticationMode;
    buffer += 1;
    EncodeUInt32(accessPoint->Protocols, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AccessPoint object.
 *  
 *  @param accessPoint
 *      AccessPoint object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded AccessPoint object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedAccessPointBufferSize
int Encoders::EncodedAccessPointBufferSize(Esp32Messaging::AccessPoint *accessPoint)
{
    return(47);
}

/**
 *  Extract the AccessPoint object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPoint object.
 *  
 *  @returns
 *      Pointer to a AccessPoint object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAccessPoint
Esp32Messaging::AccessPoint *Encoders::ExtractAccessPoint(uint8_t *buffer)
{
    Esp32Messaging::AccessPoint *accessPoint = (Esp32Messaging::AccessPoint *) pvPortMalloc(sizeof(Esp32Messaging::AccessPoint));

    memcpy((void *) accessPoint->Ssid, (void *) buffer, 33);
    buffer += 33;
    memcpy((void *) accessPoint->Bssid, (void *) buffer, 6);
    buffer += 6;
    accessPoint->PrimaryChannel = *buffer;
    buffer += 1;
    accessPoint->SecondaryChannel = *buffer;
    buffer += 1;
    accessPoint->Rssi = (int8_t) *buffer;
    buffer += 1;
    accessPoint->AuthenticationMode = *buffer;
    buffer += 1;
    accessPoint->Protocols = ExtractUInt32(buffer);
    return(accessPoint);
}

/**
 *  Convert the AccessPointList object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedAccessPointListBufferSize method.
 *  
 *  @param accessPointList
 *      AccessPointList object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AccessPointList object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeAccessPointList
void Encoders::EncodeAccessPointList(Esp32Messaging::AccessPointList *accessPointList, uint8_t *buffer)
{
    EncodeUInt32(accessPointList->NumberOfAccessPoints, buffer);
    buffer += 4;
    EncodeUInt32(accessPointList->AccessPointsLength, buffer);
    buffer += 4;
    if (accessPointList->AccessPointsLength > 0)
    {
        memcpy((void *) buffer, (void *) accessPointList->AccessPoints, accessPointList->AccessPointsLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AccessPointList object.
 *  
 *  @param accessPointList
 *      AccessPointList object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded AccessPointList object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedAccessPointListBufferSize
int Encoders::EncodedAccessPointListBufferSize(Esp32Messaging::AccessPointList *accessPointList)
{
    int result = 0;
    result += accessPointList->AccessPointsLength;
    return(result + 8);
}

/**
 *  Extract the AccessPointList object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AccessPointList object.
 *  
 *  @returns
 *      Pointer to a AccessPointList object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAccessPointList
Esp32Messaging::AccessPointList *Encoders::ExtractAccessPointList(uint8_t *buffer)
{
    Esp32Messaging::AccessPointList *accessPointList = (Esp32Messaging::AccessPointList *) pvPortMalloc(sizeof(Esp32Messaging::AccessPointList));

    accessPointList->NumberOfAccessPoints = ExtractUInt32(buffer);
    buffer += 4;
    accessPointList->AccessPointsLength = ExtractUInt32(buffer);
    buffer += 4;
    if (accessPointList->AccessPointsLength > 0)
    {
        accessPointList->AccessPoints = (uint8_t *) pvPortMalloc(accessPointList->AccessPointsLength);
        memcpy(accessPointList->AccessPoints, buffer, accessPointList->AccessPointsLength);
        buffer += accessPointList->AccessPointsLength;
    }
    else
    {
        accessPointList->AccessPoints = nullptr;
    }
    return(accessPointList);
}

/**
 *  Convert the SockAddr object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSockAddrBufferSize method.
 *  
 *  @param sockAddr
 *      SockAddr object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SockAddr object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSockAddr
void Encoders::EncodeSockAddr(Esp32Messaging::SockAddr *sockAddr, uint8_t *buffer)
{
    *buffer = sockAddr->Family;
    buffer += 1;
    EncodeUInt16(sockAddr->Port, buffer);
    buffer += 2;
    EncodeUInt32(sockAddr->Ip4Address, buffer);
    buffer += 4;
    EncodeUInt32(sockAddr->FlowInfo, buffer);
    buffer += 4;
    memcpy((void *) buffer, (void *) sockAddr->Ip6Address, 16);
    buffer += 16;
    EncodeUInt32(sockAddr->ScopeID, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SockAddr object.
 *  
 *  @param sockAddr
 *      SockAddr object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SockAddr object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSockAddrBufferSize
int Encoders::EncodedSockAddrBufferSize(Esp32Messaging::SockAddr *sockAddr)
{
    return(31);
}

/**
 *  Extract the SockAddr object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SockAddr object.
 *  
 *  @returns
 *      Pointer to a SockAddr object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSockAddr
Esp32Messaging::SockAddr *Encoders::ExtractSockAddr(uint8_t *buffer)
{
    Esp32Messaging::SockAddr *sockAddr = (Esp32Messaging::SockAddr *) pvPortMalloc(sizeof(Esp32Messaging::SockAddr));

    sockAddr->Family = *buffer;
    buffer += 1;
    sockAddr->Port = ExtractUInt16(buffer);
    buffer += 2;
    sockAddr->Ip4Address = ExtractUInt32(buffer);
    buffer += 4;
    sockAddr->FlowInfo = ExtractUInt32(buffer);
    buffer += 4;
    memcpy((void *) sockAddr->Ip6Address, (void *) buffer, 16);
    buffer += 16;
    sockAddr->ScopeID = ExtractUInt32(buffer);
    return(sockAddr);
}

/**
 *  Convert the AddrInfo object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedAddrInfoBufferSize method.
 *  
 *  @param addrInfo
 *      AddrInfo object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AddrInfo object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeAddrInfo
void Encoders::EncodeAddrInfo(Esp32Messaging::AddrInfo *addrInfo, uint8_t *buffer)
{
    EncodeUInt32(addrInfo->MyHeapAddress, buffer);
    buffer += 4;
    EncodeInt32(addrInfo->Flags, buffer);
    buffer += 4;
    EncodeInt32(addrInfo->Family, buffer);
    buffer += 4;
    EncodeInt32(addrInfo->SocketType, buffer);
    buffer += 4;
    EncodeInt32(addrInfo->Protocol, buffer);
    buffer += 4;
    EncodeUInt32(addrInfo->AddrLen, buffer);
    buffer += 4;
    EncodeUInt32(addrInfo->AddrLength, buffer);
    buffer += 4;
    if (addrInfo->AddrLength > 0)
    {
        memcpy((void *) buffer, (void *) addrInfo->Addr, addrInfo->AddrLength);
        buffer += addrInfo->AddrLength;
    }
    EncodeString(addrInfo->CanonName, buffer);
    buffer += StringLength(addrInfo->CanonName) + 1;
    EncodeUInt32((uint32_t) addrInfo->Next, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AddrInfo object.
 *  
 *  @param addrInfo
 *      AddrInfo object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded AddrInfo object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedAddrInfoBufferSize
int Encoders::EncodedAddrInfoBufferSize(Esp32Messaging::AddrInfo *addrInfo)
{
    int result = 0;
    result += addrInfo->AddrLength;
    result += Encoders::StringLength(addrInfo->CanonName);
    return(result + 33);
}

/**
 *  Extract the AddrInfo object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AddrInfo object.
 *  
 *  @returns
 *      Pointer to a AddrInfo object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAddrInfo
Esp32Messaging::AddrInfo *Encoders::ExtractAddrInfo(uint8_t *buffer)
{
    Esp32Messaging::AddrInfo *addrInfo = (Esp32Messaging::AddrInfo *) pvPortMalloc(sizeof(Esp32Messaging::AddrInfo));

    addrInfo->MyHeapAddress = ExtractUInt32(buffer);
    buffer += 4;
    addrInfo->Flags = ExtractInt32(buffer);
    buffer += 4;
    addrInfo->Family = ExtractInt32(buffer);
    buffer += 4;
    addrInfo->SocketType = ExtractInt32(buffer);
    buffer += 4;
    addrInfo->Protocol = ExtractInt32(buffer);
    buffer += 4;
    addrInfo->AddrLen = ExtractUInt32(buffer);
    buffer += 4;
    addrInfo->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (addrInfo->AddrLength > 0)
    {
        addrInfo->Addr = (uint8_t *) pvPortMalloc(addrInfo->AddrLength);
        memcpy(addrInfo->Addr, buffer, addrInfo->AddrLength);
        buffer += addrInfo->AddrLength;
    }
    else
    {
        addrInfo->Addr = nullptr;
    }
    addrInfo->CanonName = ExtractString(buffer);
    buffer += Encoders::StringLength(addrInfo->CanonName) + 1;
    addrInfo->Next = (void *) ExtractUInt32(buffer);
    return(addrInfo);
}

/**
 *  Convert the GetAddrInfoRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedGetAddrInfoRequestBufferSize method.
 *  
 *  @param getAddrInfoRequest
 *      GetAddrInfoRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeGetAddrInfoRequest
void Encoders::EncodeGetAddrInfoRequest(Esp32Messaging::GetAddrInfoRequest *getAddrInfoRequest, uint8_t *buffer)
{
    EncodeString(getAddrInfoRequest->NodeName, buffer);
    buffer += StringLength(getAddrInfoRequest->NodeName) + 1;
    EncodeString(getAddrInfoRequest->ServName, buffer);
    buffer += StringLength(getAddrInfoRequest->ServName) + 1;
    EncodeUInt32(getAddrInfoRequest->HintsLength, buffer);
    buffer += 4;
    if (getAddrInfoRequest->HintsLength > 0)
    {
        memcpy((void *) buffer, (void *) getAddrInfoRequest->Hints, getAddrInfoRequest->HintsLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetAddrInfoRequest object.
 *  
 *  @param getAddrInfoRequest
 *      GetAddrInfoRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded GetAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedGetAddrInfoRequestBufferSize
int Encoders::EncodedGetAddrInfoRequestBufferSize(Esp32Messaging::GetAddrInfoRequest *getAddrInfoRequest)
{
    int result = 0;
    result += Encoders::StringLength(getAddrInfoRequest->NodeName);
    result += Encoders::StringLength(getAddrInfoRequest->ServName);
    result += getAddrInfoRequest->HintsLength;
    return(result + 6);
}

/**
 *  Extract the GetAddrInfoRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetAddrInfoRequest object.
 *  
 *  @returns
 *      Pointer to a GetAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetAddrInfoRequest
Esp32Messaging::GetAddrInfoRequest *Encoders::ExtractGetAddrInfoRequest(uint8_t *buffer)
{
    Esp32Messaging::GetAddrInfoRequest *getAddrInfoRequest = (Esp32Messaging::GetAddrInfoRequest *) pvPortMalloc(sizeof(Esp32Messaging::GetAddrInfoRequest));

    getAddrInfoRequest->NodeName = ExtractString(buffer);
    buffer += Encoders::StringLength(getAddrInfoRequest->NodeName) + 1;
    getAddrInfoRequest->ServName = ExtractString(buffer);
    buffer += Encoders::StringLength(getAddrInfoRequest->ServName) + 1;
    getAddrInfoRequest->HintsLength = ExtractUInt32(buffer);
    buffer += 4;
    if (getAddrInfoRequest->HintsLength > 0)
    {
        getAddrInfoRequest->Hints = (uint8_t *) pvPortMalloc(getAddrInfoRequest->HintsLength);
        memcpy(getAddrInfoRequest->Hints, buffer, getAddrInfoRequest->HintsLength);
        buffer += getAddrInfoRequest->HintsLength;
    }
    else
    {
        getAddrInfoRequest->Hints = nullptr;
    }
    return(getAddrInfoRequest);
}

/**
 *  Convert the GetAddrInfoResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedGetAddrInfoResponseBufferSize method.
 *  
 *  @param getAddrInfoResponse
 *      GetAddrInfoResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetAddrInfoResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeGetAddrInfoResponse
void Encoders::EncodeGetAddrInfoResponse(Esp32Messaging::GetAddrInfoResponse *getAddrInfoResponse, uint8_t *buffer)
{
    EncodeInt32(getAddrInfoResponse->AddrInfoResponseErrno, buffer);
    buffer += 4;
    EncodeUInt32(getAddrInfoResponse->ResLength, buffer);
    buffer += 4;
    if (getAddrInfoResponse->ResLength > 0)
    {
        memcpy((void *) buffer, (void *) getAddrInfoResponse->Res, getAddrInfoResponse->ResLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetAddrInfoResponse object.
 *  
 *  @param getAddrInfoResponse
 *      GetAddrInfoResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded GetAddrInfoResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedGetAddrInfoResponseBufferSize
int Encoders::EncodedGetAddrInfoResponseBufferSize(Esp32Messaging::GetAddrInfoResponse *getAddrInfoResponse)
{
    int result = 0;
    result += getAddrInfoResponse->ResLength;
    return(result + 8);
}

/**
 *  Extract the GetAddrInfoResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetAddrInfoResponse object.
 *  
 *  @returns
 *      Pointer to a GetAddrInfoResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetAddrInfoResponse
Esp32Messaging::GetAddrInfoResponse *Encoders::ExtractGetAddrInfoResponse(uint8_t *buffer)
{
    Esp32Messaging::GetAddrInfoResponse *getAddrInfoResponse = (Esp32Messaging::GetAddrInfoResponse *) pvPortMalloc(sizeof(Esp32Messaging::GetAddrInfoResponse));

    getAddrInfoResponse->AddrInfoResponseErrno = ExtractInt32(buffer);
    buffer += 4;
    getAddrInfoResponse->ResLength = ExtractUInt32(buffer);
    buffer += 4;
    if (getAddrInfoResponse->ResLength > 0)
    {
        getAddrInfoResponse->Res = (uint8_t *) pvPortMalloc(getAddrInfoResponse->ResLength);
        memcpy(getAddrInfoResponse->Res, buffer, getAddrInfoResponse->ResLength);
        buffer += getAddrInfoResponse->ResLength;
    }
    else
    {
        getAddrInfoResponse->Res = nullptr;
    }
    return(getAddrInfoResponse);
}

/**
 *  Convert the SocketRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSocketRequestBufferSize method.
 *  
 *  @param socketRequest
 *      SocketRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SocketRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSocketRequest
void Encoders::EncodeSocketRequest(Esp32Messaging::SocketRequest *socketRequest, uint8_t *buffer)
{
    EncodeUInt32((uint32_t) socketRequest->AddressInformation, buffer);
    buffer += 4;
    EncodeInt32(socketRequest->Domain, buffer);
    buffer += 4;
    EncodeInt32(socketRequest->Type, buffer);
    buffer += 4;
    EncodeInt32(socketRequest->Protocol, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SocketRequest object.
 *  
 *  @param socketRequest
 *      SocketRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SocketRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSocketRequestBufferSize
int Encoders::EncodedSocketRequestBufferSize(Esp32Messaging::SocketRequest *socketRequest)
{
    return(16);
}

/**
 *  Extract the SocketRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SocketRequest object.
 *  
 *  @returns
 *      Pointer to a SocketRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSocketRequest
Esp32Messaging::SocketRequest *Encoders::ExtractSocketRequest(uint8_t *buffer)
{
    Esp32Messaging::SocketRequest *socketRequest = (Esp32Messaging::SocketRequest *) pvPortMalloc(sizeof(Esp32Messaging::SocketRequest));

    socketRequest->AddressInformation = (void *) ExtractUInt32(buffer);
    buffer += 4;
    socketRequest->Domain = ExtractInt32(buffer);
    buffer += 4;
    socketRequest->Type = ExtractInt32(buffer);
    buffer += 4;
    socketRequest->Protocol = ExtractInt32(buffer);
    return(socketRequest);
}

/**
 *  Convert the IntegerResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedIntegerResponseBufferSize method.
 *  
 *  @param integerResponse
 *      IntegerResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded IntegerResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeIntegerResponse
void Encoders::EncodeIntegerResponse(Esp32Messaging::IntegerResponse *integerResponse, uint8_t *buffer)
{
    EncodeInt32(integerResponse->Result, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the IntegerResponse object.
 *  
 *  @param integerResponse
 *      IntegerResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded IntegerResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedIntegerResponseBufferSize
int Encoders::EncodedIntegerResponseBufferSize(Esp32Messaging::IntegerResponse *integerResponse)
{
    return(4);
}

/**
 *  Extract the IntegerResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded IntegerResponse object.
 *  
 *  @returns
 *      Pointer to a IntegerResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractIntegerResponse
Esp32Messaging::IntegerResponse *Encoders::ExtractIntegerResponse(uint8_t *buffer)
{
    Esp32Messaging::IntegerResponse *integerResponse = (Esp32Messaging::IntegerResponse *) pvPortMalloc(sizeof(Esp32Messaging::IntegerResponse));

    integerResponse->Result = ExtractInt32(buffer);
    return(integerResponse);
}

/**
 *  Convert the IntegerAndErrnoResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedIntegerAndErrnoResponseBufferSize method.
 *  
 *  @param integerAndErrnoResponse
 *      IntegerAndErrnoResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded IntegerAndErrnoResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeIntegerAndErrnoResponse
void Encoders::EncodeIntegerAndErrnoResponse(Esp32Messaging::IntegerAndErrnoResponse *integerAndErrnoResponse, uint8_t *buffer)
{
    EncodeInt32(integerAndErrnoResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(integerAndErrnoResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the IntegerAndErrnoResponse object.
 *  
 *  @param integerAndErrnoResponse
 *      IntegerAndErrnoResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded IntegerAndErrnoResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedIntegerAndErrnoResponseBufferSize
int Encoders::EncodedIntegerAndErrnoResponseBufferSize(Esp32Messaging::IntegerAndErrnoResponse *integerAndErrnoResponse)
{
    return(8);
}

/**
 *  Extract the IntegerAndErrnoResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded IntegerAndErrnoResponse object.
 *  
 *  @returns
 *      Pointer to a IntegerAndErrnoResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractIntegerAndErrnoResponse
Esp32Messaging::IntegerAndErrnoResponse *Encoders::ExtractIntegerAndErrnoResponse(uint8_t *buffer)
{
    Esp32Messaging::IntegerAndErrnoResponse *integerAndErrnoResponse = (Esp32Messaging::IntegerAndErrnoResponse *) pvPortMalloc(sizeof(Esp32Messaging::IntegerAndErrnoResponse));

    integerAndErrnoResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    integerAndErrnoResponse->ResponseErrno = ExtractInt32(buffer);
    return(integerAndErrnoResponse);
}

/**
 *  Convert the ConnectRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedConnectRequestBufferSize method.
 *  
 *  @param connectRequest
 *      ConnectRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ConnectRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeConnectRequest
void Encoders::EncodeConnectRequest(Esp32Messaging::ConnectRequest *connectRequest, uint8_t *buffer)
{
    EncodeInt32(connectRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeUInt32(connectRequest->AddrLength, buffer);
    buffer += 4;
    if (connectRequest->AddrLength > 0)
    {
        memcpy((void *) buffer, (void *) connectRequest->Addr, connectRequest->AddrLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ConnectRequest object.
 *  
 *  @param connectRequest
 *      ConnectRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded ConnectRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedConnectRequestBufferSize
int Encoders::EncodedConnectRequestBufferSize(Esp32Messaging::ConnectRequest *connectRequest)
{
    int result = 0;
    result += connectRequest->AddrLength;
    return(result + 8);
}

/**
 *  Extract the ConnectRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ConnectRequest object.
 *  
 *  @returns
 *      Pointer to a ConnectRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractConnectRequest
Esp32Messaging::ConnectRequest *Encoders::ExtractConnectRequest(uint8_t *buffer)
{
    Esp32Messaging::ConnectRequest *connectRequest = (Esp32Messaging::ConnectRequest *) pvPortMalloc(sizeof(Esp32Messaging::ConnectRequest));

    connectRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    connectRequest->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (connectRequest->AddrLength > 0)
    {
        connectRequest->Addr = (uint8_t *) pvPortMalloc(connectRequest->AddrLength);
        memcpy(connectRequest->Addr, buffer, connectRequest->AddrLength);
        buffer += connectRequest->AddrLength;
    }
    else
    {
        connectRequest->Addr = nullptr;
    }
    return(connectRequest);
}

/**
 *  Convert the FreeAddrInfoRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedFreeAddrInfoRequestBufferSize method.
 *  
 *  @param freeAddrInfoRequest
 *      FreeAddrInfoRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded FreeAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeFreeAddrInfoRequest
void Encoders::EncodeFreeAddrInfoRequest(Esp32Messaging::FreeAddrInfoRequest *freeAddrInfoRequest, uint8_t *buffer)
{
    EncodeUInt32(freeAddrInfoRequest->AddrInfoAddress, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the FreeAddrInfoRequest object.
 *  
 *  @param freeAddrInfoRequest
 *      FreeAddrInfoRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded FreeAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedFreeAddrInfoRequestBufferSize
int Encoders::EncodedFreeAddrInfoRequestBufferSize(Esp32Messaging::FreeAddrInfoRequest *freeAddrInfoRequest)
{
    return(4);
}

/**
 *  Extract the FreeAddrInfoRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded FreeAddrInfoRequest object.
 *  
 *  @returns
 *      Pointer to a FreeAddrInfoRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractFreeAddrInfoRequest
Esp32Messaging::FreeAddrInfoRequest *Encoders::ExtractFreeAddrInfoRequest(uint8_t *buffer)
{
    Esp32Messaging::FreeAddrInfoRequest *freeAddrInfoRequest = (Esp32Messaging::FreeAddrInfoRequest *) pvPortMalloc(sizeof(Esp32Messaging::FreeAddrInfoRequest));

    freeAddrInfoRequest->AddrInfoAddress = ExtractUInt32(buffer);
    return(freeAddrInfoRequest);
}

/**
 *  Convert the TimeVal object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedTimeValBufferSize method.
 *  
 *  @param timeVal
 *      TimeVal object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded TimeVal object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeTimeVal
void Encoders::EncodeTimeVal(Esp32Messaging::TimeVal *timeVal, uint8_t *buffer)
{
    EncodeUInt32(timeVal->TvSec, buffer);
    buffer += 4;
    EncodeUInt32(timeVal->TvUsec, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the TimeVal object.
 *  
 *  @param timeVal
 *      TimeVal object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded TimeVal object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedTimeValBufferSize
int Encoders::EncodedTimeValBufferSize(Esp32Messaging::TimeVal *timeVal)
{
    return(8);
}

/**
 *  Extract the TimeVal object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded TimeVal object.
 *  
 *  @returns
 *      Pointer to a TimeVal object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractTimeVal
Esp32Messaging::TimeVal *Encoders::ExtractTimeVal(uint8_t *buffer)
{
    Esp32Messaging::TimeVal *timeVal = (Esp32Messaging::TimeVal *) pvPortMalloc(sizeof(Esp32Messaging::TimeVal));

    timeVal->TvSec = ExtractUInt32(buffer);
    buffer += 4;
    timeVal->TvUsec = ExtractUInt32(buffer);
    return(timeVal);
}

/**
 *  Convert the SetSockOptRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSetSockOptRequestBufferSize method.
 *  
 *  @param setSockOptRequest
 *      SetSockOptRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SetSockOptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSetSockOptRequest
void Encoders::EncodeSetSockOptRequest(Esp32Messaging::SetSockOptRequest *setSockOptRequest, uint8_t *buffer)
{
    EncodeInt32(setSockOptRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeInt32(setSockOptRequest->Level, buffer);
    buffer += 4;
    EncodeInt32(setSockOptRequest->OptionName, buffer);
    buffer += 4;
    EncodeUInt32(setSockOptRequest->OptionValueLength, buffer);
    buffer += 4;
    if (setSockOptRequest->OptionValueLength > 0)
    {
        memcpy((void *) buffer, (void *) setSockOptRequest->OptionValue, setSockOptRequest->OptionValueLength);
        buffer += setSockOptRequest->OptionValueLength;
    }
    EncodeInt32(setSockOptRequest->OptionLen, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SetSockOptRequest object.
 *  
 *  @param setSockOptRequest
 *      SetSockOptRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SetSockOptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSetSockOptRequestBufferSize
int Encoders::EncodedSetSockOptRequestBufferSize(Esp32Messaging::SetSockOptRequest *setSockOptRequest)
{
    int result = 0;
    result += setSockOptRequest->OptionValueLength;
    return(result + 20);
}

/**
 *  Extract the SetSockOptRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SetSockOptRequest object.
 *  
 *  @returns
 *      Pointer to a SetSockOptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSetSockOptRequest
Esp32Messaging::SetSockOptRequest *Encoders::ExtractSetSockOptRequest(uint8_t *buffer)
{
    Esp32Messaging::SetSockOptRequest *setSockOptRequest = (Esp32Messaging::SetSockOptRequest *) pvPortMalloc(sizeof(Esp32Messaging::SetSockOptRequest));

    setSockOptRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    setSockOptRequest->Level = ExtractInt32(buffer);
    buffer += 4;
    setSockOptRequest->OptionName = ExtractInt32(buffer);
    buffer += 4;
    setSockOptRequest->OptionValueLength = ExtractUInt32(buffer);
    buffer += 4;
    if (setSockOptRequest->OptionValueLength > 0)
    {
        setSockOptRequest->OptionValue = (uint8_t *) pvPortMalloc(setSockOptRequest->OptionValueLength);
        memcpy(setSockOptRequest->OptionValue, buffer, setSockOptRequest->OptionValueLength);
        buffer += setSockOptRequest->OptionValueLength;
    }
    else
    {
        setSockOptRequest->OptionValue = nullptr;
    }
    setSockOptRequest->OptionLen = ExtractInt32(buffer);
    return(setSockOptRequest);
}

/**
 *  Convert the GetSockOptRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedGetSockOptRequestBufferSize method.
 *  
 *  @param getSockOptRequest
 *      GetSockOptRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetSockOptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeGetSockOptRequest
void Encoders::EncodeGetSockOptRequest(Esp32Messaging::GetSockOptRequest *getSockOptRequest, uint8_t *buffer)
{
    EncodeInt32(getSockOptRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeInt32(getSockOptRequest->Level, buffer);
    buffer += 4;
    EncodeInt32(getSockOptRequest->OptionName, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetSockOptRequest object.
 *  
 *  @param getSockOptRequest
 *      GetSockOptRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded GetSockOptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedGetSockOptRequestBufferSize
int Encoders::EncodedGetSockOptRequestBufferSize(Esp32Messaging::GetSockOptRequest *getSockOptRequest)
{
    return(12);
}

/**
 *  Extract the GetSockOptRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetSockOptRequest object.
 *  
 *  @returns
 *      Pointer to a GetSockOptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetSockOptRequest
Esp32Messaging::GetSockOptRequest *Encoders::ExtractGetSockOptRequest(uint8_t *buffer)
{
    Esp32Messaging::GetSockOptRequest *getSockOptRequest = (Esp32Messaging::GetSockOptRequest *) pvPortMalloc(sizeof(Esp32Messaging::GetSockOptRequest));

    getSockOptRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    getSockOptRequest->Level = ExtractInt32(buffer);
    buffer += 4;
    getSockOptRequest->OptionName = ExtractInt32(buffer);
    return(getSockOptRequest);
}

/**
 *  Convert the GetSockOptResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedGetSockOptResponseBufferSize method.
 *  
 *  @param getSockOptResponse
 *      GetSockOptResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetSockOptResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeGetSockOptResponse
void Encoders::EncodeGetSockOptResponse(Esp32Messaging::GetSockOptResponse *getSockOptResponse, uint8_t *buffer)
{
    EncodeInt32(getSockOptResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(getSockOptResponse->ResponseErrno, buffer);
    buffer += 4;
    EncodeUInt32(getSockOptResponse->OptionValueLength, buffer);
    buffer += 4;
    if (getSockOptResponse->OptionValueLength > 0)
    {
        memcpy((void *) buffer, (void *) getSockOptResponse->OptionValue, getSockOptResponse->OptionValueLength);
        buffer += getSockOptResponse->OptionValueLength;
    }
    EncodeInt32(getSockOptResponse->OptionLen, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetSockOptResponse object.
 *  
 *  @param getSockOptResponse
 *      GetSockOptResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded GetSockOptResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedGetSockOptResponseBufferSize
int Encoders::EncodedGetSockOptResponseBufferSize(Esp32Messaging::GetSockOptResponse *getSockOptResponse)
{
    int result = 0;
    result += getSockOptResponse->OptionValueLength;
    return(result + 16);
}

/**
 *  Extract the GetSockOptResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetSockOptResponse object.
 *  
 *  @returns
 *      Pointer to a GetSockOptResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetSockOptResponse
Esp32Messaging::GetSockOptResponse *Encoders::ExtractGetSockOptResponse(uint8_t *buffer)
{
    Esp32Messaging::GetSockOptResponse *getSockOptResponse = (Esp32Messaging::GetSockOptResponse *) pvPortMalloc(sizeof(Esp32Messaging::GetSockOptResponse));

    getSockOptResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    getSockOptResponse->ResponseErrno = ExtractInt32(buffer);
    buffer += 4;
    getSockOptResponse->OptionValueLength = ExtractUInt32(buffer);
    buffer += 4;
    if (getSockOptResponse->OptionValueLength > 0)
    {
        getSockOptResponse->OptionValue = (uint8_t *) pvPortMalloc(getSockOptResponse->OptionValueLength);
        memcpy(getSockOptResponse->OptionValue, buffer, getSockOptResponse->OptionValueLength);
        buffer += getSockOptResponse->OptionValueLength;
    }
    else
    {
        getSockOptResponse->OptionValue = nullptr;
    }
    getSockOptResponse->OptionLen = ExtractInt32(buffer);
    return(getSockOptResponse);
}

/**
 *  Convert the Linger object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedLingerBufferSize method.
 *  
 *  @param linger
 *      Linger object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded Linger object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeLinger
void Encoders::EncodeLinger(Esp32Messaging::Linger *linger, uint8_t *buffer)
{
    EncodeInt32(linger->LOnOff, buffer);
    buffer += 4;
    EncodeInt32(linger->LLinger, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the Linger object.
 *  
 *  @param linger
 *      Linger object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded Linger object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedLingerBufferSize
int Encoders::EncodedLingerBufferSize(Esp32Messaging::Linger *linger)
{
    return(8);
}

/**
 *  Extract the Linger object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded Linger object.
 *  
 *  @returns
 *      Pointer to a Linger object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractLinger
Esp32Messaging::Linger *Encoders::ExtractLinger(uint8_t *buffer)
{
    Esp32Messaging::Linger *linger = (Esp32Messaging::Linger *) pvPortMalloc(sizeof(Esp32Messaging::Linger));

    linger->LOnOff = ExtractInt32(buffer);
    buffer += 4;
    linger->LLinger = ExtractInt32(buffer);
    return(linger);
}

/**
 *  Convert the WriteRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedWriteRequestBufferSize method.
 *  
 *  @param writeRequest
 *      WriteRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded WriteRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeWriteRequest
void Encoders::EncodeWriteRequest(Esp32Messaging::WriteRequest *writeRequest, uint8_t *buffer)
{
    EncodeInt32(writeRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeUInt32(writeRequest->BufferLength, buffer);
    buffer += 4;
    if (writeRequest->BufferLength > 0)
    {
        memcpy((void *) buffer, (void *) writeRequest->Buffer, writeRequest->BufferLength);
        buffer += writeRequest->BufferLength;
    }
    EncodeInt32(writeRequest->Count, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the WriteRequest object.
 *  
 *  @param writeRequest
 *      WriteRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded WriteRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedWriteRequestBufferSize
int Encoders::EncodedWriteRequestBufferSize(Esp32Messaging::WriteRequest *writeRequest)
{
    int result = 0;
    result += writeRequest->BufferLength;
    return(result + 12);
}

/**
 *  Extract the WriteRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded WriteRequest object.
 *  
 *  @returns
 *      Pointer to a WriteRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractWriteRequest
Esp32Messaging::WriteRequest *Encoders::ExtractWriteRequest(uint8_t *buffer)
{
    Esp32Messaging::WriteRequest *writeRequest = (Esp32Messaging::WriteRequest *) pvPortMalloc(sizeof(Esp32Messaging::WriteRequest));

    writeRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    writeRequest->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (writeRequest->BufferLength > 0)
    {
        writeRequest->Buffer = (uint8_t *) pvPortMalloc(writeRequest->BufferLength);
        memcpy(writeRequest->Buffer, buffer, writeRequest->BufferLength);
        buffer += writeRequest->BufferLength;
    }
    else
    {
        writeRequest->Buffer = nullptr;
    }
    writeRequest->Count = ExtractInt32(buffer);
    return(writeRequest);
}

/**
 *  Convert the ReadRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedReadRequestBufferSize method.
 *  
 *  @param readRequest
 *      ReadRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ReadRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeReadRequest
void Encoders::EncodeReadRequest(Esp32Messaging::ReadRequest *readRequest, uint8_t *buffer)
{
    EncodeInt32(readRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeInt32(readRequest->Count, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ReadRequest object.
 *  
 *  @param readRequest
 *      ReadRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded ReadRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedReadRequestBufferSize
int Encoders::EncodedReadRequestBufferSize(Esp32Messaging::ReadRequest *readRequest)
{
    return(8);
}

/**
 *  Extract the ReadRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ReadRequest object.
 *  
 *  @returns
 *      Pointer to a ReadRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractReadRequest
Esp32Messaging::ReadRequest *Encoders::ExtractReadRequest(uint8_t *buffer)
{
    Esp32Messaging::ReadRequest *readRequest = (Esp32Messaging::ReadRequest *) pvPortMalloc(sizeof(Esp32Messaging::ReadRequest));

    readRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    readRequest->Count = ExtractInt32(buffer);
    return(readRequest);
}

/**
 *  Convert the ReadResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedReadResponseBufferSize method.
 *  
 *  @param readResponse
 *      ReadResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ReadResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeReadResponse
void Encoders::EncodeReadResponse(Esp32Messaging::ReadResponse *readResponse, uint8_t *buffer)
{
    EncodeUInt32(readResponse->BufferLength, buffer);
    buffer += 4;
    if (readResponse->BufferLength > 0)
    {
        memcpy((void *) buffer, (void *) readResponse->Buffer, readResponse->BufferLength);
        buffer += readResponse->BufferLength;
    }
    EncodeInt32(readResponse->ReadResponseResult, buffer);
    buffer += 4;
    EncodeInt32(readResponse->ReadResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ReadResponse object.
 *  
 *  @param readResponse
 *      ReadResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded ReadResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedReadResponseBufferSize
int Encoders::EncodedReadResponseBufferSize(Esp32Messaging::ReadResponse *readResponse)
{
    int result = 0;
    result += readResponse->BufferLength;
    return(result + 12);
}

/**
 *  Extract the ReadResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ReadResponse object.
 *  
 *  @returns
 *      Pointer to a ReadResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractReadResponse
Esp32Messaging::ReadResponse *Encoders::ExtractReadResponse(uint8_t *buffer)
{
    Esp32Messaging::ReadResponse *readResponse = (Esp32Messaging::ReadResponse *) pvPortMalloc(sizeof(Esp32Messaging::ReadResponse));

    readResponse->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (readResponse->BufferLength > 0)
    {
        readResponse->Buffer = (uint8_t *) pvPortMalloc(readResponse->BufferLength);
        memcpy(readResponse->Buffer, buffer, readResponse->BufferLength);
        buffer += readResponse->BufferLength;
    }
    else
    {
        readResponse->Buffer = nullptr;
    }
    readResponse->ReadResponseResult = ExtractInt32(buffer);
    buffer += 4;
    readResponse->ReadResponseErrno = ExtractInt32(buffer);
    return(readResponse);
}

/**
 *  Convert the CloseRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedCloseRequestBufferSize method.
 *  
 *  @param closeRequest
 *      CloseRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded CloseRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeCloseRequest
void Encoders::EncodeCloseRequest(Esp32Messaging::CloseRequest *closeRequest, uint8_t *buffer)
{
    EncodeInt32(closeRequest->SocketHandle, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the CloseRequest object.
 *  
 *  @param closeRequest
 *      CloseRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded CloseRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedCloseRequestBufferSize
int Encoders::EncodedCloseRequestBufferSize(Esp32Messaging::CloseRequest *closeRequest)
{
    return(4);
}

/**
 *  Extract the CloseRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded CloseRequest object.
 *  
 *  @returns
 *      Pointer to a CloseRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractCloseRequest
Esp32Messaging::CloseRequest *Encoders::ExtractCloseRequest(uint8_t *buffer)
{
    Esp32Messaging::CloseRequest *closeRequest = (Esp32Messaging::CloseRequest *) pvPortMalloc(sizeof(Esp32Messaging::CloseRequest));

    closeRequest->SocketHandle = ExtractInt32(buffer);
    return(closeRequest);
}

/**
 *  Convert the GetBatteryChargeLevelResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedGetBatteryChargeLevelResponseBufferSize method.
 *  
 *  @param getBatteryChargeLevelResponse
 *      GetBatteryChargeLevelResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetBatteryChargeLevelResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeGetBatteryChargeLevelResponse
void Encoders::EncodeGetBatteryChargeLevelResponse(Esp32Messaging::GetBatteryChargeLevelResponse *getBatteryChargeLevelResponse, uint8_t *buffer)
{
    EncodeUInt32(getBatteryChargeLevelResponse->Level, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetBatteryChargeLevelResponse object.
 *  
 *  @param getBatteryChargeLevelResponse
 *      GetBatteryChargeLevelResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded GetBatteryChargeLevelResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedGetBatteryChargeLevelResponseBufferSize
int Encoders::EncodedGetBatteryChargeLevelResponseBufferSize(Esp32Messaging::GetBatteryChargeLevelResponse *getBatteryChargeLevelResponse)
{
    return(4);
}

/**
 *  Extract the GetBatteryChargeLevelResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetBatteryChargeLevelResponse object.
 *  
 *  @returns
 *      Pointer to a GetBatteryChargeLevelResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetBatteryChargeLevelResponse
Esp32Messaging::GetBatteryChargeLevelResponse *Encoders::ExtractGetBatteryChargeLevelResponse(uint8_t *buffer)
{
    Esp32Messaging::GetBatteryChargeLevelResponse *getBatteryChargeLevelResponse = (Esp32Messaging::GetBatteryChargeLevelResponse *) pvPortMalloc(sizeof(Esp32Messaging::GetBatteryChargeLevelResponse));

    getBatteryChargeLevelResponse->Level = ExtractUInt32(buffer);
    return(getBatteryChargeLevelResponse);
}

/**
 *  Convert the SendRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSendRequestBufferSize method.
 *  
 *  @param sendRequest
 *      SendRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SendRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSendRequest
void Encoders::EncodeSendRequest(Esp32Messaging::SendRequest *sendRequest, uint8_t *buffer)
{
    EncodeInt32(sendRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeUInt32(sendRequest->BufferLength, buffer);
    buffer += 4;
    if (sendRequest->BufferLength > 0)
    {
        memcpy((void *) buffer, (void *) sendRequest->Buffer, sendRequest->BufferLength);
        buffer += sendRequest->BufferLength;
    }
    EncodeInt32(sendRequest->Length, buffer);
    buffer += 4;
    EncodeInt32(sendRequest->Flags, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SendRequest object.
 *  
 *  @param sendRequest
 *      SendRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SendRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSendRequestBufferSize
int Encoders::EncodedSendRequestBufferSize(Esp32Messaging::SendRequest *sendRequest)
{
    int result = 0;
    result += sendRequest->BufferLength;
    return(result + 16);
}

/**
 *  Extract the SendRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SendRequest object.
 *  
 *  @returns
 *      Pointer to a SendRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSendRequest
Esp32Messaging::SendRequest *Encoders::ExtractSendRequest(uint8_t *buffer)
{
    Esp32Messaging::SendRequest *sendRequest = (Esp32Messaging::SendRequest *) pvPortMalloc(sizeof(Esp32Messaging::SendRequest));

    sendRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    sendRequest->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (sendRequest->BufferLength > 0)
    {
        sendRequest->Buffer = (uint8_t *) pvPortMalloc(sendRequest->BufferLength);
        memcpy(sendRequest->Buffer, buffer, sendRequest->BufferLength);
        buffer += sendRequest->BufferLength;
    }
    else
    {
        sendRequest->Buffer = nullptr;
    }
    sendRequest->Length = ExtractInt32(buffer);
    buffer += 4;
    sendRequest->Flags = ExtractInt32(buffer);
    return(sendRequest);
}

/**
 *  Convert the SendToRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSendToRequestBufferSize method.
 *  
 *  @param sendToRequest
 *      SendToRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SendToRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSendToRequest
void Encoders::EncodeSendToRequest(Esp32Messaging::SendToRequest *sendToRequest, uint8_t *buffer)
{
    EncodeInt32(sendToRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeUInt32(sendToRequest->BufferLength, buffer);
    buffer += 4;
    if (sendToRequest->BufferLength > 0)
    {
        memcpy((void *) buffer, (void *) sendToRequest->Buffer, sendToRequest->BufferLength);
        buffer += sendToRequest->BufferLength;
    }
    EncodeInt32(sendToRequest->Length, buffer);
    buffer += 4;
    EncodeInt32(sendToRequest->Flags, buffer);
    buffer += 4;
    EncodeUInt32(sendToRequest->DestinationAddressLength, buffer);
    buffer += 4;
    if (sendToRequest->DestinationAddressLength > 0)
    {
        memcpy((void *) buffer, (void *) sendToRequest->DestinationAddress, sendToRequest->DestinationAddressLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SendToRequest object.
 *  
 *  @param sendToRequest
 *      SendToRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SendToRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSendToRequestBufferSize
int Encoders::EncodedSendToRequestBufferSize(Esp32Messaging::SendToRequest *sendToRequest)
{
    int result = 0;
    result += sendToRequest->BufferLength;
    result += sendToRequest->DestinationAddressLength;
    return(result + 20);
}

/**
 *  Extract the SendToRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SendToRequest object.
 *  
 *  @returns
 *      Pointer to a SendToRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSendToRequest
Esp32Messaging::SendToRequest *Encoders::ExtractSendToRequest(uint8_t *buffer)
{
    Esp32Messaging::SendToRequest *sendToRequest = (Esp32Messaging::SendToRequest *) pvPortMalloc(sizeof(Esp32Messaging::SendToRequest));

    sendToRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    sendToRequest->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (sendToRequest->BufferLength > 0)
    {
        sendToRequest->Buffer = (uint8_t *) pvPortMalloc(sendToRequest->BufferLength);
        memcpy(sendToRequest->Buffer, buffer, sendToRequest->BufferLength);
        buffer += sendToRequest->BufferLength;
    }
    else
    {
        sendToRequest->Buffer = nullptr;
    }
    sendToRequest->Length = ExtractInt32(buffer);
    buffer += 4;
    sendToRequest->Flags = ExtractInt32(buffer);
    buffer += 4;
    sendToRequest->DestinationAddressLength = ExtractUInt32(buffer);
    buffer += 4;
    if (sendToRequest->DestinationAddressLength > 0)
    {
        sendToRequest->DestinationAddress = (uint8_t *) pvPortMalloc(sendToRequest->DestinationAddressLength);
        memcpy(sendToRequest->DestinationAddress, buffer, sendToRequest->DestinationAddressLength);
        buffer += sendToRequest->DestinationAddressLength;
    }
    else
    {
        sendToRequest->DestinationAddress = nullptr;
    }
    return(sendToRequest);
}

/**
 *  Convert the RecvFromRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedRecvFromRequestBufferSize method.
 *  
 *  @param recvFromRequest
 *      RecvFromRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded RecvFromRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeRecvFromRequest
void Encoders::EncodeRecvFromRequest(Esp32Messaging::RecvFromRequest *recvFromRequest, uint8_t *buffer)
{
    EncodeInt32(recvFromRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeInt32(recvFromRequest->Length, buffer);
    buffer += 4;
    EncodeInt32(recvFromRequest->Flags, buffer);
    buffer += 4;
    EncodeInt32(recvFromRequest->GetSourceAddress, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the RecvFromRequest object.
 *  
 *  @param recvFromRequest
 *      RecvFromRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded RecvFromRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedRecvFromRequestBufferSize
int Encoders::EncodedRecvFromRequestBufferSize(Esp32Messaging::RecvFromRequest *recvFromRequest)
{
    return(16);
}

/**
 *  Extract the RecvFromRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded RecvFromRequest object.
 *  
 *  @returns
 *      Pointer to a RecvFromRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractRecvFromRequest
Esp32Messaging::RecvFromRequest *Encoders::ExtractRecvFromRequest(uint8_t *buffer)
{
    Esp32Messaging::RecvFromRequest *recvFromRequest = (Esp32Messaging::RecvFromRequest *) pvPortMalloc(sizeof(Esp32Messaging::RecvFromRequest));

    recvFromRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    recvFromRequest->Length = ExtractInt32(buffer);
    buffer += 4;
    recvFromRequest->Flags = ExtractInt32(buffer);
    buffer += 4;
    recvFromRequest->GetSourceAddress = ExtractInt32(buffer);
    return(recvFromRequest);
}

/**
 *  Convert the RecvFromResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedRecvFromResponseBufferSize method.
 *  
 *  @param recvFromResponse
 *      RecvFromResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded RecvFromResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeRecvFromResponse
void Encoders::EncodeRecvFromResponse(Esp32Messaging::RecvFromResponse *recvFromResponse, uint8_t *buffer)
{
    EncodeUInt32(recvFromResponse->BufferLength, buffer);
    buffer += 4;
    if (recvFromResponse->BufferLength > 0)
    {
        memcpy((void *) buffer, (void *) recvFromResponse->Buffer, recvFromResponse->BufferLength);
        buffer += recvFromResponse->BufferLength;
    }
    EncodeInt32(recvFromResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(recvFromResponse->ResponseErrno, buffer);
    buffer += 4;
    EncodeUInt32(recvFromResponse->SourceAddressLength, buffer);
    buffer += 4;
    if (recvFromResponse->SourceAddressLength > 0)
    {
        memcpy((void *) buffer, (void *) recvFromResponse->SourceAddress, recvFromResponse->SourceAddressLength);
        buffer += recvFromResponse->SourceAddressLength;
    }
    EncodeUInt32(recvFromResponse->SourceAddressLen, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the RecvFromResponse object.
 *  
 *  @param recvFromResponse
 *      RecvFromResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded RecvFromResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedRecvFromResponseBufferSize
int Encoders::EncodedRecvFromResponseBufferSize(Esp32Messaging::RecvFromResponse *recvFromResponse)
{
    int result = 0;
    result += recvFromResponse->BufferLength;
    result += recvFromResponse->SourceAddressLength;
    return(result + 20);
}

/**
 *  Extract the RecvFromResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded RecvFromResponse object.
 *  
 *  @returns
 *      Pointer to a RecvFromResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractRecvFromResponse
Esp32Messaging::RecvFromResponse *Encoders::ExtractRecvFromResponse(uint8_t *buffer)
{
    Esp32Messaging::RecvFromResponse *recvFromResponse = (Esp32Messaging::RecvFromResponse *) pvPortMalloc(sizeof(Esp32Messaging::RecvFromResponse));

    recvFromResponse->BufferLength = ExtractUInt32(buffer);
    buffer += 4;
    if (recvFromResponse->BufferLength > 0)
    {
        recvFromResponse->Buffer = (uint8_t *) pvPortMalloc(recvFromResponse->BufferLength);
        memcpy(recvFromResponse->Buffer, buffer, recvFromResponse->BufferLength);
        buffer += recvFromResponse->BufferLength;
    }
    else
    {
        recvFromResponse->Buffer = nullptr;
    }
    recvFromResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    recvFromResponse->ResponseErrno = ExtractInt32(buffer);
    buffer += 4;
    recvFromResponse->SourceAddressLength = ExtractUInt32(buffer);
    buffer += 4;
    if (recvFromResponse->SourceAddressLength > 0)
    {
        recvFromResponse->SourceAddress = (uint8_t *) pvPortMalloc(recvFromResponse->SourceAddressLength);
        memcpy(recvFromResponse->SourceAddress, buffer, recvFromResponse->SourceAddressLength);
        buffer += recvFromResponse->SourceAddressLength;
    }
    else
    {
        recvFromResponse->SourceAddress = nullptr;
    }
    recvFromResponse->SourceAddressLen = ExtractUInt32(buffer);
    return(recvFromResponse);
}

/**
 *  Convert the PollRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedPollRequestBufferSize method.
 *  
 *  @param pollRequest
 *      PollRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded PollRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodePollRequest
void Encoders::EncodePollRequest(Esp32Messaging::PollRequest *pollRequest, uint8_t *buffer)
{
    EncodeInt32(pollRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeUInt16(pollRequest->Events, buffer);
    buffer += 2;
    EncodeInt32(pollRequest->Timeout, buffer);
    buffer += 4;
    EncodeInt32(pollRequest->Setup, buffer);
    buffer += 4;
    EncodeUInt32(pollRequest->SetupMessageId, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the PollRequest object.
 *  
 *  @param pollRequest
 *      PollRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded PollRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedPollRequestBufferSize
int Encoders::EncodedPollRequestBufferSize(Esp32Messaging::PollRequest *pollRequest)
{
    return(18);
}

/**
 *  Extract the PollRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded PollRequest object.
 *  
 *  @returns
 *      Pointer to a PollRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractPollRequest
Esp32Messaging::PollRequest *Encoders::ExtractPollRequest(uint8_t *buffer)
{
    Esp32Messaging::PollRequest *pollRequest = (Esp32Messaging::PollRequest *) pvPortMalloc(sizeof(Esp32Messaging::PollRequest));

    pollRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    pollRequest->Events = ExtractUInt16(buffer);
    buffer += 2;
    pollRequest->Timeout = ExtractInt32(buffer);
    buffer += 4;
    pollRequest->Setup = ExtractInt32(buffer);
    buffer += 4;
    pollRequest->SetupMessageId = ExtractUInt32(buffer);
    return(pollRequest);
}

/**
 *  Convert the PollResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedPollResponseBufferSize method.
 *  
 *  @param pollResponse
 *      PollResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded PollResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodePollResponse
void Encoders::EncodePollResponse(Esp32Messaging::PollResponse *pollResponse, uint8_t *buffer)
{
    EncodeUInt16(pollResponse->ReturnedEvents, buffer);
    buffer += 2;
    EncodeInt32(pollResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(pollResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the PollResponse object.
 *  
 *  @param pollResponse
 *      PollResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded PollResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedPollResponseBufferSize
int Encoders::EncodedPollResponseBufferSize(Esp32Messaging::PollResponse *pollResponse)
{
    return(10);
}

/**
 *  Extract the PollResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded PollResponse object.
 *  
 *  @returns
 *      Pointer to a PollResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractPollResponse
Esp32Messaging::PollResponse *Encoders::ExtractPollResponse(uint8_t *buffer)
{
    Esp32Messaging::PollResponse *pollResponse = (Esp32Messaging::PollResponse *) pvPortMalloc(sizeof(Esp32Messaging::PollResponse));

    pollResponse->ReturnedEvents = ExtractUInt16(buffer);
    buffer += 2;
    pollResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    pollResponse->ResponseErrno = ExtractInt32(buffer);
    return(pollResponse);
}

/**
 *  Convert the InterruptPollResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedInterruptPollResponseBufferSize method.
 *  
 *  @param interruptPollResponse
 *      InterruptPollResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded InterruptPollResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeInterruptPollResponse
void Encoders::EncodeInterruptPollResponse(Esp32Messaging::InterruptPollResponse *interruptPollResponse, uint8_t *buffer)
{
    EncodeInt32(interruptPollResponse->SocketHandle, buffer);
    buffer += 4;
    EncodeInt32(interruptPollResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(interruptPollResponse->ResponseErrno, buffer);
    buffer += 4;
    EncodeUInt16(interruptPollResponse->ReturnedEvents, buffer);
    buffer += 2;
    EncodeUInt32(interruptPollResponse->SetupMessageId, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the InterruptPollResponse object.
 *  
 *  @param interruptPollResponse
 *      InterruptPollResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded InterruptPollResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedInterruptPollResponseBufferSize
int Encoders::EncodedInterruptPollResponseBufferSize(Esp32Messaging::InterruptPollResponse *interruptPollResponse)
{
    return(18);
}

/**
 *  Extract the InterruptPollResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded InterruptPollResponse object.
 *  
 *  @returns
 *      Pointer to a InterruptPollResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractInterruptPollResponse
Esp32Messaging::InterruptPollResponse *Encoders::ExtractInterruptPollResponse(uint8_t *buffer)
{
    Esp32Messaging::InterruptPollResponse *interruptPollResponse = (Esp32Messaging::InterruptPollResponse *) pvPortMalloc(sizeof(Esp32Messaging::InterruptPollResponse));

    interruptPollResponse->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    interruptPollResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    interruptPollResponse->ResponseErrno = ExtractInt32(buffer);
    buffer += 4;
    interruptPollResponse->ReturnedEvents = ExtractUInt16(buffer);
    buffer += 2;
    interruptPollResponse->SetupMessageId = ExtractUInt32(buffer);
    return(interruptPollResponse);
}

/**
 *  Convert the ListenRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedListenRequestBufferSize method.
 *  
 *  @param listenRequest
 *      ListenRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded ListenRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeListenRequest
void Encoders::EncodeListenRequest(Esp32Messaging::ListenRequest *listenRequest, uint8_t *buffer)
{
    EncodeInt32(listenRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeInt32(listenRequest->BackLog, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the ListenRequest object.
 *  
 *  @param listenRequest
 *      ListenRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded ListenRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedListenRequestBufferSize
int Encoders::EncodedListenRequestBufferSize(Esp32Messaging::ListenRequest *listenRequest)
{
    return(8);
}

/**
 *  Extract the ListenRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded ListenRequest object.
 *  
 *  @returns
 *      Pointer to a ListenRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractListenRequest
Esp32Messaging::ListenRequest *Encoders::ExtractListenRequest(uint8_t *buffer)
{
    Esp32Messaging::ListenRequest *listenRequest = (Esp32Messaging::ListenRequest *) pvPortMalloc(sizeof(Esp32Messaging::ListenRequest));

    listenRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    listenRequest->BackLog = ExtractInt32(buffer);
    return(listenRequest);
}

/**
 *  Convert the BindRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedBindRequestBufferSize method.
 *  
 *  @param bindRequest
 *      BindRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded BindRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeBindRequest
void Encoders::EncodeBindRequest(Esp32Messaging::BindRequest *bindRequest, uint8_t *buffer)
{
    EncodeInt32(bindRequest->SocketHandle, buffer);
    buffer += 4;
    EncodeUInt32(bindRequest->AddrLength, buffer);
    buffer += 4;
    if (bindRequest->AddrLength > 0)
    {
        memcpy((void *) buffer, (void *) bindRequest->Addr, bindRequest->AddrLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the BindRequest object.
 *  
 *  @param bindRequest
 *      BindRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded BindRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedBindRequestBufferSize
int Encoders::EncodedBindRequestBufferSize(Esp32Messaging::BindRequest *bindRequest)
{
    int result = 0;
    result += bindRequest->AddrLength;
    return(result + 8);
}

/**
 *  Extract the BindRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded BindRequest object.
 *  
 *  @returns
 *      Pointer to a BindRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractBindRequest
Esp32Messaging::BindRequest *Encoders::ExtractBindRequest(uint8_t *buffer)
{
    Esp32Messaging::BindRequest *bindRequest = (Esp32Messaging::BindRequest *) pvPortMalloc(sizeof(Esp32Messaging::BindRequest));

    bindRequest->SocketHandle = ExtractInt32(buffer);
    buffer += 4;
    bindRequest->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (bindRequest->AddrLength > 0)
    {
        bindRequest->Addr = (uint8_t *) pvPortMalloc(bindRequest->AddrLength);
        memcpy(bindRequest->Addr, buffer, bindRequest->AddrLength);
        buffer += bindRequest->AddrLength;
    }
    else
    {
        bindRequest->Addr = nullptr;
    }
    return(bindRequest);
}

/**
 *  Convert the AcceptRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedAcceptRequestBufferSize method.
 *  
 *  @param acceptRequest
 *      AcceptRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AcceptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeAcceptRequest
void Encoders::EncodeAcceptRequest(Esp32Messaging::AcceptRequest *acceptRequest, uint8_t *buffer)
{
    EncodeInt32(acceptRequest->SocketHandle, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AcceptRequest object.
 *  
 *  @param acceptRequest
 *      AcceptRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded AcceptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedAcceptRequestBufferSize
int Encoders::EncodedAcceptRequestBufferSize(Esp32Messaging::AcceptRequest *acceptRequest)
{
    return(4);
}

/**
 *  Extract the AcceptRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AcceptRequest object.
 *  
 *  @returns
 *      Pointer to a AcceptRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAcceptRequest
Esp32Messaging::AcceptRequest *Encoders::ExtractAcceptRequest(uint8_t *buffer)
{
    Esp32Messaging::AcceptRequest *acceptRequest = (Esp32Messaging::AcceptRequest *) pvPortMalloc(sizeof(Esp32Messaging::AcceptRequest));

    acceptRequest->SocketHandle = ExtractInt32(buffer);
    return(acceptRequest);
}

/**
 *  Convert the AcceptResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedAcceptResponseBufferSize method.
 *  
 *  @param acceptResponse
 *      AcceptResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded AcceptResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeAcceptResponse
void Encoders::EncodeAcceptResponse(Esp32Messaging::AcceptResponse *acceptResponse, uint8_t *buffer)
{
    EncodeUInt32(acceptResponse->AddrLength, buffer);
    buffer += 4;
    if (acceptResponse->AddrLength > 0)
    {
        memcpy((void *) buffer, (void *) acceptResponse->Addr, acceptResponse->AddrLength);
        buffer += acceptResponse->AddrLength;
    }
    EncodeInt32(acceptResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(acceptResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the AcceptResponse object.
 *  
 *  @param acceptResponse
 *      AcceptResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded AcceptResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedAcceptResponseBufferSize
int Encoders::EncodedAcceptResponseBufferSize(Esp32Messaging::AcceptResponse *acceptResponse)
{
    int result = 0;
    result += acceptResponse->AddrLength;
    return(result + 12);
}

/**
 *  Extract the AcceptResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded AcceptResponse object.
 *  
 *  @returns
 *      Pointer to a AcceptResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractAcceptResponse
Esp32Messaging::AcceptResponse *Encoders::ExtractAcceptResponse(uint8_t *buffer)
{
    Esp32Messaging::AcceptResponse *acceptResponse = (Esp32Messaging::AcceptResponse *) pvPortMalloc(sizeof(Esp32Messaging::AcceptResponse));

    acceptResponse->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (acceptResponse->AddrLength > 0)
    {
        acceptResponse->Addr = (uint8_t *) pvPortMalloc(acceptResponse->AddrLength);
        memcpy(acceptResponse->Addr, buffer, acceptResponse->AddrLength);
        buffer += acceptResponse->AddrLength;
    }
    else
    {
        acceptResponse->Addr = nullptr;
    }
    acceptResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    acceptResponse->ResponseErrno = ExtractInt32(buffer);
    return(acceptResponse);
}

/**
 *  Convert the IoctlRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedIoctlRequestBufferSize method.
 *  
 *  @param ioctlRequest
 *      IoctlRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded IoctlRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeIoctlRequest
void Encoders::EncodeIoctlRequest(Esp32Messaging::IoctlRequest *ioctlRequest, uint8_t *buffer)
{
    EncodeInt32(ioctlRequest->Command, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the IoctlRequest object.
 *  
 *  @param ioctlRequest
 *      IoctlRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded IoctlRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedIoctlRequestBufferSize
int Encoders::EncodedIoctlRequestBufferSize(Esp32Messaging::IoctlRequest *ioctlRequest)
{
    return(4);
}

/**
 *  Extract the IoctlRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded IoctlRequest object.
 *  
 *  @returns
 *      Pointer to a IoctlRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractIoctlRequest
Esp32Messaging::IoctlRequest *Encoders::ExtractIoctlRequest(uint8_t *buffer)
{
    Esp32Messaging::IoctlRequest *ioctlRequest = (Esp32Messaging::IoctlRequest *) pvPortMalloc(sizeof(Esp32Messaging::IoctlRequest));

    ioctlRequest->Command = ExtractInt32(buffer);
    return(ioctlRequest);
}

/**
 *  Convert the IoctlResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedIoctlResponseBufferSize method.
 *  
 *  @param ioctlResponse
 *      IoctlResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded IoctlResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeIoctlResponse
void Encoders::EncodeIoctlResponse(Esp32Messaging::IoctlResponse *ioctlResponse, uint8_t *buffer)
{
    EncodeUInt32(ioctlResponse->AddrLength, buffer);
    buffer += 4;
    if (ioctlResponse->AddrLength > 0)
    {
        memcpy((void *) buffer, (void *) ioctlResponse->Addr, ioctlResponse->AddrLength);
        buffer += ioctlResponse->AddrLength;
    }
    EncodeInt32(ioctlResponse->Flags, buffer);
    buffer += 4;
    EncodeInt32(ioctlResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(ioctlResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the IoctlResponse object.
 *  
 *  @param ioctlResponse
 *      IoctlResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded IoctlResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedIoctlResponseBufferSize
int Encoders::EncodedIoctlResponseBufferSize(Esp32Messaging::IoctlResponse *ioctlResponse)
{
    int result = 0;
    result += ioctlResponse->AddrLength;
    return(result + 16);
}

/**
 *  Extract the IoctlResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded IoctlResponse object.
 *  
 *  @returns
 *      Pointer to a IoctlResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractIoctlResponse
Esp32Messaging::IoctlResponse *Encoders::ExtractIoctlResponse(uint8_t *buffer)
{
    Esp32Messaging::IoctlResponse *ioctlResponse = (Esp32Messaging::IoctlResponse *) pvPortMalloc(sizeof(Esp32Messaging::IoctlResponse));

    ioctlResponse->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (ioctlResponse->AddrLength > 0)
    {
        ioctlResponse->Addr = (uint8_t *) pvPortMalloc(ioctlResponse->AddrLength);
        memcpy(ioctlResponse->Addr, buffer, ioctlResponse->AddrLength);
        buffer += ioctlResponse->AddrLength;
    }
    else
    {
        ioctlResponse->Addr = nullptr;
    }
    ioctlResponse->Flags = ExtractInt32(buffer);
    buffer += 4;
    ioctlResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    ioctlResponse->ResponseErrno = ExtractInt32(buffer);
    return(ioctlResponse);
}

/**
 *  Convert the GetSockPeerNameRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedGetSockPeerNameRequestBufferSize method.
 *  
 *  @param getSockPeerNameRequest
 *      GetSockPeerNameRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetSockPeerNameRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeGetSockPeerNameRequest
void Encoders::EncodeGetSockPeerNameRequest(Esp32Messaging::GetSockPeerNameRequest *getSockPeerNameRequest, uint8_t *buffer)
{
    EncodeInt32(getSockPeerNameRequest->SocketHandle, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetSockPeerNameRequest object.
 *  
 *  @param getSockPeerNameRequest
 *      GetSockPeerNameRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded GetSockPeerNameRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedGetSockPeerNameRequestBufferSize
int Encoders::EncodedGetSockPeerNameRequestBufferSize(Esp32Messaging::GetSockPeerNameRequest *getSockPeerNameRequest)
{
    return(4);
}

/**
 *  Extract the GetSockPeerNameRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetSockPeerNameRequest object.
 *  
 *  @returns
 *      Pointer to a GetSockPeerNameRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetSockPeerNameRequest
Esp32Messaging::GetSockPeerNameRequest *Encoders::ExtractGetSockPeerNameRequest(uint8_t *buffer)
{
    Esp32Messaging::GetSockPeerNameRequest *getSockPeerNameRequest = (Esp32Messaging::GetSockPeerNameRequest *) pvPortMalloc(sizeof(Esp32Messaging::GetSockPeerNameRequest));

    getSockPeerNameRequest->SocketHandle = ExtractInt32(buffer);
    return(getSockPeerNameRequest);
}

/**
 *  Convert the GetSockPeerNameResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedGetSockPeerNameResponseBufferSize method.
 *  
 *  @param getSockPeerNameResponse
 *      GetSockPeerNameResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded GetSockPeerNameResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeGetSockPeerNameResponse
void Encoders::EncodeGetSockPeerNameResponse(Esp32Messaging::GetSockPeerNameResponse *getSockPeerNameResponse, uint8_t *buffer)
{
    EncodeUInt32(getSockPeerNameResponse->AddrLength, buffer);
    buffer += 4;
    if (getSockPeerNameResponse->AddrLength > 0)
    {
        memcpy((void *) buffer, (void *) getSockPeerNameResponse->Addr, getSockPeerNameResponse->AddrLength);
        buffer += getSockPeerNameResponse->AddrLength;
    }
    EncodeInt32(getSockPeerNameResponse->Result, buffer);
    buffer += 4;
    EncodeInt32(getSockPeerNameResponse->ResponseErrno, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the GetSockPeerNameResponse object.
 *  
 *  @param getSockPeerNameResponse
 *      GetSockPeerNameResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded GetSockPeerNameResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedGetSockPeerNameResponseBufferSize
int Encoders::EncodedGetSockPeerNameResponseBufferSize(Esp32Messaging::GetSockPeerNameResponse *getSockPeerNameResponse)
{
    int result = 0;
    result += getSockPeerNameResponse->AddrLength;
    return(result + 12);
}

/**
 *  Extract the GetSockPeerNameResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded GetSockPeerNameResponse object.
 *  
 *  @returns
 *      Pointer to a GetSockPeerNameResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractGetSockPeerNameResponse
Esp32Messaging::GetSockPeerNameResponse *Encoders::ExtractGetSockPeerNameResponse(uint8_t *buffer)
{
    Esp32Messaging::GetSockPeerNameResponse *getSockPeerNameResponse = (Esp32Messaging::GetSockPeerNameResponse *) pvPortMalloc(sizeof(Esp32Messaging::GetSockPeerNameResponse));

    getSockPeerNameResponse->AddrLength = ExtractUInt32(buffer);
    buffer += 4;
    if (getSockPeerNameResponse->AddrLength > 0)
    {
        getSockPeerNameResponse->Addr = (uint8_t *) pvPortMalloc(getSockPeerNameResponse->AddrLength);
        memcpy(getSockPeerNameResponse->Addr, buffer, getSockPeerNameResponse->AddrLength);
        buffer += getSockPeerNameResponse->AddrLength;
    }
    else
    {
        getSockPeerNameResponse->Addr = nullptr;
    }
    getSockPeerNameResponse->Result = ExtractInt32(buffer);
    buffer += 4;
    getSockPeerNameResponse->ResponseErrno = ExtractInt32(buffer);
    return(getSockPeerNameResponse);
}

/**
 *  Convert the EventData object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedEventDataBufferSize method.
 *  
 *  @param eventData
 *      EventData object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded EventData object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeEventData
void Encoders::EncodeEventData(Esp32Messaging::EventData *eventData, uint8_t *buffer)
{
    *buffer = eventData->Interface;
    buffer += 1;
    EncodeUInt32(eventData->Function, buffer);
    buffer += 4;
    EncodeUInt32(eventData->StatusCode, buffer);
    buffer += 4;
    EncodeUInt32(eventData->MessageId, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the EventData object.
 *  
 *  @param eventData
 *      EventData object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded EventData object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedEventDataBufferSize
int Encoders::EncodedEventDataBufferSize(Esp32Messaging::EventData *eventData)
{
    return(13);
}

/**
 *  Extract the EventData object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded EventData object.
 *  
 *  @returns
 *      Pointer to a EventData object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractEventData
Esp32Messaging::EventData *Encoders::ExtractEventData(uint8_t *buffer)
{
    Esp32Messaging::EventData *eventData = (Esp32Messaging::EventData *) pvPortMalloc(sizeof(Esp32Messaging::EventData));

    eventData->Interface = *buffer;
    buffer += 1;
    eventData->Function = ExtractUInt32(buffer);
    buffer += 4;
    eventData->StatusCode = ExtractUInt32(buffer);
    buffer += 4;
    eventData->MessageId = ExtractUInt32(buffer);
    return(eventData);
}

/**
 *  Convert the EventDataPayload object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedEventDataPayloadBufferSize method.
 *  
 *  @param eventDataPayload
 *      EventDataPayload object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded EventDataPayload object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeEventDataPayload
void Encoders::EncodeEventDataPayload(Esp32Messaging::EventDataPayload *eventDataPayload, uint8_t *buffer)
{
    EncodeUInt32(eventDataPayload->MessageId, buffer);
    buffer += 4;
    EncodeUInt32(eventDataPayload->PayloadLength, buffer);
    buffer += 4;
    if (eventDataPayload->PayloadLength > 0)
    {
        memcpy((void *) buffer, (void *) eventDataPayload->Payload, eventDataPayload->PayloadLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the EventDataPayload object.
 *  
 *  @param eventDataPayload
 *      EventDataPayload object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded EventDataPayload object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedEventDataPayloadBufferSize
int Encoders::EncodedEventDataPayloadBufferSize(Esp32Messaging::EventDataPayload *eventDataPayload)
{
    int result = 0;
    result += eventDataPayload->PayloadLength;
    return(result + 8);
}

/**
 *  Extract the EventDataPayload object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded EventDataPayload object.
 *  
 *  @returns
 *      Pointer to a EventDataPayload object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractEventDataPayload
Esp32Messaging::EventDataPayload *Encoders::ExtractEventDataPayload(uint8_t *buffer)
{
    Esp32Messaging::EventDataPayload *eventDataPayload = (Esp32Messaging::EventDataPayload *) pvPortMalloc(sizeof(Esp32Messaging::EventDataPayload));

    eventDataPayload->MessageId = ExtractUInt32(buffer);
    buffer += 4;
    eventDataPayload->PayloadLength = ExtractUInt32(buffer);
    buffer += 4;
    if (eventDataPayload->PayloadLength > 0)
    {
        eventDataPayload->Payload = (uint8_t *) pvPortMalloc(eventDataPayload->PayloadLength);
        memcpy(eventDataPayload->Payload, buffer, eventDataPayload->PayloadLength);
        buffer += eventDataPayload->PayloadLength;
    }
    else
    {
        eventDataPayload->Payload = nullptr;
    }
    return(eventDataPayload);
}

/**
 *  Convert the SetAntennaRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedSetAntennaRequestBufferSize method.
 *  
 *  @param setAntennaRequest
 *      SetAntennaRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded SetAntennaRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeSetAntennaRequest
void Encoders::EncodeSetAntennaRequest(Esp32Messaging::SetAntennaRequest *setAntennaRequest, uint8_t *buffer)
{
    *buffer = setAntennaRequest->Antenna;
    buffer += 1;
    *buffer = setAntennaRequest->Persist;
}

/**
 *  Calculate the amount of memory required to hold the given instance of the SetAntennaRequest object.
 *  
 *  @param setAntennaRequest
 *      SetAntennaRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded SetAntennaRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedSetAntennaRequestBufferSize
int Encoders::EncodedSetAntennaRequestBufferSize(Esp32Messaging::SetAntennaRequest *setAntennaRequest)
{
    return(2);
}

/**
 *  Extract the SetAntennaRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded SetAntennaRequest object.
 *  
 *  @returns
 *      Pointer to a SetAntennaRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractSetAntennaRequest
Esp32Messaging::SetAntennaRequest *Encoders::ExtractSetAntennaRequest(uint8_t *buffer)
{
    Esp32Messaging::SetAntennaRequest *setAntennaRequest = (Esp32Messaging::SetAntennaRequest *) pvPortMalloc(sizeof(Esp32Messaging::SetAntennaRequest));

    setAntennaRequest->Antenna = *buffer;
    buffer += 1;
    setAntennaRequest->Persist = *buffer;
    return(setAntennaRequest);
}

/**
 *  Convert the BTStackConfig object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedBTStackConfigBufferSize method.
 *  
 *  @param bTStackConfig
 *      BTStackConfig object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded BTStackConfig object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeBTStackConfig
void Encoders::EncodeBTStackConfig(Esp32Messaging::BTStackConfig *bTStackConfig, uint8_t *buffer)
{
    EncodeString(bTStackConfig->Config, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the BTStackConfig object.
 *  
 *  @param bTStackConfig
 *      BTStackConfig object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded BTStackConfig object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedBTStackConfigBufferSize
int Encoders::EncodedBTStackConfigBufferSize(Esp32Messaging::BTStackConfig *bTStackConfig)
{
    int result = 0;
    result += Encoders::StringLength(bTStackConfig->Config);
    return(result + 1);
}

/**
 *  Extract the BTStackConfig object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded BTStackConfig object.
 *  
 *  @returns
 *      Pointer to a BTStackConfig object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractBTStackConfig
Esp32Messaging::BTStackConfig *Encoders::ExtractBTStackConfig(uint8_t *buffer)
{
    Esp32Messaging::BTStackConfig *bTStackConfig = (Esp32Messaging::BTStackConfig *) pvPortMalloc(sizeof(Esp32Messaging::BTStackConfig));

    bTStackConfig->Config = ExtractString(buffer);
    return(bTStackConfig);
}

/**
 *  Convert the BTDataWriteRequest object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedBTDataWriteRequestBufferSize method.
 *  
 *  @param bTDataWriteRequest
 *      BTDataWriteRequest object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded BTDataWriteRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeBTDataWriteRequest
void Encoders::EncodeBTDataWriteRequest(Esp32Messaging::BTDataWriteRequest *bTDataWriteRequest, uint8_t *buffer)
{
    EncodeUInt16(bTDataWriteRequest->Handle, buffer);
    buffer += 2;
    EncodeUInt32(bTDataWriteRequest->DataLength, buffer);
    buffer += 4;
    if (bTDataWriteRequest->DataLength > 0)
    {
        memcpy((void *) buffer, (void *) bTDataWriteRequest->Data, bTDataWriteRequest->DataLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the BTDataWriteRequest object.
 *  
 *  @param bTDataWriteRequest
 *      BTDataWriteRequest object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded BTDataWriteRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedBTDataWriteRequestBufferSize
int Encoders::EncodedBTDataWriteRequestBufferSize(Esp32Messaging::BTDataWriteRequest *bTDataWriteRequest)
{
    int result = 0;
    result += bTDataWriteRequest->DataLength;
    return(result + 6);
}

/**
 *  Extract the BTDataWriteRequest object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded BTDataWriteRequest object.
 *  
 *  @returns
 *      Pointer to a BTDataWriteRequest object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractBTDataWriteRequest
Esp32Messaging::BTDataWriteRequest *Encoders::ExtractBTDataWriteRequest(uint8_t *buffer)
{
    Esp32Messaging::BTDataWriteRequest *bTDataWriteRequest = (Esp32Messaging::BTDataWriteRequest *) pvPortMalloc(sizeof(Esp32Messaging::BTDataWriteRequest));

    bTDataWriteRequest->Handle = ExtractUInt16(buffer);
    buffer += 2;
    bTDataWriteRequest->DataLength = ExtractUInt32(buffer);
    buffer += 4;
    if (bTDataWriteRequest->DataLength > 0)
    {
        bTDataWriteRequest->Data = (uint8_t *) pvPortMalloc(bTDataWriteRequest->DataLength);
        memcpy(bTDataWriteRequest->Data, buffer, bTDataWriteRequest->DataLength);
        buffer += bTDataWriteRequest->DataLength;
    }
    else
    {
        bTDataWriteRequest->Data = nullptr;
    }
    return(bTDataWriteRequest);
}

/**
 *  Convert the BTGetHandlesResponse object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedBTGetHandlesResponseBufferSize method.
 *  
 *  @param bTGetHandlesResponse
 *      BTGetHandlesResponse object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded BTGetHandlesResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeBTGetHandlesResponse
void Encoders::EncodeBTGetHandlesResponse(Esp32Messaging::BTGetHandlesResponse *bTGetHandlesResponse, uint8_t *buffer)
{
    EncodeUInt16(bTGetHandlesResponse->HandleCount, buffer);
    buffer += 2;
    EncodeUInt32(bTGetHandlesResponse->HandlesLength, buffer);
    buffer += 4;
    if (bTGetHandlesResponse->HandlesLength > 0)
    {
        memcpy((void *) buffer, (void *) bTGetHandlesResponse->Handles, bTGetHandlesResponse->HandlesLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the BTGetHandlesResponse object.
 *  
 *  @param bTGetHandlesResponse
 *      BTGetHandlesResponse object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded BTGetHandlesResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedBTGetHandlesResponseBufferSize
int Encoders::EncodedBTGetHandlesResponseBufferSize(Esp32Messaging::BTGetHandlesResponse *bTGetHandlesResponse)
{
    int result = 0;
    result += bTGetHandlesResponse->HandlesLength;
    return(result + 6);
}

/**
 *  Extract the BTGetHandlesResponse object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded BTGetHandlesResponse object.
 *  
 *  @returns
 *      Pointer to a BTGetHandlesResponse object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractBTGetHandlesResponse
Esp32Messaging::BTGetHandlesResponse *Encoders::ExtractBTGetHandlesResponse(uint8_t *buffer)
{
    Esp32Messaging::BTGetHandlesResponse *bTGetHandlesResponse = (Esp32Messaging::BTGetHandlesResponse *) pvPortMalloc(sizeof(Esp32Messaging::BTGetHandlesResponse));

    bTGetHandlesResponse->HandleCount = ExtractUInt16(buffer);
    buffer += 2;
    bTGetHandlesResponse->HandlesLength = ExtractUInt32(buffer);
    buffer += 4;
    if (bTGetHandlesResponse->HandlesLength > 0)
    {
        bTGetHandlesResponse->Handles = (uint8_t *) pvPortMalloc(bTGetHandlesResponse->HandlesLength);
        memcpy(bTGetHandlesResponse->Handles, buffer, bTGetHandlesResponse->HandlesLength);
        buffer += bTGetHandlesResponse->HandlesLength;
    }
    else
    {
        bTGetHandlesResponse->Handles = nullptr;
    }
    return(bTGetHandlesResponse);
}

/**
 *  Convert the BTServerDataSet object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedBTServerDataSetBufferSize method.
 *  
 *  @param bTServerDataSet
 *      BTServerDataSet object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded BTServerDataSet object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeBTServerDataSet
void Encoders::EncodeBTServerDataSet(Esp32Messaging::BTServerDataSet *bTServerDataSet, uint8_t *buffer)
{
    EncodeUInt16(bTServerDataSet->Handle, buffer);
    buffer += 2;
    EncodeUInt32(bTServerDataSet->SetDataLength, buffer);
    buffer += 4;
    if (bTServerDataSet->SetDataLength > 0)
    {
        memcpy((void *) buffer, (void *) bTServerDataSet->SetData, bTServerDataSet->SetDataLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the BTServerDataSet object.
 *  
 *  @param bTServerDataSet
 *      BTServerDataSet object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded BTServerDataSet object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedBTServerDataSetBufferSize
int Encoders::EncodedBTServerDataSetBufferSize(Esp32Messaging::BTServerDataSet *bTServerDataSet)
{
    int result = 0;
    result += bTServerDataSet->SetDataLength;
    return(result + 6);
}

/**
 *  Extract the BTServerDataSet object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded BTServerDataSet object.
 *  
 *  @returns
 *      Pointer to a BTServerDataSet object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractBTServerDataSet
Esp32Messaging::BTServerDataSet *Encoders::ExtractBTServerDataSet(uint8_t *buffer)
{
    Esp32Messaging::BTServerDataSet *bTServerDataSet = (Esp32Messaging::BTServerDataSet *) pvPortMalloc(sizeof(Esp32Messaging::BTServerDataSet));

    bTServerDataSet->Handle = ExtractUInt16(buffer);
    buffer += 2;
    bTServerDataSet->SetDataLength = ExtractUInt32(buffer);
    buffer += 4;
    if (bTServerDataSet->SetDataLength > 0)
    {
        bTServerDataSet->SetData = (uint8_t *) pvPortMalloc(bTServerDataSet->SetDataLength);
        memcpy(bTServerDataSet->SetData, buffer, bTServerDataSet->SetDataLength);
        buffer += bTServerDataSet->SetDataLength;
    }
    else
    {
        bTServerDataSet->SetData = nullptr;
    }
    return(bTServerDataSet);
}

/**
 *  Convert the FileDetails object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedFileDetailsBufferSize method.
 *  
 *  @param fileDetails
 *      FileDetails object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded FileDetails object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeFileDetails
void Encoders::EncodeFileDetails(Esp32Messaging::FileDetails *fileDetails, uint8_t *buffer)
{
    EncodeString(fileDetails->Name, buffer);
    buffer += StringLength(fileDetails->Name) + 1;
    EncodeUInt16(fileDetails->Length, buffer);
}

/**
 *  Calculate the amount of memory required to hold the given instance of the FileDetails object.
 *  
 *  @param fileDetails
 *      FileDetails object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded FileDetails object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedFileDetailsBufferSize
int Encoders::EncodedFileDetailsBufferSize(Esp32Messaging::FileDetails *fileDetails)
{
    int result = 0;
    result += Encoders::StringLength(fileDetails->Name);
    return(result + 3);
}

/**
 *  Extract the FileDetails object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded FileDetails object.
 *  
 *  @returns
 *      Pointer to a FileDetails object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractFileDetails
Esp32Messaging::FileDetails *Encoders::ExtractFileDetails(uint8_t *buffer)
{
    Esp32Messaging::FileDetails *fileDetails = (Esp32Messaging::FileDetails *) pvPortMalloc(sizeof(Esp32Messaging::FileDetails));

    fileDetails->Name = ExtractString(buffer);
    buffer += Encoders::StringLength(fileDetails->Name) + 1;
    fileDetails->Length = ExtractUInt16(buffer);
    return(fileDetails);
}

/**
 *  Convert the FileNameAndContents object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedFileNameAndContentsBufferSize method.
 *  
 *  @param fileNameAndContents
 *      FileNameAndContents object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded FileNameAndContents object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeFileNameAndContents
void Encoders::EncodeFileNameAndContents(Esp32Messaging::FileNameAndContents *fileNameAndContents, uint8_t *buffer)
{
    EncodeString(fileNameAndContents->Name, buffer);
    buffer += StringLength(fileNameAndContents->Name) + 1;
    EncodeUInt32(fileNameAndContents->ContentsLength, buffer);
    buffer += 4;
    if (fileNameAndContents->ContentsLength > 0)
    {
        memcpy((void *) buffer, (void *) fileNameAndContents->Contents, fileNameAndContents->ContentsLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the FileNameAndContents object.
 *  
 *  @param fileNameAndContents
 *      FileNameAndContents object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded FileNameAndContents object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedFileNameAndContentsBufferSize
int Encoders::EncodedFileNameAndContentsBufferSize(Esp32Messaging::FileNameAndContents *fileNameAndContents)
{
    int result = 0;
    result += Encoders::StringLength(fileNameAndContents->Name);
    result += fileNameAndContents->ContentsLength;
    return(result + 5);
}

/**
 *  Extract the FileNameAndContents object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded FileNameAndContents object.
 *  
 *  @returns
 *      Pointer to a FileNameAndContents object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractFileNameAndContents
Esp32Messaging::FileNameAndContents *Encoders::ExtractFileNameAndContents(uint8_t *buffer)
{
    Esp32Messaging::FileNameAndContents *fileNameAndContents = (Esp32Messaging::FileNameAndContents *) pvPortMalloc(sizeof(Esp32Messaging::FileNameAndContents));

    fileNameAndContents->Name = ExtractString(buffer);
    buffer += Encoders::StringLength(fileNameAndContents->Name) + 1;
    fileNameAndContents->ContentsLength = ExtractUInt32(buffer);
    buffer += 4;
    if (fileNameAndContents->ContentsLength > 0)
    {
        fileNameAndContents->Contents = (uint8_t *) pvPortMalloc(fileNameAndContents->ContentsLength);
        memcpy(fileNameAndContents->Contents, buffer, fileNameAndContents->ContentsLength);
        buffer += fileNameAndContents->ContentsLength;
    }
    else
    {
        fileNameAndContents->Contents = nullptr;
    }
    return(fileNameAndContents);
}

/**
 *  Convert the FileNameList object into a byte stream that can be sent to the STM32.
 *  
 *  The amount of memory required can be obtained by calling the EncodedFileNameListBufferSize method.
 *  
 *  @param fileNameList
 *      FileNameList object to be encoded.
 *  
 *  @param buffer
 *      Pointer to a block of memory large enough to hold the encoded FileNameList object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodeFileNameList
void Encoders::EncodeFileNameList(Esp32Messaging::FileNameList *fileNameList, uint8_t *buffer)
{
    EncodeUInt16(fileNameList->NumberOfFiles, buffer);
    buffer += 2;
    EncodeUInt32(fileNameList->FileDetailsLength, buffer);
    buffer += 4;
    if (fileNameList->FileDetailsLength > 0)
    {
        memcpy((void *) buffer, (void *) fileNameList->FileDetails, fileNameList->FileDetailsLength);
    }
}

/**
 *  Calculate the amount of memory required to hold the given instance of the FileNameList object.
 *  
 *  @param fileNameList
 *      FileNameList object to be encoded.
 *  
 *  @returns
 *      Number of bytes required to hold the encoded FileNameList object.
 */
// cppcheck-suppress unusedFunction symbolName=EncodedFileNameListBufferSize
int Encoders::EncodedFileNameListBufferSize(Esp32Messaging::FileNameList *fileNameList)
{
    int result = 0;
    result += fileNameList->FileDetailsLength;
    return(result + 6);
}

/**
 *  Extract the FileNameList object that is encoded in the given buffer.
 *  
 *  Note that the returned pointer points to a block of memory on the heap and
 *  this should eventually be released calling vPortFree(...).
 *  
 *  @param buffer
 *      Pointer to a block of memory holding the encoded FileNameList object.
 *  
 *  @returns
 *      Pointer to a FileNameList object.
 */
// cppcheck-suppress unusedFunction symbolName=ExtractFileNameList
Esp32Messaging::FileNameList *Encoders::ExtractFileNameList(uint8_t *buffer)
{
    Esp32Messaging::FileNameList *fileNameList = (Esp32Messaging::FileNameList *) pvPortMalloc(sizeof(Esp32Messaging::FileNameList));

    fileNameList->NumberOfFiles = ExtractUInt16(buffer);
    buffer += 2;
    fileNameList->FileDetailsLength = ExtractUInt32(buffer);
    buffer += 4;
    if (fileNameList->FileDetailsLength > 0)
    {
        fileNameList->FileDetails = (uint8_t *) pvPortMalloc(fileNameList->FileDetailsLength);
        memcpy(fileNameList->FileDetails, buffer, fileNameList->FileDetailsLength);
        buffer += fileNameList->FileDetailsLength;
    }
    else
    {
        fileNameList->FileDetails = nullptr;
    }
    return(fileNameList);
}


