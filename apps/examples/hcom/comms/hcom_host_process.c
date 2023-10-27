/****************************************************************************
 * \apps\examples\hcom\comms\hcom_host_process.c
 * 
 *   Copyright (C) 2019 - 2023 Wilderness Labs. All rights reserved.
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

// This file does processing of all request. For file downloads it allocates
// and populates a struct that contains file specific information.

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include "../hcom_common.h"
#include <meadow/hcom_dnld_shared.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
// The following deal with subdirectory support
#define MEADOW_FILE_SUBDIR_PREPEND_MEADOW_STR   ("/meadow0/")
#define MEADOW_FILE_SUBDIR_PREPEND_MEADOW_LEN   (9)
#define MEADOW_FILE_SUBDIR_PREPEND_SDCARD_STR   ("/mmcsd0/")
#define MEADOW_FILE_SUBDIR_PREPEND_SDCARD_LEN   (8)


/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool _shutting_down;
static hcom_dnld_shared_t *_dnldShared;
static uint8_t *_packet_dest_buf = NULL;
static uint8_t *_decode_dest_buf = NULL;

enum hcom_file_subdir_parsed
{
  fnameInvalid = 100,
  fnameOriginal = 101,        // No '/' found
  fnameMeadowFull = 102,  // Starts '/meadow0/'
  fnameMmcsdFull = 103    // Starts '/mmcsd0/'
};


/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_host_process_route_packet(const uint8_t *packet, const size_t packetSize);
static int hcom_host_process_run(void);
static int hcom_host_process_init_dnld_share(uint32_t partitionId);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Note: this thread was created by Nuttx and was used to do the Meadow
// initialization within apps. Once all initialization is completed, it is used
// here to processing the HCOM messages. Therefore, it never returns.
int hcom_host_process_setup()
{
  int ret;
  _shutting_down = false;

  _packet_dest_buf = (uint8_t *)malloc(HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE);
  if (_packet_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Packet buffer allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }
  
  _decode_dest_buf = (uint8_t *)malloc(HCOM_PROTOCOL_SAFE_ENCODED_MSG_BUF_SIZE);
  if (_decode_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Decoded allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  // Allocate for the download shared structure
  _dnldShared = zalloc(sizeof(hcom_dnld_shared_t));
  if (_decode_dest_buf == NULL)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Download shared allocation failed\n",
              thisFile, __LINE__);
    return -ENOMEM;
  }

  // The watchdog needs access to the download shared structure
  ret = hcom_host_watchdog_initialize(_dnldShared);
  if(ret < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-Watchdog init failed:%d\n",
              thisFile, __LINE__, ret);
    return -ENOEXEC;    // May be better error code....
  }

  // Set initial state to none
  _dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;

  // This threads priority and initial stack size are set via menuconfig
  // Priority: CONFIG_USERMAIN_PRIORITY with a default of 100. This priority
  // is adjusted in the following code.
  // Stacksize: CONFIG_USERMAIN_STACKSIZE with a default stack size of 2048.
  // The stack size was set to 65536, I've not changed this value (Peter).
  // RTOS Features->Stack and heap information->Main thread stack size
  //
  // See /nuttx/sched/init/nx_bringup.c for user main's creation
  struct sched_param sparam;
  sparam.sched_priority = HCOM_THREAD_PRIORITY_HCOM_PROCESS;
  sched_setparam(0, &sparam);

  // This thread needs to respond to SIGALRM
  sigset_t set;
  (void)sigemptyset(&set);
  (void)sigaddset(&set, SIGALRM);
  ret = sigprocmask(SIG_UNBLOCK, &set, NULL);
  if (ret != OK)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d-sigprocmask() failed:%d\n",
              thisFile, __LINE__, ret);
    return -EPERM;
  }

  // This thread now runs host processing of received messages. As such this
  // call will never return.
  ret = hcom_host_process_run();
 
  // This return is only reached on shutddown
  return ret;
}

//====================================================================
//
void hcom_host_process_shutdown()
{
  _shutting_down = true;

  free(_decode_dest_buf);
  free(_packet_dest_buf);
  free(_dnldShared);
}

//==========================================================================
// Are we involved in some external flash download activity? There are the
// following states: HcomStm32F7DnldStateNone, HcomStm32F7DnldStateStarting
// and HcomStm32F7DnldStateFileXfer
bool hcom_host_process_is_stm32f7_dnld_active()
{
  return((_dnldShared->dnldCurrentState == HcomStm32F7DnldStateStarting) ||
         (_dnldShared->dnldCurrentState == HcomStm32F7DnldStateFileXfer));
}

//=================================================================
// This thread processes all the messages the receive thread has written to
// the circular buffer.
int hcom_host_process_run()
{
  int ret;
  size_t packetLength;
  
  while (!_shutting_down)
  {
    // Dequeue the next packet received
    ret = hcom_host_enq_deq_dequeue_packet(_packet_dest_buf, &packetLength);
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Failed to dequeue message\n", thisFile, __LINE__);
      continue;
    }

    // We have a good packet. Drop trailing delimiter using --packetLength,
    // then decode the packet and route it for processing.
    size_t decodedPacketSize = hcom_host_cobs_decoder(_packet_dest_buf, --packetLength, _decode_dest_buf);
    if(decodedPacketSize == 0)
      continue;

    ret = hcom_host_process_route_packet(_decode_dest_buf, decodedPacketSize);
    if (ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-processing data:%d\n",
                thisFile, __LINE__, ret);
    }
  }

  // Thread is exiting
  return OK;
}

//============================================================================
// This function will return the potential depth of subdirectories in the file
// name provided.
static size_t find_subdir_depth(const char *fileName, size_t strLen)
{
  // Allocate a modifiable version of the string for tokenizing
  char *fileNameTemp = malloc(strLen + 1);
  strcpy(fileNameTemp, fileName);

  // Count the number of '/' characters to give an indication of the subdir
  // depth
  int tokenCount = 0;
  char *savePtr;
  char *token = strtok_r(fileNameTemp, "/", &savePtr);

  while (token != NULL)
  {
    tokenCount++;
    token = strtok_r(NULL, "/", &savePtr);
  }

  free(fileNameTemp);
  return tokenCount - 2;
}

//============================================================================
// This function will check the received filename and categorize it so the
// remaining steps will know what they are dealing with
static int hcom_file_subdir_categorize_filename(const char *fileName,
          size_t strLen, size_t *subdirDepth)
{
  *subdirDepth = 0;

  // Only alphanumeric, '.' or '/' are allowed
  // 'filename', '/meadow0/.../..', '/mmcsd0/../..'
  // These are illegal formats:
  // '/filename', /dirname/filename/

  // Is this a bare filename (i.e. no '/')
  if(memchr(fileName, '/', strLen) == NULL)
  {
    // No '/' in file name, this is like original naming scheme
    // 101
    return fnameOriginal;
  }
  else if (strLen < MEADOW_FILE_SUBDIR_PREPEND_SDCARD_LEN)
  {
    // 100
    return fnameInvalid;          // Too short 
  }
  else if(memcmp(MEADOW_FILE_SUBDIR_PREPEND_MEADOW_STR,
              fileName, MEADOW_FILE_SUBDIR_PREPEND_MEADOW_LEN) == 0)
  {
    // '/meadow0/' found, but can't end in '/'
    if(fileName[strLen-1] == '/')
        return fnameInvalid;
    
    *subdirDepth = find_subdir_depth(fileName, strLen);
    
    // 102
    return fnameMeadowFull;
  }
  // (--) Only do this if SD-Card is enabled
  else if(memcmp(MEADOW_FILE_SUBDIR_PREPEND_SDCARD_STR,
              fileName, MEADOW_FILE_SUBDIR_PREPEND_SDCARD_LEN) == 0)
  {
    // '/mmcsd0/' found
    if(fileName[strLen-1] == '/')
        return fnameInvalid;      // Can't end in '/'

    *subdirDepth = find_subdir_depth(fileName, strLen);

  // 103
    return fnameMmcsdFull;
  }
  else
  {
    return fnameInvalid;
  }
}

//============================================================================
// Parse and process received decoded packets as sent by host (CLI).
// Grab the sequence number, using it to determine if data or command.
int hcom_host_process_route_packet(const uint8_t *decodedPacket, const size_t decodedSize)
{
  int ret;
  uint16_t requestType;
  uint32_t userData;

  // All messages contain a sequence number field. The sequence number
  // determines if this packet is a command or data.
  HcomProtoDataMsg_t *hcomDataMsg = (HcomProtoDataMsg_t *) decodedPacket;

  hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data seq:%d, len:%d\n",
            thisFile, __LINE__, hcomDataMsg->seqNumber, decodedSize);

  // Test to determine if this is a data packet or a command
  if (hcomDataMsg->seqNumber != HCOM_PROTOCOL_NON_DATA_SEQUENCE_NUMBER)
  {
    // Must be a Data Packet because sequence number != 0.
    // What is the current download state? What is active, external flash or
    // ESP32?
    if(hcom_host_process_is_stm32f7_dnld_active())
    {
      // Keep resetting the watchdog on every packet
      ret = hcom_host_watchdog_dnld_timer_set_delay(HCOM_FILE_DNLD_STM32F7_WDOG_TIME);
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set errno:%d, ret:%d\n",
                  thisFile, __LINE__, errno, ret);
        hcom_host_process_free_dnld_share_mem();
      }

      // Here's the data
      hcom_file_dnld_stm32f7_recvd_file_data(hcomDataMsg, decodedSize, _dnldShared);
    }
    else if(hcom_file_dnld_esp32_is_active())
    {
      hcom_file_dnld_esp32_recvd_file_data(hcomDataMsg, decodedSize);
    }
    else
    {
      // CLI is confused or something else is wrong. We should never receive a
      // packet with a sequence number when no download is active.
      hcom_logging_syslog(LOG_DEBUG, "%s@%d-Data received but no active download\n",
                thisFile, __LINE__);
    }

    return OK;
  }

  //----------------------------------------------------------------------
  // Since sequence number is 0, must be a command (non-data packet). And
  // these always have the full HCOM header.
  // Note: Adding the full header to data packets is on the list to be fixed.
  const HcomProtoHdrMsg_t *hdrMsg = (HcomProtoHdrMsg_t *) decodedPacket;

#if HCOM_DIAG_INCLUDE_MESSAGE_DECODING_IN_BUILD > 0
  hcom_diag_decode_recvd_message_type(hdrMsg, decodedSize);
  usleep(100 * 1000);
#endif

  if(hdrMsg->stdHeader.version < HCOM_PROTOCOL_PREFERRED_VERSION_NUMBER)
  {
    char hostMsg[HCOM_SHORT_HOST_STRING_BUFF_LENGTH];
    snprintf_chk(hostMsg, HCOM_SHORT_HOST_STRING_BUFF_LENGTH, 
          "Meadow is expecting a newer CLI Protocol version. Please update Meadow.CLI." \
          " (version received: %04x required: %04x).",
          hdrMsg->stdHeader.version, HCOM_PROTOCOL_PREFERRED_VERSION_NUMBER);

    hcom_logging_syslog(LOG_ERR, "%s\n", hostMsg);
    uint16_t level = HCOM_HOST_REQUEST_TEXT_INFORMATION;
    if (hdrMsg->stdHeader.version < HCOM_PROTOCOL_MINIMUM_PROTOCOL_NUMBER)
    {
      level = HCOM_HOST_REQUEST_TEXT_ERROR;
    }
    else
    {
      g_current_hcom_protocol_version = hdrMsg->stdHeader.version;
    }

    hcom_host_send_simple_string_msg(level, 0, hostMsg, thisFile, __LINE__);
    if (level == HCOM_HOST_REQUEST_TEXT_ERROR)
    {
      return -ENOTSUP;
    }
  }

  // Pull out important values
  requestType = hdrMsg->stdHeader.rqstType;
  userData = hdrMsg->stdHeader.userData;

  //---------------------------------------------------------------
  // For downloading or deleting files we need more information and require
  // HCOM to keep this activity state alive while downloading. These commands
  // are those that need the file's name and may need to establish a temporary
  // state while the download is being processed.
  if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER ||
     requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME ||
     requestType == HCOM_MDOW_REQUEST_DELETE_FILE_BY_NAME)
  {
    // Verify that mono has been disabled, if not don't allow download
    if(hcom_mono_ctrl_is_mono_enabled())
    {
      char hostMsg[HCOM_TINY_HOST_STRING_BUFF_LENGTH];
      snprintf_chk(hostMsg, HCOM_TINY_HOST_STRING_BUFF_LENGTH,
              "Mono must be disabled for file download");

      hcom_logging_syslog(LOG_ERR, "%s@%d-%s\n", thisFile, __LINE__, hostMsg);

      // Let CLI user know the problem
      hcom_host_send_simple_string_msg(HCOM_HOST_REQUEST_TEXT_ERROR, 0,
              hostMsg, thisFile, __LINE__);

      // Notify CLI that download can't continue because of an error.
      hcom_host_send_header_msg(HCOM_HOST_REQUEST_INIT_DOWNLOAD_FAIL, 0, thisFile, __LINE__);

      return OK;
    }

    // Initialize the struct containing all download/delete state information
    hcom_host_process_init_dnld_share(userData);
  
    // We'll do some work here so it doesn't need to be done in multiple places
    size_t fileNameLength = decodedSize - HCOM_PROTOCOL_FILE_MSG_LENGTH;
    _dnldShared->dnldOrigFileName = malloc(fileNameLength + 1);
    if(_dnldShared->dnldOrigFileName == NULL)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
      return -ENOMEM;
    }

    //----------------------------------------------------------------------------------
    // File name processing
    HcomProtoFileMsg_t *fileMsg = (HcomProtoFileMsg_t *)hdrMsg;
    memcpy(_dnldShared->dnldOrigFileName, fileMsg->fileInfo.fileName, fileNameLength);
    _dnldShared->dnldOrigFileName[fileNameLength] = '\0';

    size_t subDirDepth;

    // There are 3 valid file name formats.
    // 1. A simple file name, with just a file name and nothing else.
    // 2. A file beginning with '/meadow0/'
    // 3. A file beginning with '/mmcsd0/'
    // This call will catergorize as one of the above or error. In the case of
    // a file within subdirectories, it provides the number of subdirectories.
    // This is used to further catergorize the request. 
    ret = hcom_file_subdir_categorize_filename(_dnldShared->dnldOrigFileName,
              fileNameLength, &subDirDepth);

// (--) DIAGNOSTIC CODE
    char fnameText[32];
    switch (ret)
    {
    case fnameInvalid:
      strcpy(fnameText, "fnameInvalid - bad filename");
      break;
    case fnameOriginal:
      strcpy(fnameText, "fnameOriginal-no '/' ");
      break;
    case fnameMeadowFull:
      strcpy(fnameText, "fnameMeadowFull-Starts '/meadow0/'");
      break;
    case fnameMmcsdFull:
      strcpy(fnameText, "fnameMmcsdFull-Starts '/mmcsd0/'");
      break;
    default:
      strcpy(fnameText, "default?");
      break;
    }
// (--) DIAGNOSTIC CODE

    if(ret == fnameInvalid)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-file name '%s' is invalid\n",
                thisFile, __LINE__, _dnldShared->dnldOrigFileName);
      // (--) Need to send a host message here
      return -EINVAL;   // Bad argument
    }

    syslog(1, "===> Valid format, file '%s'. It is categorized as %d (%s), subDirDepth:%lu\n",
              _dnldShared->dnldOrigFileName, ret, fnameText, subDirDepth);

    // A file name based on the original naming convention needs
    // '/meadow0/filename' prepended.
    if(ret == fnameOriginal)
    {
      // Build the full path plus file name string (e.g. /mnt0/FileName.ext)
      size_t fullFileNameLen = strlen(_dnldShared->dnldOrigFileName) + \
                strlen(HCOM_FILE_MOUNT_POINT_TARGET) + 3; // Room for '/', partition Id, NULL

      _dnldShared->dnldFullFileName = malloc(fullFileNameLen + 1);
      if(_dnldShared->dnldFullFileName == NULL)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
        return -ENOMEM;
      }

#ifdef CONFIG_MTD_PARTITION
      snprintf_chk(_dnldShared->dnldFullFileName, fullFileNameLen, "%s%d/%s",
                                HCOM_FILE_MOUNT_POINT_TARGET,
                                _dnldShared->dnldFilePartId,
                                _dnldShared->dnldOrigFileName);
#else
      snprintf_chk(_dnldShared->dnldFullFileName, fullFileNameLen, "%s/%s",
                                HCOM_FILE_MOUNT_POINT_TARGET,
                                _dnldShared->dnldOrigFileName);
#endif
    }
    else
    {
      // Allocate the same size buffer as originally provided
      size_t fullFileNameLen = strlen(_dnldShared->dnldOrigFileName) + 1;

      _dnldShared->dnldFullFileName = malloc(fullFileNameLen + 1);
      if(_dnldShared->dnldFullFileName == NULL)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-malloc returned NULL\n", thisFile, __LINE__);
        return -ENOMEM;
      }
      
      // Just copy the name, null and all.
      strcpy(_dnldShared->dnldFullFileName, _dnldShared->dnldOrigFileName);
    }

    syslog(1, "===> %s@%d-Full download file name:'%s' with %lu subdirectories\n",
              __FILE__, __LINE__, _dnldShared->dnldFullFileName, subDirDepth);
    usleep(20 * 1000);

    //----------------------------------------------------------------------------------

    // For the Meadow file system download start, need to initialize a
    // watchdog timer
    if(requestType == HCOM_MDOW_REQUEST_START_FILE_TRANSFER ||
       requestType == HCOM_MDOW_REQUEST_MONO_UPDATE_RUNTIME)
    {
      ret = hcom_host_watchdog_dnld_timer_initialize();
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Timer init errno:%d, ret:%d\n",
                  thisFile, __LINE__, errno, ret);
      }

      // Start the timer to ensure we start and continue to receiving data
      // from CLI
      ret = hcom_host_watchdog_dnld_timer_set_delay(HCOM_FILE_DNLD_STM32F7_WDOG_TIME);
      if(ret < 0)
      {
        hcom_logging_syslog(LOG_ERR, "%s@%d-Timer set errno:%d, ret:%d\n",
                  thisFile, __LINE__, errno, ret);
      }
    }
  }
  else if(requestType == HCOM_MDOW_REQUEST_END_FILE_TRANSFER)
  {
    // Stop and delete watchdog
    ret = hcom_host_watchdog_dnld_timer_delete();
    if(ret < 0)
    {
      hcom_logging_syslog(LOG_ERR, "%s@%d-Timer delete errno:%d, ret:%d\n",
                thisFile, __LINE__, errno, ret);
      hcom_host_process_free_dnld_share_mem();
    }
  }

  //---------------------------------------------------------------
  // All commands are routed here
  hcom_host_route_request_by_cmd_type(hdrMsg, decodedSize, userData,
            requestType, _dnldShared);

  return OK;
}

//============================================================
// Free any memory that needs freeing in struct hcom_dnld_shared_s.
// The intent is any function can call this and be assured that all the
// internally allocated memory is freed.
int hcom_host_process_free_dnld_share_mem()
{
  // Free any strings etc.
  free(_dnldShared->dnldOrigFileName);
  free(_dnldShared->dnldFullFileName);
  memset(_dnldShared, 0, sizeof(hcom_dnld_shared_t));

  return OK;
}

//============================================================
// Basic initialization
int hcom_host_process_init_dnld_share(uint32_t partitionId)
{
  memset(_dnldShared, 0, sizeof(hcom_dnld_shared_t));

  // This is a nuttx configuration about partitioning 
#ifdef CONFIG_MTD_PARTITION
  _dnldShared->dnldFilePartId = partitionId;
#else
  _dnldShared->dnldFilePartId = 0;    // Ignore any other partition value
#endif

  // Reset to None
  _dnldShared->dnldCurrentState = HcomStm32F7DnldStateNone;

  return OK;
}
