/****************************************************************************
 * espcp_file_system.c
 *
 *   Copyright (C) 2023 Wilderness Labs. All rights reserved.
 *   Author: Mark Stevens
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must resultain the above copyright
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

#include "espcp_file_system.h"

/****************************************************************************
 * Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: espcp_file_system_format
 *
 * Description:
 *  Format the ESP32 file system.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  0 if successful, -1 on error.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int32_t espcp_file_system_format(void)
{
    return(-1);
}

/****************************************************************************
 * Name: espcp_file_system_read_file
 *
 * Description:
 *  Read the contents of the file and place the data in the buffer.
 *
 * Input Parameters:
 *  name - Name of the file to read.
 *  length - Pointer to an unsigned integer to hold the number of bytes
 *           retrieved.
 *
 * Returned Value:
 *  Pointer to an array holding the contents of the file or NULL if there 
 *  was a problem.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
uint8_t *espcp_file_system_read_file(char *name, uint32_t *length)
{
    return(NULL);
}

/****************************************************************************
 * Name: espcp_file_system_write_file
 *
 * Description:
 *  Write the data in the buffer to the specified file.
 *
 * Input Parameters:
 *  name - Name of the file to write.
 *  buffer - Buffer holding the data to be written.
 *  length - Number of bytes to write.
 *
 * Returned Value:
 *  Number of bytes written or negated error code on error.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
int32_t espcp_file_system_write_file(char *name, uint8_t *buffer, uint16_t length)
{
    return(-1);
}

/****************************************************************************
 * Name: espcp_file_system_delete_file
 *
 * Description:
 *  Delete the file from the file system.
 *
 * Input Parameters:
 *  name - Name of the file to delete.
 *
 * Returned Value:
 *  0 on success, negated error code on failure.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int32_t espcp_file_system_delete_file(char *name)
{
    return(-1);
}

/****************************************************************************
 * Name: espcp_file_system_list_files
 *
 * Description:
 *  Get the list of files on the file system.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  Pointer to a espcp_file_system_info_t object holding information about
 *  the files on the file system.
 * 
 *  NULL if there was a problem.
 *
 * Assumptions/Limitations:
 *  None.
 *
 ****************************************************************************/
espcp_file_system_info_t *espcp_file_system_list_files(void)
{
    return(NULL);
}