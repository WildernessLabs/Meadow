/************************************************************************************
 * configs/stm32f777zit6-meadow/include/board.h
 *
 *   Copyright (C) 2015 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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
 ************************************************************************************/

#ifndef __CONFIG_STM32F777ZIT6_MEADOW_INCLUDE_BOARD_H
#define __CONFIG_STM32F777ZIT6_MEADOW_INCLUDE_BOARD_H

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>

#ifndef __ASSEMBLY__
# include <stdint.h>
#endif

#include "stm32_rcc.h"
#if defined(CONFIG_STM32F7_SDMMC1) || defined(CONFIG_STM32F7_SDMMC2)
#  include "stm32_sdmmc.h"
#endif

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/* Clocking *************************************************************************/
// MEADOW_CHECK
/* The Meadow board provides the following clock sources:
 *
 *   X2:  25 MHz oscillator for STM32F777ZIT6 microcontroller and Ethernet PHY.
 *   X1:  32.768 KHz crystal for STM32F777ZIT6 embedded RTC
 *
 * So we have these clock source available within the STM32
 *
 *   HSI: 16 MHz RC factory-trimmed
 *   LSI: 32 KHz RC
 *   HSE: On-board crystal frequency is 25MHz
 *   LSE: 32.768 kHz
 */

#define STM32_BOARD_XTAL        25000000ul

#define STM32_HSI_FREQUENCY     16000000ul
#define STM32_LSI_FREQUENCY     32000
#define STM32_HSE_FREQUENCY     STM32_BOARD_XTAL
#define STM32_LSE_FREQUENCY     32768

/* Main PLL Configuration.
 *
 * PLL source is HSE = 25,000,000
 *
 * PLL_VCO = (STM32_HSE_FREQUENCY / PLLM) * PLLN
 * Subject to:
 *
 *     2 <= PLLM <= 63
 *   192 <= PLLN <= 432
 *   192 MHz <= PLL_VCO <= 432MHz
 *
 * SYSCLK  = PLL_VCO / PLLP
 * Subject to
 *
 *   PLLP = {2, 4, 6, 8}
 *   SYSCLK <= 216 MHz
 *
 * USB OTG FS, SDMMC and RNG Clock = PLL_VCO / PLLQ
 * Subject to
 *   The USB OTG FS requires a 48 MHz clock to work correctly. The SDMMC
 *   and the random number generator need a frequency lower than or equal
 *   to 48 MHz to work correctly.
 *
 * 2 <= PLLQ <= 15
 */

#if defined(CONFIG_STM32F7_OTGFS)
/* USB OTG FS clock (= SDMMCCLK = RNGCLK) must be 48 MHz
 *
 * PLL_VCO = (25,000,000 / 25) * 384 = 384 MHz
 * SYSCLK  = 384 MHz / 2 = 192 MHz
 * USB OTG FS, SDMMC and RNG Clock = 384 MHz / 8 = 48MHz
 * DSI CLK = PLL_VCO / PLLR = 384 / 7 = 54,86 MHz
 */

#define STM32_PLLCFG_PLLM       RCC_PLLCFG_PLLM(25)
#define STM32_PLLCFG_PLLN       RCC_PLLCFG_PLLN(384)
#define STM32_PLLCFG_PLLP       RCC_PLLCFG_PLLP_2
#define STM32_PLLCFG_PLLQ       RCC_PLLCFG_PLLQ(8)
#define STM32_PLLCFG_PLLR       RCC_PLLCFG_PLLR(7)

#define STM32_VCO_FREQUENCY     ((STM32_HSE_FREQUENCY / 25) * 384)
#define STM32_SYSCLK_FREQUENCY  (STM32_VCO_FREQUENCY / 2)
#define STM32_OTGFS_FREQUENCY   (STM32_VCO_FREQUENCY / 8)

#elif defined(CONFIG_STM32F7_SDMMC1) || defined(CONFIG_STM32F7_SDMMC2) || defined(CONFIG_STM32F7_RNG)
/* SDMMCCLK (= USB OTG FS clock = RNGCLK) should be <= 48MHz
 *
 * PLL_VCO = (25,000,000 / 25) * 432 = 432 MHz
 * SYSCLK  = 432 MHz / 2 = 216 MHz
 * USB OTG FS, SDMMC and RNG Clock = 432 MHz / 10 = 43.2 MHz
 * DSI CLK = PLL_VCO / PLLR = 432 / 8 = 54 MHz
 */

#define STM32_PLLCFG_PLLM       RCC_PLLCFG_PLLM(25)
#define STM32_PLLCFG_PLLN       RCC_PLLCFG_PLLN(432)
#define STM32_PLLCFG_PLLP       RCC_PLLCFG_PLLP_2
#define STM32_PLLCFG_PLLQ       RCC_PLLCFG_PLLQ(10)
#define STM32_PLLCFG_PLLR       RCC_PLLCFG_PLLR(8)

#define STM32_VCO_FREQUENCY     ((STM32_HSE_FREQUENCY / 25) * 432)
#define STM32_SYSCLK_FREQUENCY  (STM32_VCO_FREQUENCY / 2)
#define STM32_OTGFS_FREQUENCY   (STM32_VCO_FREQUENCY / 10)

#else
/* No restrictions by OTGFS
 *
 * PLL_VCO = (25,000,000 / 25) * 432 = 432 MHz
 * SYSCLK  = 432 MHz / 2 = 216 MHz
 * USB OTG FS, SDMMC and RNG Clock = 432 MHz / 10 = 43.2 MHz
 * DSI CLK = PLL_VCO / PLLR = 432 / 8 = 54 MHz
 */

#define STM32_PLLCFG_PLLM       RCC_PLLCFG_PLLM(25)
#define STM32_PLLCFG_PLLN       RCC_PLLCFG_PLLN(432)
#define STM32_PLLCFG_PLLP       RCC_PLLCFG_PLLP_2
#define STM32_PLLCFG_PLLQ       RCC_PLLCFG_PLLQ(10)
#define STM32_PLLCFG_PLLR       RCC_PLLCFG_PLLR(8)

#define STM32_VCO_FREQUENCY     ((STM32_HSE_FREQUENCY / 25) * 432)
#define STM32_SYSCLK_FREQUENCY  (STM32_VCO_FREQUENCY / 2)
#define STM32_OTGFS_FREQUENCY   (STM32_VCO_FREQUENCY / 10)
#endif

/* Configure factors for  PLLSAI clock */

#define CONFIG_STM32F7_PLLSAI 1
#define STM32_RCC_PLLSAICFGR_PLLSAIN    RCC_PLLSAICFGR_PLLSAIN(384)
#define STM32_RCC_PLLSAICFGR_PLLSAIP    RCC_PLLSAICFGR_PLLSAIP(8)
#define STM32_RCC_PLLSAICFGR_PLLSAIQ    RCC_PLLSAICFGR_PLLSAIQ(2)
#define STM32_RCC_PLLSAICFGR_PLLSAIR    RCC_PLLSAICFGR_PLLSAIR(2)

/* Configure Dedicated Clock Configuration Register */

#define STM32_RCC_DCKCFGR1_PLLI2SDIVQ  RCC_DCKCFGR1_PLLI2SDIVQ(1)
#define STM32_RCC_DCKCFGR1_PLLSAIDIVQ  RCC_DCKCFGR1_PLLSAIDIVQ(1)
#define STM32_RCC_DCKCFGR1_PLLSAIDIVR  RCC_DCKCFGR1_PLLSAIDIVR(0)
#define STM32_RCC_DCKCFGR1_SAI1SRC     RCC_DCKCFGR1_SAI1SEL(0)
#define STM32_RCC_DCKCFGR1_SAI2SRC     RCC_DCKCFGR1_SAI2SEL(0)
#define STM32_RCC_DCKCFGR1_TIMPRESRC   0
#define STM32_RCC_DCKCFGR1_DFSDM1SRC   0
#define STM32_RCC_DCKCFGR1_ADFSDM1SRC  0



/* Configure factors for  PLLI2S clock */

#define STM32_RCC_PLLI2SCFGR_PLLI2SN   RCC_PLLI2SCFGR_PLLI2SN(192)
#define STM32_RCC_PLLI2SCFGR_PLLI2SP   RCC_PLLI2SCFGR_PLLI2SP(2)
#define STM32_RCC_PLLI2SCFGR_PLLI2SQ   RCC_PLLI2SCFGR_PLLI2SQ(2)
#define STM32_RCC_PLLI2SCFGR_PLLI2SR   RCC_PLLI2SCFGR_PLLI2SR(2)

/* Configure Dedicated Clock Configuration Register 2 */

#define STM32_RCC_DCKCFGR2_USART1SRC  RCC_DCKCFGR2_USART1SEL_APB
#define STM32_RCC_DCKCFGR2_USART2SRC  RCC_DCKCFGR2_USART2SEL_APB
#define STM32_RCC_DCKCFGR2_UART4SRC   RCC_DCKCFGR2_UART4SEL_APB
#define STM32_RCC_DCKCFGR2_UART5SRC   RCC_DCKCFGR2_UART5SEL_APB
#define STM32_RCC_DCKCFGR2_USART6SRC  RCC_DCKCFGR2_USART6SEL_APB
#define STM32_RCC_DCKCFGR2_UART7SRC   RCC_DCKCFGR2_UART7SEL_APB
#define STM32_RCC_DCKCFGR2_UART8SRC   RCC_DCKCFGR2_UART8SEL_APB
#define STM32_RCC_DCKCFGR2_I2C1SRC    RCC_DCKCFGR2_I2C1SEL_HSI
#define STM32_RCC_DCKCFGR2_I2C2SRC    RCC_DCKCFGR2_I2C2SEL_HSI
#define STM32_RCC_DCKCFGR2_I2C3SRC    RCC_DCKCFGR2_I2C3SEL_HSI
#define STM32_RCC_DCKCFGR2_I2C4SRC    RCC_DCKCFGR2_I2C4SEL_HSI
#define STM32_RCC_DCKCFGR2_LPTIM1SRC  RCC_DCKCFGR2_LPTIM1SEL_APB
#define STM32_RCC_DCKCFGR2_CECSRC     RCC_DCKCFGR2_CECSEL_HSI
#define STM32_RCC_DCKCFGR2_CK48MSRC   RCC_DCKCFGR2_CK48MSEL_PLLSAI
#define STM32_RCC_DCKCFGR2_SDMMCSRC   RCC_DCKCFGR2_SDMMCSEL_48MHZ
#define STM32_RCC_DCKCFGR2_SDMMC2SRC  RCC_DCKCFGR2_SDMMC2SEL_48MHZ
#define STM32_RCC_DCKCFGR2_DSISRC     RCC_DCKCFGR2_DSISEL_PHY

/* Several prescalers allow the configuration of the two AHB buses, the
 * high-speed APB (APB2) and the low-speed APB (APB1) domains. The maximum
 * frequency of the two AHB buses is 216 MHz while the maximum frequency of
 * the high-speed APB domains is 108 MHz. The maximum allowed frequency of
 * the low-speed APB domain is 54 MHz.
 */

/* AHB clock (HCLK) is SYSCLK (216 MHz) */

#define STM32_RCC_CFGR_HPRE     RCC_CFGR_HPRE_SYSCLK  /* HCLK  = SYSCLK / 1 */
#define STM32_HCLK_FREQUENCY    STM32_SYSCLK_FREQUENCY
#define STM32_BOARD_HCLK        STM32_HCLK_FREQUENCY  /* same as above, to satisfy compiler */

#define BOARD_AHB_FREQUENCY     STM32_HCLK_FREQUENCY

/* APB1 clock (PCLK1) is HCLK/4 (54 MHz) */

#define STM32_RCC_CFGR_PPRE1    RCC_CFGR_PPRE1_HCLKd4     /* PCLK1 = HCLK / 4 */
#define STM32_PCLK1_FREQUENCY   (STM32_HCLK_FREQUENCY/4)

/* Timers driven from APB1 will be twice PCLK1 */

#define STM32_APB1_TIM2_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM3_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM4_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM5_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM6_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM7_CLKIN   (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM12_CLKIN  (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM13_CLKIN  (2*STM32_PCLK1_FREQUENCY)
#define STM32_APB1_TIM14_CLKIN  (2*STM32_PCLK1_FREQUENCY)

/* APB2 clock (PCLK2) is HCLK/2 (108MHz) */

#define STM32_RCC_CFGR_PPRE2    RCC_CFGR_PPRE2_HCLKd2     /* PCLK2 = HCLK / 2 */
#define STM32_PCLK2_FREQUENCY   (STM32_HCLK_FREQUENCY/2)

/* Timers driven from APB2 will be twice PCLK2 */

#define STM32_APB2_TIM1_CLKIN   (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM8_CLKIN   (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM9_CLKIN   (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM10_CLKIN  (2*STM32_PCLK2_FREQUENCY)
#define STM32_APB2_TIM11_CLKIN  (2*STM32_PCLK2_FREQUENCY)

/* FLASH wait states
 *
 *  --------- ---------- -----------
 *  VDD       MAX SYSCLK WAIT STATES
 *  --------- ---------- -----------
 *  1.7-2.1 V   180 MHz    8
 *  2.1-2.4 V   216 MHz    9
 *  2.4-2.7 V   216 MHz    8
 *  2.7-3.6 V   216 MHz    7
 *  --------- ---------- -----------
 */

#define BOARD_FLASH_WAITSTATES 7

/* LED definitions ******************************************************************/
// MEADOW_CHECK
/* The STM32F777ZIT6-MEADOW board has numerous LEDs but only one, LD1 located near the
 * reset button, that can be controlled by software (LD2 is a power indicator, LD3-6
 * indicate USB status, LD7 is controlled by the ST-Link).
 *
 * LD1 is controlled by PI1 which is also the SPI2_SCK at the Arduino interface.
 * One end of LD1 is grounded so a high output on PI1 will illuminate the LED.
 *
 * If CONFIG_ARCH_LEDS is not defined, then the user can control the LEDs in any way.
 * The following definitions are used to access individual LEDs.
 */

/* LED index values for use with board_userled() */


#define BOARD_LED1        0
#define BOARD_LED2        1
#define BOARD_LED3        2
#define BOARD_NLEDS       3

#define BOARD_LED_GREEN   BOARD_LED1
#define BOARD_LED_BLUE    BOARD_LED2
#define BOARD_LED_RED     BOARD_LED3

/* LED bits for use with board_userled_all() */

#define BOARD_LED1_BIT    (1 << BOARD_LED1)
#define BOARD_LED2_BIT    (1 << BOARD_LED2)
#define BOARD_LED3_BIT    (1 << BOARD_LED3)

/* If CONFIG_ARCH_LEDS is defined, the usage by the board port is defined in
 * include/board.h and src/stm32_leds.c. The LEDs are used to encode OS-related
 * events as follows:
 *
 *   SYMBOL              Meaning                 LD1
 *   ------------------- ----------------------- ------
 *   LED_STARTED         NuttX has been started  OFF
 *   LED_HEAPALLOCATE    Heap has been allocated OFF
 *   LED_IRQSENABLED     Interrupts enabled      OFF
 *   LED_STACKCREATED    Idle stack created      ON
 *   LED_INIRQ           In an interrupt         N/C
 *   LED_SIGNAL          In a signal handler     N/C
 *   LED_ASSERTION       An assertion failed     N/C
 *   LED_PANIC           The system has crashed  FLASH
 *
 * Thus is LD1 is statically on, NuttX has successfully  booted and is,
 * apparently, running normally.  If LD1 is flashing at approximately
 * 2Hz, then a fatal error has been detected and the system has halted.
 */

#define LED_STARTED        0 /* NuttX has been started   OFF    OFF   OFF  */
#define LED_HEAPALLOCATE   1 /* Heap has been allocated  OFF    OFF   ON   */
#define LED_IRQSENABLED    2 /* Interrupts enabled       OFF    ON    OFF  */
#define LED_STACKCREATED   3 /* Idle stack created       OFF    ON    ON   */
#define LED_INIRQ          4 /* In an interrupt          N/C    N/C   GLOW */
#define LED_SIGNAL         5 /* In a signal handler      N/C    GLOW  N/C  */
#define LED_ASSERTION      6 /* An assertion failed      GLOW   N/C   GLOW */
#define LED_PANIC          7 /* The system has crashed   Blink  OFF   N/C  */
#define LED_IDLE           8 /* MCU is is sleep mode     ON     OFF   OFF  */

/* Button definitions ***************************************************************/
/* The STM32F7 Discovery supports one button:  Pushbutton B1, labelled "User", is
 * connected to GPIO PA0.  A high value will be sensed when the button is depressed.
 */

#define BUTTON_USER        0
#define NUM_BUTTONS        1
#define BUTTON_USER_BIT    (1 << BUTTON_USER)

/* Meadow ESP32 Boot and Reset pin connections **************************************/

// GPIO_FLOAT because on board resistors pull up
#define MEADOW_ESP32_ONBOARD_BOOT_PIN_OUTPUT  (GPIO_OUTPUT | GPIO_FLOAT | GPIO_OPENDRAIN | GPIO_SPEED_100MHz | GPIO_PORTI | GPIO_PIN10)
#define MEADOW_ESP32_ONBOARD_RESET_PIN_OUTPUT (GPIO_OUTPUT | GPIO_FLOAT | GPIO_OPENDRAIN | GPIO_SPEED_100MHz | GPIO_PORTF | GPIO_PIN7)

#define MEADOW_ESP32_ONBOARD_RESET_PIN_INPUT  (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTF | GPIO_PIN7)
#define MEADOW_ESP32_ONBOARD_BOOT_PIN_INPUT   (GPIO_INPUT | GPIO_FLOAT | GPIO_SPEED_100MHz | GPIO_PORTI | GPIO_PIN10)

/* Alternate function pin selections ************************************************/

// Note: The source defns are from \arch\arm\src\stm32f7\chip\stm32f76xx77xx_pinmap.h

// USART1 (Meadow COM1)
#define GPIO_USART1_RX GPIO_USART1_RX_3 // PB15
#define GPIO_USART1_TX GPIO_USART1_TX_3 // PB14

// UART4 (Meadow COM1)
#define GPIO_UART4_RX GPIO_UART4_RX_6 // PI9
#define GPIO_UART4_TX GPIO_UART4_TX_5 // PH13

// UART5 (Meadow STM32F7 to ESP32)
#define GPIO_UART5_RX     GPIO_UART5_RX_1 // PD2
// Left the original '#define GPIO_UART5_TX' so modifying Nuttx code not needed
#define GPIO_UART5_TX     GPIO_UART5_TX_3 // PB13 - default
#define GPIO_UART5_TX_V1  GPIO_UART5_TX_3 // PB13 - F7v1
#define GPIO_UART5_TX_V2  GPIO_UART5_TX_1 // PC12 - F7v2 & CCMv2

// UART6
// F7v1 and F7v2 both exposed UART6 but on different pins
// F7v1 pins USART6_TX = D02 and USART6_RX = D05
// F7v2 pins USART6_TX = D09 and USART6_RX = D10
#define GPIO_USART6_RX GPIO_USART6_RX_1 // PC7
#define GPIO_USART6_TX GPIO_USART6_TX_1 // PC6

/* PWM
 */

#define GPIO_TIM2_CH1OUT  GPIO_TIM2_CH1OUT_1
#define GPIO_TIM2_CH2OUT  GPIO_TIM2_CH2OUT_1
#define GPIO_TIM2_CH3OUT  GPIO_TIM2_CH3OUT_1
#define GPIO_TIM3_CH2OUT  GPIO_TIM3_CH2OUT_3
#define GPIO_TIM3_CH3OUT  GPIO_TIM3_CH3OUT_1
#define GPIO_TIM3_CH4OUT  GPIO_TIM3_CH4OUT_1
#define GPIO_TIM4_CH1OUT  GPIO_TIM4_CH1OUT_1
#define GPIO_TIM4_CH2OUT  GPIO_TIM4_CH2OUT_1
#define GPIO_TIM4_CH3OUT  GPIO_TIM4_CH3OUT_1
#define GPIO_TIM4_CH4OUT  GPIO_TIM4_CH4OUT_1
#define GPIO_TIM5_CH1OUT  GPIO_TIM5_CH1OUT_2
#define GPIO_TIM8_CH1OUT  GPIO_TIM8_CH1OUT_1
#define GPIO_TIM8_CH4OUT  GPIO_TIM8_CH4OUT_1
#define GPIO_TIM9_CH2OUT  GPIO_TIM9_CH2OUT_1
#define GPIO_TIM12_CH1OUT  GPIO_TIM12_CH1OUT_1
#define GPIO_TIM12_CH2OUT  GPIO_TIM12_CH2OUT_1
#define GPIO_TIM14_CH1OUT  GPIO_TIM14_CH1OUT_1


/* The STM32 F7 connects to a SMSC LAN8742A PHY using these pins:
 *
 *   STM32 F7  BOARD        LAN8742A
 *   GPIO      SIGNAL       PIN NAME
 *   --------- ------------ -------------
 *   PG11/PB11 RMII_TX_EN   TXEN
 *   PG13      RMII_TXD0    TXD0
 *   PG14      RMII_TXD1    TXD1
 *   PC4       RMII_RXD0    RXD0/MODE0
 *   PC5       RMII_RXD1    RXD1/MODE1
 *   PD5       RMII_RXER    RXER/PHYAD0
 *   PA7       RMII_CRS_DV  CRS_DV/MODE2
 *   PC1       RMII_MDC     MDC
 *   PA2       RMII_MDIO    MDIO
 *   N/A       NRST         nRST
 *   PA1       RMII_REF_CLK nINT/REFCLK0
 *   N/A       OSC_25M      XTAL1/CLKIN
 *
 * The PHY address is 0, since RMII_RXER/PHYAD0 features a pull down.
 * After reset, RMII_RXER/PHYAD0 switches to the RXER function,
 * receive errors can be detected using GPIO pin PD5
 */

// These are the only GPIOs define here because they are the only ones
// that have more that one GPIO option.The other 6 RMII GPIOs are
// fixed by the STM32F777.
// However, There was a F7v1 embedded board followed by a F7v2 Core-Compute
// Module (CCM). The F7v1 version never shipped to customers but was the first
// one to work with Ethernet. The F7v1 version used PG11 for RMMI_TX_EN and
// The CCM used PB11. Therefore, the following will not be necessary in the
// future, F7v2 uses PB11.
#define GPIO_ETH_RMII_TX_EN   GPIO_ETH_RMII_TX_EN_1 // PB11 F7v2
// #define GPIO_ETH_RMII_TX_EN   GPIO_ETH_RMII_TX_EN_2 // PG11 F7v1
#define GPIO_ETH_RMII_TXD0    GPIO_ETH_RMII_TXD0_2  // PG13
#define GPIO_ETH_RMII_TXD1    GPIO_ETH_RMII_TXD1_2  // PG14

/* I2C Mapping
 * I2C #4 is connected to the LCD daughter board
 * and the WM8994 audio codec.
 *
 * I2C4_SCL - PD12
 * I2C4_SDA - PB7
 */
#define GPIO_I2C4_SCL        GPIO_I2C4_SCL_1
#define GPIO_I2C4_SDA        GPIO_I2C4_SDA_5


/* QSPI Mapping  */

#define GPIO_QSPI_CS  (GPIO_QUADSPI_BK1_NCS_2 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define GPIO_QSPI_IO0 (GPIO_QUADSPI_BK1_IO0_3 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define GPIO_QSPI_IO1 (GPIO_QUADSPI_BK1_IO1_3 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define GPIO_QSPI_IO2 (GPIO_QUADSPI_BK1_IO2_1 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define GPIO_QSPI_IO3 (GPIO_QUADSPI_BK1_IO3_2 | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)
#define GPIO_QSPI_SCK (GPIO_QUADSPI_CLK_1     | GPIO_FLOAT | GPIO_PUSHPULL | GPIO_SPEED_100MHz)

/* SDMMC */

/* Stream selections are arbitrary for now but might become important in the future
 * if we set aside more DMA channels/streams.
 *
 * SDIO DMA
 *   DMAMAP_SDMMC1_1 = Channel 4, Stream 3
 *   DMAMAP_SDMMC1_2 = Channel 4, Stream 6
 *
 *   DMAMAP_SDMMC2_1 = Channel 11, Stream 0
 *   DMAMAP_SDMMC2_2 = Channel 11, Stream 5
 */

// #define DMAMAP_SDMMC1  DMAMAP_SDMMC1_1
#define DMAMAP_SDMMC2  DMAMAP_SDMMC2_1

/* SDIO dividers.  Note that slower clocking is required when DMA is disabled
 * in order to avoid RX overrun/TX underrun errors due to delayed responses
 * to service FIFOs in interrupt driven mode.  These values have not been
 * tuned!!!
 *
 * SDIOCLK=48MHz, SDIO_CK=SDIOCLK/(118+2)=400 KHz
 */

#define STM32_SDMMC_INIT_CLKDIV      (118 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)

/* DMA ON:  SDIOCLK=48MHz, SDIO_CK=SDIOCLK/(1+2)=16 MHz
 * DMA OFF: SDIOCLK=48MHz, SDIO_CK=SDIOCLK/(2+2)=12 MHz
 */

#ifdef CONFIG_SDIO_DMA
#  define STM32_SDMMC_MMCXFR_CLKDIV  (1 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)
#else
#  define STM32_SDMMC_MMCXFR_CLKDIV  (2 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)
#endif

/* DMA ON:  SDIOCLK=48MHz, SDIO_CK=SDIOCLK/(1+2)=16 MHz
 * DMA OFF: SDIOCLK=48MHz, SDIO_CK=SDIOCLK/(2+2)=12 MHz
 */

#ifdef CONFIG_SDIO_DMA
#  define STM32_SDMMC_SDXFR_CLKDIV   (1 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)
#else
#  define STM32_SDMMC_SDXFR_CLKDIV   (2 << STM32_SDMMC_CLKCR_CLKDIV_SHIFT)
#endif

/* SDMMC2 Pin mapping
 *
 * D0 - PB14 or PG9
 * D1 - PB15 or PG10
 * D2 - PB3 or PG11
 * D3 - PB4 or PG12
 */
#define GPIO_SDMMC2_D0  GPIO_SDMMC2_D0_2  // PG9
#define GPIO_SDMMC2_D1  GPIO_SDMMC2_D1_2  // PG10
#define GPIO_SDMMC2_D2  GPIO_SDMMC2_D2_2  // PG11
#define GPIO_SDMMC2_D3  GPIO_SDMMC2_D3_2  // PG12

// SDCard present detection pin (CCM v2a this is CCM pin 28, PG6)
#define GPIO_MEADOW_SDIO_NCD  (GPIO_INPUT|GPIO_PULLUP|GPIO_EXTI|GPIO_PORTG|GPIO_PIN6)

/* FMC - SDRAM */

#define GPIO_FMC_SDCKE1 GPIO_FMC_SDCKE1_1
#define GPIO_FMC_SDNE1  GPIO_FMC_SDNE1_1
#define GPIO_FMC_SDNWE  GPIO_FMC_SDNWE_1

/* LCD DISPLAY
 * (work in progress as of 2017 07 19)
 */
#define	BOARD_LTDC_WIDTH        800
#define	BOARD_LTDC_HEIGHT       472

#define	BOARD_LTDC_HSYNC        10
#define	BOARD_LTDC_HFP          10
#define	BOARD_LTDC_HBP          20
#define	BOARD_LTDC_VSYNC        2
#define	BOARD_LTDC_VFP          4
#define	BOARD_LTDC_VBP          2

#define	BOARD_LTDC_GCR_PCPOL    0
#define	BOARD_LTDC_GCR_DEPOL    0
#define	BOARD_LTDC_GCR_VSPOL    0
#define	BOARD_LTDC_GCR_HSPOL    0

// Meadow feather exposes only 1 I2C bus.  It's on D07 (SDA) and D08 (SCL)
#define GPIO_I2C1_SCL  GPIO_I2C1_SCL_1
#define GPIO_I2C1_SDA  GPIO_I2C1_SDA_1
//meadow core compute has an additional I2C
#define GPIO_I2C3_SCL  GPIO_I2C3_SCL_2
#define GPIO_I2C3_SDA  GPIO_I2C3_SDA_2

//#define GPIO_I2C1_SCL   GPIO_I2C4_SCL_5
//#define GPIO_I2C1_SDA   GPIO_I2C4_SDA_4

#define CONFIG_STM32F7_I2CTIMEOSEC 0     /* 0 seconds */
#define CONFIG_STM32F7_I2CTIMEOMS  500   /* plus 500 milliseconds */

// #define	BOARD_LTDC_OUTPUT_BPP   16

//#define	BOARD_LTDC_GCR_DEN
//#define	BOARD_LTDC_GCR_DBW
//#define	BOARD_LTDC_GCR_DGW
//#define	BOARD_LTDC_GCR_DRW

#define GPIO_SPI_CS    (GPIO_OUTPUT | GPIO_PUSHPULL | GPIO_SPEED_50MHz | \
                        GPIO_OUTPUT_SET)

#define GPIO_SPI3_SCK         (GPIO_ALT|GPIO_AF6|GPIO_SPEED_50MHz|GPIO_PORTC|GPIO_PIN10)
#define GPIO_SPI3_MISO        (GPIO_ALT|GPIO_AF6|GPIO_SPEED_50MHz|GPIO_PORTC|GPIO_PIN11)
#define GPIO_SPI3_MOSI        (GPIO_ALT|GPIO_AF6|GPIO_SPEED_50MHz|GPIO_PORTB|GPIO_PIN5)

#define GPIO_SPI2_CS          (GPIO_SPI_CS | GPIO_PORTI | GPIO_PIN0)
#define GPIO_SPI2_SCK         GPIO_SPI2_SCK_4
#define GPIO_SPI2_MISO        GPIO_SPI2_MISO_3
#define GPIO_SPI2_MOSI        GPIO_SPI2_MOSI_3

/************************************************************************************
 * Public Data
 ************************************************************************************/
#ifndef __ASSEMBLY__

#undef EXTERN
#if defined(__cplusplus)
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/************************************************************************************
 * Public Function Prototypes
 ************************************************************************************/

/************************************************************************************
 * Name: stm32_boardinitialize
 *
 * Description:
 *   All STM32 architectures must provide the following entry point.  This entry point
 *   is called early in the initialization -- after all memory has been configured
 *   and mapped but before any devices have been initialized.
 *
 ************************************************************************************/

void stm32_boardinitialize(void);

#undef EXTERN
#if defined(__cplusplus)
}
#endif

#endif /* __ASSEMBLY__ */
#endif  /* __CONFIG_STM32F777ZIT6_MEADOW_INCLUDE_BOARD_H */
