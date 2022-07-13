/****************************************************************************
 * /include/meadow/meadow_debug_helpers.h
 * 
 *   Copyright (C) 2021-2022 Wilderness Labs. All rights reserved.
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

#include <syslog.h>


/**
 *  The following trace macros are always defined.
 */
#define MEADOW_CRITICAL_LOG(format, ...) syslog((LOG_CRIT), format, ##__VA_ARGS__)
#define MEADOW_EMERGENCY_LOG(format, ...) syslog((LOG_EMERG), format, ##__VA_ARGS__)

/**
 *  Define USE_MEADOW_DEBUG_HELPERS in your source file and then include this file to use these defintions.
 */
#if defined(USE_MEADOW_DEBUG_HELPERS)

#warning "Meadow debug helpers are active, this may interfere with .NET applications!"

// #if defined(__KERNEL__) && defined(CONFIG_BUILD_PROTECTED)
#if defined(CONFIG_BUILD_PROTECTED)
    #define LOG_INFO    1
    #define LOG_DEBUG   1
    #define LOG_CRIT    1
#endif

//
//  Trace and debug output macros.
//
#define MEADOW_TRACE_INFORMATION(format, ...) syslog((LOG_INFO), format, ##__VA_ARGS__)

#define MEADOW_TRACE_DEBUG(format, ...) syslog((LOG_DEBUG), format, ##__VA_ARGS__)

#define MEADOW_TRACE_CRITICAL(format, ...) syslog((LOG_CRIT), format, ##__VA_ARGS__)

//
//  Turn optimisation off for files with Meadow debug helpers turned on.
//
#pragma GCC optimize "Og"

// Meadow F7v1
#define DEBUG_PIN_V1_A0   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN4)
#define DEBUG_PIN_V1_A1   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN5)
#define DEBUG_PIN_V1_A2   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN3)
#define DEBUG_PIN_V1_A3   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN7)
#define DEBUG_PIN_V1_A4   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN0)
#define DEBUG_PIN_V1_A5   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN1)

#define DEBUG_PIN_V1_SCK  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN10)
#define DEBUG_PIN_V1_COPI (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN5)
#define DEBUG_PIN_V1_CIPO (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN11)

#define DEBUG_PIN_V1_D00  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTI | GPIO_PIN9)
#define DEBUG_PIN_V1_D01  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTH | GPIO_PIN13)
#define DEBUG_PIN_V1_D02  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN6)
#define DEBUG_PIN_V1_D03  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN8)
#define DEBUG_PIN_V1_D04  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN9)
 
#define DEBUG_PIN_V1_D05  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN7)
#define DEBUG_PIN_V1_D06  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN0)
#define DEBUG_PIN_V1_D07  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN7)
#define DEBUG_PIN_V1_D08  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN6)
#define DEBUG_PIN_V1_D09  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN1)
#define DEBUG_PIN_V1_D10  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTH | GPIO_PIN10)
#define DEBUG_PIN_V1_D11  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN9)
#define DEBUG_PIN_V1_D12  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN14)
#define DEBUG_PIN_V1_D13  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN15)
#define DEBUG_PIN_V1_D14  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTG | GPIO_PIN3)
#define DEBUG_PIN_V1_D15  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTE | GPIO_PIN3)

#define DEBUG_PIN_V1_RED_LED   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN2)
#define DEBUG_PIN_V1_GREEN_LED (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN1)
#define DEBUG_PIN_V1_BLUE_LED  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN0)

// Meadow F7v2 - Many of the F7v1 exposed pins did not change. They are duplicated here for convenience.
#define DEBUG_PIN_V2_A0   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN4)
#define DEBUG_PIN_V2_A1   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN5)
#define DEBUG_PIN_V2_A2   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN3)
#define DEBUG_PIN_V2_A3   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN0)
#define DEBUG_PIN_V2_A4   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN1)
#define DEBUG_PIN_V2_A5   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN0)

#define DEBUG_PIN_V2_SCK  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN10)
#define DEBUG_PIN_V2_COPI (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN5)
#define DEBUG_PIN_V2_CIPO (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN11)

#define DEBUG_PIN_V2_D00  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTI | GPIO_PIN9)
#define DEBUG_PIN_V2_D01  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTH | GPIO_PIN13)
#define DEBUG_PIN_V2_D02  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTH | GPIO_PIN10)
#define DEBUG_PIN_V2_D03  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN8)
#define DEBUG_PIN_V2_D04  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN9)

#define DEBUG_PIN_V2_D05  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN4)
#define DEBUG_PIN_V2_D06  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN13)
#define DEBUG_PIN_V2_D07  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN7)
#define DEBUG_PIN_V2_D08  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN6)
#define DEBUG_PIN_V2_D09  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN6)
#define DEBUG_PIN_V2_D10  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN7)
#define DEBUG_PIN_V2_D11  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTC | GPIO_PIN9)
#define DEBUG_PIN_V2_D12  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN14)
#define DEBUG_PIN_V2_D13  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN15)
#define DEBUG_PIN_V2_D14  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTB | GPIO_PIN12)
#define DEBUG_PIN_V2_D15  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTG | GPIO_PIN12)

#define DEBUG_PIN_V2_RED_LED   (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN2)
#define DEBUG_PIN_V2_GREEN_LED (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN1)
#define DEBUG_PIN_V2_BLUE_LED  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz | GPIO_PORTA | GPIO_PIN0)

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

#define MEADOW_TRACE_INFORMATION(format, ...)

#define MEADOW_TRACE_DEBUG(format, ...)

#define MEADOW_TRACE_CRITICAL(format, ...)


// Meadow F7v1
#define DEBUG_PIN_V1_A0
#define DEBUG_PIN_V1_A1
#define DEBUG_PIN_V1_A2
#define DEBUG_PIN_V1_A3
#define DEBUG_PIN_V1_A4
#define DEBUG_PIN_V1_A5

#define DEBUG_PIN_V1_SCK
#define DEBUG_PIN_V1_COPI
#define DEBUG_PIN_V1_CIPO

#define DEBUG_PIN_V1_D00
#define DEBUG_PIN_V1_D01
#define DEBUG_PIN_V1_D02
#define DEBUG_PIN_V1_D03
#define DEBUG_PIN_V1_D04

#define DEBUG_PIN_V1_D05
#define DEBUG_PIN_V1_D06
#define DEBUG_PIN_V1_D07
#define DEBUG_PIN_V1_D08
#define DEBUG_PIN_V1_D09
#define DEBUG_PIN_V1_D10
#define DEBUG_PIN_V1_D11
#define DEBUG_PIN_V1_D12
#define DEBUG_PIN_V1_D13
#define DEBUG_PIN_V1_D14
#define DEBUG_PIN_V1_D15

#define DEBUG_PIN_V1_RED_LED
#define DEBUG_PIN_V1_GREEN_LED
#define DEBUG_PIN_V1_BLUE_LED

// Meadow F7v2
#define DEBUG_PIN_V2_A0
#define DEBUG_PIN_V2_A1
#define DEBUG_PIN_V2_A2
#define DEBUG_PIN_V2_A3
#define DEBUG_PIN_V2_A4
#define DEBUG_PIN_V2_A5

#define DEBUG_PIN_V2_SCK
#define DEBUG_PIN_V2_COPI
#define DEBUG_PIN_V2_CIPO

#define DEBUG_PIN_V2_D00
#define DEBUG_PIN_V2_D01
#define DEBUG_PIN_V2_D02
#define DEBUG_PIN_V2_D03
#define DEBUG_PIN_V2_D04

#define DEBUG_PIN_V2_D05
#define DEBUG_PIN_V2_D06
#define DEBUG_PIN_V2_D07
#define DEBUG_PIN_V2_D08
#define DEBUG_PIN_V2_D09
#define DEBUG_PIN_V2_D10
#define DEBUG_PIN_V2_D11
#define DEBUG_PIN_V2_D12
#define DEBUG_PIN_V2_D13
#define DEBUG_PIN_V2_D14
#define DEBUG_PIN_V2_D15

#define DEBUG_PIN_V2_RED_LED
#define DEBUG_PIN_V2_GREEN_LED
#define DEBUG_PIN_V2_BLUE_LED

// Null functions
#define DEBUG_CONFIGURE_PIN(pin)
#define DEBUG_SET_HIGH(pin)
#define DEBUG_SET_LOW(pin)
#define DEBUG_PULSE(pin, duration)

#endif /* #if defined(USE_MEADOW_DEBUG_HELPERS) */

#endif /* __MEADOW_DEBUG_HELPERS_H */

// The following where used to create #defines for the apps side.
// To use copy the following so it will be executed. Then the syslog
// output can  be copied and pasted into an app side header file.
//
// On apps side they are in /apps/examples/hcom/diag/hcom_diag_gpio.h
//
// F7v1
// syslog(2, "#define DEBUG_PIN_V1_A0 (0x%08x)\n", DEBUG_PIN_V1_A0);
// syslog(2, "#define DEBUG_PIN_V1_A1 (0x%08x)\n", DEBUG_PIN_V1_A1);
// syslog(2, "#define DEBUG_PIN_V1_A2 (0x%08x)\n", DEBUG_PIN_V1_A2);
// syslog(2, "#define DEBUG_PIN_V1_A3 (0x%08x)\n", DEBUG_PIN_V1_A3);
// syslog(2, "#define DEBUG_PIN_V1_A4 (0x%08x)\n", DEBUG_PIN_V1_A4);
// syslog(2, "#define DEBUG_PIN_V1_A5 (0x%08x)\n", DEBUG_PIN_V1_A5);

// syslog(2, "#define DEBUG_PIN_V1_SCK (0x%08x)\n", DEBUG_PIN_V1_SCK);
// syslog(2, "#define DEBUG_PIN_V1_COPI (0x%08x)\n", DEBUG_PIN_V1_COPI);
// syslog(2, "#define DEBUG_PIN_V1_CIPO (0x%08x)\n", DEBUG_PIN_V1_CIPO);

// syslog(2, "#define DEBUG_PIN_V1_D00 (0x%08x)\n", DEBUG_PIN_V1_D00);
// syslog(2, "#define DEBUG_PIN_V1_D01 (0x%08x)\n", DEBUG_PIN_V1_D01);
// syslog(2, "#define DEBUG_PIN_V1_D02 (0x%08x)\n", DEBUG_PIN_V1_D02);
// syslog(2, "#define DEBUG_PIN_V1_D03 (0x%08x)\n", DEBUG_PIN_V1_D03);
// syslog(2, "#define DEBUG_PIN_V1_D04 (0x%08x)\n", DEBUG_PIN_V1_D04);

// syslog(2, "#define DEBUG_PIN_V1_D05 (0x%08x)\n", DEBUG_PIN_V1_D05);
// syslog(2, "#define DEBUG_PIN_V1_D06 (0x%08x)\n", DEBUG_PIN_V1_D06);
// syslog(2, "#define DEBUG_PIN_V1_D07 (0x%08x)\n", DEBUG_PIN_V1_D07);
// syslog(2, "#define DEBUG_PIN_V1_D08 (0x%08x)\n", DEBUG_PIN_V1_D08);
// syslog(2, "#define DEBUG_PIN_V1_D09 (0x%08x)\n", DEBUG_PIN_V1_D09);
// syslog(2, "#define DEBUG_PIN_V1_D10 (0x%08x)\n", DEBUG_PIN_V1_D10);
// syslog(2, "#define DEBUG_PIN_V1_D11 (0x%08x)\n", DEBUG_PIN_V1_D11);
// syslog(2, "#define DEBUG_PIN_V1_D12 (0x%08x)\n", DEBUG_PIN_V1_D12);
// syslog(2, "#define DEBUG_PIN_V1_D13 (0x%08x)\n", DEBUG_PIN_V1_D13);
// syslog(2, "#define DEBUG_PIN_V1_D14 (0x%08x)\n", DEBUG_PIN_V1_D14);
// syslog(2, "#define DEBUG_PIN_V1_D15 (0x%08x)\n", DEBUG_PIN_V1_D15);

// syslog(2, "#define DEBUG_PIN_V1_RED_LED (0x%08x)\n", DEBUG_PIN_V1_RED_LED);
// syslog(2, "#define DEBUG_PIN_V1_GREEN_LED (0x%08x)\n", DEBUG_PIN_V1_GREEN_LED);
// syslog(2, "#define DEBUG_PIN_V1_BLUE_LED (0x%08x)\n", DEBUG_PIN_V1_BLUE_LED);

// // F7v2
// syslog(2, "#define DEBUG_PIN_V2_A0 (0x%08x)\n", DEBUG_PIN_V2_A0);
// syslog(2, "#define DEBUG_PIN_V2_A1 (0x%08x)\n", DEBUG_PIN_V2_A1);
// syslog(2, "#define DEBUG_PIN_V2_A2 (0x%08x)\n", DEBUG_PIN_V2_A2);
// syslog(2, "#define DEBUG_PIN_V2_A3 (0x%08x)\n", DEBUG_PIN_V2_A3);
// syslog(2, "#define DEBUG_PIN_V2_A4 (0x%08x)\n", DEBUG_PIN_V2_A4);
// syslog(2, "#define DEBUG_PIN_V2_A5 (0x%08x)\n", DEBUG_PIN_V2_A5);

// syslog(2, "#define DEBUG_PIN_V2_SCK (0x%08x)\n", DEBUG_PIN_V2_SCK);
// syslog(2, "#define DEBUG_PIN_V2_COPI (0x%08x)\n", DEBUG_PIN_V2_COPI);
// syslog(2, "#define DEBUG_PIN_V2_CIPO (0x%08x)\n", DEBUG_PIN_V2_CIPO);

// syslog(2, "#define DEBUG_PIN_V2_D00 (0x%08x)\n", DEBUG_PIN_V2_D00);
// syslog(2, "#define DEBUG_PIN_V2_D01 (0x%08x)\n", DEBUG_PIN_V2_D01);
// syslog(2, "#define DEBUG_PIN_V2_D02 (0x%08x)\n", DEBUG_PIN_V2_D02);
// syslog(2, "#define DEBUG_PIN_V2_D03 (0x%08x)\n", DEBUG_PIN_V2_D03);
// syslog(2, "#define DEBUG_PIN_V2_D04 (0x%08x)\n", DEBUG_PIN_V2_D04);

// syslog(2, "#define DEBUG_PIN_V2_D05 (0x%08x)\n", DEBUG_PIN_V2_D05);
// syslog(2, "#define DEBUG_PIN_V2_D06 (0x%08x)\n", DEBUG_PIN_V2_D06);
// syslog(2, "#define DEBUG_PIN_V2_D07 (0x%08x)\n", DEBUG_PIN_V2_D07);
// syslog(2, "#define DEBUG_PIN_V2_D08 (0x%08x)\n", DEBUG_PIN_V2_D08);
// syslog(2, "#define DEBUG_PIN_V2_D09 (0x%08x)\n", DEBUG_PIN_V2_D09);
// syslog(2, "#define DEBUG_PIN_V2_D10 (0x%08x)\n", DEBUG_PIN_V2_D10);
// syslog(2, "#define DEBUG_PIN_V2_D11 (0x%08x)\n", DEBUG_PIN_V2_D11);
// syslog(2, "#define DEBUG_PIN_V2_D12 (0x%08x)\n", DEBUG_PIN_V2_D12);
// syslog(2, "#define DEBUG_PIN_V2_D13 (0x%08x)\n", DEBUG_PIN_V2_D13);
// syslog(2, "#define DEBUG_PIN_V2_D14 (0x%08x)\n", DEBUG_PIN_V2_D14);
// syslog(2, "#define DEBUG_PIN_V2_D15 (0x%08x)\n", DEBUG_PIN_V2_D15);

// syslog(2, "#define DEBUG_PIN_V2_RED_LED (0x%08x)\n", DEBUG_PIN_V2_RED_LED);
// syslog(2, "#define DEBUG_PIN_V2_GREEN_LED (0x%08x)\n", DEBUG_PIN_V2_GREEN_LED);
// syslog(2, "#define DEBUG_PIN_V2_BLUE_LED (0x%08x)\n", DEBUG_PIN_V2_BLUE_LED);
