/****************************************************************************
 * /include/meadow/meadow_debug_helpers.h
 * 
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
 * 
 *   Provide macros and method defintions to assist in debugging
 *   NuttX and meadow code.
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
#ifndef __MEADOW_DEBUG_HELPERS_H
#define __MEADOW_DEBUG_HELPERS_H

/**
 *  Define USE_MEADOW_DEBUG_HELPERS in your source file and then include this file to use these defintions.
 */
#if defined(USE_MEADOW_DEBUG_HELPERS)

#warning "Meadow debug helpers are active, this may interfere with .NET applications!"

#define DEBUG_PIN_A0 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN4)
#define DEBUG_PIN_A1 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN5)
#define DEBUG_PIN_A2 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN3)
#define DEBUG_PIN_A3 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN7)
#define DEBUG_PIN_A4 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN0)
#define DEBUG_PIN_A5 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN1)

#define DEBUG_PIN_SCK (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN10)
#define DEBUG_PIN_COPI (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN5)
#define DEBUG_PIN_CIPO (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN11)

#define DEBUG_PIN_D02 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN6)
#define DEBUG_PIN_D03 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN8)
#define DEBUG_PIN_D04 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN9)

#define DEBUG_PIN_D05 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN7)
#define DEBUG_PIN_D06 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN0)
#define DEBUG_PIN_D07 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN7)
#define DEBUG_PIN_D08 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN6)
#define DEBUG_PIN_D09 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN1)
#define DEBUG_PIN_D10 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTH | GPIO_PIN10)
#define DEBUG_PIN_D11 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN9)
#define DEBUG_PIN_D14 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTG | GPIO_PIN3)
#define DEBUG_PIN_D15 (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTE | GPIO_PIN3)

#define DEBUG_PIN_RED_LED   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN2)
#define DEBUG_PIN_GREEN_LED (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN1)
#define DEBUG_PIN_BLUE_LED  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN0)

/**
 *  Configure a GPIO for debugging and set the initial state to low.
 */
#define DEBUG_CONFIGURE_PIN(pin)        do { stm32_unconfiggpio((pin)); stm32_configgpio((pin)); stm32_gpiowrite((pin), false); } while (0)

/**
 *  Set the specified GPIO pin high (assumes DEBUG_CONFIGURE_PIN has been called).
 */
#define DEBUG_SET_HIGH(pin)             stm32_gpiowrite((pin), true)

/**
 *  Set the specified GPIO pin low (assumes DEBUG_CONFIGURE_PIN has been called).
 */
#define DEBUG_SET_LOW(pin)              stm32_gpiowrite((pin), false)

/**
 *  Pulse the specified GPIO pin for the specified number of microseconds 
 *  (assumes DEBUG_CONFIGURE_PIN has been called and the pin is already low).
 */
#define DEBUG_PULSE(pin, duration)      do { stm32_gpiowrite((pin), true); usleep((duration)); stm32_gpiowrite((pin), false); } while (0)

#else

#define DEBUG_PIN_A0
#define DEBUG_PIN_A1
#define DEBUG_PIN_A2
#define DEBUG_PIN_A3
#define DEBUG_PIN_A4
#define DEBUG_PIN_A5

#define DEBUG_PIN_SCK
#define DEBUG_PIN_COPI
#define DEBUG_PIN_CIPO

#define DEBUG_PIN_D02
#define DEBUG_PIN_D03
#define DEBUG_PIN_D04

#define DEBUG_PIN_D05
#define DEBUG_PIN_D06
#define DEBUG_PIN_D07
#define DEBUG_PIN_D08
#define DEBUG_PIN_D09
#define DEBUG_PIN_D10
#define DEBUG_PIN_D11
#define DEBUG_PIN_D14
#define DEBUG_PIN_D15

#define DEBUG_PIN_RED_LED
#define DEBUG_PIN_GREEN_LED
#define DEBUG_PIN_BLUE_LED

#define DEBUG_CONFIGURE_PIN(pin)
#define DEBUG_SET_HIGH(pin)
#define DEBUG_SET_LOW(pin)
#define DEBUG_PULSE(pin, duration)


#endif

#endif /* __MEADOW_DEBUG_HELPERS_H */