/****************************************************************************************************
 * configs/stm32f777zit6-meadow/src/stm32f777zit6-meadow.h
 *
 *   Copyright (C) 2015 Gregory Nutt. All rights reserved.
 *   Authors: Gregory Nutt <gnutt@nuttx.org>
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
 ****************************************************************************************************/

#ifndef __CONFIGS_MEADOW_SRC_STM32F777ZIT6_MEADOW__H
#define __CONFIGS_MEADOW_SRC_STM32F777ZIT6_MEADOW__H

/****************************************************************************************************
 * Included Files
 ****************************************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <stdint.h>
#include "stm32_gpio.h"

/****************************************************************************************************
 * Pre-processor Definitions
 ****************************************************************************************************/
/* procfs File System */

#ifdef CONFIG_FS_PROCFS
#  ifdef CONFIG_NSH_PROC_MOUNTPOINT
#    define STM32_PROCFS_MOUNTPOINT CONFIG_NSH_PROC_MOUNTPOINT
#  else
#    define STM32_PROCFS_MOUNTPOINT "/proc"
#  endif
#endif

#define GPIO_LD1        (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_50MHz | GPIO_OUTPUT_CLEAR | GPIO_PORTA | GPIO_PIN1)
#define GPIO_LD2        (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_50MHz | GPIO_OUTPUT_CLEAR | GPIO_PORTA | GPIO_PIN0)
#define GPIO_LD3        (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_50MHz | GPIO_OUTPUT_CLEAR | GPIO_PORTA | GPIO_PIN2)

#define GPIO_LED_GREEN GPIO_LD1
#define GPIO_LED_BLUE  GPIO_LD2
#define GPIO_LED_RED   GPIO_LD3

#define BOARD_NGPIOIN	1
#define BOARD_NGPIOOUT	1
#define BOARD_NGPIOINT	1

#define GPIO_IN1          0
#define GPIO_OUT1         0 

#define GPIO_INT1         0



#define MGPIO_PIN_COUNT			16

#define MGPIO_PIN_TYPE_INPUT	0
#define MGPIO_PIN_TYPE_OUTPUT	1
#define MGPIO_PIN_TYPE_INT		2

#define MGPIO_SET_CONFIG        1
#define MGPIO_WRITE             3
#define MGPIO_READ              4




/* Pushbutton B1, labelled "User", is connected to GPIO PA0.  A high value will be sensed when the
 * button is depressed. Note that the EXTI interrupt is configured.
 */

#define GPIO_BTN_USER      (GPIO_INPUT | GPIO_FLOAT | GPIO_EXTI | GPIO_PORTA | GPIO_PIN0)

/* Sporadic scheduler instrumentation. This configuration has been used for evaluating the NuttX
 * sporadic scheduler.  In this evaluation, two GPIO outputs are used.  One indicating the priority
 * (high or low) of the sporadic thread and one indicating where the thread is running or not.
 *
 * There is nothing special about the pin selections:
 *
 *   Arduino D2 PJ1 - Indicates priority1
 *   Arduino D4 PJ0 - Indicates that the thread is running
 */

#define GPIO_SCHED_HIGHPRI (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_50MHz | GPIO_OUTPUT_CLEAR | \
                            GPIO_PORTJ | GPIO_PIN1)
#define GPIO_SCHED_RUNNING (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_50MHz | GPIO_OUTPUT_CLEAR | \
                            GPIO_PORTJ | GPIO_PIN0)

/****************************************************************************************************
 * Public data
 ****************************************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************************************
 * Public Functions
 ****************************************************************************************************/

/****************************************************************************************************
 * Name: stm32_spidev_initialize
 *
 * Description:
 *   Called to configure SPI chip select GPIO pins for the stm32f777zit6-meadow board.
 *
 ****************************************************************************************************/

void weak_function stm32_spidev_initialize(void);

/****************************************************************************************************
 * Name: arch_sporadic_initialize
 *
 * Description:
 *   This configuration has been used for evaluating the NuttX sporadic scheduler.
 *
 ****************************************************************************************************/

#ifdef CONFIG_SPORADIC_INSTRUMENTATION
void arch_sporadic_initialize(void);
#endif

/****************************************************************************************************
 * Name: stm32_enablefmc
 *
 * Description:
 *  enable clocking to the FMC module
 *
 ****************************************************************************************************/

#ifdef CONFIG_STM32F7_FMC
void stm32_enablefmc(void);
#endif

/****************************************************************************************************
 * Name: stm32_disablefmc
 *
 * Description:
 *  enable clocking to the FMC module
 *
 ****************************************************************************************************/

#ifdef CONFIG_STM32F7_FMC
void stm32_disablefmc(void);
#endif

void stm32_quadspi_init(void);


int stm32_gpio_initialize(void);


#endif /* __ASSEMBLY__ */
#endif /* __CONFIGS_MEADOW_SRC_STM32F777ZIT6_MEADOW_H */

