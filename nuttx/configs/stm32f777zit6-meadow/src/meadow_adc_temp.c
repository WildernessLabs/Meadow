// This file is a throw away
// /Meadow/nuttx/configs/stm32f777zit6-meadow/src/meadow_adc_temp.c
#include <nuttx/config.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

#include <nuttx/arch.h>   // up_enable_irq
#include "up_arch.h"      // getreg32 & putreg32
#include <arch/stm32f7/chip.h>
#include "stm32_gpio.h"
#include "stm32_dma.h"
#include <nuttx/kthread.h>
#include "chip/stm32f74xx77xx_adc.h"
#include "chip/stm32f76xx77xx_rcc.h"
#include "chip/stm32f76xx77xx_memorymap.h"
#include <meadow/hcom_shared_common.h>
#include "hcom_nx/hcom_nx_common.h"
#include "chip/stm32f76xx77xx_dma.h"

// This example was from here (near the bottom of the page):
// https://controllerstech.com/dma-with-adc-using-registers-in-stm32/
// I converted it to remove any references to libraries not available within
// Nuttx.
// Also, This example was to return the CPU temperature not battery voltage.
// Which is, per all the documentation done by changing 2 bits in the "ADC 
// common control register" (STM32_ADC_CCR register) bits 23 and 22. 
// From Ref Man 15.13.16:
// Bit 23 TSVREFE: Temperature sensor and VREFINT enable
//   This bit is set and cleared by software to enable/disable the temperature
//   sensor and the VREFINT channel.
//      0: Temperature sensor and VREFINT channel disabled
//      1: Temperature sensor and VREFINT channel enabled
//      Note: VBATE must be disabled when TSVREFE is set. If both bits are set,
//      only the VBAT conversion is performed.
// Bit 22 VBATE: VBAT enable
//   This bit is set and cleared by software to enable/disable the VBAT channel.
//      0: VBAT channel disabled
//      1: VBAT channel enabled
// There appears to be no other action needed to switch from reading temperature
// and VREFINT to reading VBAT
void ADC_Init (void)
{
/************** STEPS TO FOLLOW *****************
1. Enable ADC and GPIO clock
2. Set the prescalar in the Common Control Register (CCR)
3. Set the Scan Mode and Resolution in the Control Register 1 (CR1)
4. Set the Continuous Conversion, EOC, and Data Alignment in Control Reg 2 (CR2)
5. Set the Sampling Time for the channels in ADC_SMPRx
6. Set the Regular channel sequence length in ADC_SQR1
7. Set the Respective GPIO PINs in the Analog Mode
************************************************/
//1. Enable ADC and GPIO clock
	// RCC->APB2ENR |= (1<<8);  // enable ADC1 clock
	// RCC->AHB1ENR |= (1<<0);  // enable GPIOA clock
  // Insure the correct ADC clock is on. If not enabled it was impossible
  // to successfully write values into some ADC configuration registers.
  uint32_t regval;

  regval = getreg32(STM32_RCC_APB2ENR);
  regval |= RCC_APB2ENR_ADC1EN;
  putreg32(regval, STM32_RCC_APB2ENR);

  // Reset all the ADCs via Reset and Clock Control (RCC). For the STM32F7
  // there is a single bit for all ADCs. Other MCUs have a bit for each ADC.
  regval = getreg32(STM32_RCC_APB2RSTR);
  regval |= RCC_APB2RSTR_ADCRST;
  putreg32(regval, STM32_RCC_APB2RSTR);

  // Restore ADC from reset state
  regval = getreg32(STM32_RCC_APB2RSTR);
  regval &= ~RCC_APB2RSTR_ADCRST;
  putreg32(regval, STM32_RCC_APB2RSTR);

  // Set the ADC watchdog high and low threshold to max and min
  putreg32(0x00000fff, STM32_ADC1_BASE + STM32_ADC_HTR_OFFSET);
  putreg32(0x00000000, STM32_ADC1_BASE + STM32_ADC_LTR_OFFSET);

//2. Set the prescalar in the Common Control Register (CCR)	
	// ADC->CCR |= 1<<16;  		 // PCLK2 divide by 4
	modifyreg32(STM32_ADC_CCR,
                ADC_CCR_ADCPRE_MASK,
                ADC_CCR_ADCPRE_DIV4);
	
//3. Set the Scan Mode and Resolution in the Control Register 1 (CR1)	
	// ADC1->CR1 = (1<<8);    // SCAN mode enabled [no]
	// ADC1->CR1 &= ~(1<<24);   // 12 bit RES
	modifyreg32(STM32_ADC1_BASE + STM32_ADC_CR1_OFFSET,
                ADC_CR1_RES_MASK,
                ADC_CR1_SCAN | ADC_CR1_RES_12BIT);
	
//4. Set the Continuous Conversion, EOC, and Data Alignment in Control Reg 2 (CR2)
	// ADC1->CR2 = (1<<1);     // enable continuous conversion mode [NO]
	// ADC1->CR2 |= (1<<10);    // EOC after each conversion
	// ADC1->CR2 &= ~(1<<11);   // Data Alignment RIGHT
	modifyreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET,
                ADC_CR2_ALIGN | ADC_CR2_CONT,
                ADC_CR2_EOCS);
	
//5. Set the Sampling Time for the channel VbaT 
	// ADC1->SMPR2 &= ~((7<<3) | (7<<12));  // Sampling time of 3 cycles for channel 1 and channel 4
	modifyreg32(STM32_ADC1_BASE + STM32_ADC_SMPR1_OFFSET, 0xf8000000, 0);

    // Set channel adc sample time
	modifyreg32(STM32_ADC1_BASE + STM32_ADC_SMPR2_OFFSET,
                ADC_SMPR1_SMP18_MASK,
                (ADC_SMPR_480 << ADC_SMPR1_SMP18_SHIFT));

//7. Set the Respective GPIO PINs in the Analog Mode [GPIOs NOT USED]
	// GPIOA->MODER |= (3<<2);  // analog mode for PA 1
	// GPIOA->MODER |= (3<<8);  // analog mode for PA 4
	
	/**************************************************************************************************/
	// Sampling Freq for Temp Sensor (See above)
	// ADC1->SMPR1 |= (7<<24);  // Sampling time of 21 us

	// Set the TSVREFE Bit to wake the sensor [NO VBAT, NOT TEMP SENSOR]
	// ADC->CCR |= (1<<23);
	modifyreg32(STM32_ADC_CCR,
                ADC_CCR_TSVREFE | ADC_CCR_VBATE,
                ADC_CCR_VBATE);
	
	// Enable DMA for ADC [NO DMA]
	// ADC1->CR2 |= (1<<8);
	
	// Enable Continuous Request [NO]
	// ADC1->CR2 |= (1<<9);
	
	// Channel Sequence (See above)
	// ADC1->SQR3 |= (1<<0);  // SEQ1 for Channel 1
	// ADC1->SQR3 |= (4<<5);  // SEQ2 for CHannel 4
	// ADC1->SQR3 |= (18<<10);  // SEQ3 for CHannel 18
	modifyreg32(STM32_ADC1_SQR3,
                ADC_SQR3_RESERVED,          // Clear
                (18 << ADC_SQR3_SQ1_SHIFT));   // ADC_IN18 - Vsense (temp or vbat);
	modifyreg32(STM32_ADC1_SQR2,
                ADC_SQR2_RESERVED,          // Clear
                0);
//6. Set the Regular channel sequence length in ADC_SQR1
	// ADC1->SQR1 |= (2<<20);   // SQR1_L =2 for 3 conversions
	modifyreg32(STM32_ADC1_SQR1,
                ADC_SQR1_RESERVED,
                0);      // Length 0 = 1 in sequence
}

void ADC_Enable (void)
{
	/************** STEPS TO FOLLOW *****************
	1. Enable the ADC by setting ADON bit in CR2
	2. Wait for ADC to stabilize (approx 10us) 
	************************************************/
	// ADC1->CR2 |= 1<<0;   // ADON =1 enable ADC1
	modifyreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET,
                0,
                ADC_CR2_ADON);

	uint32_t delay = 10000;
	while (delay--);
}

void ADC_Start (void)
{
	/************** STEPS TO FOLLOW *****************
	1. Clear the Status register
	2. Start the Conversion by Setting the SWSTART bit in CR2
	************************************************/
	
	// ADC1->SR = 0;        // clear the status register
	modifyreg32(STM32_ADC1_BASE + STM32_ADC_SR_OFFSET,
                    0X0000001f,     // Clear all bits used
                    0);

	// ADC1->CR2 |= (1<<30);  // start the conversion
	modifyreg32(STM32_ADC1_BASE + STM32_ADC_CR2_OFFSET,
                    ADC_CR2_SWSTART,
                    ADC_CR2_SWSTART);
}

int meadow_adc_start_temp_code()
{
    uint16_t RxData;
    float Vbat;
    uint32_t regval;

    syslog(1, "--> Entered meadow_adc_start_temp_code\n");
	
	ADC_Init ();
	ADC_Enable ();
	ADC_Start ();
	
	while (1)
	{
        int countTimes = 0;

        countTimes++;
        // Poll till done with conversion
        while((getreg32(STM32_ADC1_BASE + STM32_ADC_SR_OFFSET) & ADC_SR_EOC) == 0);

        RxData = getreg32(STM32_ADC1_BASE + STM32_ADC_DR_OFFSET);
        
        // Clear end of conversion bit
        regval = getreg32(STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);
        regval &= ~ADC_SR_EOC;
        putreg32(regval, STM32_ADC1_BASE + STM32_ADC_SR_OFFSET);

        // This math is for the temperture sensor
        // Vbat = (((float)(3.3*(RxData*4)/(float)4095) - 0.76) / 0.0025) + 25;

        syslog(1, "--> %03d-adc value:%u\n", countTimes, RxData);
	    usleep(2000 * 1000);

        ADC_Start ();
	}
}
