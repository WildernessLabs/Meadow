/****************************************************************************
 * \apps\examples\hcom\file\hcom_file_dnld_proc.c
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

// The functions in the file setup to write a file, write the file and
// end the download process while verifying file integrity.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_shared_common.h>

#include <nuttx/arch.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_file_upld_proc_setup()
{
  return OK;
}

//==========================================================================
// This function receives a command from CLI and builds a single message to
// send back to the CLI. This message contains the first of a file. However, the
// maximum number of bytes is fixed at HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN.
void hcom_file_upld_proc_initial_bytes_in_file(const uint8_t *recvPayloadData,
          const size_t recvPayloadSize, uint32_t partitionId)
{
  int ret;
  int fd;
  char *fileNameBuffer;
  size_t fileNameLength = recvPayloadSize;

#ifndef CONFIG_MTD_PARTITION
  partitionId = 0;    // Ignore any other partition value if no partitioning
#endif

  // Only thing in payload is the file name
  fileNameBuffer = malloc(fileNameLength + 1);
  memset(fileNameBuffer, 0, fileNameLength + 1);
  memcpy(fileNameBuffer, recvPayloadData, fileNameLength);

  // Create the name of the mount point part of the file name
  char *fullMountPtName = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);

#ifdef CONFIG_MTD_PARTITION
  snprintf_chk(fullMountPtName, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s%d",
            HCOM_FILE_MOUNT_POINT_TARGET, partitionId);
#else
  DEBUGASSERT(strlen(HCOM_FILE_MOUNT_POINT_TARGET) < HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  strncpy(fullMountPtName, HCOM_FILE_MOUNT_POINT_TARGET, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
#endif

  char *completeFilePath = malloc(HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH);
  snprintf_chk(completeFilePath, HCOM_MAX_PATH_AND_FILE_BUFF_LENGTH, "%s/%s", 
                fullMountPtName, fileNameBuffer);
  free(fullMountPtName);

  // Open the file
  fd = open(completeFilePath, O_RDONLY);
  if (fd == -1)
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "The file '%s' cannot be opened by Meadow", completeFilePath);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);

    hcom_logging_syslog(LOG_ERR, "%s@%d-open '%s', errno: %d\n",
                thisFile, __LINE__, completeFilePath, errno);
    free(completeFilePath);
    return;
  }

  // Seek to beginning
  off_t offset = lseek(fd, 0, SEEK_SET);
  if (offset == (off_t)-1)
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "The file '%s' encountered a lseek error", completeFilePath);
    hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0, hostMsg,
            thisFile, __LINE__);

    hcom_logging_syslog(LOG_ERR, "%s@%d-lseek failed %s, errno:%d\n",
              thisFile, __LINE__, completeFilePath, errno);
    free(completeFilePath);
    return;
  }

  // Read all the data
  uint8_t *returnBinData = malloc(HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN);
  ssize_t nbytes;
  int bufOff = 0;
  do
  {
    nbytes = read(fd, returnBinData + bufOff, HCOM_PROTOCOL_REQUEST_MAX_PAYLOAD_LEN - bufOff);
    if (nbytes < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-read %s, errno:%d\n",
                thisFile, __LINE__, completeFilePath, errno); usleep(20 * 1000);
      free(completeFilePath);
      free(returnBinData);
      return;
    }
    bufOff += nbytes;
  } while (nbytes > 0);

  ret = close(fd);
  if (ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-close %s, errno:%d\n",
             thisFile, __LINE__, completeFilePath, errno);
    free(completeFilePath);
    free(returnBinData);
    return;
  }

  // Send the data to CLI
  hcom_host_send_binary_data_msg(HCOM_HOST_REQUEST_SEND_INITIAL_FILE_BYTES,
            0, returnBinData, bufOff, __FILE__, __LINE__);

  free(returnBinData);
}