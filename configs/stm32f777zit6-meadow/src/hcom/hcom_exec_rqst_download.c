/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom_exed_download.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2017 Alan Carvalho de Assis. All rights reserved.
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

#include "hcom_common.h"

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

#define HCOM_RECV_DEBUG_TIMING 1          // Enables the display of time spent

/****************************************************************************
 * Private Data
 ****************************************************************************/
static int _currentHcomDataPacketAction;
static bool _fileSystemOpenFailed;

static uint32_t _xferRecvFullFileCrc;
static uint32_t _xferRecvFullFileSize;
static uint32_t _xferCalcFullFileCrc = 0;  // This is over all the payload (original data)
static uint32_t _xferCalcFullFileSize = 0; // This is the size of the original
static uint32_t _xferCalcPacketCrc = 0;    // This is over all packets
static int _dbgNumbPacketsRecvd = 0;

#if HCOM_RECV_DEBUG_TIMING
uint64_t _dbgReceptionBeganAt;
uint64_t _dbgReceptionEndedAt;

static uint64_t get_current_time64(void)
{
  struct timespec ts;
#ifdef CONFIG_CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(CLOCK_REALTIME, &ts);
#endif
  return (uint64_t)ts.tv_sec * NSEC_PER_SEC + (uint64_t)ts.tv_nsec;
}
#endif

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_exec_rqst_download_file_rqst_setup()
{
  _fileSystemOpenFailed = false;
  _currentHcomDataPacketAction = CurrentHcomDataPacketActionNone;
  return OK;
}

//====================================================================
bool hcom_exec_rqst_download_is_dowload_active()
{
  return (_currentHcomDataPacketAction != CurrentHcomDataPacketActionNone);
}

//=======================================================================================
void hcom_exec_rqst_download_file_rqst_start(const uint8_t *recvPacketData, const size_t recvPacketDataSize,
                                                uint32_t partitionId)
{
  off_t msgOffset = 0;
  char *sendStartMsg;

  _xferCalcFullFileCrc = 0; // Setup for checksum calculation of orig file
  _fileSystemOpenFailed = false;

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionBeganAt = get_current_time64();
#endif

  // File size
  _xferRecvFullFileSize = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                          (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // Checksum
  _xferRecvFullFileCrc = recvPacketData[msgOffset] + (recvPacketData[msgOffset + 1] << 8) +
                         (recvPacketData[msgOffset + 2] << 16) + (recvPacketData[msgOffset + 3] << 24);
  msgOffset += sizeof(uint32_t);

  // FileName
  size_t fileNameLength = recvPacketDataSize - msgOffset;
  char *fileNameBuffer = malloc(fileNameLength + 1);
  fileNameBuffer[fileNameLength] = '\0';

  memcpy(fileNameBuffer, recvPacketData + msgOffset, fileNameLength);
  // msgOffset += fileNameLength;

  _currentHcomDataPacketAction = CurrentHcomDataPacketActionExtFileXfer;

  f7syslog(LOG_NOTICE, "--------- Header for file transfer -------------\n");
  f7syslog(LOG_INFO, "PartitionId=%d, FullFileSize=%d, FullFileCrc=0x%08x FileName = %s\n",
           partitionId, _xferRecvFullFileSize, _xferRecvFullFileCrc, fileNameBuffer);
  hcom_diag_print_buffer(recvPacketData, recvPacketDataSize, LOG_DEBUG);

  int ret = hcom_file_commands_open_active_file(partitionId, HCOM_FILE_MOUNT_POINT_TARGET, fileNameBuffer);
  if (ret != OK)
  {
    _fileSystemOpenFailed = true;
    f7syslog(LOG_ERR, "%s() Error returned from call to hcom_file_commands_open_active_file: %d\n", __func__, ret);
  }
  free(fileNameBuffer);

  // Send text message to host
  if (_fileSystemOpenFailed)
    sendStartMsg = "Failed to open target file\0";
  else
    sendStartMsg = "File transfer header received with no errors\0";
  hcom_host_msg_builder_send_text(sendStartMsg, strlen((char *)sendStartMsg));
}

//=======================================================================================
// Process a end of file transfer message
void hcom_exec_rqst_download_file_rqst_end(uint32_t userData)
{
  char hostMsg[HCOM_TEMP_MAX_HOST_STRING_LEN];
  char *sendMsgToHost;

  f7syslog(LOG_NOTICE, "--------- End of File Transfer Trailer -------------\n");

  int ret = hcom_file_commands_close_active_file();
  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s() ERROR: File close failed %d\n", __func__, ret);
  }

  // Compare results and report to host
  if (_fileSystemOpenFailed)
  {
    sendMsgToHost = "File Send Failed, file system could not be opened.\0";
  }
  else if (_xferCalcFullFileCrc == _xferRecvFullFileCrc && _xferCalcFullFileSize == _xferRecvFullFileSize)
  {
    snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN,
        "File Sent Successfully (checksums calculated = 0x%08X, received = 0x%08X)\0",
        _xferCalcFullFileCrc, _xferRecvFullFileCrc);
    sendMsgToHost = hostMsg;
  }
  else
  {
    if (_xferCalcFullFileCrc != _xferRecvFullFileCrc)
    {
      snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "Checksum matching error Calc = 0x%08X, Recv = 0x%08X\0",
               _xferCalcFullFileCrc, _xferRecvFullFileCrc);
      sendMsgToHost = hostMsg;
    }
    else
    {
      DEBUGASSERT(_xferCalcFullFileSize != _xferRecvFullFileSize);
      snprintf(hostMsg, HCOM_TEMP_MAX_HOST_STRING_LEN, "File size mismatch error Calc = %d, Recv = %d\0",
               _xferCalcFullFileSize, _xferRecvFullFileSize);
      sendMsgToHost = hostMsg;
    }
  }
  // Send text message to host
  hcom_host_msg_builder_send_text(sendMsgToHost, strlen((char *)sendMsgToHost));

#if HCOM_RECV_DEBUG_TIMING
  _dbgReceptionEndedAt = get_current_time64();
  f7syslog(LOG_DEBUG, "File transfer %d packets, took %llu mSec, CalcPacketCRC:0x%08x CalcFileCRC:0x%08x\n",
           _dbgNumbPacketsRecvd, ((_dbgReceptionEndedAt - _dbgReceptionBeganAt) / 1000000),
           _xferCalcPacketCrc, _xferCalcFullFileCrc);
#else
  f7syslog(LOG_DEBUG, "Host has sent %d packets\n", _dbgNumbPacketsRecvd);
#endif

  _xferCalcPacketCrc = 0;
  _xferCalcFullFileSize = 0;
  _xferCalcFullFileCrc = 0; // Set to 0 for next message

  _currentHcomDataPacketAction = CurrentHcomDataPacketActionNone;
}

//============================================================================
// Process data packet based on currently active state
void hcom_exec_rqst_download_data_packet(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb)
{
  // TODO - insure that packets are numbered sequentially

  int ret;
  int msgOffset = sizeof(uint16_t); // size of sequence number

  if (_fileSystemOpenFailed)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Data Packet received but ignored - previous requested file open failed (seq %d)\n",
             __func__, seqNumb);
    return;
  }

  _dbgNumbPacketsRecvd++;

  // Calculate the running checksum which includes the sequence number
  _xferCalcPacketCrc = crc32part(packet, packetSize, _xferCalcPacketCrc);

  const uint8_t *recvOrigData = packet + msgOffset;
  const size_t recvOrigDataSize = packetSize - msgOffset;

  // Calculate CRC checksum of the payload without sequence number
  _xferCalcFullFileCrc = crc32part(recvOrigData, recvOrigDataSize, _xferCalcFullFileCrc);
  _xferCalcFullFileSize += recvOrigDataSize;

  // Depending on what we're doing process this data packet
  switch (_currentHcomDataPacketAction)
  {
    case CurrentHcomDataPacketActionExtFileXfer:
      ret = hcom_file_commands_write_to_active_file(recvOrigData, recvOrigDataSize);
      break;

    default:
      ret = -1;
      f7syslog(LOG_ERR, "%s() ERROR: Data Packet (SeqNumb=%d), but Data Packet Action unknown\n",
              __func__, seqNumb);
      break;
  }

  if (ret != OK)
  {
    f7syslog(LOG_ERR, "%s() ERROR: Data Packet received but write failed [%d] for sequence %d\n",
             __func__, ret, seqNumb);
  }
}
