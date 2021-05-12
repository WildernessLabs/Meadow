/****************************************************************************
 * apps\examples\hcom\diag\hcom_diag_decode_protocol.c
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <meadow/hcom_shared_common.h>

#if HCOM_DIAG_INCLUDE_DIAG_DECODE_MESSAGE_CODE > 0
#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/
/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static char * hcom_diag_decode_recvd_find_minor_str(uint8_t minorRqstType);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

//=======================================================================================
// Takes a hcom message and outputs a string contining the header information
void hcom_diag_decode_recvd_message_type(const uint8_t *packet, const size_t packetSize)
{  
#if HCOM_INCLUDE_DIAG_PRINT_BUFFER_CODE > 0
  hcom_diag_misc_print_buffer(packet, packetSize, 2);
#endif
  struct HcomProtocolHeader_s *msgHeader = (struct HcomProtocolHeader_s *) packet;

  char *MajorRqstType[] = 
  {
    "UNDEFINED",              // 0x0000
    "SIMPLE",                 // 0x0100
    "FILE_START",             // 0x0200
    "SIMPLE_TEXT",            // 0x0300
    "SIMPLE_BINARY"           // 0x0400
  };

  char *MinorFileStartRqstType[] = 
  {
    "Undefined",
    "START_FILE_TRANSFER",     // 0x01
    "DELETE_FILE_BY_NAME",     // 0x02
    "START_ESP_FILE_TRANSFER", // 0x03
  };

  char *MinorBinaryRqstType[] = 
  {
    "Undefined",
    "DEBUGGER_MSG",            // 0x01
  };
  
  uint8_t majorRqstType = (msgHeader->rqstType & HCOM_PROTOCOL_HEADER_MAJOR_TYPE_MASK) >> 8;
  uint8_t minorRqstType = msgHeader->rqstType & HCOM_PROTOCOL_HEADER_MINOR_TYPE_MASK;
  
  // Look up the correct strings
  char *strMajorRqstType = NULL;
  char *strMinorRqstType;

  if(majorRqstType <= sizeof(MajorRqstType) - 1)
    strMajorRqstType = MajorRqstType[majorRqstType];    // Overflow text handled by switch

  switch(majorRqstType)
  {
    case 0:   // HCOM_PROTOCOL_HEADER_TYPE_UNDEFINED
      strMajorRqstType = "Zero is not defined";
      strMinorRqstType = "Undefined";    
      break;
    
    case 1:   // HCOM_PROTOCOL_HEADER_TYPE_SIMPLE
      strMinorRqstType = hcom_diag_decode_recvd_find_minor_str(minorRqstType);    
      break;
    
    case 2:   // HCOM_PROTOCOL_HEADER_TYPE_FILE_START
      if(minorRqstType > sizeof(MinorFileStartRqstType) - 1)
      {
        syslog(2, "Minor rqst type is too large for MinorFileStartRqstType which has %d elements\n",
                sizeof(MinorFileStartRqstType));
        strMinorRqstType = "Out of range";
      }
      else
        strMinorRqstType = MinorFileStartRqstType[minorRqstType];
      break;

    case 3:   // HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_TEXT
      // Currently, simple text is only used for message from HCOM to CLI
      syslog(2, "=== Undefined major request type ===\n");
      strMajorRqstType = "unexpected";   // This is for message to CLI not from
      strMinorRqstType = "unsupported";    
      break;

    case 4:   // HCOM_PROTOCOL_HEADER_TYPE_SIMPLE_BINARY (CLI -> HCOM)
      if(minorRqstType > sizeof(MinorBinaryRqstType) - 1)
      {
        syslog(2, "Minor rqst type is too large for MinorBinaryRqstType which has %d elements\n",
                  sizeof(MinorBinaryRqstType));
        strMinorRqstType = "Out of range";
      }
      else
        strMinorRqstType = MinorBinaryRqstType[minorRqstType];
      break;

    default:
      syslog(2, "=== Illegal Major type ===\n");
      strMinorRqstType = "unknown";
      strMajorRqstType = "Out of Range";
  }

  // Build final strings for the user
  syslog(2, ">>> Message-SeqNumb:%d, Version:0x%04x, RqstType:0x%04x, userData:0x%08x (%d) <<<\n",
            msgHeader->seqNumber, msgHeader->version,
            msgHeader->rqstType, msgHeader->userData, msgHeader->userData);
  syslog(2, ">>> Request Type %s : %s <<<\n", strMajorRqstType, strMinorRqstType);
  usleep(10 * 1000); // Give time for syslog to output
}

char * hcom_diag_decode_recvd_find_minor_str(uint8_t minorRqstType)
{
  char *MinorSimpleStdRqstType[] = 
  {
    "UNDEFINED_REQUEST",       // 0x00
    "CREATE_ENTIRE_FLASH_FS",  // 0x01
    "CHANGE_TRACE_LEVEL",      // 0x02
    "FORMAT_FLASH_FILE_SYS",   // 0x03
    "END_FILE_TRANSFER",       // 0x04
    "RESTART_PRIMARY_MCU",     // 0x05
    "VERIFY_ERASED_FLASH",     // 0x06
    "PARTITION_FLASH_FS",      // 0x07
    "MOUNT_FLASH_FS",          // 0x08
    "INITIALIZE_FLASH_FS",     // 0x09
    "BULK_FLASH_ERASE",        // 0x0a
    "ENTER_DFU_MODE",          // 0x0b
    "ENABLE_DISABLE_NSH",      // 0x0c
    "LIST_PARTITION_FILES",    // 0x0d
    "LIST_PART_FILES_AND_CRC", // 0x0e
    "MONO_DISABLE",            // 0x0f
    "MONO_ENABLE",             // 0x10
    "MONO_RUN_STATE",          // 0x11
    "GET_DEVICE_INFORMATION",  // 0x12
    "PART_RENEW_FILE_SYS",     // 0x13
    "NO_TRACE_TO_HOST",        // 0x14
    "SEND_TRACE_TO_HOST",      // 0x15
    "END_ESP_FILE_TRANSFER",   // 0x16
    "READ_ESP_MAC_ADDRESS",    // 0x17
    "RESTART_ESP32",           // 0x18
    "MONO_FLASH",              // 0x19
    "SEND_TRACE_TO_UART",      // 0x1a
    "NO_TRACE_TO_UART",        // 0x1b
    "MONO_UPDATE_RUNTIME",     // 0x1c
    "MONO_UPDATE_FILE_END",    // 0x1d
    "MONO_START_DBG_SESSION",  // 0x1e
    "GET_DEVICE_NAME",         // 0x1f
  };

  // Minor simple type but 0xf0-0x0ff
  char *MinorSimpleDevRqstType[] = 
  {
    "DEVELOPER_1",             // 0xf0
    "DEVELOPER_2",             // 0xf1
    "DEVELOPER_3",             // 0xf2
    "DEVELOPER_4",             // 0xf3
    "FLASH_QSPI_INIT",         // 0xf4
    "FLASH_QSPI_WRITE",        // 0xf5
    "FLASH_QSPI_READ",         // 0xf6
  };

    if(minorRqstType < 0xf0)
    {
      if(minorRqstType > (sizeof(MinorSimpleStdRqstType) - 1))
      {
        syslog(2, "Minor rqst type is too large for MinorSimpleStdRqstType which has %d elements\n",
                  sizeof(MinorSimpleStdRqstType));
        return "Out of range";
      }
      return MinorSimpleStdRqstType[minorRqstType];
    }
    else
    {
      if(minorRqstType > (sizeof(MinorSimpleDevRqstType) - 1))
      {
        syslog(2, "Minor rqst type is too large for MinorSimpleDevRqstType which has %d elements\n",
                  sizeof(MinorSimpleDevRqstType));
        return "Out of range";
      }
      return MinorSimpleDevRqstType[minorRqstType - 0xf0];
    }
}

#else

void hcom_diag_decode_recvd_message_type(const uint8_t *packet, const size_t packetSize)
{

}
#endif
