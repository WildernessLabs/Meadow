
#include "espcp_encoders.h"


#include "espcp_encoders.h"

/****************************************************************************
 * Name: espcp_calculate_spi_buffer_size
 *
 * Description:
*Calculate the amount of memory that should be allocated for
*the receive buffer.
 *
*SPI reception on the ESP32 should be on a 32-bit boundary and
*also a multiple of 4 bytes long (See the article linked below).
 *
*https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/spi_slave.html#restrictions-and-known-issues
 *
 * Input Parameters:
*requestedSize - Actual amount of data requested / to be received.
 *
 * Returned Value:
*Number of bytes that should be allocated.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
uint32_t espcp_calculate_spi_buffer_size(uint32_t requestedSize)
{
    uint32_t result = requestedSize;

    if (result < 8)
    {
        result = 8;
    }
    //
    //  The final buffer must be a multiple of 4 bytes so round up if necessary.
    //
    if ((requestedSize & 3) != 0)
    {
        result = (requestedSize & 0xfffffffc) + 4;
    }
    //
    //  There is an issue with the SPI interface that can corrupt the last
    //  few bytes of a message to pad this message out and we will ignore
    //  the last few bytes when encoding / decoding.
    //
    result += ESPCP_SPI_MESSAGE_OVERHEAD;
    return (result);
}

/****************************************************************************
 * Name: espcp_message_buffer_size
 *
 * Description:
*Get the amount of memory needed to store an encoded message.
 *
 * Input Parameters:
* message - Message to be encoded.
* header_only - Are we encoding the fill packet or just the header?
 *
* headerOnly - Will the buffer hold the full message or just the header?
 *
 ****************************************************************************/
uint32_t espcp_encoded_packet_size(espcp_message_t *message, bool header_only)
{
    uint32_t message_size = ESPCP_MESSAGE_HEADER_SIZE;
    if (!header_only)
    {
        message_size += message->packet_length;
    }
    return(message_size);
}

/****************************************************************************
 * Name: espcp_extract_uint16
 *
 * Description:
*Take the first two bytes from the buffer and encode them as a 16 bit
*integer.
 *
*Note that the data should be encoded as LSB first.
 *
 * Input Parameters:
*buffer -  Pointer to the buffer holding the data that should be used 
*to created the 16-bit unsigned integer.
 *
 * Returned Value:
*16-bit unsigned integer value from the first two byte in the buffer.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
uint16_t espcp_extract_uint16(uint8_t *buffer)
{
    uint16_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    return (result);
}

/****************************************************************************
 * Name: espcp_encode_uint16
 *
 * Description:
*Extract the message that is encoded in the byte buffer..
 *
 * Input Parameters:
*value - 16-bit value to encode in the first two bytes of the buffer.
*buffer - Pointer to the buffer where the two bytes will be inserted
*         from the 16-bit unsigned integer (LSB first).
 *
 * Returned Value:
*None
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
void espcp_encode_uint16(uint16_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
}

/****************************************************************************
 * Name: espcp_extract_uint32
 *
 * Description:
*Take the first four bytes from the buffer and encode them as a 32 bit 
*unsigned integer.
 *
*Note that the data should be encoded as LSB first.
 *
 * Input Parameters:
*buffer - Pointer to the buffer where the next four bytes should be
*extracted and a 32-bit unsigned integer created (LSB first).
 *
 * Returned Value:
*32-bit unsigned integer extracted from the first 4 bytes in the buffer.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
uint32_t espcp_extract_uint32(uint8_t *buffer)
{
    uint32_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    result |= (buffer[2] << 16);
    result |= (buffer[3] << 24);
    return (result);
}

/****************************************************************************
 * Name: espcp_encode_uint32
 *
 * Description:
*Encode a 32-bit unsigned integer as four bytes in the buffer.
 *
*Note that the data will be encoded LSB first.
 *
 * Input Parameters:
*value - Unsigned 32-bit integer value to write into the buffer.
*buffer -  Pointer to the buffer where 4 bytes will be replaced with 
*          the four bytes representing unsigned integer (LSB first).
 *
 * Returned Value:
*None
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
void espcp_encode_uint32(uint32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/****************************************************************************
 * Name: espcp_extract_int32
 *
 * Description:
*Take the first four bytes from the buffer and encode them as a 32 bit
*integer.
 *
*Note that the data should be encoded as LSB first.
 *
 * Input Parameters:
*buffer -  Pointer to the buffer where the next four bytes should be
*          extracted and a 32-bit unsigned integer created (LSB first).
 *
 * Returned Value:
*32-bit unsigned integer extracted from the first 4 bytes in the buffer.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
int32_t espcp_extract_int32(uint8_t *buffer)
{
    int32_t result;

    result = buffer[0];
    result |= (buffer[1] << 8);
    result |= (buffer[2] << 16);
    result |= (buffer[3] << 24);
    return (result);
}

/****************************************************************************
 * Name: espcp_extract_message
 *
 * Description:
*Encode a 32-bit integer as four bytes in the buffer.
 *
*Note that the data will be encoded LSB first.
 *
 * Input Parameters:
*value - Unsigned 32-bit integer value to write into the buffer.
*buffer - Pointer to the buffer where 4 bytes will be replaced with the
*         four bytes representing unsigned integer (LSB first).
 * Returned Value:
*None.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
void espcp_encode_int32(int32_t value, uint8_t *buffer)
{
    buffer[0] = (value & 0xff);
    buffer[1] = ((value >> 8) & 0xff);
    buffer[2] = ((value >> 16) & 0xff);
    buffer[3] = ((value >> 24) & 0xff);
}

/****************************************************************************
 * Name: espcp_extract_string
 *
 * Description:
*Extract a string (terminated by 0) from a block of memory.
 *
 * Input Parameters:
*buffer - Pointer to the block of memory containing the string.
 *
 * Returned Value:
*Pointer to the extracted string.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
char *espcp_extract_string(uint8_t *buffer)
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
        ptr = NULL;
    }
    else
    {
        ptr = (uint8_t *) malloc(length + 1);
        if (ptr != NULL)
        {
            strcpy((char *) ptr, (char *) buffer);
        }
    }
    return((char *) ptr);
}

/****************************************************************************
 * Name: espcp_encode_string
 *
 * Description:
*Copy the string into the buffer.
 *
 * Input Parameters:
*source - Block of memory containing the string.
*buffer - Pointer to a block of memory to take the string.
 *
 * Returned Value:
*None.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
void espcp_encode_string(char *source, uint8_t *buffer)
{
    if (source == NULL)
    {
        *buffer = 0;
    }
    else
    {
        strcpy((char *) buffer, (char *) source);
    }
}

/****************************************************************************
 * Name: espcp_string_length
 *
 * Description:
*Get the length of a string taking into account that the string pointer
*may be NULL.
 *
 * Input Parameters:
*string - Block of memory containing the string.
 *
 * Returned Value:
*None.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
uint32_t espcp_string_length(char *string)
{
    uint32_t result = 0;

    if (string != NULL)
    {
        result = strlen(string);
    }
    return(result);
}

/****************************************************************************
 * Name: espcp_crc8
 *
 * Description:
*Calculate the 8-bit CRC value for the specified data buffer.
 *
*This algorithm is loosely based upon the Dallas 1-Wire algorithm.
 *
 * Input Parameters:
*data - Pointer to the buffer containing the data for which the CRC should
*       be calculated.
*len - Number of bytes in the data buffer.
 *
 * Returned Value:
*8-bit checksum of the bytes in the buffer.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
uint8_t espcp_crc8(const uint8_t *data, uint16_t len)
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

/****************************************************************************
 * Name: espcp_crc32
 *
 * Description:
*Calculate the 32-bit CRC value for the specified data buffer.
 *
 * Input Parameters:
*data - Pointer to the buffer containing the data for which the CRC should
*       be calculated.
*len - Number of bytes in the data buffer.
 *
 * Returned Value:
*32-bit checksum of the bytes in the buffer.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
uint32_t espcp_crc32(const uint8_t *data, uint16_t len)
{
    uint32_t crc = ESPCP_CRC32_SEED;

    for (uint16_t byteCounter = 0; byteCounter < len; byteCounter++)
    {
        crc = espcp_progressive_crc32(crc, data[byteCounter]);
    }
    return (crc);
}

/****************************************************************************
 * Name: espcp_progressive_crc32
 *
 * Description:
*Calculate the CRC32 value change for the specified byte.
 *
 * Input Parameters:
*byte - Next byte to use in the checksum calculation.
*currentChecksum - Current value of the checksum.
 *
 * Returned Value:
*Next checksum value.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
uint32_t espcp_progressive_crc32(uint32_t currentChecksum, uint8_t byte)
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

/****************************************************************************
 * Name: espcp_extract_message
 *
 * Description:
*Extract the message that is encoded in the byte buffer.
 * 
*Note:
*There are four bytes at the end of the message that may be corrupted so
*we will pad out the message with four additional bytes and ignore these
*four bytes when calculating CRCs (we do not know what they will contain
*post transmission).
 *
 * Input Parameters:
*buffer - uint8_t array of bytes containing the encoded message.
*buffer_length - size of the buffer holding the data to be decoded.
*header_only - if true then the header will be extracted but not the
*    payload.
 *
 * Returned Value:
*Pointer to the decoded message.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
espcp_message_t *espcp_extract_message(uint8_t *buffer, uint32_t bufferLength, bool headerOnly)
{
    uint32_t crc = espcp_extract_uint32(buffer + ESPCP_MESSAGE_CRC_OFFSET);
    espcp_encode_uint32(0, buffer + ESPCP_MESSAGE_CRC_OFFSET);
    
    espcp_message_t *message = NULL;
    if (crc == espcp_crc32(buffer, bufferLength - 4))   // See note in comment above.
    {
        message = (espcp_message_t *) malloc(sizeof(espcp_message_t));

        memset((void *) message, 0, sizeof(espcp_message_t));
        buffer += 5;                                        // Skip the protocol and CRC.
        message->packet_offset = espcp_extract_uint16(buffer);
        buffer += 2;
        message->packet_length = espcp_extract_uint16(buffer);
        buffer += 2;
        message->message_type = *buffer;
        buffer++;
        message->interface = *buffer;
        buffer++;
        message->function = espcp_extract_uint32(buffer);
        buffer += 4;
        message->status_code = espcp_extract_uint32(buffer);
        buffer += 4;
        message->message_id = espcp_extract_uint32(buffer);
        buffer += 4;
        message->payload_length = espcp_extract_uint32(buffer);
        buffer += 4;
        if (!headerOnly && (message->packet_length > 0))
        {
            message->payload = (uint8_t *) malloc(message->packet_length);
            memcpy(message->payload, buffer, message->packet_length);
        }
        else
        {
            message->payload = NULL;
        }
    }
    else
    {
        message = NULL;
    }
    return(message);
}

/****************************************************************************
 * Name: espcp_encode_message
 *
 * Description:
*Encode a message in a byte buffer.
 *
 * Input Parameters:
*message - Message to be encoded.
*buffer - Pointer to a buffer to take the encoded message.
*buffer_length - Pointer to a uint32_t object that the contain the size of the
*    buffer that contains the encoded message.  This is a return
*    value from the method.
*header_only - if true then the header will be encoded but not the
*    payload.
 *
 * Returned Value:
*Pointer to the block of memory containing the encoded message.
 *
 * Assumptions/Limitations:
*None
 *
 ****************************************************************************/
void espcp_encode_message(espcp_message_t *message, uint8_t *buffer, uint32_t *buffer_length, bool header_only)
{
    uint32_t message_size = espcp_encoded_packet_size(message, header_only);
    uint32_t buffer_size = espcp_calculate_spi_buffer_size(message_size);
    if (buffer != NULL)
    {
        memset(buffer, 0, buffer_size);
        uint8_t *next_location = buffer;

        *next_location = ESPCP_PROTOCOL_NUMBER;                         // 0: Protocol
        next_location++;
        espcp_encode_uint32(0, next_location);                          // 1 - 4: CRC (filled in later)
        next_location += 4;
        espcp_encode_uint16(message->packet_offset, next_location);     // 5 - 6: Packet offset.
        next_location += 2;
        espcp_encode_uint16(message->packet_length, next_location);     // 7 - 8: Packet size
        next_location += 2;
        *next_location = message->message_type;                         // 9: Message type
        next_location++;
        *next_location = message->interface;                            // 10: Interface
        next_location++;
        espcp_encode_uint32(message->function, next_location);          // 11 - 14: Function
        next_location += 4;
        espcp_encode_uint32(message->status_code, next_location);       // 15 - 18: Status code
        next_location += 4;
        espcp_encode_uint32(message->message_id, next_location);        // 19 - 22: Message ID
        next_location += 4;
        espcp_encode_uint32(message->payload_length, next_location);    // 23 - 26: Payload length
        next_location += 4;
        if (!header_only && (message->packet_length > 0))               // 27+: Payload (packet of data)
        {
            memcpy(next_location, message->payload + message->packet_offset, message->packet_length);
        }
        uint32_t crc = espcp_crc32(buffer, buffer_size - 4);            // See note in espcp_extract_message comment.
        espcp_encode_uint32(crc, buffer + ESPCP_MESSAGE_CRC_OFFSET);
    }
    else
    {
        buffer_size = 0;
    }

    *buffer_length = buffer_size;
}

/****************************************************************************
* Name: espcp_extract_system_configuration
*
* Description:
*  Extract the espcp_system_configuration_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  system_configuration - pointer to the buffer containing the encoded
*  espcp_system_configuration_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_system_configuration_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_system_configuration_t *espcp_extract_system_configuration(uint8_t *buffer)
{
    espcp_system_configuration_t *system_configuration = (espcp_system_configuration_t *) malloc(sizeof(espcp_system_configuration_t));

    system_configuration->maximum_message_queue_length = *buffer;
    buffer += 1;
    system_configuration->maximum_retry_count = espcp_extract_int32(buffer);
    buffer += 4;
    system_configuration->antenna = *buffer;
    buffer += 1;
    memcpy((void *) system_configuration->board_mac_address, (void *) buffer, 6);
    buffer += 6;
    memcpy((void *) system_configuration->soft_ap_mac_address, (void *) buffer, 6);
    buffer += 6;
    memcpy((void *) system_configuration->bluetooth_mac_address, (void *) buffer, 6);
    buffer += 6;
    system_configuration->device_name = espcp_extract_string(buffer);
    buffer += espcp_string_length(system_configuration->device_name) + 1;
    system_configuration->default_access_point = espcp_extract_string(buffer);
    buffer += espcp_string_length(system_configuration->default_access_point) + 1;
    system_configuration->reset_reason = *buffer;
    buffer += 1;
    system_configuration->version_major = espcp_extract_uint32(buffer);
    buffer += 4;
    system_configuration->version_minor = espcp_extract_uint32(buffer);
    buffer += 4;
    system_configuration->version_revision = espcp_extract_uint32(buffer);
    buffer += 4;
    system_configuration->version_build = espcp_extract_uint32(buffer);
    buffer += 4;
    system_configuration->build_day = *buffer;
    buffer += 1;
    system_configuration->build_month = *buffer;
    buffer += 1;
    system_configuration->build_year = *buffer;
    buffer += 1;
    system_configuration->build_hour = *buffer;
    buffer += 1;
    system_configuration->build_minute = *buffer;
    buffer += 1;
    system_configuration->build_second = *buffer;
    buffer += 1;
    system_configuration->build_hash = espcp_extract_uint32(buffer);
    buffer += 4;
    system_configuration->build_branch_name = espcp_extract_string(buffer);
    return(system_configuration);
}

/****************************************************************************
* Name: espcp_encode_configuration_value
*
* Description:
*  Convert the espcp_configuration_value_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  configuration_value - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_configuration_value(espcp_configuration_value_t *configuration_value, uint8_t *buffer)
{
    espcp_encode_uint32(configuration_value->item, buffer);
    buffer += 4;
    espcp_encode_uint32(configuration_value->value_length, buffer);
    buffer += 4;
    if (configuration_value->value_length > 0)
    {
        memcpy((void *) buffer, (void *) configuration_value->value, configuration_value->value_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_configuration_value_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_configuration_value_t object.
*
* Input Parameters:
*  espcp_configuration_value_t - espcp_espcp_configuration_value_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_configuration_value_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_configuration_value_buffer_size(espcp_configuration_value_t *configuration_value)
{
    int result = 0;
    result += configuration_value->value_length;
    return(result + 8);
}

/****************************************************************************
* Name: espcp_extract_configuration_value
*
* Description:
*  Extract the espcp_configuration_value_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  configuration_value - pointer to the buffer containing the encoded
*  espcp_configuration_value_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_configuration_value_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_configuration_value_t *espcp_extract_configuration_value(uint8_t *buffer)
{
    espcp_configuration_value_t *configuration_value = (espcp_configuration_value_t *) malloc(sizeof(espcp_configuration_value_t));

    configuration_value->item = espcp_extract_uint32(buffer);
    buffer += 4;
    configuration_value->value_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (configuration_value->value_length > 0)
    {
        configuration_value->value = (uint8_t *) malloc(configuration_value->value_length);
        memcpy(configuration_value->value, buffer, configuration_value->value_length);
        buffer += configuration_value->value_length;
    }
    else
    {
        configuration_value->value = NULL;
    }
    return(configuration_value);
}

/****************************************************************************
* Name: espcp_extract_error_event
*
* Description:
*  Extract the espcp_error_event_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  error_event - pointer to the buffer containing the encoded
*  espcp_error_event_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_error_event_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_error_event_t *espcp_extract_error_event(uint8_t *buffer)
{
    espcp_error_event_t *error_event = (espcp_error_event_t *) malloc(sizeof(espcp_error_event_t));

    error_event->error_code = espcp_extract_uint32(buffer);
    buffer += 4;
    error_event->interface = *buffer;
    buffer += 1;
    error_event->error_data_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (error_event->error_data_length > 0)
    {
        error_event->error_data = (uint8_t *) malloc(error_event->error_data_length);
        memcpy(error_event->error_data, buffer, error_event->error_data_length);
        buffer += error_event->error_data_length;
    }
    else
    {
        error_event->error_data = NULL;
    }
    return(error_event);
}

/****************************************************************************
* Name: espcp_extract_access_point_information
*
* Description:
*  Extract the espcp_access_point_information_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  access_point_information - pointer to the buffer containing the encoded
*  espcp_access_point_information_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_access_point_information_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_access_point_information_t *espcp_extract_access_point_information(uint8_t *buffer)
{
    espcp_access_point_information_t *access_point_information = (espcp_access_point_information_t *) malloc(sizeof(espcp_access_point_information_t));

    access_point_information->network_name = espcp_extract_string(buffer);
    buffer += espcp_string_length(access_point_information->network_name) + 1;
    access_point_information->password = espcp_extract_string(buffer);
    buffer += espcp_string_length(access_point_information->password) + 1;
    access_point_information->ip_address = espcp_extract_uint32(buffer);
    buffer += 4;
    access_point_information->subnet_mask = espcp_extract_uint32(buffer);
    buffer += 4;
    access_point_information->gateway = espcp_extract_uint32(buffer);
    buffer += 4;
    access_point_information->wi_fi_authentication_mode = *buffer;
    buffer += 1;
    access_point_information->channel = *buffer;
    buffer += 1;
    access_point_information->hidden = *buffer;
    return(access_point_information);
}

/****************************************************************************
* Name: espcp_encode_connect_event_data
*
* Description:
*  Convert the espcp_connect_event_data_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  connect_event_data - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_connect_event_data(espcp_connect_event_data_t *connect_event_data, uint8_t *buffer)
{
    espcp_encode_uint32(connect_event_data->ip_address, buffer);
    buffer += 4;
    espcp_encode_uint32(connect_event_data->subnet_mask, buffer);
    buffer += 4;
    espcp_encode_uint32(connect_event_data->gateway, buffer);
    buffer += 4;
    memcpy((void *) buffer, (void *) connect_event_data->ssid, 33);
    buffer += 33;
    memcpy((void *) buffer, (void *) connect_event_data->bssid, 6);
    buffer += 6;
    *buffer = connect_event_data->channel;
    buffer += 1;
    *buffer = connect_event_data->authentication_mode;
    buffer += 1;
    espcp_encode_uint32(connect_event_data->reason, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_connect_event_data_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_connect_event_data_t object.
*
* Input Parameters:
*  espcp_connect_event_data_t - espcp_espcp_connect_event_data_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_connect_event_data_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_connect_event_data_buffer_size(espcp_connect_event_data_t *connect_event_data)
{
    return(57);
}

/****************************************************************************
* Name: espcp_extract_connect_event_data
*  
* Description:
*  Extract the espcp_connect_event_data_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  connect_event_data - pointer to the buffer containing the encoded
*  espcp_connect_event_data_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_connect_event_data_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_connect_event_data_t *espcp_extract_connect_event_data(uint8_t *buffer)
{
    espcp_connect_event_data_t *connect_event_data = (espcp_connect_event_data_t *) malloc(sizeof(espcp_connect_event_data_t));

    connect_event_data->ip_address = espcp_extract_uint32(buffer);
    buffer += 4;
    connect_event_data->subnet_mask = espcp_extract_uint32(buffer);
    buffer += 4;
    connect_event_data->gateway = espcp_extract_uint32(buffer);
    buffer += 4;
    memcpy((void *) connect_event_data->ssid, (void *) buffer, 33);
    buffer += 33;
    memcpy((void *) connect_event_data->bssid, (void *) buffer, 6);
    buffer += 6;
    connect_event_data->channel = *buffer;
    buffer += 1;
    connect_event_data->authentication_mode = *buffer;
    buffer += 1;
    connect_event_data->reason = espcp_extract_uint32(buffer);
    return(connect_event_data);
}

/****************************************************************************
* Name: espcp_extract_disconnect_event_data
*
* Description:
*  Extract the espcp_disconnect_event_data_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  disconnect_event_data - pointer to the buffer containing the encoded
*  espcp_disconnect_event_data_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_disconnect_event_data_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_disconnect_event_data_t *espcp_extract_disconnect_event_data(uint8_t *buffer)
{
    espcp_disconnect_event_data_t *disconnect_event_data = (espcp_disconnect_event_data_t *) malloc(sizeof(espcp_disconnect_event_data_t));

    disconnect_event_data->retrying = *buffer;
    buffer += 1;
    disconnect_event_data->retries_remaining = espcp_extract_int32(buffer);
    buffer += 4;
    disconnect_event_data->reason = espcp_extract_uint32(buffer);
    return(disconnect_event_data);
}

/****************************************************************************
* Name: espcp_extract_access_point
*
* Description:
*  Extract the espcp_access_point_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  access_point - pointer to the buffer containing the encoded
*  espcp_access_point_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_access_point_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_access_point_t *espcp_extract_access_point(uint8_t *buffer)
{
    espcp_access_point_t *access_point = (espcp_access_point_t *) malloc(sizeof(espcp_access_point_t));

    memcpy((void *) access_point->ssid, (void *) buffer, 33);
    buffer += 33;
    memcpy((void *) access_point->bssid, (void *) buffer, 6);
    buffer += 6;
    access_point->primary_channel = *buffer;
    buffer += 1;
    access_point->secondary_channel = *buffer;
    buffer += 1;
    access_point->rssi = (int8_t) *buffer;
    buffer += 1;
    access_point->authentication_mode = *buffer;
    buffer += 1;
    access_point->protocols = espcp_extract_uint32(buffer);
    return(access_point);
}

/****************************************************************************
* Name: espcp_extract_access_point_list
*
* Description:
*  Extract the espcp_access_point_list_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  access_point_list - pointer to the buffer containing the encoded
*  espcp_access_point_list_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_access_point_list_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_access_point_list_t *espcp_extract_access_point_list(uint8_t *buffer)
{
    espcp_access_point_list_t *access_point_list = (espcp_access_point_list_t *) malloc(sizeof(espcp_access_point_list_t));

    access_point_list->number_of_access_points = espcp_extract_uint32(buffer);
    buffer += 4;
    access_point_list->access_points_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (access_point_list->access_points_length > 0)
    {
        access_point_list->access_points = (uint8_t *) malloc(access_point_list->access_points_length);
        memcpy(access_point_list->access_points, buffer, access_point_list->access_points_length);
        buffer += access_point_list->access_points_length;
    }
    else
    {
        access_point_list->access_points = NULL;
    }
    return(access_point_list);
}

/****************************************************************************
* Name: espcp_encode_sock_addr
*
* Description:
*  Convert the espcp_sock_addr_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  sock_addr - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_sock_addr(espcp_sock_addr_t *sock_addr, uint8_t *buffer)
{
    *buffer = sock_addr->family;
    buffer += 1;
    espcp_encode_uint16(sock_addr->port, buffer);
    buffer += 2;
    espcp_encode_uint32(sock_addr->ip4_address, buffer);
    buffer += 4;
    espcp_encode_uint32(sock_addr->flow_info, buffer);
    buffer += 4;
    memcpy((void *) buffer, (void *) sock_addr->ip6_address, 16);
    buffer += 16;
    espcp_encode_uint32(sock_addr->scope_i_d, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_sock_addr_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_sock_addr_t object.
*
* Input Parameters:
*  espcp_sock_addr_t - espcp_espcp_sock_addr_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_sock_addr_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_sock_addr_buffer_size(espcp_sock_addr_t *sock_addr)
{
    return(31);
}

/****************************************************************************
* Name: espcp_extract_sock_addr
*
* Description:
*  Extract the espcp_sock_addr_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  sock_addr - pointer to the buffer containing the encoded
*  espcp_sock_addr_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_sock_addr_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_sock_addr_t *espcp_extract_sock_addr(uint8_t *buffer)
{
    espcp_sock_addr_t *sock_addr = (espcp_sock_addr_t *) malloc(sizeof(espcp_sock_addr_t));

    sock_addr->family = *buffer;
    buffer += 1;
    sock_addr->port = espcp_extract_uint16(buffer);
    buffer += 2;
    sock_addr->ip4_address = espcp_extract_uint32(buffer);
    buffer += 4;
    sock_addr->flow_info = espcp_extract_uint32(buffer);
    buffer += 4;
    memcpy((void *) sock_addr->ip6_address, (void *) buffer, 16);
    buffer += 16;
    sock_addr->scope_i_d = espcp_extract_uint32(buffer);
    return(sock_addr);
}

/****************************************************************************
* Name: espcp_extract_addr_info
*
* Description:
*  Extract the espcp_addr_info_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  addr_info - pointer to the buffer containing the encoded
*  espcp_addr_info_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_addr_info_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_addr_info_t *espcp_extract_addr_info(uint8_t *buffer)
{
    espcp_addr_info_t *addr_info = (espcp_addr_info_t *) malloc(sizeof(espcp_addr_info_t));

    addr_info->my_heap_address = espcp_extract_uint32(buffer);
    buffer += 4;
    addr_info->flags = espcp_extract_int32(buffer);
    buffer += 4;
    addr_info->family = espcp_extract_int32(buffer);
    buffer += 4;
    addr_info->socket_type = espcp_extract_int32(buffer);
    buffer += 4;
    addr_info->protocol = espcp_extract_int32(buffer);
    buffer += 4;
    addr_info->addr_len = espcp_extract_uint32(buffer);
    buffer += 4;
    addr_info->addr_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (addr_info->addr_length > 0)
    {
        addr_info->addr = (uint8_t *) malloc(addr_info->addr_length);
        memcpy(addr_info->addr, buffer, addr_info->addr_length);
        buffer += addr_info->addr_length;
    }
    else
    {
        addr_info->addr = NULL;
    }
    addr_info->canon_name = espcp_extract_string(buffer);
    buffer += espcp_string_length(addr_info->canon_name) + 1;
    addr_info->next = (void *) espcp_extract_uint32(buffer);
    return(addr_info);
}

/****************************************************************************
* Name: espcp_encode_get_addr_info_request
*
* Description:
*  Convert the espcp_get_addr_info_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  get_addr_info_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_get_addr_info_request(espcp_get_addr_info_request_t *get_addr_info_request, uint8_t *buffer)
{
    espcp_encode_string(get_addr_info_request->node_name, buffer);
    buffer += espcp_string_length(get_addr_info_request->node_name) + 1;
    espcp_encode_string(get_addr_info_request->serv_name, buffer);
    buffer += espcp_string_length(get_addr_info_request->serv_name) + 1;
    espcp_encode_uint32(get_addr_info_request->hints_length, buffer);
    buffer += 4;
    if (get_addr_info_request->hints_length > 0)
    {
        memcpy((void *) buffer, (void *) get_addr_info_request->hints, get_addr_info_request->hints_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_get_addr_info_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_get_addr_info_request_t object.
*
* Input Parameters:
*  espcp_get_addr_info_request_t - espcp_espcp_get_addr_info_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_get_addr_info_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_get_addr_info_request_buffer_size(espcp_get_addr_info_request_t *get_addr_info_request)
{
    int result = 0;
    result += espcp_string_length(get_addr_info_request->node_name);
    result += espcp_string_length(get_addr_info_request->serv_name);
    result += get_addr_info_request->hints_length;
    return(result + 6);
}

/****************************************************************************
* Name: espcp_extract_get_addr_info_response
*
* Description:
*  Extract the espcp_get_addr_info_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  get_addr_info_response - pointer to the buffer containing the encoded
*  espcp_get_addr_info_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_get_addr_info_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_get_addr_info_response_t *espcp_extract_get_addr_info_response(uint8_t *buffer)
{
    espcp_get_addr_info_response_t *get_addr_info_response = (espcp_get_addr_info_response_t *) malloc(sizeof(espcp_get_addr_info_response_t));

    get_addr_info_response->addr_info_response_errno = espcp_extract_int32(buffer);
    buffer += 4;
    get_addr_info_response->res_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (get_addr_info_response->res_length > 0)
    {
        get_addr_info_response->res = (uint8_t *) malloc(get_addr_info_response->res_length);
        memcpy(get_addr_info_response->res, buffer, get_addr_info_response->res_length);
        buffer += get_addr_info_response->res_length;
    }
    else
    {
        get_addr_info_response->res = NULL;
    }
    return(get_addr_info_response);
}

/****************************************************************************
* Name: espcp_encode_socket_request
*
* Description:
*  Convert the espcp_socket_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  socket_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_socket_request(espcp_socket_request_t *socket_request, uint8_t *buffer)
{
    espcp_encode_uint32((uint32_t) socket_request->address_information, buffer);
    buffer += 4;
    espcp_encode_int32(socket_request->domain, buffer);
    buffer += 4;
    espcp_encode_int32(socket_request->type, buffer);
    buffer += 4;
    espcp_encode_int32(socket_request->protocol, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_socket_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_socket_request_t object.
*
* Input Parameters:
*  espcp_socket_request_t - espcp_espcp_socket_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_socket_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_socket_request_buffer_size(espcp_socket_request_t *socket_request)
{
    return(16);
}

/****************************************************************************
* Name: espcp_extract_integer_response
*
* Description:
*  Extract the espcp_integer_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  integer_response - pointer to the buffer containing the encoded
*  espcp_integer_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_integer_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_integer_response_t *espcp_extract_integer_response(uint8_t *buffer)
{
    espcp_integer_response_t *integer_response = (espcp_integer_response_t *) malloc(sizeof(espcp_integer_response_t));

    integer_response->result = espcp_extract_int32(buffer);
    return(integer_response);
}

/****************************************************************************
* Name: espcp_extract_integer_and_errno_response
*
* Description:
*  Extract the espcp_integer_and_errno_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  integer_and_errno_response - pointer to the buffer containing the encoded
*  espcp_integer_and_errno_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_integer_and_errno_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_integer_and_errno_response_t *espcp_extract_integer_and_errno_response(uint8_t *buffer)
{
    espcp_integer_and_errno_response_t *integer_and_errno_response = (espcp_integer_and_errno_response_t *) malloc(sizeof(espcp_integer_and_errno_response_t));

    integer_and_errno_response->result = espcp_extract_int32(buffer);
    buffer += 4;
    integer_and_errno_response->response_errno = espcp_extract_int32(buffer);
    return(integer_and_errno_response);
}

/****************************************************************************
* Name: espcp_encode_connect_request
*
* Description:
*  Convert the espcp_connect_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  connect_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_connect_request(espcp_connect_request_t *connect_request, uint8_t *buffer)
{
    espcp_encode_int32(connect_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_uint32(connect_request->addr_length, buffer);
    buffer += 4;
    if (connect_request->addr_length > 0)
    {
        memcpy((void *) buffer, (void *) connect_request->addr, connect_request->addr_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_connect_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_connect_request_t object.
*
* Input Parameters:
*  espcp_connect_request_t - espcp_espcp_connect_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_connect_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_connect_request_buffer_size(espcp_connect_request_t *connect_request)
{
    int result = 0;
    result += connect_request->addr_length;
    return(result + 8);
}

/****************************************************************************
* Name: espcp_encode_free_addr_info_request
*
* Description:
*  Convert the espcp_free_addr_info_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  free_addr_info_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_free_addr_info_request(espcp_free_addr_info_request_t *free_addr_info_request, uint8_t *buffer)
{
    espcp_encode_uint32(free_addr_info_request->addr_info_address, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_free_addr_info_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_free_addr_info_request_t object.
*
* Input Parameters:
*  espcp_free_addr_info_request_t - espcp_espcp_free_addr_info_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_free_addr_info_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_free_addr_info_request_buffer_size(espcp_free_addr_info_request_t *free_addr_info_request)
{
    return(4);
}

/****************************************************************************
* Name: espcp_encode_time_val
*
* Description:
*  Convert the espcp_time_val_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  time_val - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_time_val(espcp_time_val_t *time_val, uint8_t *buffer)
{
    espcp_encode_uint32(time_val->tv_sec, buffer);
    buffer += 4;
    espcp_encode_uint32(time_val->tv_usec, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_time_val_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_time_val_t object.
*
* Input Parameters:
*  espcp_time_val_t - espcp_espcp_time_val_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_time_val_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_time_val_buffer_size(espcp_time_val_t *time_val)
{
    return(8);
}

/****************************************************************************
* Name: espcp_extract_time_val
*
* Description:
*  Extract the espcp_time_val_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  time_val - pointer to the buffer containing the encoded
*  espcp_time_val_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_time_val_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_time_val_t *espcp_extract_time_val(uint8_t *buffer)
{
    espcp_time_val_t *time_val = (espcp_time_val_t *) malloc(sizeof(espcp_time_val_t));

    time_val->tv_sec = espcp_extract_uint32(buffer);
    buffer += 4;
    time_val->tv_usec = espcp_extract_uint32(buffer);
    return(time_val);
}

/****************************************************************************
* Name: espcp_encode_set_sock_opt_request
*
* Description:
*  Convert the espcp_set_sock_opt_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  set_sock_opt_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_set_sock_opt_request(espcp_set_sock_opt_request_t *set_sock_opt_request, uint8_t *buffer)
{
    espcp_encode_int32(set_sock_opt_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_int32(set_sock_opt_request->level, buffer);
    buffer += 4;
    espcp_encode_int32(set_sock_opt_request->option_name, buffer);
    buffer += 4;
    espcp_encode_uint32(set_sock_opt_request->option_value_length, buffer);
    buffer += 4;
    if (set_sock_opt_request->option_value_length > 0)
    {
        memcpy((void *) buffer, (void *) set_sock_opt_request->option_value, set_sock_opt_request->option_value_length);
        buffer += set_sock_opt_request->option_value_length;
    }
    espcp_encode_int32(set_sock_opt_request->option_len, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_set_sock_opt_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_set_sock_opt_request_t object.
*
* Input Parameters:
*  espcp_set_sock_opt_request_t - espcp_espcp_set_sock_opt_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_set_sock_opt_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_set_sock_opt_request_buffer_size(espcp_set_sock_opt_request_t *set_sock_opt_request)
{
    int result = 0;
    result += set_sock_opt_request->option_value_length;
    return(result + 20);
}

/****************************************************************************
* Name: espcp_encode_get_sock_opt_request
*
* Description:
*  Convert the espcp_get_sock_opt_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  get_sock_opt_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_get_sock_opt_request(espcp_get_sock_opt_request_t *get_sock_opt_request, uint8_t *buffer)
{
    espcp_encode_int32(get_sock_opt_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_int32(get_sock_opt_request->level, buffer);
    buffer += 4;
    espcp_encode_int32(get_sock_opt_request->option_name, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_get_sock_opt_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_get_sock_opt_request_t object.
*
* Input Parameters:
*  espcp_get_sock_opt_request_t - espcp_espcp_get_sock_opt_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_get_sock_opt_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_get_sock_opt_request_buffer_size(espcp_get_sock_opt_request_t *get_sock_opt_request)
{
    return(12);
}

/****************************************************************************
* Name: espcp_extract_get_sock_opt_response
*
* Description:
*  Extract the espcp_get_sock_opt_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  get_sock_opt_response - pointer to the buffer containing the encoded
*  espcp_get_sock_opt_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_get_sock_opt_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_get_sock_opt_response_t *espcp_extract_get_sock_opt_response(uint8_t *buffer)
{
    espcp_get_sock_opt_response_t *get_sock_opt_response = (espcp_get_sock_opt_response_t *) malloc(sizeof(espcp_get_sock_opt_response_t));

    get_sock_opt_response->result = espcp_extract_int32(buffer);
    buffer += 4;
    get_sock_opt_response->response_errno = espcp_extract_int32(buffer);
    buffer += 4;
    get_sock_opt_response->option_value_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (get_sock_opt_response->option_value_length > 0)
    {
        get_sock_opt_response->option_value = (uint8_t *) malloc(get_sock_opt_response->option_value_length);
        memcpy(get_sock_opt_response->option_value, buffer, get_sock_opt_response->option_value_length);
        buffer += get_sock_opt_response->option_value_length;
    }
    else
    {
        get_sock_opt_response->option_value = NULL;
    }
    get_sock_opt_response->option_len = espcp_extract_int32(buffer);
    return(get_sock_opt_response);
}

/****************************************************************************
* Name: espcp_encode_linger
*
* Description:
*  Convert the espcp_linger_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  linger - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_linger(espcp_linger_t *linger, uint8_t *buffer)
{
    espcp_encode_int32(linger->l_on_off, buffer);
    buffer += 4;
    espcp_encode_int32(linger->l_linger, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_linger_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_linger_t object.
*
* Input Parameters:
*  espcp_linger_t - espcp_espcp_linger_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_linger_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_linger_buffer_size(espcp_linger_t *linger)
{
    return(8);
}

/****************************************************************************
* Name: espcp_extract_linger
*
* Description:
*  Extract the espcp_linger_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  linger - pointer to the buffer containing the encoded
*  espcp_linger_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_linger_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_linger_t *espcp_extract_linger(uint8_t *buffer)
{
    espcp_linger_t *linger = (espcp_linger_t *) malloc(sizeof(espcp_linger_t));

    linger->l_on_off = espcp_extract_int32(buffer);
    buffer += 4;
    linger->l_linger = espcp_extract_int32(buffer);
    return(linger);
}

/****************************************************************************
* Name: espcp_encode_write_request
*
* Description:
*  Convert the espcp_write_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  write_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_write_request(espcp_write_request_t *write_request, uint8_t *buffer)
{
    espcp_encode_int32(write_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_uint32(write_request->buffer_length, buffer);
    buffer += 4;
    if (write_request->buffer_length > 0)
    {
        memcpy((void *) buffer, (void *) write_request->buffer, write_request->buffer_length);
        buffer += write_request->buffer_length;
    }
    espcp_encode_int32(write_request->count, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_write_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_write_request_t object.
*
* Input Parameters:
*  espcp_write_request_t - espcp_espcp_write_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_write_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_write_request_buffer_size(espcp_write_request_t *write_request)
{
    int result = 0;
    result += write_request->buffer_length;
    return(result + 12);
}

/****************************************************************************
* Name: espcp_encode_read_request
*
* Description:
*  Convert the espcp_read_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  read_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_read_request(espcp_read_request_t *read_request, uint8_t *buffer)
{
    espcp_encode_int32(read_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_int32(read_request->count, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_read_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_read_request_t object.
*
* Input Parameters:
*  espcp_read_request_t - espcp_espcp_read_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_read_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_read_request_buffer_size(espcp_read_request_t *read_request)
{
    return(8);
}

/****************************************************************************
* Name: espcp_extract_read_response
*
* Description:
*  Extract the espcp_read_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  read_response - pointer to the buffer containing the encoded
*  espcp_read_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_read_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_read_response_t *espcp_extract_read_response(uint8_t *buffer)
{
    espcp_read_response_t *read_response = (espcp_read_response_t *) malloc(sizeof(espcp_read_response_t));

    read_response->buffer_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (read_response->buffer_length > 0)
    {
        read_response->buffer = (uint8_t *) malloc(read_response->buffer_length);
        memcpy(read_response->buffer, buffer, read_response->buffer_length);
        buffer += read_response->buffer_length;
    }
    else
    {
        read_response->buffer = NULL;
    }
    read_response->read_response_result = espcp_extract_int32(buffer);
    buffer += 4;
    read_response->read_response_errno = espcp_extract_int32(buffer);
    return(read_response);
}

/****************************************************************************
* Name: espcp_encode_close_request
*
* Description:
*  Convert the espcp_close_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  close_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_close_request(espcp_close_request_t *close_request, uint8_t *buffer)
{
    espcp_encode_int32(close_request->socket_handle, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_close_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_close_request_t object.
*
* Input Parameters:
*  espcp_close_request_t - espcp_espcp_close_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_close_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_close_request_buffer_size(espcp_close_request_t *close_request)
{
    return(4);
}

/****************************************************************************
* Name: espcp_extract_get_battery_charge_level_response
*
* Description:
*  Extract the espcp_get_battery_charge_level_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  get_battery_charge_level_response - pointer to the buffer containing the encoded
*  espcp_get_battery_charge_level_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_get_battery_charge_level_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_get_battery_charge_level_response_t *espcp_extract_get_battery_charge_level_response(uint8_t *buffer)
{
    espcp_get_battery_charge_level_response_t *get_battery_charge_level_response = (espcp_get_battery_charge_level_response_t *) malloc(sizeof(espcp_get_battery_charge_level_response_t));

    get_battery_charge_level_response->level = espcp_extract_uint32(buffer);
    return(get_battery_charge_level_response);
}

/****************************************************************************
* Name: espcp_encode_send_to_request
*
* Description:
*  Convert the espcp_send_to_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  send_to_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_send_to_request(espcp_send_to_request_t *send_to_request, uint8_t *buffer)
{
    espcp_encode_int32(send_to_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_uint32(send_to_request->buffer_length, buffer);
    buffer += 4;
    if (send_to_request->buffer_length > 0)
    {
        memcpy((void *) buffer, (void *) send_to_request->buffer, send_to_request->buffer_length);
        buffer += send_to_request->buffer_length;
    }
    espcp_encode_int32(send_to_request->length, buffer);
    buffer += 4;
    espcp_encode_int32(send_to_request->flags, buffer);
    buffer += 4;
    espcp_encode_uint32(send_to_request->destination_address_length, buffer);
    buffer += 4;
    if (send_to_request->destination_address_length > 0)
    {
        memcpy((void *) buffer, (void *) send_to_request->destination_address, send_to_request->destination_address_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_send_to_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_send_to_request_t object.
*
* Input Parameters:
*  espcp_send_to_request_t - espcp_espcp_send_to_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_send_to_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_send_to_request_buffer_size(espcp_send_to_request_t *send_to_request)
{
    int result = 0;
    result += send_to_request->buffer_length;
    result += send_to_request->destination_address_length;
    return(result + 20);
}

/****************************************************************************
* Name: espcp_encode_recv_from_request
*
* Description:
*  Convert the espcp_recv_from_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  recv_from_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_recv_from_request(espcp_recv_from_request_t *recv_from_request, uint8_t *buffer)
{
    espcp_encode_int32(recv_from_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_int32(recv_from_request->length, buffer);
    buffer += 4;
    espcp_encode_int32(recv_from_request->flags, buffer);
    buffer += 4;
    espcp_encode_int32(recv_from_request->get_source_address, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_recv_from_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_recv_from_request_t object.
*
* Input Parameters:
*  espcp_recv_from_request_t - espcp_espcp_recv_from_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_recv_from_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_recv_from_request_buffer_size(espcp_recv_from_request_t *recv_from_request)
{
    return(16);
}

/****************************************************************************
* Name: espcp_extract_recv_from_response
*
* Description:
*  Extract the espcp_recv_from_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  recv_from_response - pointer to the buffer containing the encoded
*  espcp_recv_from_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_recv_from_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_recv_from_response_t *espcp_extract_recv_from_response(uint8_t *buffer)
{
    espcp_recv_from_response_t *recv_from_response = (espcp_recv_from_response_t *) malloc(sizeof(espcp_recv_from_response_t));

    recv_from_response->buffer_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (recv_from_response->buffer_length > 0)
    {
        recv_from_response->buffer = (uint8_t *) malloc(recv_from_response->buffer_length);
        memcpy(recv_from_response->buffer, buffer, recv_from_response->buffer_length);
        buffer += recv_from_response->buffer_length;
    }
    else
    {
        recv_from_response->buffer = NULL;
    }
    recv_from_response->result = espcp_extract_int32(buffer);
    buffer += 4;
    recv_from_response->response_errno = espcp_extract_int32(buffer);
    buffer += 4;
    recv_from_response->source_address_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (recv_from_response->source_address_length > 0)
    {
        recv_from_response->source_address = (uint8_t *) malloc(recv_from_response->source_address_length);
        memcpy(recv_from_response->source_address, buffer, recv_from_response->source_address_length);
        buffer += recv_from_response->source_address_length;
    }
    else
    {
        recv_from_response->source_address = NULL;
    }
    recv_from_response->source_address_len = espcp_extract_uint32(buffer);
    return(recv_from_response);
}

/****************************************************************************
* Name: espcp_encode_poll_request
*
* Description:
*  Convert the espcp_poll_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  poll_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_poll_request(espcp_poll_request_t *poll_request, uint8_t *buffer)
{
    espcp_encode_int32(poll_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_uint16(poll_request->events, buffer);
    buffer += 2;
    espcp_encode_int32(poll_request->timeout, buffer);
    buffer += 4;
    espcp_encode_int32(poll_request->setup, buffer);
    buffer += 4;
    espcp_encode_uint32(poll_request->setup_message_id, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_poll_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_poll_request_t object.
*
* Input Parameters:
*  espcp_poll_request_t - espcp_espcp_poll_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_poll_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_poll_request_buffer_size(espcp_poll_request_t *poll_request)
{
    return(18);
}

/****************************************************************************
* Name: espcp_extract_poll_response
*
* Description:
*  Extract the espcp_poll_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  poll_response - pointer to the buffer containing the encoded
*  espcp_poll_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_poll_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_poll_response_t *espcp_extract_poll_response(uint8_t *buffer)
{
    espcp_poll_response_t *poll_response = (espcp_poll_response_t *) malloc(sizeof(espcp_poll_response_t));

    poll_response->returned_events = espcp_extract_uint16(buffer);
    buffer += 2;
    poll_response->result = espcp_extract_int32(buffer);
    buffer += 4;
    poll_response->response_errno = espcp_extract_int32(buffer);
    return(poll_response);
}

/****************************************************************************
* Name: espcp_extract_interrupt_poll_response
*
* Description:
*  Extract the espcp_interrupt_poll_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  interrupt_poll_response - pointer to the buffer containing the encoded
*  espcp_interrupt_poll_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_interrupt_poll_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_interrupt_poll_response_t *espcp_extract_interrupt_poll_response(uint8_t *buffer)
{
    espcp_interrupt_poll_response_t *interrupt_poll_response = (espcp_interrupt_poll_response_t *) malloc(sizeof(espcp_interrupt_poll_response_t));

    interrupt_poll_response->socket_handle = espcp_extract_int32(buffer);
    buffer += 4;
    interrupt_poll_response->result = espcp_extract_int32(buffer);
    buffer += 4;
    interrupt_poll_response->response_errno = espcp_extract_int32(buffer);
    buffer += 4;
    interrupt_poll_response->returned_events = espcp_extract_uint16(buffer);
    buffer += 2;
    interrupt_poll_response->setup_message_id = espcp_extract_uint32(buffer);
    return(interrupt_poll_response);
}

/****************************************************************************
* Name: espcp_encode_listen_request
*
* Description:
*  Convert the espcp_listen_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  listen_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_listen_request(espcp_listen_request_t *listen_request, uint8_t *buffer)
{
    espcp_encode_int32(listen_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_int32(listen_request->back_log, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_listen_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_listen_request_t object.
*
* Input Parameters:
*  espcp_listen_request_t - espcp_espcp_listen_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_listen_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_listen_request_buffer_size(espcp_listen_request_t *listen_request)
{
    return(8);
}

/****************************************************************************
* Name: espcp_encode_bind_request
*
* Description:
*  Convert the espcp_bind_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  bind_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_bind_request(espcp_bind_request_t *bind_request, uint8_t *buffer)
{
    espcp_encode_int32(bind_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_uint32(bind_request->addr_length, buffer);
    buffer += 4;
    if (bind_request->addr_length > 0)
    {
        memcpy((void *) buffer, (void *) bind_request->addr, bind_request->addr_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_bind_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_bind_request_t object.
*
* Input Parameters:
*  espcp_bind_request_t - espcp_espcp_bind_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_bind_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_bind_request_buffer_size(espcp_bind_request_t *bind_request)
{
    int result = 0;
    result += bind_request->addr_length;
    return(result + 8);
}

/****************************************************************************
* Name: espcp_encode_accept_request
*
* Description:
*  Convert the espcp_accept_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  accept_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_accept_request(espcp_accept_request_t *accept_request, uint8_t *buffer)
{
    espcp_encode_int32(accept_request->socket_handle, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_accept_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_accept_request_t object.
*
* Input Parameters:
*  espcp_accept_request_t - espcp_espcp_accept_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_accept_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_accept_request_buffer_size(espcp_accept_request_t *accept_request)
{
    return(4);
}

/****************************************************************************
* Name: espcp_extract_accept_response
*
* Description:
*  Extract the espcp_accept_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  accept_response - pointer to the buffer containing the encoded
*  espcp_accept_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_accept_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_accept_response_t *espcp_extract_accept_response(uint8_t *buffer)
{
    espcp_accept_response_t *accept_response = (espcp_accept_response_t *) malloc(sizeof(espcp_accept_response_t));

    accept_response->addr_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (accept_response->addr_length > 0)
    {
        accept_response->addr = (uint8_t *) malloc(accept_response->addr_length);
        memcpy(accept_response->addr, buffer, accept_response->addr_length);
        buffer += accept_response->addr_length;
    }
    else
    {
        accept_response->addr = NULL;
    }
    accept_response->result = espcp_extract_int32(buffer);
    buffer += 4;
    accept_response->response_errno = espcp_extract_int32(buffer);
    return(accept_response);
}

/****************************************************************************
* Name: espcp_encode_ioctl_request
*
* Description:
*  Convert the espcp_ioctl_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  ioctl_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_ioctl_request(espcp_ioctl_request_t *ioctl_request, uint8_t *buffer)
{
    espcp_encode_int32(ioctl_request->command, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_ioctl_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_ioctl_request_t object.
*
* Input Parameters:
*  espcp_ioctl_request_t - espcp_espcp_ioctl_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_ioctl_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_ioctl_request_buffer_size(espcp_ioctl_request_t *ioctl_request)
{
    return(4);
}

/****************************************************************************
* Name: espcp_extract_ioctl_response
*
* Description:
*  Extract the espcp_ioctl_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  ioctl_response - pointer to the buffer containing the encoded
*  espcp_ioctl_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_ioctl_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_ioctl_response_t *espcp_extract_ioctl_response(uint8_t *buffer)
{
    espcp_ioctl_response_t *ioctl_response = (espcp_ioctl_response_t *) malloc(sizeof(espcp_ioctl_response_t));

    ioctl_response->addr_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (ioctl_response->addr_length > 0)
    {
        ioctl_response->addr = (uint8_t *) malloc(ioctl_response->addr_length);
        memcpy(ioctl_response->addr, buffer, ioctl_response->addr_length);
        buffer += ioctl_response->addr_length;
    }
    else
    {
        ioctl_response->addr = NULL;
    }
    ioctl_response->flags = espcp_extract_int32(buffer);
    buffer += 4;
    ioctl_response->result = espcp_extract_int32(buffer);
    buffer += 4;
    ioctl_response->response_errno = espcp_extract_int32(buffer);
    return(ioctl_response);
}

/****************************************************************************
* Name: espcp_encode_get_sock_peer_name_request
*
* Description:
*  Convert the espcp_get_sock_peer_name_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  get_sock_peer_name_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_get_sock_peer_name_request(espcp_get_sock_peer_name_request_t *get_sock_peer_name_request, uint8_t *buffer)
{
    espcp_encode_int32(get_sock_peer_name_request->socket_handle, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_get_sock_peer_name_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_get_sock_peer_name_request_t object.
*
* Input Parameters:
*  espcp_get_sock_peer_name_request_t - espcp_espcp_get_sock_peer_name_request_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_get_sock_peer_name_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_get_sock_peer_name_request_buffer_size(espcp_get_sock_peer_name_request_t *get_sock_peer_name_request)
{
    return(4);
}

/****************************************************************************
* Name: espcp_extract_get_sock_peer_name_response
*
* Description:
*  Extract the espcp_get_sock_peer_name_response_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  get_sock_peer_name_response - pointer to the buffer containing the encoded
*  espcp_get_sock_peer_name_response_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_get_sock_peer_name_response_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_get_sock_peer_name_response_t *espcp_extract_get_sock_peer_name_response(uint8_t *buffer)
{
    espcp_get_sock_peer_name_response_t *get_sock_peer_name_response = (espcp_get_sock_peer_name_response_t *) malloc(sizeof(espcp_get_sock_peer_name_response_t));

    get_sock_peer_name_response->addr_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (get_sock_peer_name_response->addr_length > 0)
    {
        get_sock_peer_name_response->addr = (uint8_t *) malloc(get_sock_peer_name_response->addr_length);
        memcpy(get_sock_peer_name_response->addr, buffer, get_sock_peer_name_response->addr_length);
        buffer += get_sock_peer_name_response->addr_length;
    }
    else
    {
        get_sock_peer_name_response->addr = NULL;
    }
    get_sock_peer_name_response->result = espcp_extract_int32(buffer);
    buffer += 4;
    get_sock_peer_name_response->response_errno = espcp_extract_int32(buffer);
    return(get_sock_peer_name_response);
}

/****************************************************************************
* Name: espcp_encode_event_data
*
* Description:
*  Convert the espcp_event_data_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  event_data - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_event_data(espcp_event_data_t *event_data, uint8_t *buffer)
{
    *buffer = event_data->interface;
    buffer += 1;
    espcp_encode_uint32(event_data->function, buffer);
    buffer += 4;
    espcp_encode_uint32(event_data->status_code, buffer);
    buffer += 4;
    espcp_encode_uint32(event_data->message_id, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_event_data_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_event_data_t object.
*
* Input Parameters:
*  espcp_event_data_t - espcp_espcp_event_data_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_event_data_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_event_data_buffer_size(espcp_event_data_t *event_data)
{
    return(13);
}

/****************************************************************************
* Name: espcp_extract_event_data
*
* Description:
*  Extract the espcp_event_data_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  event_data - pointer to the buffer containing the encoded
*  espcp_event_data_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_event_data_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_event_data_t *espcp_extract_event_data(uint8_t *buffer)
{
    espcp_event_data_t *event_data = (espcp_event_data_t *) malloc(sizeof(espcp_event_data_t));

    event_data->interface = *buffer;
    buffer += 1;
    event_data->function = espcp_extract_uint32(buffer);
    buffer += 4;
    event_data->status_code = espcp_extract_uint32(buffer);
    buffer += 4;
    event_data->message_id = espcp_extract_uint32(buffer);
    return(event_data);
}

/****************************************************************************
* Name: espcp_extract_event_data_payload
*
* Description:
*  Extract the espcp_event_data_payload_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  event_data_payload - pointer to the buffer containing the encoded
*  espcp_event_data_payload_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_event_data_payload_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_event_data_payload_t *espcp_extract_event_data_payload(uint8_t *buffer)
{
    espcp_event_data_payload_t *event_data_payload = (espcp_event_data_payload_t *) malloc(sizeof(espcp_event_data_payload_t));

    event_data_payload->message_id = espcp_extract_uint32(buffer);
    buffer += 4;
    event_data_payload->payload_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (event_data_payload->payload_length > 0)
    {
        event_data_payload->payload = (uint8_t *) malloc(event_data_payload->payload_length);
        memcpy(event_data_payload->payload, buffer, event_data_payload->payload_length);
        buffer += event_data_payload->payload_length;
    }
    else
    {
        event_data_payload->payload = NULL;
    }
    return(event_data_payload);
}

/****************************************************************************
* Name: espcp_encode_file_details
*
* Description:
*  Convert the espcp_file_details_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  file_details - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_file_details(espcp_file_details_t *file_details, uint8_t *buffer)
{
    espcp_encode_string(file_details->name, buffer);
    buffer += espcp_string_length(file_details->name) + 1;
    espcp_encode_uint16(file_details->length, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_file_details_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_file_details_t object.
*
* Input Parameters:
*  espcp_file_details_t - espcp_espcp_file_details_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_file_details_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_file_details_buffer_size(espcp_file_details_t *file_details)
{
    int result = 0;
    result += espcp_string_length(file_details->name);
    return(result + 3);
}

/****************************************************************************
* Name: espcp_extract_file_details
*
* Description:
*  Extract the espcp_file_details_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  file_details - pointer to the buffer containing the encoded
*  espcp_file_details_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_file_details_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_file_details_t *espcp_extract_file_details(uint8_t *buffer)
{
    espcp_file_details_t *file_details = (espcp_file_details_t *) malloc(sizeof(espcp_file_details_t));

    file_details->name = espcp_extract_string(buffer);
    buffer += espcp_string_length(file_details->name) + 1;
    file_details->length = espcp_extract_uint16(buffer);
    return(file_details);
}

/****************************************************************************
* Name: espcp_encode_file_name_and_contents
*
* Description:
*  Convert the espcp_file_name_and_contents_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  file_name_and_contents - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_file_name_and_contents(espcp_file_name_and_contents_t *file_name_and_contents, uint8_t *buffer)
{
    espcp_encode_string(file_name_and_contents->name, buffer);
    buffer += espcp_string_length(file_name_and_contents->name) + 1;
    espcp_encode_uint32(file_name_and_contents->contents_length, buffer);
    buffer += 4;
    if (file_name_and_contents->contents_length > 0)
    {
        memcpy((void *) buffer, (void *) file_name_and_contents->contents, file_name_and_contents->contents_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_file_name_and_contents_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_file_name_and_contents_t object.
*
* Input Parameters:
*  espcp_file_name_and_contents_t - espcp_espcp_file_name_and_contents_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_file_name_and_contents_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_file_name_and_contents_buffer_size(espcp_file_name_and_contents_t *file_name_and_contents)
{
    int result = 0;
    result += espcp_string_length(file_name_and_contents->name);
    result += file_name_and_contents->contents_length;
    return(result + 5);
}

/****************************************************************************
* Name: espcp_extract_file_name_and_contents
*
* Description:
*  Extract the espcp_file_name_and_contents_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  file_name_and_contents - pointer to the buffer containing the encoded
*  espcp_file_name_and_contents_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_file_name_and_contents_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_file_name_and_contents_t *espcp_extract_file_name_and_contents(uint8_t *buffer)
{
    espcp_file_name_and_contents_t *file_name_and_contents = (espcp_file_name_and_contents_t *) malloc(sizeof(espcp_file_name_and_contents_t));

    file_name_and_contents->name = espcp_extract_string(buffer);
    buffer += espcp_string_length(file_name_and_contents->name) + 1;
    file_name_and_contents->contents_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (file_name_and_contents->contents_length > 0)
    {
        file_name_and_contents->contents = (uint8_t *) malloc(file_name_and_contents->contents_length);
        memcpy(file_name_and_contents->contents, buffer, file_name_and_contents->contents_length);
        buffer += file_name_and_contents->contents_length;
    }
    else
    {
        file_name_and_contents->contents = NULL;
    }
    return(file_name_and_contents);
}

/****************************************************************************
* Name: espcp_encode_file_name_list
*
* Description:
*  Convert the espcp_file_name_list_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  file_name_list - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_file_name_list(espcp_file_name_list_t *file_name_list, uint8_t *buffer)
{
    espcp_encode_uint16(file_name_list->number_of_files, buffer);
    buffer += 2;
    espcp_encode_uint32(file_name_list->file_details_length, buffer);
    buffer += 4;
    if (file_name_list->file_details_length > 0)
    {
        memcpy((void *) buffer, (void *) file_name_list->file_details, file_name_list->file_details_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_file_name_list_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_file_name_list_t object.
*
* Input Parameters:
*  espcp_file_name_list_t - espcp_espcp_file_name_list_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_file_name_list_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_file_name_list_buffer_size(espcp_file_name_list_t *file_name_list)
{
    int result = 0;
    result += file_name_list->file_details_length;
    return(result + 6);
}

/****************************************************************************
* Name: espcp_extract_file_name_list
*
* Description:
*  Extract the espcp_file_name_list_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  file_name_list - pointer to the buffer containing the encoded
*  espcp_file_name_list_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_file_name_list_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_file_name_list_t *espcp_extract_file_name_list(uint8_t *buffer)
{
    espcp_file_name_list_t *file_name_list = (espcp_file_name_list_t *) malloc(sizeof(espcp_file_name_list_t));

    file_name_list->number_of_files = espcp_extract_uint16(buffer);
    buffer += 2;
    file_name_list->file_details_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (file_name_list->file_details_length > 0)
    {
        file_name_list->file_details = (uint8_t *) malloc(file_name_list->file_details_length);
        memcpy(file_name_list->file_details, buffer, file_name_list->file_details_length);
        buffer += file_name_list->file_details_length;
    }
    else
    {
        file_name_list->file_details = NULL;
    }
    return(file_name_list);
}


