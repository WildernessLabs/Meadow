/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_esp32_execute.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
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

// The Host PC will send to hcom the following 3 messages
// 1. Start of download
// 2. One to N data packets
// 3. End of download

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"

#include "hcom_esp32_comms.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

//#define HCOM_ESP32_USING_STUB_LOADER

#ifdef HCOM_ESP32_USING_STUB_LOADER
#define HCOM_ESP32_LONGEST_FLASH_MSG_LENGTH (HCOM_ESP32_PROTOCOL_PRI_HDR_LENGTH + \
                                      HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH + \
                                      HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE)
#else
#define HCOM_ESP32_LONGEST_FLASH_MSG_LENGTH (HCOM_ESP32_PROTOCOL_PRI_HDR_LENGTH + \
                                      HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH + \
                                      HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE)
#endif
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
#ifdef HCOM_ESP32_USING_STUB_LOADER
static int hcom_esp32_exec_download_stub_loader(void);
#endif
static int send_data_block_buffer_to_esp32(uint8_t *downloadData, size_t dnldDataSize, bool isLastDownload);
static uint32_t erase_time_per_mega_byte(size_t xmit_size);

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
// Calculates approximately how much time to erase destination flash
uint32_t erase_time_per_mega_byte(size_t xmit_size)
{
  uint32_t timeout = HCOM_ESP32_ERASE_TIME_PER_MEGA_BYTE * (xmit_size / 1e6);
  return MAX(timeout, HCOM_ESP_XMIT_FLASH_DELAY_MS);
}

#ifdef HCOM_ESP32_USING_STUB_LOADER
//====================================================================
// The stub loader is stored in a file ready to be sent to the ESP32.
// Returns length of stub loader or if > 0 an error
int hcom_esp32_exec_get_stub_loader_data(uint8_t **stubLoader)
{
  int ret;
  int fd;
  ssize_t stubLoaderLen;

  // p-m magic esp32 don't hard code
  char *stubLoaderFileName = "/meadow/Esp32StubLoader2_8.bin";
  f7syslog(LOG_DEBUG, "%s@%d-opening '%s'\n",
              thisFile, __LINE__, stubLoaderFileName);

  // Existing file - open read only
  set_errno(0);
  fd = open(stubLoaderFileName, O_RDONLY);
  if (fd == -1)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:failed to open '%s' for reading. errno: %d\n",
               thisFile, __LINE__, stubLoaderFileName, errno);
    return -errno;
  }

  // Seek to beginning
  off_t fileSize = lseek(fd, 0, SEEK_END);
  if (fileSize == (off_t)-1)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:lseek failed: %s errno %d\n", thisFile, __LINE__, stubLoaderFileName, errno);
    return -errno;
  }

  off_t offset = lseek(fd, 0, SEEK_SET);
  if (offset == (off_t)-1)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:lseek failed: %s errno %d\n", thisFile, __LINE__, stubLoaderFileName, errno);
    return -errno;
  }

  *stubLoader = malloc(fileSize);  
  DEBUGASSERT(*stubLoader != NULL);

  // Read the data
  stubLoaderLen = read(fd, *stubLoader, fileSize);
  if (stubLoaderLen < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:read:%s, errno:%d\n",
              thisFile, __LINE__, stubLoaderFileName, errno);
    free(*stubLoader);
    return -errno;
  }
  
  DEBUGASSERT(fileSize == stubLoaderLen);

  ret = close(fd);
  if (ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:close:%s, errno:%d\n",
             thisFile, __LINE__, stubLoaderFileName, errno);
    free(*stubLoader);
    return -errno;
  }

  return stubLoaderLen;
}

//====================================================================
// To download and start the ESP32 stub loader we must:
// 1. MEM_BEGIN - stub loader coming
// 2. MEM_DATA  - write stub loader to ESP32
// 3. MEM_BEGIN - 
// 4. MEM_DATA - 
// 5. MEM_END - start stub loader running
// 6. Return ESP sends "OHAI" meaning stub loader running
// Many of the following values were copied off the output of esptool.py ver 2.8
// fia a tool that recorded the serial stream, while it wsa running under a debugger
int hcom_esp32_exec_download_stub_loader(void)
{
  int ret;
  uint8_t *stubLoader = NULL;
  struct HcomEsp32UserRecvdData_s recvdData;
  struct HcomEsp32SecHdrBegin_s memBegin;
  struct HcomEsp32SecHdrData_s memData;
  struct HcomEsp32SecHdrMemEnd_s memEnd;

  // #1 MEM_BEGIN
  // p-m big magic esp32 What can be calculated? [esptool.py @623]
  memBegin.eraseSize = 3268;
  memBegin.numbBlocks = 1;
  memBegin.downloadWriteSize = 6144;
  memBegin.downloadOffset = 0x4009f000;

  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&memBegin, HCOM_ESP32_PROTOCOL_BEGIN_HDR_LENGTH,
        Esp32CommandMemBegin, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:MEM_BEGIN ret:%d errno:\n", thisFile, __LINE__, ret, errno);
    return ret;
  }

  // #2 MEM_DATA
  // Because the stub loader binary was taken from the wire it includes
  // the secondary mem data header as well as the data to write
  ssize_t stubLoaderLen = hcom_esp32_exec_get_stub_loader_data(&stubLoader);
  if(stubLoaderLen < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Read stub ret:%d errno:\n", thisFile, __LINE__, ret, errno);
    return stubLoaderLen;
  }

  if(stubLoader == NULL)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:StubLoader NULL\n", thisFile, __LINE__);
    return -1;
  }

  // Send stub loader software to ESP
  ret = hcom_esp32_xmit_send_complete_msg(stubLoader, stubLoaderLen,
        Esp32CommandMemData, 1000, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Send stub ret:%d errno:\n", thisFile, __LINE__, ret, errno);
    free(stubLoader);
    return ret;
  }

  free(stubLoader);

  // #3 MEM_BEGIN
  // p-m magic
  memBegin.eraseSize = 4;
  memBegin.numbBlocks = 1;
  memBegin.downloadWriteSize = 6144;
  memBegin.downloadOffset = 0x3fffeba4;

  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&memBegin, HCOM_ESP32_PROTOCOL_BEGIN_HDR_LENGTH,
        Esp32CommandMemBegin, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:MEM_BEGIN ret:%d errno:\n", thisFile, __LINE__, ret, errno);
    return ret;
  }

  // #4 MEM_DATA
  // p-m magic 0x3ffec008
  uint8_t stubLoaderMemData2[] = {0x08, 0xc0, 0xfe, 0x3f};

  memData.dataSize = sizeof(stubLoaderMemData2);
  memData.sequence = 0;
  memData.zero1 = 0;
  memData.zero2 = 0;

  uint8_t *stubLoaderMemData2Msg = malloc(HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH + sizeof(stubLoaderMemData2));
  DEBUGASSERT(stubLoaderMemData2Msg != NULL);
  memcpy(stubLoaderMemData2Msg, &memData, HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH);
  memcpy(stubLoaderMemData2Msg + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH, stubLoaderMemData2, sizeof(stubLoaderMemData2));
 
  ret = hcom_esp32_xmit_build_and_send_msg(stubLoaderMemData2Msg, 
        HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH + sizeof(stubLoaderMemData2),
        Esp32CommandMemData, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:MEM_DATA ret:%d errno:\n", thisFile, __LINE__,
              ret, errno);
    free(stubLoaderMemData2Msg);
    return ret;
  }
  free(stubLoaderMemData2Msg);

  // #5 MEM_END
  // p-m magic
  memEnd.execFlag = 0;      // 0 = Jump to entry point
  memEnd.entryPt = 0x4009f568;

  int memEndAlloc = HCOM_ESP32_PROTOCOL_MEM_END_HDR_LENGTH;
  uint8_t *stubLoaderMemEndMsg = malloc(memEndAlloc);
  memcpy(stubLoaderMemEndMsg, &memEnd, HCOM_ESP32_PROTOCOL_MEM_END_HDR_LENGTH);

  ret = hcom_esp32_xmit_build_and_send_msg(stubLoaderMemEndMsg, memEndAlloc,
        Esp32CommandMemEnd, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);

  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:MEM_END ret:%d errno:\n", thisFile, __LINE__, ret, errno);
    free(stubLoaderMemEndMsg);
    return ret;
  }
  free(stubLoaderMemEndMsg);

  // #6 Wait for stub loader to send "OHAI" which indicates it's running
  ret = hcom_esp32_recv_is_stub_loader_running(3000);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Stub run errno:%d\n", thisFile, __LINE__, errno);
    return ret;
  }
  
  return OK;
}
#endif

//===================================================================
// File Start is first and must prepare the ESP32 for the flash download
int hcom_esp32_exec_download_flash_start(const size_t entireFileSize,
          const uint32_t targetAddr, const char *md5Hash)
{
  int ret;
  struct HcomEsp32UserRecvdData_s recvdData;
  struct HcomEsp32SecHdrSpiAttach_s spiAttach;  
  struct HcomEsp32SecHdrBegin_s flashBegin;
  char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
  int stringLen;

  // Verify file is not too large to fit in 4MB ESP32-PICO-D4 flash
  if(entireFileSize > HCOM_ESP32_PICO_D4_FLASH_SIZE)
  {
    stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH,
            "File is %d bytes, ESP32-PICO-D4 max %d",
            entireFileSize , HCOM_ESP32_PICO_D4_FLASH_SIZE);
    f7syslog(LOG_ERR, "Error:File size too big '%s'\n", hostMsg);
    goto errorExitHostMsg;
  }

  // Prepare for download
  _totalSizeOfDownload = entireFileSize;
  _targetAddr = targetAddr;
  _espSeqNumb = 0;

  // 1. Establish communications with ESP32
  ret = hcom_esp32_util_initialize_communications();
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Init Comms:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

#ifdef HCOM_ESP32_USING_STUB_LOADER
  // 2. Download and start running stub loader
  ret = hcom_esp32_exec_download_stub_loader();
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:Stub err:%d\n", thisFile, __LINE__, ret);
    return ret;
  }
#else
  // Without the stub loader we need to send SPI_ATTACH which the ESP32 ROM loader supports
  // 6-bits each into a 32 bit number clock, q, d, hd, cs
  // See https://github.com/espressif/esptool/wiki/Advanced-Options#custom-spi-pin-configuration

  uint8_t spiConnClk = 6;    // Clock - SD_CLK
  uint8_t spiConnQ = 17;     // DO - GPIO17
  uint8_t spiConnD = 8;      // DI - SD_DATA_1
  uint8_t spiConnHD = 11;    // Hold - SD_CMD
  uint8_t spiConnCS = 16;    // Chip Select - GPIO 16

  spiAttach.spiPins = (spiConnHD << 24) | (spiConnCS << 18) | (spiConnD << 12) | (spiConnQ << 6) | spiConnClk;
  spiAttach.legacyFlag = 0;   // Not used

  f7syslog(LOG_DEBUG, "%s@%d-Send SPI attach\n", thisFile, __LINE__);
  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&spiAttach,
        sizeof(struct HcomEsp32SecHdrSpiAttach_s),
        Esp32CommandSpiAttach, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:send SPI attach:%d\n", thisFile, __LINE__, ret);
    goto errorExit;
  }
#endif

  // 3. Set SPI Parameters 
  struct HcomEsp32SecHdrSpiParms_s spiParms;
  spiParms.flId = HCOM_ESP32_PICO_D4_FLASH_ID;
  spiParms.sizeInBytes = HCOM_ESP32_PICO_D4_FLASH_SIZE;
  spiParms.blockSize = HCOM_ESP32_PICO_D4_FLASH_BLOCK_SIZE;
  spiParms.sectorSize = HCOM_ESP32_PICO_D4_FLASH_SECTOR_SIZE;
  spiParms.pageSize = HCOM_ESP32_PICO_D4_FLASH_PAGE_SIZE;
  spiParms.statusMask = HCOM_ESP32_PICO_D4_FLASH_STATUS_MASK;

  f7syslog(LOG_DEBUG, "%s@%d-Send SPI params\n", thisFile, __LINE__);
  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&spiParms, sizeof(struct HcomEsp32SecHdrSpiParms_s),
        Esp32CommandSpiSetParams, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:send SPI params:%d\n", thisFile, __LINE__, ret);
    goto errorExit;
  }

  //--------------------------------------------------------
  // The Flash Begin command is the final command to prepare the ESP32 for data
  stringLen = snprintf(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, "Initiating ESP32 download.");
  DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
  ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_INFORMATION, 0, hostMsg);
  if (ret < 0)
    f7syslog(LOG_ERR, "%s@%d-Host xmit err:%d\n", thisFile, __LINE__, ret);

#ifdef HCOM_ESP32_USING_STUB_LOADER
  uint32_t _numberOfPackets = (entireFileSize + HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE - 1) / HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE;

  f7syslog(0, "%s@%d-File: length:%d, numb blocks:%u, WriteSize:%d\n", thisFile, __LINE__,
    entireFileSize, _numberOfPackets, HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE);

  // Send the Flash Begin command
  flashBegin.eraseSize = entireFileSize;
  flashBegin.numbBlocks = _numberOfPackets;
  flashBegin.downloadWriteSize = HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE;
  flashBegin.downloadOffset = _targetAddr;

  // This does not erase the flash so each call is faster
  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&flashBegin, HCOM_ESP32_PROTOCOL_BEGIN_HDR_LENGTH,
        Esp32CommandFlashBegin, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:FLASH_BEGIN %d\n", thisFile, __LINE__, ret);
    goto errorExit;
  }

#else
  _numberOfPackets = (entireFileSize + HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE - 1) / HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE;
  // Send the Flash Begin command
  // flashBegin.eraseSize = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE * _numberOfPackets;
  flashBegin.eraseSize = entireFileSize;
  flashBegin.numbBlocks = _numberOfPackets;
  flashBegin.downloadWriteSize = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE;
  flashBegin.downloadOffset = _targetAddr;   // Where data is flashed to

  f7syslog(LOG_DEBUG, "%s@%d-File:eraseSize:%d, numbBlocks:%u, WriteSize:%d, Offset:0x%08x\n", thisFile, __LINE__,
    flashBegin.eraseSize, flashBegin.numbBlocks, flashBegin.downloadWriteSize, flashBegin.downloadOffset);

  // This will erase all needed flash, thus needing a bit more time
  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&flashBegin, HCOM_ESP32_PROTOCOL_BEGIN_HDR_LENGTH,
        Esp32CommandFlashBegin, erase_time_per_mega_byte(entireFileSize), &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:FLASH_BEGIN %d\n", thisFile, __LINE__, ret);
    goto errorExit;
  }
#endif
  return OK;

errorExitHostMsg:
    DEBUGASSERT(stringLen < HCOM_SHORT_HOST_STRING_BUFF_LENGTH);
    ret = hcom_comms_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg);
    if (ret < 0)
      f7syslog(LOG_ERR, "%s@%d-Host xmit err:%d\n", thisFile, __LINE__, ret);

errorExit:
  return -1;
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
    f7syslog(LOG_DEBUG, "%s@%d-%d bytes in save\n", thisFile, __LINE__, tempSaveBufLen);

    // Copy saved data to block buffer and free the space
    memcpy(_downloadBuffer + downloadBuffOffset, tempSaveBuffer, tempSaveBufLen);
    downloadBuffOffset += tempSaveBufLen;
    free(tempSaveBuffer);
    tempSaveBufLen = 0;
  }

  // Offset is relative to the full buffer, including the secondary header
#ifdef HCOM_ESP32_USING_STUB_LOADER
  size_t freeDataBufSpace = HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH - downloadBuffOffset;
#else
  size_t freeDataBufSpace = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH - downloadBuffOffset;
#endif
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

    f7syslog(LOG_DEBUG, "%s@%d-Won't fit, recvd %d, send %d, download %d, saving %d\n",
            thisFile, __LINE__, packetSize, freeDataBufSpace, downloadBuffOffset, tempSaveBufLen);
  }
  
  // Is the download buffer now full?
#ifdef HCOM_ESP32_USING_STUB_LOADER
  if(downloadBuffOffset == HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH)
#else
  if(downloadBuffOffset == HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH)
#endif
  {
    // Send this full buffer and determine if this is the last packet
    f7syslog(LOG_DEBUG, "%s@%d-dnld buf FULL (%d), must send\n",
            thisFile, __LINE__, downloadBuffOffset);

    ret = send_data_block_buffer_to_esp32(_downloadBuffer,
            downloadBuffOffset, isLastPacket ? true : false);
    downloadBuffOffset = 0;
    if(ret < 0)
    {
      f7syslog(LOG_ERR, "%s@%d-Error:FLASH_DATA:%d\n", thisFile, __LINE__, ret);
      return ret;
    }

    if(isLastPacket)
    {
      // This IS the last packet (i.e. no more chances to download).
      f7syslog(LOG_DEBUG, "%s@%d-Last Packet, %s\n", thisFile, __LINE__,
            tempSaveBufLen == 0 ? "save buf empty, bye" : "must send saved");

      if(tempSaveBufLen == 0)
        return OK;              // Nothing saved, we're done!

      // Copy any saved data to the now empty download buffer
      memcpy(_downloadBuffer + downloadBuffOffset, tempSaveBuffer, tempSaveBufLen);
      downloadBuffOffset += tempSaveBufLen;
      free(tempSaveBuffer);
      tempSaveBufLen = 0;    

      // Send this final buffer of data
      ret = send_data_block_buffer_to_esp32(_downloadBuffer,
                downloadBuffOffset + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH, true);
      downloadBuffOffset = 0;
      if(ret < 0)
      {
        f7syslog(LOG_ERR, "%s@%d-Error:Final FLASH_DATA:%d\n", thisFile, __LINE__, ret);
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

      ret = send_data_block_buffer_to_esp32(_downloadBuffer, downloadBuffOffset, true);
      downloadBuffOffset = 0;
      if(ret < 0)
      {
        f7syslog(LOG_ERR, "%s@%d-Error:Last FLASH_DATA:%d\n", thisFile, __LINE__, ret);
        return ret;
      }
    }
  }
  
  return OK;
}

//====================================================================
// The data in the packets is actually downloaed here.
int send_data_block_buffer_to_esp32(uint8_t *downloadData, size_t dnldDataSize, bool isLastDownload)
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
#ifdef HCOM_ESP32_USING_STUB_LOADER
  flashData.dataSize = HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE;
#else
  flashData.dataSize = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE;
#endif
  flashData.sequence = _espSeqNumb++;    // starts at 0
  flashData.zero1 = 0;
  flashData.zero2 = 0;

  // Copy the secondary header at the head of the provided buffer
  memcpy(downloadData, &flashData, HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH);

  // If last packet may need padding per protocol requirements
  if(isLastDownload)
  {
#ifdef HCOM_ESP32_USING_STUB_LOADER
    paddingLength = HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH - dnldDataSize;
#else
    paddingLength = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH - dnldDataSize;
#endif
    if(paddingLength > 0)
    {
      // Assumes there's room in the buffer
      memset(downloadData + dataDnldOffset, 0xff, paddingLength);
      dataDnldOffset += paddingLength;
    }
  }

  // Verify that the buffer was large enough
#ifdef HCOM_ESP32_USING_STUB_LOADER
  DEBUGASSERT(dataDnldOffset <= HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH);
  // For the Stub Loader, this typically takes almost 2 seconds because the ESP32
  // erases flash on an as-needed basis, just before writing
#else
  DEBUGASSERT(dataDnldOffset <= HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE + HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH);
  // For the Boot Loader the flash has already been deleted
#endif

syslog(0, "SENDING DATA PACKET, seq:%d\n", _espSeqNumb - 1);
//hcom_utils_diag_print_buffer(_downloadBuffer, dataDnldOffset, 0);

  ret = hcom_esp32_xmit_build_and_send_msg(_downloadBuffer, dataDnldOffset,
        Esp32CommandFlashData, HCOM_ESP_XMIT_FLASH_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:FLASH_DATA send:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  if(recvdData.esp32Status)
  {
    f7syslog(LOG_ERR, "%s@%d-ESP32 err:0x%02x, ESP32 err value:0x%02x\n", thisFile, __LINE__,
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
      f7syslog(LOG_ERR, "%s@%d-Error:FLASH_BEGIN:%d\n", thisFile, __LINE__, ret);
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
int hcom_esp32_exec_add_flash_end(uint32_t lastFile)
{
  int ret;
  struct HcomEsp32UserRecvdData_s recvdData;
  struct HcomEsp32SecHdrBegin_s flashBegin;
  struct HcomEsp32SecHdrFlashEnd_s flashEnd;

  if(lastFile == 0)
    return OK;

  // ----------------------------------------------
  // ESPTOOL at this point request the ESP32 to calculate and return
  // the MD5 checksum using the SPI_FLASH_MD5 command
  // p-m need to implement this needed validation step!
  //
  // NuttShell documentation states that the MD5 hash can be found via
  // NuttShell if CONFIG_NETUTILS_CODECS && CONFIG_CODECS_HASH_MD5 are
  // defined. Look in nuttx user for implementation.
  // Also, the MD5 RFC has a 'C' implementation at the end of this page
  // https://tools.ietf.org/html/rfc1321
  // Also found at https://gist.github.com/creationix/4710780
  //

  // ----------------------------------------------
  // p-m esp32 Try to remove this and see if it still works
  // esptool sends a flash begin at this point,
  // the purpose is unknown. Mostly full of zeros.

  flashBegin.eraseSize = 0;
  flashBegin.numbBlocks = 0;
#ifdef HCOM_ESP32_USING_STUB_LOADER
  flashBegin.downloadWriteSize = HCOM_ESP32_STUB_LOADER_PAYLOAD_SIZE;
#else
  flashBegin.downloadWriteSize = HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE;
#endif
  flashBegin.downloadOffset = 0;

  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&flashBegin, HCOM_ESP32_PROTOCOL_BEGIN_HDR_LENGTH,
        Esp32CommandFlashBegin, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:FLASH_BEGIN:%d\n", thisFile, __LINE__, ret);
    return ret;
  }

  // ----------------------------------------------
  // Flash End is the last command for flashing a file to the ESP32.
  // This will cause the ESP32 to leave the boot
  flashEnd.execFlag = 0;    // 0 = reboot (software reset), 1 = don't reboot, stay in bootloader
  flashEnd.zero1 = 0;
  flashEnd.zero2 = 0;
  flashEnd.zero3 = 0;

  ret = hcom_esp32_xmit_build_and_send_msg((uint8_t *)&flashEnd, HCOM_ESP32_PROTOCOL_FLASH_END_HDR_LENGTH,
        Esp32CommandFlashEnd, HCOM_ESP_XMIT_TYPICAL_DELAY_MS, &recvdData);
  if(ret < 0)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:esp32 xmit 2:%d\n", thisFile, __LINE__, ret);
  }
  else if(recvdData.esp32Status)
  {
    f7syslog(LOG_ERR, "%s@%d-Error:FLASH_END:%d \n", thisFile, __LINE__, recvdData.esp32Error);
  }

  DEBUGASSERT(recvdData.espHdr.direction == 1);
  DEBUGASSERT(recvdData.espHdr.command == Esp32CommandFlashEnd);
  return ret;
}

