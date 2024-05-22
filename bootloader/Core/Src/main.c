/* USER CODE BEGIN Header */
/**
	******************************************************************************
	* @file           : main.c
	* @brief          : Main program body
	******************************************************************************
	* @attention
	*
	* <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
	* All rights reserved.</center></h2>
	*
	* This software component is licensed by ST under BSD 3-Clause license,
	* the "License"; You may not use this file except in compliance with the
	* License. You may obtain a copy of the License at:
	*                        opensource.org/licenses/BSD-3-Clause
	*
	******************************************************************************
	*/
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "bootloader.h"
#include "crc.h"
#include "quadspi.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t board_version = 0;
uint8_t bootloader_status = bootloader_no_op;
uint32_t resetReason = 0x0a0a;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void BootMeadowOS(void);
void ClearOTAFlag(uint8_t flag);
void WriteNuttxPrimaryBlock(uint32_t block, uint32_t* data_block, uint32_t block_size);
void SetOTAFlagState(uint8_t flag, uint8_t state);
void ErasePrimaryNuttx(void);
void PerformUpdate(void);
void PerformRollback(void);
void BackupPrimaryImage(void);
uint8_t VerifyPrimaryImage(void);
uint8_t VerifySecondaryImage(void);
void CheckPreviousOperationFailure(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  //
  //	Grab the RCC clock control and status register contents and save
  //	them for later.
  //
  resetReason = RCC->CSR;
  //
  //	Clear the reset status register as otherwise the bits can hang around.
  //
  __HAL_RCC_CLEAR_RESET_FLAGS();

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();

#ifdef ENABLE_BL_CDC
  MX_USB_DEVICE_Init();
#endif
#ifdef ENABLE_BL_UART
  MX_UART4_Init();
#endif
  MX_QUADSPI_Init();
  MX_FMC_Init();
  MX_CRC_Init();
  /* USER CODE BEGIN 2 */

  //	Turn off onboard LEDs
    HAL_GPIO_WritePin(OnboardLedGreen_GPIO_Port, OnboardLedGreen_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(OnboardLedBlue_GPIO_Port, OnboardLedBlue_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(OnboardLedRed_GPIO_Port, OnboardLedRed_Pin, GPIO_PIN_SET);

	//Turn on Green LED to indicate BL in operation
	HAL_GPIO_WritePin(OnboardLedGreen_GPIO_Port, OnboardLedGreen_Pin, GPIO_PIN_RESET);

	LogConsole(INIT_MSG, SIZEOF(INIT_MSG));
	//	CONFIG & VERIFY FMC

	FMC_SDRAM_CommandTypeDef clk_en_cmd;
	clk_en_cmd.CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
	clk_en_cmd.CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
	clk_en_cmd.AutoRefreshNumber = 3;
	clk_en_cmd.ModeRegisterDefinition = 32;

	HAL_SDRAM_SendCommand(&hsdram1, &clk_en_cmd, 0xFFFF);
	HAL_Delay(50);
	clk_en_cmd.CommandMode = FMC_SDRAM_CMD_PALL;
	HAL_SDRAM_SendCommand(&hsdram1, &clk_en_cmd, 0xFFFF);
	HAL_Delay(10);
	clk_en_cmd.CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
	HAL_SDRAM_SendCommand(&hsdram1, &clk_en_cmd, 0xFFFF);
	HAL_Delay(10);
	clk_en_cmd.CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
	HAL_SDRAM_SendCommand(&hsdram1, &clk_en_cmd, 0xFFFF);
	HAL_Delay(10);
	FMC_SDRAM_ProgramRefreshRate(hsdram1.Instance, 683);
	HAL_SDRAM_WriteProtection_Disable(&hsdram1);

	HAL_Delay(1);

	//	CONFIG & VERIFY QSPI
	QSPI_Disable_QPI();
	QSPI_Disable_4Byte_Addressing();

	uint32_t qspi_jedec_id = 0;
	QSPI_Get_Dev_ID(&qspi_jedec_id);

	if(qspi_jedec_id == QSPI_FLASH_SPANSION_S25FL256L)
	{
		//	v1 board
		LogConsole(VERIFY_QSPI_SUCCESS_MSG_, SIZEOF(VERIFY_QSPI_SUCCESS_MSG_));
		LogConsole(V1_BOARD_DETECT_MSG, SIZEOF(V1_BOARD_DETECT_MSG));

		board_version = 1;
	}
	else if(qspi_jedec_id == QSPI_FLASH_WINBOND_W25Q512JVxxQ || \
			qspi_jedec_id == QSPI_FLASH_WINBOND_W25Q512JVxxM)
	{
		//	v2 board
		LogConsole(VERIFY_QSPI_SUCCESS_MSG_, SIZEOF(VERIFY_QSPI_SUCCESS_MSG_));
		LogConsole(V2_BOARD_DETECT_MSG, SIZEOF(V2_BOARD_DETECT_MSG));

		board_version = 2;
	}
	else
	{
		LogConsole(VERIFY_QSPI_FAIL_MSG, SIZEOF(VERIFY_QSPI_FAIL_MSG));
	}

	QSPI_Enable_QPI();
	QSPI_Enable_4Byte_Addressing();

	CheckPreviousOperationFailure();

#ifdef WAIT_FOR_HOST_COMMS
  	HAL_Delay(100);
	uint8_t data_buff[10];
	while(1)
	{
		memset(data_buff, 0, SIZEOF(data_buff));
		HAL_Delay(100);
#ifdef ENABLE_BL_CDC
		USBD_CDC_SetRxBuffer(&hUsbDeviceFS, &data_buff[0]);
		USBD_CDC_ReceivePacket(&hUsbDeviceFS);
#endif
		if(data_buff[0] == 0x73)	//'s' Character - Continue normal Bootloader Sequence
		{
			break;
		}
		else if((data_buff[0] == 0x63))	//'c' Characters - Verify Image
		{
			if(data_buff[1] == 0x31)	//'c1' - Verify Primary Image
			{
				if(VerifyPrimaryImage())
				{
					LogConsole(PRIMARY_IMG_VERIFY_SUCCESS_MSG, SIZEOF(PRIMARY_IMG_VERIFY_SUCCESS_MSG));
				}
				else
				{
					LogConsole(PRIMARY_IMG_VERIFY_FAIL_MSG, SIZEOF(PRIMARY_IMG_VERIFY_FAIL_MSG));
				}
			}
			else if(data_buff[1] == 0x32)	//'c2' - Verify Secondary Image
			{
				if(VerifySecondaryImage())
				{
					LogConsole(SECONDARY_IMG_VERIFY_SUCCESS_MSG, SIZEOF(SECONDARY_IMG_VERIFY_SUCCESS_MSG));
				}
				else
				{
					LogConsole(SECONDARY_IMG_VERIFY_FAIL_MSG, SIZEOF(SECONDARY_IMG_VERIFY_FAIL_MSG));
				}
			}

		}
		else if((data_buff[0] == 0x65))	//'e' Characters - Erase Image
		{
			if(data_buff[1] == 0x31)	//'e1' - Erase Primary Image
			{
				LogConsole(ERASE_PRIMARY_START_MSG, SIZEOF(ERASE_PRIMARY_START_MSG));
				ErasePrimaryNuttx();
				LogConsole(ERASE_PRIMARY_COMPLETE_MSG, SIZEOF(ERASE_PRIMARY_COMPLETE_MSG));

			}
			else if(data_buff[1] == 0x32)	//'e2' - Erase Secondary Image
			{
				LogConsole(ERASE_SECONDARY_START_MSG, SIZEOF(ERASE_SECONDARY_START_MSG));
				EraseSecondaryNuttx();
				LogConsole(ERASE_SECONDARY_COMPLETE_MSG, SIZEOF(ERASE_SECONDARY_COMPLETE_MSG));
			}

		}
		else if(data_buff[0] == 0x75)	//'u' Character - Perform Update
		{
			PerformUpdate();
		}
		else if(data_buff[0] == 0x72)	//'r' Character - Perform Rollback
		{
			PerformRollback();
		}
		else if(data_buff[0] == 0x62)	//'b' Character - Perform Image Backup
		{
			BackupPrimaryImage();
		}
		else if(data_buff[0] == 0x66)	//'f' Character - Get OTA flags
		{
			PrintOtaFlags();
		}
		else if((data_buff[0] == 0x71))	//'q' Character - Read QSPI status reg
		{
			if((data_buff[1] == 0x31))	// '1' Character - Read status reg 1
			{
				uint8_t data = 0;
				QSPI_Read_StatusRegisterOne(&data);
				char data_char[5];
				memset(data_char, 0 , SIZEOF(data_char));
				sprintf(data_char, "%d", data);
				LogConsole("Status Reg 1: ", SIZEOF("Status Reg 1: "));
				LogConsole(data_char, SIZEOF(data_char));
				LogConsole("\r\n", SIZEOF("\r\n"));
			}
		}
		else if((data_buff[0] == 0x74))	//'t' Character - Test commands
		{
			if((data_buff[1] == 0x31))	// '1' Character - Test command 1
			{
				uint8_t mono_img_header[8];
				QSPI_Quad_Read(QSPI_FLASH_LOC_INTERNAL, mono_img_header, SIZEOF(mono_img_header));

				char mono_version_msg[60];
				memset(mono_version_msg, 0 , SIZEOF(mono_version_msg));
				sprintf(mono_version_msg, "Found signature %02X%02X%02X%02X, Mono Version %u.%u.%u.%u\r\n",	\
				mono_img_header[0],	mono_img_header[1], mono_img_header[2], mono_img_header[3], \
				mono_img_header[7],mono_img_header[6],mono_img_header[5],mono_img_header[4]);

				LogConsole(mono_version_msg, SIZEOF(mono_version_msg));
				
			}
			if((data_buff[1] == 0x32))	// '2' Character - Test command 2
			{
				uint8_t data_buff[8];
				QSPI_Quad_Read(NUTTX_SEC_QSPI_LOC, data_buff, SIZEOF(data_buff));

				char data_buff_str[60];
				memset(data_buff_str, 0 , SIZEOF(data_buff_str));
				sprintf(data_buff_str, "Found data %02X%02X%02X%02X%02X%02X%02X%02X\r\n",	\
				data_buff[0],	data_buff[1], data_buff[2], data_buff[3], \
				data_buff[4],data_buff[5],data_buff[6],data_buff[7]);

				LogConsole(data_buff_str, SIZEOF(data_buff_str));
				
			}
		}
		else if(data_buff[0] == 0x68)	//'h' Character - Display help message
		{
			LogConsole(CONSOLE_HELP_MSG, SIZEOF(CONSOLE_HELP_MSG));
		}
		else if((data_buff[0] == 0x76) && (data_buff[1] == 0x3F))	//'v?' Characters - Return the Bootloader Version
		{
			PrintVersion();

			//!<TODO:	Test Section Start
				char dev_id[9];
				memset(dev_id, 0 , SIZEOF(dev_id));

				sprintf(&dev_id[0], "%08lX", qspi_jedec_id);
				LogConsole("QSPI ID: ", SIZEOF("QSPI ID: "));
				LogConsole(dev_id, SIZEOF(dev_id));
				LogConsole("\r\n", SIZEOF("\r\n"));

				//	Test Section end
		}
	}


#endif

	//	UPDATE CHECK STAGE#
	//	If there is an update pending
	if(getOTAFlagState(update_flag) == update_nuttx_pending)
	{
		if(VerifySecondaryImage())
		{
			PerformUpdate();
		}
		else
		{
			SetOTAFlagState(update_failure_flag, update_fail_invalid_image);
		}
	}
	//	If previous update operation failed, perform recovery.
	else if(getOTAFlagState(update_flag) == update_nuttx_failed)
	{
		if(getOTAFlagState(update_failure_flag) == update_fail_stage_one)
		{
			if(VerifySecondaryImage())
			{
				PerformUpdate();
			}
			else
			{
				SetOTAFlagState(update_failure_flag, update_fail_invalid_image);
			}
		}
		else if(getOTAFlagState(update_failure_flag) == update_fail_stage_three)
		{
			if(VerifySecondaryImage())
			{
				PerformRollback();
				
			}
		}
		
	} 

	//	ROLLBACK CHECK STAGE
	//	Rollback process takes approx 5s.
	else if(getOTAFlagState(rollback_flag) == rollback_nuttx_pending || \
				getOTAFlagState(rollback_flag) == rollback_nuttx_failed)
	{
		if(VerifySecondaryImage())
		{
			PerformRollback();
		}
		else
		{
			SetOTAFlagState(rollback_failure_flag, rollback_fail_invalid_image);
		}
	}
	else if(getOTAFlagState(backup_flag) == backup_nuttx_pending || \
				getOTAFlagState(backup_flag) == backup_nuttx_failed)
	{
		//!< TODO: At this point, bootloader doesn't check or care about primary image. Is check needed?
		BackupPrimaryImage();
	}

	//	BOOT STAGE
	//	This performs CRC check on primary image and compares it to crc result stored in last 4 bytes during build process.
	//	If CRC passes, then boot; otherwise panic (or rollback if rollback_on_fail_flag is set to enabled).
	LogConsole(NUTTX_IMG_CHK_MSG, SIZEOF(NUTTX_IMG_CHK_MSG));

	if(VerifyPrimaryImage())
	{
		LogConsole(PRIMARY_IMG_VERIFY_SUCCESS_MSG, SIZEOF(PRIMARY_IMG_VERIFY_SUCCESS_MSG));
		BootMeadowOS();
	}
	else
	{
		LogConsole(PRIMARY_IMG_VERIFY_FAIL_MSG, SIZEOF(PRIMARY_IMG_VERIFY_FAIL_MSG));

		if(getOTAFlagState(rollback_on_fail_flag) == rollback_on_fail_enabled)
		{
			PerformRollback();

			if(VerifyPrimaryImage())
			{
				LogConsole(PRIMARY_IMG_VERIFY_SUCCESS_MSG, SIZEOF(PRIMARY_IMG_VERIFY_SUCCESS_MSG));
				BootMeadowOS();
			}
			else
			{
				//Turn off Green LED and blink Blue LED to indicate image check fail.
				LogConsole(PRIMARY_IMG_VERIFY_FAIL_MSG, SIZEOF(PRIMARY_IMG_VERIFY_FAIL_MSG));
				HAL_GPIO_WritePin(OnboardLedGreen_GPIO_Port, OnboardLedGreen_Pin, GPIO_PIN_SET);
				while(1)
				{
					HAL_GPIO_TogglePin(OnboardLedRed_GPIO_Port, OnboardLedBlue_Pin);
					HAL_Delay(250);
				}
			}
		}
		else
		{
			//Turn off Green LED and blink Blue LED to indicate image check fail.
			HAL_GPIO_WritePin(OnboardLedGreen_GPIO_Port, OnboardLedGreen_Pin, GPIO_PIN_SET);
			while(1)
			{
				HAL_GPIO_TogglePin(OnboardLedRed_GPIO_Port, OnboardLedBlue_Pin);
				HAL_Delay(3000);
			}
		}
	}

	//	This point should never be reached by the bootloader.
	while(1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 384;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 8;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK)
  {
    Error_Handler();
  }
  
#ifdef ENABLE_BL_UART
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_UART4|RCC_PERIPHCLK_CLK48;
  PeriphClkInitStruct.Uart4ClockSelection = RCC_UART4CLKSOURCE_PCLK1;
#else
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
#endif
  PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

//static void MX_GPIO_DeInit(void)
//{
//  GPIO_InitTypeDef GPIO_InitStruct = {0};
//
//  /* GPIO Ports Clock Enable */
//  __HAL_RCC_GPIOA_CLK_ENABLE();
//
//  /*Configure GPIO pins : OnboardLedGreen_Pin OnboardLedBlue_Pin OnboardLedRed_Pin */
//  HAL_GPIO_DeInit(GPIOA, OnboardLedGreen_Pin);
//  HAL_GPIO_DeInit(GPIOA, OnboardLedBlue_Pin);
//  HAL_GPIO_DeInit(GPIOA, OnboardLedRed_Pin);
//  __HAL_RCC_GPIOA_CLK_DISABLE();
//
//
//}
void NVIC_DeInit(void)
{
	uint8_t tmp;

	/* Disable all interrupts */
	NVIC->ICER[0] = 0xFFFFFFFF;
	NVIC->ICER[1] = 0x00000001;
	/* Clear all pending interrupts */
	NVIC->ICPR[0] = 0xFFFFFFFF;
	NVIC->ICPR[1] = 0x00000001;

	/* Clear all interrupt priority */
	for (tmp = 0; tmp < 32; tmp++)
	{
		NVIC->IP[tmp] = 0x00;
	}
}

void NVIC_SCBDeInit(void)
{
    uint8_t tmp;

    SCB->ICSR = 0x0A000000;
    SCB->VTOR = 0x08040000;
    SCB->AIRCR = 0x05FA0000;
    SCB->SCR = 0x00000000;
    SCB->CCR = 0x00000000;

    for (tmp = 0; tmp < 32; tmp++) {
        SCB->SHPR[tmp] = 0x00;
    }

    SCB->SHCSR = 0x00000000;
    SCB->CFSR = 0xFFFFFFFF;
    SCB->HFSR = 0xFFFFFFFF;
    SCB->DFSR = 0xFFFFFFFF;
}

void BootMeadowOS(void)
{
	LogConsole(JUMP_MSG, SIZEOF(JUMP_MSG));
	uint32_t i=0;
	void (*JumpOS)(void);

	//	De-Init anything that uses HAL here before Systick timer Disabled
#ifdef ENABLE_BL_UART
	HAL_UART_DeInit(&huart4);
#endif
#ifdef ENABLE_BL_CDC
	USBD_DeInit(&hUsbDeviceFS);
#endif
	QSPI_Disable_4Byte_Addressing();
	QSPI_Disable_QPI();
	HAL_QSPI_DeInit(&hqspi);

	//Turn off Green LED to indicate exiting BL
	HAL_GPIO_WritePin(OnboardLedGreen_GPIO_Port, OnboardLedGreen_Pin, GPIO_PIN_SET);

	//	Reset RCC clock Config to default state
	HAL_RCC_DeInit();

	// Disable all interrupts
	__disable_irq();

	// Disable Systick timer
	SysTick->CTRL = 0;

	// Clear Interrupt Enable Register & Interrupt Pending Register
	for (i=0;i<8;i++)
	{
		NVIC->ICER[i]=0xFFFFFFFF;
		NVIC->ICPR[i]=0xFFFFFFFF;
	}

	__DSB();
	__ISB();

	// Re-enable all interrupts
	__enable_irq();

	// Set up the jump to Nuttx address + 4 (entry point).
	JumpOS = (void (*)(void)) (*((uint32_t *) ((NUTTX_PRI_LOC + 4))));
	SCB->VTOR = NUTTX_PRI_LOC;

	// Set the main stack pointer to the Nuttx stack
	__set_MSP(*(uint32_t *)NUTTX_PRI_LOC);

	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL  = 0;

	//
	//	Write the reset reason into battery backed register 30 ready
	//	for the OS to pick up when it starts.
	//
	HAL_PWR_EnableBkUpAccess();
	*((uint32_t *) 0x400028c8) = resetReason;

    HAL_DeInit();

    // Perform Jump
	JumpOS();

	while (1)
	{
	// Code should never reach this loop
	}
}

void ErasePrimaryNuttx(void)
{
	HAL_FLASH_Unlock();

	FLASH_Erase_Sector(FLASH_SECTOR_5, FLASH_VOLTAGE_RANGE_3);
	FLASH_Erase_Sector(FLASH_SECTOR_6, FLASH_VOLTAGE_RANGE_3);
	FLASH_Erase_Sector(FLASH_SECTOR_7, FLASH_VOLTAGE_RANGE_3);
	FLASH_Erase_Sector(FLASH_SECTOR_8, FLASH_VOLTAGE_RANGE_3);
	FLASH_Erase_Sector(FLASH_SECTOR_9, FLASH_VOLTAGE_RANGE_3);
	FLASH_Erase_Sector(FLASH_SECTOR_10, FLASH_VOLTAGE_RANGE_3);
	FLASH_Erase_Sector(FLASH_SECTOR_11, FLASH_VOLTAGE_RANGE_3);

	HAL_FLASH_Lock();
}

//	Write Primary nuttx block of 256Kbytes
void WriteNuttxPrimaryBlock(uint32_t block, uint32_t* data_block, uint32_t block_size)
{
	HAL_FLASH_Unlock();

	uint32_t words_to_flash = (block_size / 4);	//	256K / 4bytes = 0x10000
	uint32_t index = 0;
	while(index < words_to_flash)
	{
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, (NUTTX_PRI_LOC + (block * 0x40000) + (index*4)), *((uint32_t*)(data_block + index)));
		index++;
	}

	HAL_FLASH_Lock();
}

void setOTAData(uint8_t *buf)
{
	uint8_t *zeroes = calloc (QSPI_PAGE_SIZE, 1);
	QSPI_Quad_Write_Page(OTA_DATA_LOC, zeroes, QSPI_PAGE_SIZE);
	QSPI_Quad_Write_Page(OTA_DATA_LOC, buf, QSPI_PAGE_SIZE);
	free(zeroes);
}

uint8_t * getOTAData()
{
	uint8_t *data_buf = calloc (QSPI_PAGE_SIZE, 1);
	QSPI_Quad_Read(OTA_DATA_LOC, data_buf, QSPI_PAGE_SIZE);
	return data_buf;
}

void SetOTAFlagState(uint8_t flag, uint8_t state)
{
	uint8_t *ota_state = getOTAData();
	memset((ota_state + flag), state, 1);
	setOTAData(ota_state);

	free(ota_state);
}

uint8_t getOTAFlagState(uint8_t flag)
{
	uint8_t *ota_state = getOTAData();
	uint8_t val = *(ota_state + flag);
	free(ota_state);
	return val;
}

void ClearOTAFlag(uint8_t flag)
{
	uint8_t *ota_state = getOTAData();
	memset((ota_state + flag), 0, 1);
	setOTAData(ota_state);
	free(ota_state);
}

void PerformUpdate(void)
{
	bootloader_status = bootloader_update;
	LogConsole(UPDATE_START_MSG, SIZEOF(UPDATE_START_MSG));
	HAL_GPIO_WritePin(OnboardLedBlue_GPIO_Port, OnboardLedBlue_Pin, GPIO_PIN_RESET);
	SetOTAFlagState(update_flag, update_nuttx_in_progress);
	SetOTAFlagState(update_failure_flag, update_fail_stage_one);
	//	Stage 1 of 3: Copy nuttx kernel and user update from NUTTX_SEC_QSPI_LOC into SDRAM
	//	This roughly takes 2s
	LogConsole(UPDATE_STAGE_1_START_MSG, SIZEOF(UPDATE_STAGE_1_START_MSG));

	//	Create 256K block in internal RAM for copy operations
	uint8_t *data_buff;
	data_buff = malloc(IO_BLOCK_SIZE);

	for(uint8_t i = 0; i < (NUTTX_SIZE/IO_BLOCK_SIZE); i++)
	{
		memset(data_buff, 0 , IO_BLOCK_SIZE);
		QSPI_Quad_Read((NUTTX_SEC_QSPI_LOC) + (i*IO_BLOCK_SIZE), data_buff, IO_BLOCK_SIZE);
		memcpy((uint32_t*)(SDRAM_LOC  + (i*IO_BLOCK_SIZE)), data_buff, IO_BLOCK_SIZE);
	}

	//	Destroy 256K block
	free(data_buff);

	//	Stage 2 of 3: Copy nuttx kernel and user current from Primary Location into NUTTX_SEC_QSPI_LOC
	//	This roughly takes 15s

	LogConsole(UPDATE_STAGE_2_START_MSG, SIZEOF(UPDATE_STAGE_2_START_MSG));
	SetOTAFlagState(update_failure_flag, update_fail_stage_two);
	EraseSecondaryNuttx();

	for(uint32_t i = 0; i < (NUTTX_SIZE/QSPI_PAGE_SIZE); i++)
	{
		QSPI_Quad_Write_Page((NUTTX_SEC_QSPI_LOC) + (i * QSPI_PAGE_SIZE), (uint8_t*)(NUTTX_PRI_LOC + (QSPI_PAGE_SIZE * i)), QSPI_PAGE_SIZE);
	}

	//	Stage 3 of 3: Copy nuttx kernel and user update from SDRAM into Primary Location
	//	This roughly takes 15s

	//	Flash erase disables the blink interrupt. Keep Blue LED on to avoid user confusion
	HAL_GPIO_WritePin(OnboardLedBlue_GPIO_Port, OnboardLedBlue_Pin, GPIO_PIN_RESET);
	LogConsole(UPDATE_STAGE_3_START_MSG, SIZEOF(UPDATE_STAGE_3_START_MSG));
	SetOTAFlagState(update_failure_flag, update_fail_stage_three);
	
	ErasePrimaryNuttx();

	for(uint8_t i = 0; i < (NUTTX_SIZE/IO_BLOCK_SIZE); i++)
	{
		WriteNuttxPrimaryBlock(i, (uint32_t*)(SDRAM_LOC  + (i*IO_BLOCK_SIZE)), IO_BLOCK_SIZE);
	}

	SetOTAFlagState(update_flag, update_nuttx_complete);
	SetOTAFlagState(update_failure_flag, update_fail_none);
	HAL_GPIO_WritePin(OnboardLedBlue_GPIO_Port, OnboardLedBlue_Pin, GPIO_PIN_SET);
	LogConsole(UPDATE_COMPLETE_MSG, SIZEOF(UPDATE_COMPLETE_MSG));
	bootloader_status = bootloader_no_op;
}

void PerformRollback(void)
{
	//!<TODO:	Set a ROLLBACK_INTERRUPTED flag that will be cleared at the end of the rollback progress (right before rollback_nuttx_pending is cleared)
	bootloader_status = bootloader_rollback;
	LogConsole(ROLLBACK_START_MSG, SIZEOF(ROLLBACK_START_MSG));
	HAL_GPIO_WritePin(OnboardLedRed_GPIO_Port, OnboardLedRed_Pin, GPIO_PIN_RESET);
	SetOTAFlagState(rollback_flag, rollback_nuttx_in_progress);
	SetOTAFlagState(rollback_failure_flag, rollback_fail);
	//	Stage 1 of 1: Copy nuttx kernel and user previous from Secondary Location into Primary Location.

	//	Create 256K block in internal RAM for copy operations
	uint8_t *data_buff;
	data_buff = malloc(IO_BLOCK_SIZE);

	//	Flash erase disables the blink interrupt. Keep Red LED on to avoid user confusion
	HAL_GPIO_WritePin(OnboardLedRed_GPIO_Port, OnboardLedRed_Pin, GPIO_PIN_RESET);
	ErasePrimaryNuttx();
	for(uint8_t i = 0; i < (NUTTX_SIZE/IO_BLOCK_SIZE); i++)
	{
		memset(data_buff, 0 , IO_BLOCK_SIZE);
		QSPI_Quad_Read((NUTTX_SEC_QSPI_LOC) + (i*IO_BLOCK_SIZE), data_buff, IO_BLOCK_SIZE);
		WriteNuttxPrimaryBlock(i, (uint32_t*)data_buff, IO_BLOCK_SIZE);
	}

	//	Destroy 256K block
	free(data_buff);

	SetOTAFlagState(rollback_flag, rollback_nuttx_complete);
	SetOTAFlagState(rollback_failure_flag, rollback_fail_none);
	LogConsole(ROLLBACK_COMPLETE_MSG, SIZEOF(ROLLBACK_COMPLETE_MSG));
	HAL_GPIO_WritePin(OnboardLedRed_GPIO_Port, OnboardLedRed_Pin, GPIO_PIN_SET);
	bootloader_status = bootloader_no_op;
}

void BackupPrimaryImage(void)
{
	bootloader_status = bootloader_backup;
	LogConsole(BACKUP_START_MSG, SIZEOF(BACKUP_START_MSG));
	HAL_GPIO_WritePin(OnboardLedBlue_GPIO_Port, OnboardLedBlue_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(OnboardLedRed_GPIO_Port, OnboardLedRed_Pin, GPIO_PIN_RESET);
	SetOTAFlagState(backup_flag, backup_nuttx_in_progress);
	SetOTAFlagState(backup_failure_flag, backup_fail);
	//	Stage 1 of 1: Copy nuttx kernel and user current from Primary Location into NUTTX_SEC_QSPI_LOC	
	EraseSecondaryNuttx();

	for(uint32_t i = 0; i < (NUTTX_SIZE/QSPI_PAGE_SIZE); i++)
	{
		QSPI_Quad_Write_Page((NUTTX_SEC_QSPI_LOC) + (i * QSPI_PAGE_SIZE), (uint8_t*)(NUTTX_PRI_LOC + (QSPI_PAGE_SIZE * i)), QSPI_PAGE_SIZE);
	}
	HAL_GPIO_WritePin(OnboardLedBlue_GPIO_Port, OnboardLedBlue_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(OnboardLedRed_GPIO_Port, OnboardLedRed_Pin, GPIO_PIN_SET);
	SetOTAFlagState(backup_flag, backup_nuttx_complete);
	SetOTAFlagState(backup_failure_flag, backup_fail_none);
	LogConsole(BACKUP_COMPLETE_MSG, SIZEOF(BACKUP_COMPLETE_MSG));
	bootloader_status = bootloader_no_op;
}

uint8_t VerifyPrimaryImage(void)
{
	uint32_t primary_crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)NUTTX_PRI_LOC, 0x6FFFF);	//Size in bytes
	if(primary_crc == *(uint32_t*)NUTTX_PRI_CRC_LOC)
	{
		return crc_pass;
	}
	else
	{
		return crc_fail;
	}

}

uint8_t VerifySecondaryImage(void)
{
	//	Create 256K block in internal RAM for copy operations
	uint8_t *data_buff;
	data_buff = malloc(IO_BLOCK_SIZE);

	//	Buffer secondary image from QSPI to internal SDRAM in 256K chunks
	for(uint8_t i = 0; i < (NUTTX_SIZE/IO_BLOCK_SIZE); i++)
	{
		memset(data_buff, 0 , IO_BLOCK_SIZE);
		QSPI_Quad_Read((NUTTX_SEC_QSPI_LOC) + (i*IO_BLOCK_SIZE), data_buff, IO_BLOCK_SIZE);
		memcpy((uint32_t*)(SDRAM_LOC  + (i*IO_BLOCK_SIZE)), data_buff, IO_BLOCK_SIZE);
	}

	//	Destroy 256K block
	free(data_buff);

	uint32_t secondary_crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)SDRAM_LOC, 0x6FFFF);	//Size in bytes
	if(secondary_crc == *(uint32_t*)(SDRAM_LOC + NUTTX_SIZE - 4))
	{
		return crc_pass;
	}
	else
	{
		return crc_fail;
	}
}

void CheckPreviousOperationFailure(void)
{
	if(getOTAFlagState(update_failure_flag) != update_fail_none)
	{
		SetOTAFlagState(update_flag, update_nuttx_failed);
	}
	if(getOTAFlagState(rollback_failure_flag) != rollback_fail_none)
	{
		SetOTAFlagState(rollback_flag, rollback_nuttx_failed);
	}
	if(getOTAFlagState(backup_failure_flag) != backup_fail_none)
	{
		SetOTAFlagState(backup_flag, backup_nuttx_failed);
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	 tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
