/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/meadow-adc.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2017 Alan Carvalho de Assis. All rights reserved.
 *   Author:  Wilderness Labs
 *
 *   Based on: configs/stm32f334-disco/src/stm32_adc.c
 *   Authors:  Ivan Ucherdzhiev <ivanucherdjiev@gmail.com>
 * 
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

#include <nuttx/config.h>

#include <stdbool.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/board.h>
#include <nuttx/analog/adc.h>

#include "stm32_gpio.h"
#include "stm32_adc.h"


/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/* If DMA support is not enabled, then only a single channel
 * can be sampled.  Otherwise, data overruns would occur.
 */

#ifdef ADC_HAVE_DMA
# define ADC1_NCHANNELS 6
#else
# define ADC1_NCHANNELS 1
#endif

/* The number of ADC channels in the conversion list */
/* TODO DMA */

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* Identifying number of each ADC channel (even if NCHANNELS is less ) */

static const uint8_t g_chanlist[4] =
{
  7
};

static const uint8_t g_chanlist2[4] =
{
  3,
  7,
  10,
  11
};

/* Configurations of pins used by each ADC channel */

static const uint32_t g_pinlist[1]  =
{
  GPIO_ADC1_IN7                 /* PA7 */
};

static const uint32_t g_pinlist2[4]  =
{
  GPIO_ADC1_IN3,                 /* PA3 */
  GPIO_ADC1_IN7,                 /* PA7 */
  GPIO_ADC1_IN10,                /* PC0 */
  GPIO_ADC1_IN11,                /* PC1 */
};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_adc_setup
 *
 * Description:
 *   Initialize ADC and register the ADC driver.
 *
 ****************************************************************************/

 int stm32_adc_setup(void)
 {
   static bool initialized = false;
   struct adc_dev_s *adc;
   int ret;
   int i;

   /* Check if we have already initialized */

   if (!initialized)
     {
       /* Configure the pins as analog inputs for the selected channels */

       for (i = 0; i < ADC1_NCHANNELS; i++)
         {
           stm32_configgpio(g_pinlist[i]);
         }

       /* Call stm32_adcinitialize() to get an instance of the ADC interface */

       adc = stm32_adc_initialize(1, g_chanlist, ADC1_NCHANNELS);
       if (adc == NULL)
         {
           aerr("ERROR: Failed to get ADC interface\n");
           return -ENODEV;
         }

       /* Register the ADC driver at "/dev/adc" */

       ret = adc_register("/dev/adc", adc);
       if (ret < 0)
         {
           aerr("ERROR: adc_register failed: %d\n", ret);
           return ret;
         }

       /* Now we are initialized */

       initialized = true;
     }

   return OK;
 }
