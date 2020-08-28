
#include "espcp_encoders.h"

/****************************************************************************
 * Name: espcp_calculate_spi_buffer_size
 *
 * Description:
 *  Calculate the amount of memory that should be allocated for
 *  the receive buffer.
 *
 *  SPI reception on the ESP32 should be on a 32-bit boundary and
 *  also a multiple of 4 bytes long (See the article linked below).
 *
 *  https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/spi_slave.html#restrictions-and-known-issues
 *
 * Input Parameters:
 *  requestedSize - Actual amount of data requested / to be received.
 *
 * Returned Value:
 *  Number of bytes that should be allocated.
 *
 * Assumptions/Limitations:
 *  None
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
    //  The buffer should always be 4 bytes longer than needed.  During development it
    //  was found that the last four bytes of any transmission were being discarded.
    //  Empirical tests proved this for 24, 32 and 40 byte packets.
    //
    //  The work around is to increase the packet size by 4 and have dummy data in the
    //  last four bytes and discard the bytes.
    //
    //
    //  See support post: https://esp32.com/viewtopic.php?f=13&t=10117
    //
    requestedSize += 4;
    if ((requestedSize & 3) != 0)
    {
        result = (requestedSize & 0xfffffffc) + 4;
    }
    else
    {
        result += 4;
    }
    return (result);
}

/****************************************************************************
 * Name: espcp_extract_uint16
 *
 * Description:
 *  Take the first two bytes from the buffer and encode them as a 16 bit
 *  integer.
 *
 *  Note that the data should be encoded as LSB first.
 *
 * Input Parameters:
 *  buffer -  Pointer to the buffer holding the data that should be used 
 *  to created the 16-bit unsigned integer.
 *
 * Returned Value:
 *  16-bit unsigned integer value from the first two byte in the buffer.
 *
 * Assumptions/Limitations:
 *  None
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
 *  Extract the message that is encoded in the byte buffer..
 *
 * Input Parameters:
 *  value - 16-bit value to encode in the first two bytes of the buffer.
 *  buffer - Pointer to the buffer where the two bytes will be inserted
 *           from the 16-bit unsigned integer (LSB first).
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
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
 *  Take the first four bytes from the buffer and encode them as a 32 bit 
 *  unsigned integer.
 *
 *  Note that the data should be encoded as LSB first.
 *
 * Input Parameters:
 *  buffer - Pointer to the buffer where the next four bytes should be
 *  extracted and a 32-bit unsigned integer created (LSB first).
 *
 * Returned Value:
 *  32-bit unsigned integer extracted from the first 4 bytes in the buffer.
 *
 * Assumptions/Limitations:
 *  None
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
 *  Encode a 32-bit unsigned integer as four bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 * Input Parameters:
 *  value - Unsigned 32-bit integer value to write into the buffer.
 *  buffer -  Pointer to the buffer where 4 bytes will be replaced with 
 *            the four bytes representing unsigned integer (LSB first).
 *
 * Returned Value:
 *  None
 *
 * Assumptions/Limitations:
 *  None
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
 *  Take the first four bytes from the buffer and encode them as a 32 bit
 *  integer.
 *
 *  Note that the data should be encoded as LSB first.
 *
 * Input Parameters:
 *  buffer -  Pointer to the buffer where the next four bytes should be
 *            extracted and a 32-bit unsigned integer created (LSB first).
 *
 * Returned Value:
 *  32-bit unsigned integer extracted from the first 4 bytes in the buffer.
 *
 * Assumptions/Limitations:
 *  None
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
 *  Encode a 32-bit integer as four bytes in the buffer.
 *
 *  Note that the data will be encoded LSB first.
 *
 * Input Parameters:
 *  value - Unsigned 32-bit integer value to write into the buffer.
 *  buffer - Pointer to the buffer where 4 bytes will be replaced with the
 *           four bytes representing unsigned integer (LSB first).
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
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
 *  Extract a string (terminated by 0) from a block of memory.
 *
 * Input Parameters:
 *  buffer - Pointer to the block of memory containing the string.
 *
 * Returned Value:
 *  Pointer to the extracted string.
 *
 * Assumptions/Limitations:
 *  None
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

    ptr = (uint8_t *) malloc(length + 1);
    strcpy((char *) ptr, (char *) buffer);
    return((char *) ptr);
}

/****************************************************************************
 * Name: espcp_encode_string
 *
 * Description:
 *  Copy the string into the buffer.
 *
 * Input Parameters:
 *  source - Block of memory containing the string.
 *  buffer - Pointer to a block of memory to take the string.

 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void espcp_encode_string(char *source, uint8_t *buffer)
{
    strcpy((char *) buffer, (char *) source);
}

/****************************************************************************
 * Name: espcp_crc8
 *
 * Description:
 *  Calculate the 8-bit CRC value for the specified data buffer.
 *
 *  This algorithm is loosely based upon the Dallas 1-Wire algorithm.
 *
 * Input Parameters:
 *  data - Pointer to the buffer containing the data for which the CRC should
 *         be calculated.
 *  len - Number of bytes in the data buffer.
 *
 * Returned Value:
 *  8-bit checksum of the bytes in the buffer.
 *
 * Assumptions/Limitations:
 *  None
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
 *  Calculate the 32-bit CRC value for the specified data buffer.
 *
 * Input Parameters:
 *  data - Pointer to the buffer containing the data for which the CRC should
 *         be calculated.
 *  len - Number of bytes in the data buffer.
 *
 * Returned Value:
 *  32-bit checksum of the bytes in the buffer.
 *
 * Assumptions/Limitations:
 *  None
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
 *  Calculate the CRC32 value change for the specified byte.
 *
 * Input Parameters:
 *  byte - Next byte to use in the checksum calculation.
 *  currentChecksum - Current value of the checksum.
 *
 * Returned Value:
 *  Next checksum value.
 *
 * Assumptions/Limitations:
 *  None
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
 *  Extract the message that is encoded in the byte buffer.
 *
 * Input Parameters:
 *  buffer - uint8_t array of bytes containing the encoded message.
 *  buffer_length - size of the buffer holding the data to be decoded.
 *  header_only - if true then the header will be extracted but not the
 *      payload.
 *
 * Returned Value:
 *  Pointer to the decoded message.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
espcp_message_t *espcp_extract_message(uint8_t *buffer, uint32_t bufferLength, bool headerOnly)
{
    uint32_t crc = espcp_extract_uint32(buffer + ESPCP_CRC_OFFSET);
    espcp_encode_uint32(0, buffer + ESPCP_CRC_OFFSET);
    
    espcp_message_t *message = NULL;
    if (crc == espcp_crc32(buffer, bufferLength))
    {
        message = (espcp_message_t *) malloc(sizeof(espcp_message_t));

        memset((void *) message, 0, sizeof(espcp_message_t));
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
        //
        //  Move over the last field and skip the CRC entry.
        //
        buffer += 8;
        if (!headerOnly && (message->payload_length > 0))
        {
            message->payload = (uint8_t *) malloc(message->payload_length);
            memcpy(message->payload, buffer, message->payload_length);
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
 *  Encode a message in a byte buffer.
 *
 * Input Parameters:
 *  message - Message to be encoded.
 *  buffer_length - Pointer to a uint32_t object that the contain the size of the
 *      buffer that contains the encoded message.  This is a return
 *      value from the method.
 *  header_only - if true then the header will be encoded but not the
 *      payload.
 *
 * Returned Value:
 *  Pointer to the block of memory containing the encoded message.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
uint8_t *espcp_encode_message(espcp_message_t *message, uint32_t *buffer_length, bool header_only)
{
    uint32_t message_size = ESPCP_HEADER_SIZE;
    if (!header_only)
    {
        message_size += message->payload_length;
    }
    uint32_t buffer_size = espcp_calculate_spi_buffer_size(message_size);
    uint8_t *buffer = (uint8_t *) malloc(buffer_size);
    if (buffer != NULL)
    {
        memset(buffer, 0, buffer_size);

        int offset = 0;
        *buffer = message->message_type;
        offset++;
        *(buffer + offset) = message->interface;
        offset++;
        espcp_encode_uint32(message->function, buffer + offset);
        offset += 4;
        espcp_encode_uint32(message->status_code, buffer + offset);
        offset += 4;
        espcp_encode_uint32(message->message_id, buffer + offset);
        offset += 4;
        espcp_encode_uint32(message->payload_length, buffer + offset);
        offset += 4;
        //
        //  The byte after the payload_length is the CRC.  This is filled in later
        //  and so a 0 is written into the message temporarily.
        //
        espcp_encode_uint32(0, buffer + offset);
        offset += 4;
        if (!header_only && (message->payload_length > 0))
        {
            memcpy((uint8_t *) (buffer + offset), message->payload, message->payload_length);
        }
        else
        {
            espcp_encode_uint32(0, buffer + offset);
        }
        uint32_t crc = espcp_crc32(buffer, buffer_size);
        espcp_encode_uint32(crc, buffer + ESPCP_CRC_OFFSET);
    }
    else
    {
        buffer_size = 0;
    }

    *buffer_length = buffer_size;
    return(buffer);
}


/*
 *******************************************************************************

       THE METHODS BELOW HAVE BEEN AUTOMATICALLY GENERATED BASED UPON THE
       Messages.json METADATA FILE.

 *******************************************************************************
*/

/****************************************************************************
* Name: espcp_encode_system_configuration
*
* Description:
*  Convert the espcp_system_configuration_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  system_configuration - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_system_configuration(espcp_system_configuration_t *system_configuration, uint8_t *buffer)
{
    espcp_encode_uint32(system_configuration->message_size, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_system_configuration_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_system_configuration_t_t object.
*
* Input Parameters:
*  espcp_system_configuration_t - espcp_espcp_system_configuration_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_system_configuration_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_system_configuration_buffer_size(espcp_system_configuration_t *system_configuration)
{
    return(4);
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

    system_configuration->message_size = espcp_extract_uint32(buffer);
    return(system_configuration);
}

/****************************************************************************
* Name: espcp_encode_wi_fi_configuration
*
* Description:
*  Convert the espcp_wi_fi_configuration_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  wi_fi_configuration - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_wi_fi_configuration(espcp_wi_fi_configuration_t *wi_fi_configuration, uint8_t *buffer)
{
    *buffer = wi_fi_configuration->automatic_reconnect;
    buffer += 1;
    espcp_encode_uint32(wi_fi_configuration->maximum_retry_count, buffer);
    buffer += 4;
    *buffer = wi_fi_configuration->antenna;
    buffer += 1;
    *buffer = wi_fi_configuration->maximum_message_queue_length;
}

/****************************************************************************
* Name: espcp_encoded_espcp_wi_fi_configuration_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_wi_fi_configuration_t_t object.
*
* Input Parameters:
*  espcp_wi_fi_configuration_t - espcp_espcp_wi_fi_configuration_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_wi_fi_configuration_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_wi_fi_configuration_buffer_size(espcp_wi_fi_configuration_t *wi_fi_configuration)
{
    return(7);
}

/****************************************************************************
* Name: espcp_extract_wi_fi_configuration
 *  
* Description:
*  Extract the espcp_wi_fi_configuration_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  wi_fi_configuration - pointer to the buffer containing the encoded
*  espcp_wi_fi_configuration_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_wi_fi_configuration_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_wi_fi_configuration_t *espcp_extract_wi_fi_configuration(uint8_t *buffer)
{
    espcp_wi_fi_configuration_t *wi_fi_configuration = (espcp_wi_fi_configuration_t *) malloc(sizeof(espcp_wi_fi_configuration_t));

    wi_fi_configuration->automatic_reconnect = *buffer;
    buffer += 1;
    wi_fi_configuration->maximum_retry_count = espcp_extract_uint32(buffer);
    buffer += 4;
    wi_fi_configuration->antenna = *buffer;
    buffer += 1;
    wi_fi_configuration->maximum_message_queue_length = *buffer;
    return(wi_fi_configuration);
}

/****************************************************************************
* Name: espcp_encode_wi_fi_credentials
*
* Description:
*  Convert the espcp_wi_fi_credentials_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  wi_fi_credentials - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_wi_fi_credentials(espcp_wi_fi_credentials_t *wi_fi_credentials, uint8_t *buffer)
{
    espcp_encode_string(wi_fi_credentials->network_name, buffer);
    buffer += strlen(wi_fi_credentials->network_name) + 1;
    espcp_encode_string(wi_fi_credentials->password, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_wi_fi_credentials_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_wi_fi_credentials_t_t object.
*
* Input Parameters:
*  espcp_wi_fi_credentials_t - espcp_espcp_wi_fi_credentials_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_wi_fi_credentials_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_wi_fi_credentials_buffer_size(espcp_wi_fi_credentials_t *wi_fi_credentials)
{
    int result = 0;
    result += strlen(wi_fi_credentials->network_name);
    result += strlen(wi_fi_credentials->password);
    return(result + 2);
}

/****************************************************************************
* Name: espcp_extract_wi_fi_credentials
 *  
* Description:
*  Extract the espcp_wi_fi_credentials_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  wi_fi_credentials - pointer to the buffer containing the encoded
*  espcp_wi_fi_credentials_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_wi_fi_credentials_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_wi_fi_credentials_t *espcp_extract_wi_fi_credentials(uint8_t *buffer)
{
    espcp_wi_fi_credentials_t *wi_fi_credentials = (espcp_wi_fi_credentials_t *) malloc(sizeof(espcp_wi_fi_credentials_t));

    wi_fi_credentials->network_name = espcp_extract_string(buffer);
    buffer += strlen(wi_fi_credentials->network_name) + 1;
    wi_fi_credentials->password = espcp_extract_string(buffer);
    return(wi_fi_credentials);
}

/****************************************************************************
* Name: espcp_encode_antenna_info
*
* Description:
*  Convert the espcp_antenna_info_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  antenna_info - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_antenna_info(espcp_antenna_info_t *antenna_info, uint8_t *buffer)
{
    *buffer = antenna_info->antenna;
}

/****************************************************************************
* Name: espcp_encoded_espcp_antenna_info_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_antenna_info_t_t object.
*
* Input Parameters:
*  espcp_antenna_info_t - espcp_espcp_antenna_info_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_antenna_info_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_antenna_info_buffer_size(espcp_antenna_info_t *antenna_info)
{
    return(1);
}

/****************************************************************************
* Name: espcp_extract_antenna_info
 *  
* Description:
*  Extract the espcp_antenna_info_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  antenna_info - pointer to the buffer containing the encoded
*  espcp_antenna_info_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_antenna_info_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_antenna_info_t *espcp_extract_antenna_info(uint8_t *buffer)
{
    espcp_antenna_info_t *antenna_info = (espcp_antenna_info_t *) malloc(sizeof(espcp_antenna_info_t));

    antenna_info->antenna = *buffer;
    return(antenna_info);
}

/****************************************************************************
* Name: espcp_encode_access_point
*
* Description:
*  Convert the espcp_access_point_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  access_point - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_access_point(espcp_access_point_t *access_point, uint8_t *buffer)
{
    memcpy((void *) buffer, (void *) access_point->ssid, 33);
    buffer += 33;
    memcpy((void *) buffer, (void *) access_point->bssid, 6);
    buffer += 6;
    *buffer = access_point->primary_channel;
    buffer += 1;
    *buffer = access_point->secondary_channel;
    buffer += 1;
    *buffer = (uint8_t) access_point->rssi;
    buffer += 1;
    *buffer = access_point->authentication_mode;
    buffer += 1;
    espcp_encode_uint32(access_point->protocols, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_access_point_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_access_point_t_t object.
*
* Input Parameters:
*  espcp_access_point_t - espcp_espcp_access_point_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_access_point_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_access_point_buffer_size(espcp_access_point_t *access_point)
{
    return(47);
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
* Name: espcp_encode_access_point_list
*
* Description:
*  Convert the espcp_access_point_list_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  access_point_list - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_access_point_list(espcp_access_point_list_t *access_point_list, uint8_t *buffer)
{
    espcp_encode_uint32(access_point_list->number_of_access_points, buffer);
    buffer += 4;
    espcp_encode_uint32(access_point_list->access_points_length, buffer);
    buffer += 4;
    if (access_point_list->access_points_length > 0)
    {
        memcpy((void *) buffer, (void *) access_point_list->access_points, access_point_list->access_points_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_access_point_list_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_access_point_list_t_t object.
*
* Input Parameters:
*  espcp_access_point_list_t - espcp_espcp_access_point_list_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_access_point_list_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_access_point_list_buffer_size(espcp_access_point_list_t *access_point_list)
{
    int result = 0;
    result += access_point_list->access_points_length;
    return(result + 8);
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
*  espcp_espcp_sock_addr_t_t object.
*
* Input Parameters:
*  espcp_sock_addr_t - espcp_espcp_sock_addr_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_sock_addr_t_t object.
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
* Name: espcp_encode_addr_info
*
* Description:
*  Convert the espcp_addr_info_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  addr_info - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_addr_info(espcp_addr_info_t *addr_info, uint8_t *buffer)
{
    espcp_encode_uint32(addr_info->my_heap_address, buffer);
    buffer += 4;
    espcp_encode_int32(addr_info->flags, buffer);
    buffer += 4;
    espcp_encode_int32(addr_info->family, buffer);
    buffer += 4;
    espcp_encode_int32(addr_info->socket_type, buffer);
    buffer += 4;
    espcp_encode_int32(addr_info->protocol, buffer);
    buffer += 4;
    espcp_encode_uint32(addr_info->addr_len, buffer);
    buffer += 4;
    espcp_encode_uint32(addr_info->addr_length, buffer);
    buffer += 4;
    if (addr_info->addr_length > 0)
    {
        memcpy((void *) buffer, (void *) addr_info->addr, addr_info->addr_length);
        buffer += addr_info->addr_length;
    }
    espcp_encode_string(addr_info->canon_name, buffer);
    buffer += strlen(addr_info->canon_name) + 1;
    espcp_encode_uint32((uint32_t) addr_info->next, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_addr_info_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_addr_info_t_t object.
*
* Input Parameters:
*  espcp_addr_info_t - espcp_espcp_addr_info_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_addr_info_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_addr_info_buffer_size(espcp_addr_info_t *addr_info)
{
    int result = 0;
    result += addr_info->addr_length;
    result += strlen(addr_info->canon_name);
    return(result + 33);
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
    buffer += strlen(addr_info->canon_name) + 1;
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
    buffer += strlen(get_addr_info_request->node_name) + 1;
    espcp_encode_string(get_addr_info_request->serv_name, buffer);
    buffer += strlen(get_addr_info_request->serv_name) + 1;
    espcp_encode_uint32(get_addr_info_request->hints_length, buffer);
    buffer += 4;
    if (get_addr_info_request->hints_length > 0)
    {
        memcpy((void *) buffer, (void *) get_addr_info_request->hints, get_addr_info_request->hints_length);
        buffer += get_addr_info_request->hints_length;
    }
    espcp_encode_uint32(get_addr_info_request->result_length, buffer);
    buffer += 4;
    if (get_addr_info_request->result_length > 0)
    {
        memcpy((void *) buffer, (void *) get_addr_info_request->result, get_addr_info_request->result_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_get_addr_info_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_get_addr_info_request_t_t object.
*
* Input Parameters:
*  espcp_get_addr_info_request_t - espcp_espcp_get_addr_info_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_get_addr_info_request_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_get_addr_info_request_buffer_size(espcp_get_addr_info_request_t *get_addr_info_request)
{
    int result = 0;
    result += strlen(get_addr_info_request->node_name);
    result += strlen(get_addr_info_request->serv_name);
    result += get_addr_info_request->hints_length;
    result += get_addr_info_request->result_length;
    return(result + 10);
}

/****************************************************************************
* Name: espcp_extract_get_addr_info_request
 *  
* Description:
*  Extract the espcp_get_addr_info_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  get_addr_info_request - pointer to the buffer containing the encoded
*  espcp_get_addr_info_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_get_addr_info_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_get_addr_info_request_t *espcp_extract_get_addr_info_request(uint8_t *buffer)
{
    espcp_get_addr_info_request_t *get_addr_info_request = (espcp_get_addr_info_request_t *) malloc(sizeof(espcp_get_addr_info_request_t));

    get_addr_info_request->node_name = espcp_extract_string(buffer);
    buffer += strlen(get_addr_info_request->node_name) + 1;
    get_addr_info_request->serv_name = espcp_extract_string(buffer);
    buffer += strlen(get_addr_info_request->serv_name) + 1;
    get_addr_info_request->hints_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (get_addr_info_request->hints_length > 0)
    {
        get_addr_info_request->hints = (uint8_t *) malloc(get_addr_info_request->hints_length);
        memcpy(get_addr_info_request->hints, buffer, get_addr_info_request->hints_length);
        buffer += get_addr_info_request->hints_length;
    }
    else
    {
        get_addr_info_request->hints = NULL;
    }
    get_addr_info_request->result_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (get_addr_info_request->result_length > 0)
    {
        get_addr_info_request->result = (uint8_t *) malloc(get_addr_info_request->result_length);
        memcpy(get_addr_info_request->result, buffer, get_addr_info_request->result_length);
        buffer += get_addr_info_request->result_length;
    }
    else
    {
        get_addr_info_request->result = NULL;
    }
    return(get_addr_info_request);
}

/****************************************************************************
* Name: espcp_encode_get_addr_info_response
*
* Description:
*  Convert the espcp_get_addr_info_response_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  get_addr_info_response - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_get_addr_info_response(espcp_get_addr_info_response_t *get_addr_info_response, uint8_t *buffer)
{
    espcp_encode_int32(get_addr_info_response->addr_info_response_errno, buffer);
    buffer += 4;
    espcp_encode_uint32(get_addr_info_response->res_length, buffer);
    buffer += 4;
    if (get_addr_info_response->res_length > 0)
    {
        memcpy((void *) buffer, (void *) get_addr_info_response->res, get_addr_info_response->res_length);
    }
}

/****************************************************************************
* Name: espcp_encoded_espcp_get_addr_info_response_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_get_addr_info_response_t_t object.
*
* Input Parameters:
*  espcp_get_addr_info_response_t - espcp_espcp_get_addr_info_response_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_get_addr_info_response_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_get_addr_info_response_buffer_size(espcp_get_addr_info_response_t *get_addr_info_response)
{
    int result = 0;
    result += get_addr_info_response->res_length;
    return(result + 8);
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
*  espcp_espcp_socket_request_t_t object.
*
* Input Parameters:
*  espcp_socket_request_t - espcp_espcp_socket_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_socket_request_t_t object.
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
* Name: espcp_extract_socket_request
 *  
* Description:
*  Extract the espcp_socket_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  socket_request - pointer to the buffer containing the encoded
*  espcp_socket_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_socket_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_socket_request_t *espcp_extract_socket_request(uint8_t *buffer)
{
    espcp_socket_request_t *socket_request = (espcp_socket_request_t *) malloc(sizeof(espcp_socket_request_t));

    socket_request->address_information = (void *) espcp_extract_uint32(buffer);
    buffer += 4;
    socket_request->domain = espcp_extract_int32(buffer);
    buffer += 4;
    socket_request->type = espcp_extract_int32(buffer);
    buffer += 4;
    socket_request->protocol = espcp_extract_int32(buffer);
    return(socket_request);
}

/****************************************************************************
* Name: espcp_encode_integer_response
*
* Description:
*  Convert the espcp_integer_response_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  integer_response - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_integer_response(espcp_integer_response_t *integer_response, uint8_t *buffer)
{
    espcp_encode_int32(integer_response->result, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_integer_response_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_integer_response_t_t object.
*
* Input Parameters:
*  espcp_integer_response_t - espcp_espcp_integer_response_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_integer_response_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_integer_response_buffer_size(espcp_integer_response_t *integer_response)
{
    return(4);
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
* Name: espcp_encode_integer_and_errno_response
*
* Description:
*  Convert the espcp_integer_and_errno_response_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  integer_and_errno_response - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_integer_and_errno_response(espcp_integer_and_errno_response_t *integer_and_errno_response, uint8_t *buffer)
{
    espcp_encode_int32(integer_and_errno_response->result, buffer);
    buffer += 4;
    espcp_encode_int32(integer_and_errno_response->response_errno, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_integer_and_errno_response_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_integer_and_errno_response_t_t object.
*
* Input Parameters:
*  espcp_integer_and_errno_response_t - espcp_espcp_integer_and_errno_response_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_integer_and_errno_response_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_integer_and_errno_response_buffer_size(espcp_integer_and_errno_response_t *integer_and_errno_response)
{
    return(8);
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
*  espcp_espcp_connect_request_t_t object.
*
* Input Parameters:
*  espcp_connect_request_t - espcp_espcp_connect_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_connect_request_t_t object.
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
* Name: espcp_extract_connect_request
 *  
* Description:
*  Extract the espcp_connect_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  connect_request - pointer to the buffer containing the encoded
*  espcp_connect_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_connect_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_connect_request_t *espcp_extract_connect_request(uint8_t *buffer)
{
    espcp_connect_request_t *connect_request = (espcp_connect_request_t *) malloc(sizeof(espcp_connect_request_t));

    connect_request->socket_handle = espcp_extract_int32(buffer);
    buffer += 4;
    connect_request->addr_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (connect_request->addr_length > 0)
    {
        connect_request->addr = (uint8_t *) malloc(connect_request->addr_length);
        memcpy(connect_request->addr, buffer, connect_request->addr_length);
        buffer += connect_request->addr_length;
    }
    else
    {
        connect_request->addr = NULL;
    }
    return(connect_request);
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
*  espcp_espcp_free_addr_info_request_t_t object.
*
* Input Parameters:
*  espcp_free_addr_info_request_t - espcp_espcp_free_addr_info_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_free_addr_info_request_t_t object.
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
* Name: espcp_extract_free_addr_info_request
 *  
* Description:
*  Extract the espcp_free_addr_info_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  free_addr_info_request - pointer to the buffer containing the encoded
*  espcp_free_addr_info_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_free_addr_info_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_free_addr_info_request_t *espcp_extract_free_addr_info_request(uint8_t *buffer)
{
    espcp_free_addr_info_request_t *free_addr_info_request = (espcp_free_addr_info_request_t *) malloc(sizeof(espcp_free_addr_info_request_t));

    free_addr_info_request->addr_info_address = espcp_extract_uint32(buffer);
    return(free_addr_info_request);
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
*  espcp_espcp_time_val_t_t object.
*
* Input Parameters:
*  espcp_time_val_t - espcp_espcp_time_val_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_time_val_t_t object.
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
*  espcp_espcp_set_sock_opt_request_t_t object.
*
* Input Parameters:
*  espcp_set_sock_opt_request_t - espcp_espcp_set_sock_opt_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_set_sock_opt_request_t_t object.
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
* Name: espcp_extract_set_sock_opt_request
 *  
* Description:
*  Extract the espcp_set_sock_opt_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  set_sock_opt_request - pointer to the buffer containing the encoded
*  espcp_set_sock_opt_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_set_sock_opt_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_set_sock_opt_request_t *espcp_extract_set_sock_opt_request(uint8_t *buffer)
{
    espcp_set_sock_opt_request_t *set_sock_opt_request = (espcp_set_sock_opt_request_t *) malloc(sizeof(espcp_set_sock_opt_request_t));

    set_sock_opt_request->socket_handle = espcp_extract_int32(buffer);
    buffer += 4;
    set_sock_opt_request->level = espcp_extract_int32(buffer);
    buffer += 4;
    set_sock_opt_request->option_name = espcp_extract_int32(buffer);
    buffer += 4;
    set_sock_opt_request->option_value_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (set_sock_opt_request->option_value_length > 0)
    {
        set_sock_opt_request->option_value = (uint8_t *) malloc(set_sock_opt_request->option_value_length);
        memcpy(set_sock_opt_request->option_value, buffer, set_sock_opt_request->option_value_length);
        buffer += set_sock_opt_request->option_value_length;
    }
    else
    {
        set_sock_opt_request->option_value = NULL;
    }
    set_sock_opt_request->option_len = espcp_extract_int32(buffer);
    return(set_sock_opt_request);
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
*  espcp_espcp_linger_t_t object.
*
* Input Parameters:
*  espcp_linger_t - espcp_espcp_linger_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_linger_t_t object.
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
*  espcp_espcp_write_request_t_t object.
*
* Input Parameters:
*  espcp_write_request_t - espcp_espcp_write_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_write_request_t_t object.
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
* Name: espcp_extract_write_request
 *  
* Description:
*  Extract the espcp_write_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  write_request - pointer to the buffer containing the encoded
*  espcp_write_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_write_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_write_request_t *espcp_extract_write_request(uint8_t *buffer)
{
    espcp_write_request_t *write_request = (espcp_write_request_t *) malloc(sizeof(espcp_write_request_t));

    write_request->socket_handle = espcp_extract_int32(buffer);
    buffer += 4;
    write_request->buffer_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (write_request->buffer_length > 0)
    {
        write_request->buffer = (uint8_t *) malloc(write_request->buffer_length);
        memcpy(write_request->buffer, buffer, write_request->buffer_length);
        buffer += write_request->buffer_length;
    }
    else
    {
        write_request->buffer = NULL;
    }
    write_request->count = espcp_extract_int32(buffer);
    return(write_request);
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
*  espcp_espcp_read_request_t_t object.
*
* Input Parameters:
*  espcp_read_request_t - espcp_espcp_read_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_read_request_t_t object.
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
* Name: espcp_extract_read_request
 *  
* Description:
*  Extract the espcp_read_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  read_request - pointer to the buffer containing the encoded
*  espcp_read_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_read_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_read_request_t *espcp_extract_read_request(uint8_t *buffer)
{
    espcp_read_request_t *read_request = (espcp_read_request_t *) malloc(sizeof(espcp_read_request_t));

    read_request->socket_handle = espcp_extract_int32(buffer);
    buffer += 4;
    read_request->count = espcp_extract_int32(buffer);
    return(read_request);
}

/****************************************************************************
* Name: espcp_encode_read_response
*
* Description:
*  Convert the espcp_read_response_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  read_response - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_read_response(espcp_read_response_t *read_response, uint8_t *buffer)
{
    espcp_encode_uint32(read_response->buffer_length, buffer);
    buffer += 4;
    if (read_response->buffer_length > 0)
    {
        memcpy((void *) buffer, (void *) read_response->buffer, read_response->buffer_length);
        buffer += read_response->buffer_length;
    }
    espcp_encode_int32(read_response->read_response_result, buffer);
    buffer += 4;
    espcp_encode_int32(read_response->read_response_errno, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_read_response_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_read_response_t_t object.
*
* Input Parameters:
*  espcp_read_response_t - espcp_espcp_read_response_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_read_response_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_read_response_buffer_size(espcp_read_response_t *read_response)
{
    int result = 0;
    result += read_response->buffer_length;
    return(result + 12);
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
*  espcp_espcp_close_request_t_t object.
*
* Input Parameters:
*  espcp_close_request_t - espcp_espcp_close_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_close_request_t_t object.
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
* Name: espcp_extract_close_request
 *  
* Description:
*  Extract the espcp_close_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  close_request - pointer to the buffer containing the encoded
*  espcp_close_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_close_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_close_request_t *espcp_extract_close_request(uint8_t *buffer)
{
    espcp_close_request_t *close_request = (espcp_close_request_t *) malloc(sizeof(espcp_close_request_t));

    close_request->socket_handle = espcp_extract_int32(buffer);
    return(close_request);
}

/****************************************************************************
* Name: espcp_encode_get_battery_charge_level_response
*
* Description:
*  Convert the espcp_get_battery_charge_level_response_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  get_battery_charge_level_response - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_get_battery_charge_level_response(espcp_get_battery_charge_level_response_t *get_battery_charge_level_response, uint8_t *buffer)
{
    espcp_encode_uint32(get_battery_charge_level_response->level, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_get_battery_charge_level_response_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_get_battery_charge_level_response_t_t object.
*
* Input Parameters:
*  espcp_get_battery_charge_level_response_t - espcp_espcp_get_battery_charge_level_response_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_get_battery_charge_level_response_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_get_battery_charge_level_response_buffer_size(espcp_get_battery_charge_level_response_t *get_battery_charge_level_response)
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
* Name: espcp_encode_send_request
*
* Description:
*  Convert the espcp_send_request_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  send_request - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_send_request(espcp_send_request_t *send_request, uint8_t *buffer)
{
    espcp_encode_int32(send_request->socket_handle, buffer);
    buffer += 4;
    espcp_encode_uint32(send_request->buffer_length, buffer);
    buffer += 4;
    if (send_request->buffer_length > 0)
    {
        memcpy((void *) buffer, (void *) send_request->buffer, send_request->buffer_length);
        buffer += send_request->buffer_length;
    }
    espcp_encode_int32(send_request->length, buffer);
    buffer += 4;
    espcp_encode_int32(send_request->flags, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_send_request_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_send_request_t_t object.
*
* Input Parameters:
*  espcp_send_request_t - espcp_espcp_send_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_send_request_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_send_request_buffer_size(espcp_send_request_t *send_request)
{
    int result = 0;
    result += send_request->buffer_length;
    return(result + 16);
}

/****************************************************************************
* Name: espcp_extract_send_request
 *  
* Description:
*  Extract the espcp_send_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  send_request - pointer to the buffer containing the encoded
*  espcp_send_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_send_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_send_request_t *espcp_extract_send_request(uint8_t *buffer)
{
    espcp_send_request_t *send_request = (espcp_send_request_t *) malloc(sizeof(espcp_send_request_t));

    send_request->socket_handle = espcp_extract_int32(buffer);
    buffer += 4;
    send_request->buffer_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (send_request->buffer_length > 0)
    {
        send_request->buffer = (uint8_t *) malloc(send_request->buffer_length);
        memcpy(send_request->buffer, buffer, send_request->buffer_length);
        buffer += send_request->buffer_length;
    }
    else
    {
        send_request->buffer = NULL;
    }
    send_request->length = espcp_extract_int32(buffer);
    buffer += 4;
    send_request->flags = espcp_extract_int32(buffer);
    return(send_request);
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
*  espcp_espcp_send_to_request_t_t object.
*
* Input Parameters:
*  espcp_send_to_request_t - espcp_espcp_send_to_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_send_to_request_t_t object.
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
* Name: espcp_extract_send_to_request
 *  
* Description:
*  Extract the espcp_send_to_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  send_to_request - pointer to the buffer containing the encoded
*  espcp_send_to_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_send_to_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_send_to_request_t *espcp_extract_send_to_request(uint8_t *buffer)
{
    espcp_send_to_request_t *send_to_request = (espcp_send_to_request_t *) malloc(sizeof(espcp_send_to_request_t));

    send_to_request->socket_handle = espcp_extract_int32(buffer);
    buffer += 4;
    send_to_request->buffer_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (send_to_request->buffer_length > 0)
    {
        send_to_request->buffer = (uint8_t *) malloc(send_to_request->buffer_length);
        memcpy(send_to_request->buffer, buffer, send_to_request->buffer_length);
        buffer += send_to_request->buffer_length;
    }
    else
    {
        send_to_request->buffer = NULL;
    }
    send_to_request->length = espcp_extract_int32(buffer);
    buffer += 4;
    send_to_request->flags = espcp_extract_int32(buffer);
    buffer += 4;
    send_to_request->destination_address_length = espcp_extract_uint32(buffer);
    buffer += 4;
    if (send_to_request->destination_address_length > 0)
    {
        send_to_request->destination_address = (uint8_t *) malloc(send_to_request->destination_address_length);
        memcpy(send_to_request->destination_address, buffer, send_to_request->destination_address_length);
        buffer += send_to_request->destination_address_length;
    }
    else
    {
        send_to_request->destination_address = NULL;
    }
    return(send_to_request);
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
*  espcp_espcp_recv_from_request_t_t object.
*
* Input Parameters:
*  espcp_recv_from_request_t - espcp_espcp_recv_from_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_recv_from_request_t_t object.
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
* Name: espcp_extract_recv_from_request
 *  
* Description:
*  Extract the espcp_recv_from_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  recv_from_request - pointer to the buffer containing the encoded
*  espcp_recv_from_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_recv_from_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_recv_from_request_t *espcp_extract_recv_from_request(uint8_t *buffer)
{
    espcp_recv_from_request_t *recv_from_request = (espcp_recv_from_request_t *) malloc(sizeof(espcp_recv_from_request_t));

    recv_from_request->socket_handle = espcp_extract_int32(buffer);
    buffer += 4;
    recv_from_request->length = espcp_extract_int32(buffer);
    buffer += 4;
    recv_from_request->flags = espcp_extract_int32(buffer);
    buffer += 4;
    recv_from_request->get_source_address = espcp_extract_int32(buffer);
    return(recv_from_request);
}

/****************************************************************************
* Name: espcp_encode_recv_from_response
*
* Description:
*  Convert the espcp_recv_from_response_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  recv_from_response - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_recv_from_response(espcp_recv_from_response_t *recv_from_response, uint8_t *buffer)
{
    espcp_encode_uint32(recv_from_response->buffer_length, buffer);
    buffer += 4;
    if (recv_from_response->buffer_length > 0)
    {
        memcpy((void *) buffer, (void *) recv_from_response->buffer, recv_from_response->buffer_length);
        buffer += recv_from_response->buffer_length;
    }
    espcp_encode_int32(recv_from_response->result, buffer);
    buffer += 4;
    espcp_encode_int32(recv_from_response->response_errno, buffer);
    buffer += 4;
    espcp_encode_uint32(recv_from_response->source_address_length, buffer);
    buffer += 4;
    if (recv_from_response->source_address_length > 0)
    {
        memcpy((void *) buffer, (void *) recv_from_response->source_address, recv_from_response->source_address_length);
        buffer += recv_from_response->source_address_length;
    }
    espcp_encode_uint32(recv_from_response->source_address_len, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_recv_from_response_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_recv_from_response_t_t object.
*
* Input Parameters:
*  espcp_recv_from_response_t - espcp_espcp_recv_from_response_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_recv_from_response_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_recv_from_response_buffer_size(espcp_recv_from_response_t *recv_from_response)
{
    int result = 0;
    result += recv_from_response->buffer_length;
    result += recv_from_response->source_address_length;
    return(result + 20);
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
*  espcp_espcp_poll_request_t_t object.
*
* Input Parameters:
*  espcp_poll_request_t - espcp_espcp_poll_request_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_poll_request_t_t object.
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
* Name: espcp_extract_poll_request
 *  
* Description:
*  Extract the espcp_poll_request_ object that is
*  encoded in the given buffer.
*  
*  Note that the returned pointer points to a block of memory on the heap and
*  this should eventually be released calling free(...).
*  
* Input Parameters:
*  poll_request - pointer to the buffer containing the encoded
*  espcp_poll_request_t object.
*
* Returned Value:
*  Pointer to the extracted espcp_poll_request_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
espcp_poll_request_t *espcp_extract_poll_request(uint8_t *buffer)
{
    espcp_poll_request_t *poll_request = (espcp_poll_request_t *) malloc(sizeof(espcp_poll_request_t));

    poll_request->socket_handle = espcp_extract_int32(buffer);
    buffer += 4;
    poll_request->events = espcp_extract_uint16(buffer);
    buffer += 2;
    poll_request->timeout = espcp_extract_int32(buffer);
    buffer += 4;
    poll_request->setup = espcp_extract_int32(buffer);
    buffer += 4;
    poll_request->setup_message_id = espcp_extract_uint32(buffer);
    return(poll_request);
}

/****************************************************************************
* Name: espcp_encode_poll_response
*
* Description:
*  Convert the espcp_poll_response_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  poll_response - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_poll_response(espcp_poll_response_t *poll_response, uint8_t *buffer)
{
    espcp_encode_uint16(poll_response->returned_events, buffer);
    buffer += 2;
    espcp_encode_int32(poll_response->result, buffer);
    buffer += 4;
    espcp_encode_int32(poll_response->response_errno, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_poll_response_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_poll_response_t_t object.
*
* Input Parameters:
*  espcp_poll_response_t - espcp_espcp_poll_response_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_poll_response_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_poll_response_buffer_size(espcp_poll_response_t *poll_response)
{
    return(10);
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
* Name: espcp_encode_interrupt_poll_response
*
* Description:
*  Convert the espcp_interrupt_poll_response_t object into a byte stream that can 
*  be sent to the ESP32.
*
* Input Parameters:
*  interrupt_poll_response - object to be encoded.
*
* Returned Value:
*  None
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
void espcp_encode_interrupt_poll_response(espcp_interrupt_poll_response_t *interrupt_poll_response, uint8_t *buffer)
{
    espcp_encode_int32(interrupt_poll_response->socket_handle, buffer);
    buffer += 4;
    espcp_encode_int32(interrupt_poll_response->result, buffer);
    buffer += 4;
    espcp_encode_int32(interrupt_poll_response->response_errno, buffer);
    buffer += 4;
    espcp_encode_uint16(interrupt_poll_response->returned_events, buffer);
    buffer += 2;
    espcp_encode_uint32(interrupt_poll_response->setup_message_id, buffer);
}

/****************************************************************************
* Name: espcp_encoded_espcp_interrupt_poll_response_t_buffer_size
*
* Description:
*  Calculate the amount of memory needed to store and encoded version of an
*  espcp_espcp_interrupt_poll_response_t_t object.
*
* Input Parameters:
*  espcp_interrupt_poll_response_t - espcp_espcp_interrupt_poll_response_t_t object to be encoded.
*
* Returned Value:
*  Number of bytes required to hold the encoded espcp_espcp_interrupt_poll_response_t_t object.
*
* Assumptions/Limitations:
*  None
*
****************************************************************************/
int espcp_interrupt_poll_response_buffer_size(espcp_interrupt_poll_response_t *interrupt_poll_response)
{
    return(18);
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


