/****************************************************************************
 * nuttx\configs\stm32f777zit6-meadow\src\meadow_inicfg.c
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
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>    // for strcasecmp
#include <fcntl.h>

#include <nuttx/mm/mm.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_cirbuf.h>

#include "meadow_inicfg.h"
#include "meadow_ini.h"       // Orginal header

#define MEADOW_CONFIG_MAX_BUFFER_SPACE 256
#define MEADOW_DEFAULT_INI_CFG_BUF_LEN  128

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int callbackChkSectionKey(void *user, const char *section, const char *key, const char *value);

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static bool isInitialized = false;
static struct host_com_cir_buffer_s *_config_cbuf;

typedef struct
{
  const char *cfgSectionName;
  const char *cfgKeyName;
  char *cfgValueBuf;
  int cfgReturnBufLen;
  int cfgNeededLength;
} meadow_config_find_data;

//=====================================================
// Not a lot to configure
int initialize_meadow_config(void)
{
  // Create a circular buffer to manage configuration file's read data
  _config_cbuf = (struct host_com_cir_buffer_s *)malloc(sizeof(struct host_com_cir_buffer_s));
  if (_config_cbuf == NULL)
  {
    syslog(LOG_ERR, "%s@%d-cir buf alloc\n", thisFile, __LINE__);
    return -1;
  }

  // Allocate a buffer for the data
  int result = hcom_cirbuf_init(_config_cbuf, MEADOW_CONFIG_MAX_BUFFER_SPACE, 0x0a); // 0x0a line feed

  if (result == HCOM_CIR_BUF_INIT_FAILED)
  {
    syslog(LOG_ERR, "%s@%d-cirbuf_init\n", thisFile, __LINE__);
    return -1;
  }
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// The following functions are for use by both the Nuttx Apps side of meadow.
// The apps side access these three key functions via hcom_nx_upd.c
//
// Return true if 'matchValue' == value found by key
bool meadow_ini_cfg_is_match(const char *fileName, const char *sectionName,
                                  const char *keyName, const char *matchValue,
                                  int *result)
{
  int ret;
  char returnValueBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];

  ret = meadow_config_find_value_from_key(fileName, sectionName, keyName, returnValueBuf,
                  MEADOW_DEFAULT_INI_CFG_BUF_LEN);
  *result = ret;

  // Case insensitivty compare
  if(ret == OK && strcasecmp(returnValueBuf, matchValue) == 0)
  {
    return true;
  }

  // Return buf has error text
  if(ret < 0 || ret > 0)
  {
    if(ret == MEADOW_CONFIG_ERROR_NO_KEY_FOUND)
      syslog(LOG_DEBUG, "%s@%d-For file:%s, section:%s, key:%s, '%s', errno:%d\n", thisFile, __LINE__,
                      fileName, sectionName, keyName, returnValueBuf, errno);
    else
      syslog(LOG_ERR, "%s@%d-For file:%s, section:%s, key:%s, '%s', errno:%d\n", thisFile, __LINE__,
                      fileName, sectionName, keyName, returnValueBuf, errno);
  }

  // Error or non-match return false
  return false;
}

//===================================================================
// Return interger value found by key or defval
int meadow_ini_cfg_get_int_default(const char *fileName, const char *sectionName,
                    const char *keyName, const int defval, int *result)
{
  int ret;
  char returnValueBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];

  ret = meadow_config_find_value_from_key(fileName, sectionName, keyName, returnValueBuf,
                  MEADOW_DEFAULT_INI_CFG_BUF_LEN);
  *result = ret;
  if(ret != OK)
  {
    if(ret < 0 || ret > 0)
    {
      // Return buf has error text
      if(ret == MEADOW_CONFIG_ERROR_NO_KEY_FOUND)
        syslog(LOG_DEBUG, "%s@%d-Key not found file:%s, section:%s, key:%s, '%s', errno:%d\n", thisFile, __LINE__,
                        fileName, sectionName, keyName, returnValueBuf, errno);
      else
        syslog(LOG_ERR, "%s@%d-Error file:%s, section:%s, key:%s, '%s', errno:%d\n", thisFile, __LINE__,
                        fileName, sectionName, keyName, returnValueBuf, errno);
    }

    // returnValueBuf has the error in text
    return defval;
  }
  
  return atoi(returnValueBuf);
}

//===================================================================
// Return interger value found by key
// DELETE THIS FUNCTION ASAP
#warning(Function 'meadow_ini_cfg_get_int()', is obsolete, use 'meadow_ini_cfg_get_int_default()')
int meadow_ini_cfg_get_int(const char *fileName, const char *sectionName, const char *keyName)
{
  int ret;
  char returnValueBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];

  ret = meadow_config_find_value_from_key(fileName, sectionName, keyName, returnValueBuf,
                  MEADOW_DEFAULT_INI_CFG_BUF_LEN);
  if(ret != OK)
  {
    syslog(LOG_WARNING, "(Warn) %s@%d-%s\n", thisFile, __LINE__, returnValueBuf);

    // This could be a valid return value!
    return -1;
  }
  
  return atoi(returnValueBuf);
}

//=====================================================
// Main entry point for checking a configuration file
// If 'fileName' is NULL the default 'meadow.cfg' will be used.
// If 'section' is NULL the sections will be ignored and only the 'key' will be used.
// Return value:
// 0 = success, returnValueBuf holds the value
// +1-n Configuration file parsing error and the return value is the line number
// -1-n specific errors defined in meadow/hcom_shared_common.h
int meadow_config_find_value_from_key(const char *fileName, const char *sectionName, const char *keyName,
                                      char returnValueBuf[], int returnBufLen)
{
  int ret;

  if(keyName == NULL || strlen(keyName) == 0)
  {
    syslog(LOG_ERR, "%s@%d-Required argument 'key' not provided\n", thisFile, __LINE__);
    return MEADOW_CONFIG_ERROR_NO_KEY_PROVIDED;
  }

  if (!isInitialized)
  {
    ret = initialize_meadow_config();
    if (ret < 0)
    {
      return ret;
    }

    isInitialized = true;
  }

  const char *useFileName;
  if(fileName == NULL || strlen(fileName) == 0)
    useFileName = MEADOW_DEFAULT_CONFIG_FILE_NAME;
  else
    useFileName = fileName;

  // Prep request
  meadow_config_find_data find_data;

  if (sectionName == NULL || strlen(sectionName) == 0)
    find_data.cfgSectionName = NULL;
  else
    find_data.cfgSectionName = sectionName;
  find_data.cfgKeyName = keyName;
  find_data.cfgValueBuf = returnValueBuf;
  find_data.cfgReturnBufLen = returnBufLen;
  find_data.cfgNeededLength = 0;

  ret = ini_parse(useFileName, callbackChkSectionKey, &find_data);

  if (find_data.cfgNeededLength)
    ret = MEADOW_CONFIG_ERROR_PROVIDED_BUF_TOO_SMALL;

  if (ret == OK)
  {
    // Result is already in the proper buffer
    return ret;
  }

  // Note: The following error messages used throughout the system
  // Greater than zero means parsing error and value is line number.
  if (ret > 0)
  {
    snprintf(find_data.cfgValueBuf, find_data.cfgReturnBufLen,
          "Parsing error in file:%s on line number:%d", useFileName, ret);
    return ret;
  }

  // Detectable errors
  switch(ret)
  {
    case MEADOW_CONFIG_ERROR_NO_KEY_FOUND:
      snprintf(find_data.cfgValueBuf, find_data.cfgReturnBufLen,
          "Key not found. File:%s, section:%s, key:%s", useFileName, find_data.cfgSectionName, keyName);
      break;

    case MEADOW_CONFIG_ERROR_CFG_FILE_OPEN:
      snprintf(find_data.cfgValueBuf, find_data.cfgReturnBufLen,
          "Config file:%s could not be opened", useFileName);
      break;

    case MEADOW_CONFIG_ERROR_PROVIDED_BUF_TOO_SMALL:
      snprintf(find_data.cfgValueBuf, find_data.cfgReturnBufLen,
          "Return buffer too small, need %d bytes, result truncated", find_data.cfgNeededLength);
      break;

    case MEADOW_CONFIG_ERROR_MEM_ALLOC_ERROR:
      snprintf(find_data.cfgValueBuf, find_data.cfgReturnBufLen,
          "Memory alloation error processing ini config file");
      break;

    case MEADOW_CONFIG_ERROR_CFG_LINE_TOO_LONG:
      snprintf(find_data.cfgValueBuf, find_data.cfgReturnBufLen,
          "Line too long in file:%s. Max length is:%d", useFileName, INI_MAX_LINE);
      break;

    case MEADOW_CONFIG_ERROR_CFG_FILE_READ_ERR:
      snprintf(find_data.cfgValueBuf, find_data.cfgReturnBufLen,
          "Config file:%s could not be read", useFileName);
      break;

    default:
      snprintf(find_data.cfgValueBuf, find_data.cfgReturnBufLen,
          "Undefined error returned by ini config:%d", ret);
      break;
  }
  syslog(LOG_DEBUG, "(Debug) %s@%d-%s\n", thisFile, __LINE__, find_data.cfgValueBuf);
  return ret;
}

//=====================================================
// Find the first/next line
// Returns number of characters on the line returned. On error
// returns a negative number. A return of '0' indicates that the
// end of file has been reached and there is no line to return.
int meadow_config_get_next_line(int filefd, uint8_t *lineBuf, size_t maxLine)
{
  int ret;
  int cirBufReturn;
  size_t actualLen;
  size_t totalAdded = 0;

  for (;;)
  {
    cirBufReturn = hcom_cirbuf_get_next_packet(_config_cbuf, lineBuf,
                                               maxLine, &actualLen);
    if (cirBufReturn == HCOM_CIR_BUF_GET_FOUND_MSG)
    {
      lineBuf[actualLen] = '\0';    // null terminate line
      return actualLen;
    }

    if (cirBufReturn == HCOM_CIR_BUF_GET_DEST_NO_ROOM)
    {
      // String too long to fit in line buffer. this is an error
      // The field actualLen contains the needed size of buffer
      return MEADOW_CONFIG_ERROR_CFG_LINE_TOO_LONG;
    }

    if (cirBufReturn == HCOM_CIR_BUF_GET_NONE_FOUND)
    {
      // Need to read from file and add to cirbuf
      size_t roomInCir;
      size_t readSize;
      uint8_t tempBuf[64];
      ssize_t bytesRead;

      while ((roomInCir = hcom_cirbuf_avail_space(_config_cbuf)) > 0)
      {
        if (roomInCir < 64)
          readSize = roomInCir;
        else
          readSize = 64;

        bytesRead = read(filefd, tempBuf, readSize);
        if (bytesRead < 0)
          return MEADOW_CONFIG_ERROR_CFG_FILE_READ_ERR;

        if (bytesRead == 0)
          break;          // End of file reached. Exit this loop and read

        // Successful read
        ret = hcom_cirbuf_add_bytes(_config_cbuf, tempBuf, bytesRead);
        DEBUGASSERT(ret == HCOM_CIR_BUF_ADD_SUCCESS);
        DEBUGASSERT(ret != HCOM_CIR_BUF_ADD_WONT_FIT);
        DEBUGASSERT(ret != HCOM_CIR_BUF_ADD_BAD_ARG);

        totalAdded += bytesRead;
      } // while()

      // No characters available, reached end of file etc. Conclusion
      // no matching key found.
      if (totalAdded == 0)
        return MEADOW_CONFIG_ERROR_NO_KEY_FOUND;
    }
  } // for(;;)
}

//=====================================================
// Callback for each section/key pair located. This function looks
// for the desired match
int callbackChkSectionKey(void *user, const char *section, const char *key,
                          const char *value)
{
  meadow_config_find_data *find_data = (meadow_config_find_data *)user;

  // Ignore section?
  if (find_data->cfgSectionName == NULL)
  {
    // 'strcasecmp' for case insensitive compare
    // 'strcmp' for case sensitive compare
    if (strcasecmp(find_data->cfgKeyName, key) != 0)
      return 1; // Tell parser to keep looking
  }
  else
  {
    if ((strcasecmp(find_data->cfgSectionName, section) != 0) ||
        (strcasecmp(find_data->cfgKeyName, key) != 0))
      return 1; // Tell parser to keep looking
  }

  // Found match! Insure it fit in the provided buffer?
  size_t neededLength = strlen(value) + 1;
  if (neededLength > find_data->cfgReturnBufLen)
  {
    find_data->cfgNeededLength = neededLength;
    neededLength = find_data->cfgReturnBufLen - 1;
  }

  strncpy(find_data->cfgValueBuf, value, neededLength);
  find_data->cfgValueBuf[neededLength] = '\0';
  return 0; // Tell parser to exit with success
}

