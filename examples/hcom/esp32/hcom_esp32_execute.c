/****************************************************************************
 * \apps\examples\hcom\esp32\hcom_esp32_execute.c
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
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

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include "hcom_esp32_comms.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define HCOM_ESP32_LONGEST_FLASH_MSG_LENGTH (HCOM_ESP32_PROTOCOL_PRI_HDR_LENGTH + \
                                      HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH + \
                                      HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE)

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static uint32_t _espSeqNumb;
static size_t _totalSizeOfDownload;
static uint32_t _targetAddr;
static uint8_t *_downloadBuffer;
static uint32_t _numberOfPackets;
static char _espCalcMd5Hash[HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH + 1];

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_esp32_exec_buffer_to_esp32(uint8_t *downloadData, size_t dnldDataSize, bool isLastDownload);
static uint32_t hcom_esp32_exec_era_time_per_mega_byte(size_t xmit_size);

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int hcom_esp32_exec_setup_lazy()
{
  _shutting_down = false;

  // We'll assemble multiple hcom downloads into this buffer.
 _downloadBuffer = malloc(HCOM_ESP32_LONGEST_FLASH_MSG_LENGTH);
  DEBUGASSERT(_downloadBuffer != NULL);

  return OK;
}

//====================================================================
void hcom_esp32_exec_shutdown()
{
  _shutting_down = true;
  free(_downloadBuffer);
}

//====================================================================
// Returns approximately how much time to erase destination flash
uint32_t hcom_esp32_exec_era_time_per_mega_byte(size_t xmit_size)
{
  uint32_t timeout = HCOM_ESP32_ERASE_TIME_PER_MEGA_BYTE * (xmit_size / 1e6);
  return timeout > HCOM_ESP_XMIT_FLASH_DELAY_MS ? timeout : HCOM_ESP_XMIT_FLASH_DELAY_MS;
}

//===================================================================
// File Start is first and prepares the ESP32 for the flash download
// by getting the ESP32 into the proper condition to receive ROM loader
// commands.
int hcom_esp32_exec_download_flash_start(const size_t entireFileSize,
          const uint32_t targetAddr)
{
  int ret;
  struct HcomEsp32UserRecvdData_s recvdData;
  struct HcomEsp32SecHdrSpiAttach_s spiAttach;  
  struct HcomEsp32SecHdrBegin_s flashBegin;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen = 0;

  // Verify that mono has been disabled
  if(hcom_mono_ctrl_is_mono_enabled())
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "Mono must be disabled for ESP32 file download");
    hcom_logging_syslog(LOG_ERR, "%s@%d-\n", thisFile, __LINE__, hostMsg);
    
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
            hostMsg, thisFile, __LINE__);
    return -1;
  }

  // Verify file is not too large to fit in 4MB ESP32-PICO-D4 flash
  if(entireFileSize > HCOM_ESP32_PICO_D4_FLASH_SIZE)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "File is %d bytes long, ESP32-PICO-D4 max file size is %d bytes",
            entireFileSize , HCOM_ESP32_PICO_D4_FLASH_SIZE);
    hcom_logging_syslog(LOG_ERR, "%s@%d-File size too big '%s'\n", thisFile, __LINE__, hostMsg);

    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
              hostMsg, thisFile, __LINE__);
    return -1;
  }

  // Prepare for download
  _totalSizeOfDownload = entireFileSize;
  _targetAddr = targetAddr;
  _espSeqNumb = 0;

  // Establish communications with ESP32
  ret = hcom_esp32_util_init_comms_enter_boot_mode();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Init Comms for download:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // Need to send SPI_ATTACH which the ESP32 ROM loader supports
  // 6-bits each into a 32 bit number clock, q, d, hd, cs
  // See https://github.com/espressif/esptool/wiki/Advanced-Options#custom-spi-pin-configuration
  uint8_t spiConnClk = 6;    // Clock - SD_CLK
  uint8_t spiConnQ = 17;     // DO - GPIO17
  uint8_t spiConnD = 8;      // DI - SD_DATA_1
  uint8_t spiConnHD = 11;    // Hold - SD_CMD
  uint8_t spiConnCS = 16;    // Chip Select - GPIO 16

  spiAttach.spiPins = (spiConnHD << 24) | (spiConnCS << 18) | (spiConnD << 12) | (spiConnQ << 6) | spiConnClk;
  spiAttach.legacyFlag = 0;   // Not used

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Send SPI attach\n", thisFile, __LINE__);
  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&spiAttach,
        sizeof(struct HcomEsp32SecHdrSpiAttach_s),
        Esp32CommandSpiAttach, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-send SPI attach:%d\n", thisFile, __LINE__, ret);
    return -1;
  }

  // 3. Set SPI Parameters 
  struct HcomEsp32SecHdrSpiParms_s spiParms;
  spiParms.flId = HCOM_ESP32_PICO_D4_FLASH_ID;
  spiParms.sizeInBytes = HCOM_ESP32_PICO_D4_FLASH_SIZE;
  spiParms.blockSize = HCOM_ESP32_PICO_D4_FLASH_BLOCK_SIZE;
  spiParms.sectorSize = HCOM_ESP32_PICO_D4_FLASH_SECTOR_SIZE;
  spiParms.pageSize = HCOM_ESP32_PICO_D4_FLASH_PAGE_SIZE;
  spiParms.statusMask = HCOM_ESP32_PICO_D4_FLASH_STATUS_MASK;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Send SPI params\n", thisFile, __LINE__);
  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&spiParms, sizeof(struct HcomEsp32SecHdrSpiParms_s),
        Esp32CommandSpiSetParams, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-send SPI params:%d\n", thisFile, __LINE__, ret);
    return -1;
  }

  //--------------------------------------------------------
  // The Flash Begin command is the final command to prepare the ESP32 for data
  stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Initiating ESP32 download.");
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0,
          hostMsg, thisFile, __LINE__);

  _numberOfPackets = (entireFileSize + HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE - 1) / HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE;
  // Send the Flash Begin command
  // flashBegin.eraseSize = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE * _numberOfPackets;
  flashBegin.eraseSize = entireFileSize;
  flashBegin.numbBlocks = _numberOfPackets;
  flashBegin.downloadWriteSize = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE;
  flashBegin.downloadOffset = _targetAddr;   // Where data is flashed to

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-File:eraseSize:%d, numbBlocks:%u, WriteSize:%d, Offset:0x%08x\n",
          thisFile, __LINE__, flashBegin.eraseSize, flashBegin.numbBlocks,
          flashBegin.downloadWriteSize, flashBegin.downloadOffset);

  // This command also erases all needed flash, thus needing a bit more time
  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&flashBegin, HCOM_ESP32_PROTOCOL_BEGIN_HDR_LENGTH,
        Esp32CommandFlashBegin, hcom_esp32_exec_era_time_per_mega_byte(entireFileSize), &recvdData);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-FLASH_BEGIN %d\n", thisFile, __LINE__, ret);
    return -1;
  }
  return OK;
}

//====================================================================
// The CLI will send 1 - n data packets after the start message. These
// are processed here.
int hcom_esp32_exec_add_flash_data(const uint8_t *packet, const size_t packetSize, uint16_t hostSeqNumb)
{
  int ret;
  bool isLastPacket;
  static size_t totalDataBytesReceived;
  static uint8_t *tempSaveBuffer;
  static size_t tempSaveBufLen;
  static off_t downloadBuffOffset;

  // First download packet of this file?
  if(hostSeqNumb == 1)
  {
    totalDataBytesReceived = 0;
    tempSaveBufLen = 0;
    downloadBuffOffset = 0;
  }

  totalDataBytesReceived += packetSize;
  DEBUGASSERT(totalDataBytesReceived <=_totalSizeOfDownload);

  if(totalDataBytesReceived ==_totalSizeOfDownload)
    isLastPacket = true;
  else
    isLastPacket = false;
    
  if(downloadBuffOffset == 0)
  {
    // Start of new download
    // Need to reserve space for the 16 byte secondary header. It will be
    // populate just before transmission
    downloadBuffOffset += HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH;
  }

  if(tempSaveBufLen != 0)
  {
    // Data is saved from a previous download
    // Save data is only saved when block buffer is full. Since there is saved
    // data, we must have just reset the offset
    DEBUGASSERT(downloadBuffOffset == HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH);
    hcom_logging_syslog(LOG_DEBUG, "%s@%d-%d bytes in save\n", thisFile, __LINE__, tempSaveBufLen);

    // Copy saved data to block buffer and free the space
    memcpy(_downloadBuffer + downloadBuffOffset, tempSaveBuffer, tempSaveBufLen);
    downloadBuffOffset += tempSaveBufLen;
    free(tempSaveBuffer);
    tempSaveBufLen = 0;
  }

  // Offset is relative to the full buffer, including the secondary header
  size_t freeDataBufSpace = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH - downloadBuffOffset;
  if(freeDataBufSpace >= packetSize)
  {
    // It will all fit in the download buffer
    memcpy(_downloadBuffer + downloadBuffOffset, packet, packetSize);
    downloadBuffOffset += packetSize;
  }
  else
  {
    // Free space < packet size -> Won't all fit. Allocate a save buffer
    tempSaveBufLen = packetSize - freeDataBufSpace;
    tempSaveBuffer = malloc(tempSaveBufLen);
    DEBUGASSERT(tempSaveBuffer != NULL);

    // Some in download buffer
    memcpy(_downloadBuffer + downloadBuffOffset, packet, freeDataBufSpace);
    downloadBuffOffset += freeDataBufSpace;

    // The rest saved for next time
    memcpy(tempSaveBuffer, packet + tempSaveBufLen, tempSaveBufLen);

    hcom_logging_syslog(LOG_DEBUG, "%s@%d-Won't fit, recvd %d, send %d, download %d, saving %d\n",
            thisFile, __LINE__, packetSize, freeDataBufSpace, downloadBuffOffset, tempSaveBufLen);
  }
  
  // Is the download buffer now full?
  if(downloadBuffOffset == HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH)
  {
    // Send this full buffer and determine if this is the last packet
    hcom_logging_syslog(LOG_DEBUG, "%s@%d-dnld buf FULL (%d), must send\n",
            thisFile, __LINE__, downloadBuffOffset);

    ret = hcom_esp32_exec_buffer_to_esp32(_downloadBuffer,
            downloadBuffOffset, isLastPacket ? true : false);
    downloadBuffOffset = 0;
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-FLASH_DATA:%d\n", thisFile, __LINE__, ret);
      return ret;
    }

    if(isLastPacket)
    {
      // This IS the last packet (i.e. no more chances to download).
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Last Packet, %s\n", thisFile, __LINE__,
            tempSaveBufLen == 0 ? "save buf empty, exit" : "must send saved");

      if(tempSaveBufLen == 0)
        return OK;              // Nothing saved, we're done!

      // Copy any saved data to the now empty download buffer
      memcpy(_downloadBuffer + downloadBuffOffset, tempSaveBuffer, tempSaveBufLen);
      downloadBuffOffset += tempSaveBufLen;
      free(tempSaveBuffer);
      tempSaveBufLen = 0;    

      // Send this final buffer of data
      ret = hcom_esp32_exec_buffer_to_esp32(_downloadBuffer,
                downloadBuffOffset + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH, true);
      downloadBuffOffset = 0;
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Final FLASH_DATA:%d\n", thisFile, __LINE__, ret);
        return ret;
      }
    }
  }
  else
  {
    // Since the download buffer is not full, we must check if this is the
    // last packet. If it is we needed to send whatever we've got.
    if(isLastPacket)
    {
      DEBUGASSERT(tempSaveBufLen == 0); // How could there be saved if buffer not full?

      ret = hcom_esp32_exec_buffer_to_esp32(_downloadBuffer, downloadBuffOffset, true);
      downloadBuffOffset = 0;
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Last FLASH_DATA:%d\n", thisFile, __LINE__, ret);
        return ret;
      }
    }
  }
  
  return OK;
}

//====================================================================
// The data in the packets is actually downloaed here.
int hcom_esp32_exec_buffer_to_esp32(uint8_t *downloadData, size_t dnldDataSize, bool isLastDownload)
{
  int ret;
  off_t dataDnldOffset = dnldDataSize;
  size_t paddingLength = 0;  
  struct HcomEsp32SecHdrData_s flashData;
  struct HcomEsp32UserRecvdData_s recvdData;

  if(dataDnldOffset == 0)
    return OK;

  // Note: the first 16 bytes of this buffer have been reserved for
  // this HcomEsp32SecHdrData_s structures data
  flashData.dataSize = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE;
  flashData.sequence = _espSeqNumb++;    // starts at 0
  flashData.zero1 = 0;
  flashData.zero2 = 0;

  // Copy the secondary header at the head of the provided buffer
  memcpy(downloadData, &flashData, HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH);

  // If last packet may need padding per protocol requirements
  if(isLastDownload)
  {
    paddingLength = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH - dnldDataSize;
    if(paddingLength > 0)
    {
      // Assumes there's room in the buffer
      memset(downloadData + dataDnldOffset, 0xff, paddingLength);
      dataDnldOffset += paddingLength;
    }
  }

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-SENDING DATA PACKET, seq:%d\n",
            thisFile, __LINE__, _espSeqNumb - 1);
#if HCOM_OUTPUT_DATA_BUFFER_INFO_VIA_SYSLOG > 0
  hcom_diag_misc_print_buffer(_downloadBuffer, dataDnldOffset, LOG_DEBUG);
#endif

  ret = hcom_esp32_xmit_build_and_send_msg(_downloadBuffer, dataDnldOffset,
        Esp32CommandFlashData, HCOM_ESP_XMIT_FLASH_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-FLASH_DATA send:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  if(recvdData.esp32Status)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-ESP32 err:0x%02x, ESP32 err value:0x%02x\n", thisFile, __LINE__,
                recvdData.esp32Status, recvdData.esp32Error);
    return -1;
  }

  DEBUGASSERT(recvdData.espHdr.direction == 1);
  DEBUGASSERT(recvdData.espHdr.command == Esp32CommandFlashData);

  if(isLastDownload)
  {
    // Ask the ESP32 to calculate the MD5 hash and retun it.
    struct HcomEsp32SecHdrFlashMD5_s flashMd5;

    flashMd5.address = _targetAddr;
    flashMd5.size = _totalSizeOfDownload;
    flashMd5.zero1 = 0;
    flashMd5.zero2 = 0;

    ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&flashMd5, HCOM_ESP32_PROTOCOL_FLASH_MD5_HDR_LENGTH,
          Esp32CommandSpiFlashMd5, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-FLASH_BEGIN:%d\n", thisFile, __LINE__, ret);
      return ret;
    }

    recvdData.recvdData[HCOM_PROTOCOL_REQUEST_MD5_HASH_LENGTH] = '\0';

    // Save for reading
    strcpy(_espCalcMd5Hash, (char *)recvdData.recvdData);
  }
  return OK;
}

//====================================================================
char *hcom_esp32_exec_get_md5_file_hash()
{
  return _espCalcMd5Hash;
}

//====================================================================
// The user data contains 1 if this is the last file, meaning it's time
// to leave boot mode
int hcom_esp32_exec_add_flash_end()
{
  int ret;
  struct HcomEsp32UserRecvdData_s recvdData;
  struct HcomEsp32SecHdrBegin_s flashBegin;
  struct HcomEsp32SecHdrFlashEnd_s flashEnd;

  // I don't know why but the esptool sends a flash begin at this point, the
  // purpose is unknown. But it must do something good? So, it's here.
  flashBegin.eraseSize = 0;
  flashBegin.numbBlocks = 0;
  flashBegin.downloadWriteSize = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE;
  flashBegin.downloadOffset = 0;

  // Note: Returns number of bytes in ESP32 response or -Error
  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&flashBegin, HCOM_ESP32_PROTOCOL_BEGIN_HDR_LENGTH,
        Esp32CommandFlashBegin, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-FLASH_BEGIN:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // ----------------------------------------------
  // Flash End is the last command for flashing files to the ESP32
  // after all downloading is completed.
  flashEnd.execFlag = 1;   // 1 = don't reboot, 0 = reboot (software reset)
  flashEnd.zero1 = 0;
  flashEnd.zero2 = 0;
  flashEnd.zero3 = 0;

  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&flashEnd, HCOM_ESP32_PROTOCOL_FLASH_END_HDR_LENGTH,
        Esp32CommandFlashEnd, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-esp32 xmit 2:%d\n", thisFile, __LINE__, ret);
  }
  else if(recvdData.esp32Status)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-FLASH_END:%d\n", thisFile, __LINE__, recvdData.esp32Error);
  }

  DEBUGASSERT(recvdData.espHdr.direction == 1);
  DEBUGASSERT(recvdData.espHdr.command == Esp32CommandFlashEnd);

  ret = hcom_esp32_util_hardware_restart();
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-restart failed:%d\n", thisFile, __LINE__, ret);
  }

  return ret;
}

