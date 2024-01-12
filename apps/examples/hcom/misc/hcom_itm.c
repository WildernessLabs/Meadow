/****************************************************************************
 * hcom_itm.c
 *
 *   Copyright (C) 2023 Wilderness Labs. All rights reserved.
 *   Author:  Mark Stevens
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

#if defined(CONFIG_MEADOW_ITM_ENABLED)

#include <stdlib.h>

#include <meadow/meadow_os.h>

/****************************************************************************
 * Uncomment the #define below to turn on debug help macros.
 ****************************************************************************/

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: hcom_itm_send_word
 *
 * Description:
 *  Send a single uint32_t word to the specified ITM channel.
 *
 * Input Parameters:
 *  channel - ITM channel to send the word to.
 *  word - Word to be sent to the ITM channel.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_itm_send_word(volatile uint32_t *channel, uint32_t word)
{
    while (!(*channel & 1));
    *channel = word;
}

/****************************************************************************
 * Name: hcom_itm_send_string
 *
 * Description:
 *  Send a string to ITM channel 0.
 *
 * Input Parameters:
 *  str - String to be sent to ITM channel 0.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_itm_send_string(char *str)
{
    if ((str == NULL) || (*str == '\0'))
    {
        return;
    }

    while (*str)
    {
        hcom_itm_send_word(MEADOW_ITM_PRINTF_CHANNEL, (uint32_t) *str++);
    }
}

/****************************************************************************
 * Name: hcom_itm_send_words
 *
 * Description:
 *  Send an array of uint32_t words to the specified ITM channel.
 *
 * Input Parameters:
 *  channel - ITM channel to send the words to.
 *  words - Words to be sent to the ITM channel.
 *  length - Number of words to send.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void hcom_itm_send_words(volatile uint32_t *channel, uint32_t *words, uint32_t length)
{
    for (int index = 0; index < length; index++)
    {
        hcom_itm_send_word(channel, words[index]);
    }
}

#endif /* CONFIG_MEADOW_ITM_ENABLED */