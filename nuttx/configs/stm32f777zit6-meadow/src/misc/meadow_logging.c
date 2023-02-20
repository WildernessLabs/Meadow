/****************************************************************************
 * meadow_logging.c
 *
 *   Copyright (C) 2023 Wilderness Labs. All rights reserved.
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

#include <nuttx/config.h>

#include <stdio.h>

#include <nuttx/semaphore.h>

#include <meadow/hcom_shared_common.h>
#include <meadow/semaphore_lock_unlock.h>
#include "meadow_logging.h"

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/
// #define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/**
 *  Mutex to be used by any code that wants access to the configuration.
 */
static sem_t log_file_lock = { };

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: meadow_logging_write_to_file
 *
 * Description:
 *  Write the given message to the specified log file.
 *
 * Input Parameters:
 *  filename - Name of the file to write the message to.
 *  level - Logging level for the message.
 *  message - Message to write.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_logging_write_to_file(const char *filename, meadow_file_logging_level_t level, const char *message)
{
    if ((filename != NULL) && (message != NULL))
    {
        MEADOW_SEMAPHORE_LOCK(&log_file_lock);

        char *buffer = malloc(256);
        if (buffer != NULL)
        {
            char *level_text = "I";
            switch (level)
            {
                case mfl_debug:
                    level_text = "D";
                    break;
                case mfl_warning:
                    level_text = "W";
                    break;
                case mfl_error:
                    level_text = "E";
                    break;
            }
            time_t current_time = time(NULL);
            struct tm *time_info = localtime(&current_time);
            
            char the_time[30];
            strftime(the_time, 29, "%d-%b-%Y %H:%M:%S UTC", time_info);
            snprintf(buffer, 255, "%s~%s~%s", level_text, the_time, message);

            FILE *file = fopen(filename, "a");
            if (file != NULL)
            {
                fprintf(file, "%s\n", buffer);
                fclose(file);
            }

            free(buffer);
        }

        MEADOW_SEMAPHORE_UNLOCK(&log_file_lock);
    }
}

/****************************************************************************
 * Name: meadow_logging_write
 *
 * Description:
 *  Write the specified message to the default (OS) log file.
 *
 * Input Parameters:
 *  level - Logging level for the message.
 *  message - Message to write.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_logging_write(meadow_file_logging_level_t level, const char *message)
{
    meadow_logging_write_to_file(MEADOW_LOGGING_OS_FILE_NAME, level, message);
}

/****************************************************************************
 * Name: meadow_logging_clear_logfile
 *
 * Description:
 *  Name of the file to clear.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_logging_clear_logfile(const char *filename)
{
    if (filename != NULL)
    {
        MEADOW_SEMAPHORE_LOCK(&log_file_lock);

        FILE *file = fopen(filename, "w");
        if (file != NULL)
        {
            fclose(file);
        }

        MEADOW_SEMAPHORE_UNLOCK(&log_file_lock);
    }
}

/****************************************************************************
 * Name: meadow_logging_clear_os_logfile
 *
 * Description:
 *  Empty the default log file (i.e. the OS log file).
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_logging_clear_os_logfile(void)
{
    meadow_logging_clear_logfile(MEADOW_LOGGING_OS_FILE_NAME);
}

/****************************************************************************
 * Name: meadow_logging_send_log_file_to_syslog
 *
 * Description:
 *  Copy the contents of the specified file to syslog.
 *
 * Input Parameters:
 *  filename - Name of the file to copy to syslog.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_logging_send_log_file_to_syslog(const char *filename)
{
    if (filename != NULL)
    {
        MEADOW_SEMAPHORE_LOCK(&log_file_lock);

        FILE *file = fopen(filename, "r");
        if (file != NULL)
        {
            char *buffer = malloc(256);
            if (buffer != NULL)
            {
                while (fgets(buffer, 255, file) != NULL)
                {
                    syslog(LOG_INFO, "%s", buffer);
                }
                
                free(buffer);
            }
            fclose(file);
        }

        MEADOW_SEMAPHORE_UNLOCK(&log_file_lock);
    }
}

/****************************************************************************
 * Name: meadow_logging_send_os_log_file_to_syslog
 *
 * Description:
 *  Copy the contents of the OS log file to syslog.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_logging_send_os_log_file_to_syslog(void)
{
    meadow_logging_send_log_file_to_syslog(MEADOW_LOGGING_OS_FILE_NAME);
}


/****************************************************************************
 * Name: meadow_logging_init
 *
 * Description:
 *  Prepare the OS logging system.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void meadow_logging_init_os_logging(void)
{
    sem_init(&log_file_lock, 0, 1);
    sem_setprotocol(&log_file_lock, SEM_PRIO_NONE);
    meadow_logging_clear_os_logfile();
    meadow_logging_write(mfl_info, "Logging started");
}