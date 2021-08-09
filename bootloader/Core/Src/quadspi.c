/**
  ******************************************************************************
  * File Name          : QUADSPI.c
  * Description        : This file provides code for the configuration
  *                      of the QUADSPI instances.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "quadspi.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

QSPI_HandleTypeDef hqspi;

/* QUADSPI init function */
void MX_QUADSPI_Init(void)
{

  hqspi.Instance = QUADSPI;
  hqspi.Init.ClockPrescaler = 1;
  hqspi.Init.FifoThreshold = 1;
  hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_NONE;
  hqspi.Init.FlashSize = 24;
  hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_1_CYCLE;
  hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
  hqspi.Init.FlashID = QSPI_FLASH_ID_1;
  hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
  if (HAL_QSPI_Init(&hqspi) != HAL_OK)
  {
    Error_Handler();
  }

}

void HAL_QSPI_MspInit(QSPI_HandleTypeDef* qspiHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(qspiHandle->Instance==QUADSPI)
  {
  /* USER CODE BEGIN QUADSPI_MspInit 0 */

  /* USER CODE END QUADSPI_MspInit 0 */
    /* QUADSPI clock enable */
    __HAL_RCC_QSPI_CLK_ENABLE();

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**QUADSPI GPIO Configuration
    PE2     ------> QUADSPI_BK1_IO2
    PB2     ------> QUADSPI_CLK
    PD13     ------> QUADSPI_BK1_IO3
    PD12     ------> QUADSPI_BK1_IO1
    PD11     ------> QUADSPI_BK1_IO0
    PB10     ------> QUADSPI_BK1_NCS
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_12|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN QUADSPI_MspInit 1 */

  /* USER CODE END QUADSPI_MspInit 1 */
  }
}

void HAL_QSPI_MspDeInit(QSPI_HandleTypeDef* qspiHandle)
{

  if(qspiHandle->Instance==QUADSPI)
  {
  /* USER CODE BEGIN QUADSPI_MspDeInit 0 */

  /* USER CODE END QUADSPI_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_QSPI_CLK_DISABLE();

    /**QUADSPI GPIO Configuration
    PE2     ------> QUADSPI_BK1_IO2
    PB2     ------> QUADSPI_CLK
    PD13     ------> QUADSPI_BK1_IO3
    PD12     ------> QUADSPI_BK1_IO1
    PD11     ------> QUADSPI_BK1_IO0
    PB10     ------> QUADSPI_BK1_NCS
    */
    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_2);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_2|GPIO_PIN_10);

    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_13|GPIO_PIN_12|GPIO_PIN_11);

  /* USER CODE BEGIN QUADSPI_MspDeInit 1 */

  /* USER CODE END QUADSPI_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

void QSPI_Get_Dev_ID(uint8_t* id_buff)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef rdid_cmd;

	// Read command settings
	rdid_cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	rdid_cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	rdid_cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	rdid_cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	rdid_cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	rdid_cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	rdid_cmd.AddressMode = QSPI_ADDRESS_NONE;
	rdid_cmd.DataMode = QSPI_DATA_1_LINE;
	rdid_cmd.DummyCycles = 0;
	rdid_cmd.NbData = 3;
	rdid_cmd.Instruction = READ_ID_CMD;

	// Initiate read and wait for the event
	result = HAL_QSPI_Command(&hqspi, &rdid_cmd, 1000);
	result = HAL_QSPI_Receive(&hqspi, id_buff, 1000);
}

void QSPI_Read_StatusRegisters(uint8_t* reg_data)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef rdid_cmd;
	uint8_t data[3];
	// Read command settings
	rdid_cmd.AddressSize = QSPI_ADDRESS_32_BITS;
	rdid_cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	rdid_cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	rdid_cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	rdid_cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	rdid_cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	rdid_cmd.AddressMode = QSPI_ADDRESS_4_LINES;
	rdid_cmd.DataMode = QSPI_DATA_4_LINES;
	rdid_cmd.DummyCycles = 10;
	rdid_cmd.NbData = 1;
	rdid_cmd.Instruction = READ_REG_CMD;


	rdid_cmd.Address = SR1V_ADDR;
	// Initiate read and wait for the event
	result = HAL_QSPI_Command(&hqspi, &rdid_cmd, 1000);
	result = HAL_QSPI_Receive(&hqspi, &data[0], 1000);

	rdid_cmd.Address = SR2V_ADDR;
	// Initiate read and wait for the event
	result = HAL_QSPI_Command(&hqspi, &rdid_cmd, 1000);
	result = HAL_QSPI_Receive(&hqspi, &data[1], 1000);

	memcpy(reg_data, data, SIZEOF(data));
}

void QSPI_Quad_Read(uint32_t start_addr, uint8_t* data_buff, uint32_t size)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef rdreg_cmd;

	// Read command settings
	rdreg_cmd.AddressSize = QSPI_ADDRESS_32_BITS;
	rdreg_cmd.Address = start_addr;
	rdreg_cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	rdreg_cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	rdreg_cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	rdreg_cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	rdreg_cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	rdreg_cmd.AddressMode = QSPI_ADDRESS_4_LINES;
	rdreg_cmd.DataMode = QSPI_DATA_4_LINES;

	//	Datasheet defines 8 dummy cycles required but we get 2 extra bytes.
	//	Setting to 10 fixes this
	rdreg_cmd.DummyCycles = 10;

	rdreg_cmd.NbData = size;
	rdreg_cmd.Instruction = QUADIO_READ;

	memset(data_buff, 0, size);
//	WRITE_REG(hqspi.Instance->DLR, (size - 1U));
	// Initiate read and wait for the event
	result = HAL_QSPI_Command(&hqspi, &rdreg_cmd, 1000);
	result = HAL_QSPI_Receive(&hqspi, data_buff, 1000);
}

void QSPI_Quad_Write_Page(uint32_t page_start_addr, uint8_t* data_buff, uint32_t size)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef qspi_cmd;

	qspi_cmd.AddressSize = QSPI_ADDRESS_32_BITS;
	qspi_cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	qspi_cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	qspi_cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	qspi_cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	qspi_cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	qspi_cmd.AddressMode = QSPI_ADDRESS_NONE;
	qspi_cmd.DummyCycles = 0;

	//	Write Enable
	qspi_cmd.NbData = 0;
	qspi_cmd.Instruction = WRITE_ENABLE_CMD;
	qspi_cmd.AddressMode = QSPI_ADDRESS_NONE;
	qspi_cmd.DataMode = QSPI_DATA_NONE;
	result = HAL_QSPI_Command(&hqspi, &qspi_cmd, 1000);

	//	Page Program
	qspi_cmd.NbData = size;
	qspi_cmd.Instruction = PAGE_PROGRAM_CMD;
	qspi_cmd.AddressMode = QSPI_ADDRESS_4_LINES;
	qspi_cmd.Address = page_start_addr;
	qspi_cmd.DataMode = QSPI_DATA_4_LINES;

	// Initiate Write
	result = HAL_QSPI_Command(&hqspi, &qspi_cmd, 1000);
	result = HAL_QSPI_Transmit(&hqspi, data_buff, 1000);

	//	Super fancy delay (0.5ns per NOP)
	for(uint32_t j = 0; j < 10000; j++)
	{
		asm("NOP");
	}
	uint8_t reg_data[2];
	//	Wait for write in progress it to clear in status register 1
	QSPI_Read_StatusRegisters(reg_data);
	while(reg_data[1] && 0x01 == 0x01)
	{
		//	Super fancy delay (0.5ns per NOP)
		for(uint32_t j = 0; j < 10000; j++)
		{
			asm("NOP");
		}
		QSPI_Read_StatusRegisters(reg_data);
	}

	//	Write Disable
	qspi_cmd.Instruction = WRITE_DISABLE_CMD;
	qspi_cmd.AddressMode = QSPI_ADDRESS_NONE;
	qspi_cmd.DataMode = QSPI_DATA_NONE;
	qspi_cmd.NbData = 0;
	result = HAL_QSPI_Command(&hqspi, &qspi_cmd, 1000);

}

void Backup_Primary_Nuttx(void)
{
	return;

}

void EraseSecondaryNuttx(void)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef qspi_cmd;

	qspi_cmd.AddressSize = QSPI_ADDRESS_32_BITS;
	qspi_cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	qspi_cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	qspi_cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	qspi_cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	qspi_cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	qspi_cmd.AddressMode = QSPI_ADDRESS_NONE;
	qspi_cmd.DataMode = QSPI_DATA_NONE;
	qspi_cmd.DummyCycles = 0;
	qspi_cmd.NbData = 0;

	uint8_t reg_data[2];
	uint32_t blocks_to_erase = 28;

	for(uint32_t index = 0; index < blocks_to_erase; index++)
	{
		//	Write Enable
		qspi_cmd.Instruction = WRITE_ENABLE_CMD;
		qspi_cmd.AddressMode = QSPI_ADDRESS_NONE;
		result = HAL_QSPI_Command(&hqspi, &qspi_cmd, 1000);

		//	Erase Blocks
		qspi_cmd.Instruction = BLOCK_ERASE_CMD;
		qspi_cmd.AddressMode = QSPI_ADDRESS_4_LINES;
		qspi_cmd.Address = (NUTTX_SEC_QSPI_LOC) +  (index * QSPI_BLOCK_SIZE);
		result = HAL_QSPI_Command(&hqspi, &qspi_cmd, 1000);

		HAL_Delay(250);
//		//	Super fancy delay (0.5ns per NOP)
//		for(uint32_t j = 0; j < 10000; j++)
//		{
//			asm("NOP");
//		}
		//	Wait for write in progress it to clear in status register 1
		QSPI_Read_StatusRegisters(reg_data);
		while(reg_data[1] && 0x01 == 0x01)
		{
			HAL_Delay(250);
//			//	Super fancy delay (0.5ns per NOP)
//			for(uint32_t j = 0; j < 10000; j++)
//			{
//				asm("NOP");
//			}
			QSPI_Read_StatusRegisters(reg_data);
		}

	}

	//	Write Disable
	qspi_cmd.Instruction = WRITE_DISABLE_CMD;
	qspi_cmd.AddressMode = QSPI_ADDRESS_NONE;
	result = HAL_QSPI_Command(&hqspi, &qspi_cmd, 1000);

}

void QSPI_Read_Config_Registers(uint8_t* data_buff)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef rdreg_cmd;
	uint8_t data[3];

	//	Command settings
	rdreg_cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	rdreg_cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	rdreg_cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	rdreg_cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	rdreg_cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	rdreg_cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	rdreg_cmd.AddressMode = QSPI_ADDRESS_NONE;
	rdreg_cmd.DataMode = QSPI_DATA_1_LINE;
	rdreg_cmd.DummyCycles = 0;
	rdreg_cmd.NbData = 1;

	//	Read Configuration Register 1
	rdreg_cmd.Instruction = READ_CONFIG_REG_1_CMD;

	result = HAL_QSPI_Command(&hqspi, &rdreg_cmd, 1000);
	result = HAL_QSPI_Receive(&hqspi, &data[0], 1000);

	//	Read Configuration Register 2
	rdreg_cmd.Instruction = READ_CONFIG_REG_2_CMD;

	result = HAL_QSPI_Command(&hqspi, &rdreg_cmd, 1000);
	result = HAL_QSPI_Receive(&hqspi, &data[1], 1000);

	//	Read Configuration Register 3
	rdreg_cmd.Instruction = READ_CONFIG_REG_3_CMD;

	result = HAL_QSPI_Command(&hqspi, &rdreg_cmd, 1000);
	result = HAL_QSPI_Receive(&hqspi, &data[2], 1000);

	memcpy(data_buff, data, SIZEOF(data));
}

void QSPI_Enable_QPI(void)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef cmd;

	//	Command settings
	cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode = QSPI_ADDRESS_NONE;
	cmd.DataMode = QSPI_DATA_NONE;
	cmd.DummyCycles = 0;
	cmd.NbData = 0;
	cmd.Instruction = ENTER_QPI_CMD;

	//	Enable QPI
	result = HAL_QSPI_Command(&hqspi, &cmd, 1000);
}

void QSPI_Disable_QPI(void)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef cmd;

	//	Command settings
	cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	cmd.AddressMode = QSPI_ADDRESS_NONE;
	cmd.DataMode = QSPI_DATA_NONE;
	cmd.DummyCycles = 0;
	cmd.NbData = 0;

	if(board_version == 1)
	{
		cmd.Instruction = EXIT_QPI_CMD;
	}
	else if(board_version == 2)
	{
		cmd.Instruction = WINBOND_EXIT_QPI_CMD;
	}

	//	Disable QPI
	result = HAL_QSPI_Command(&hqspi, &cmd, 1000);
}

void QSPI_Enable_4Byte_Addressing(void)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef cmd;

	//	Command settings
	cmd.AddressSize = QSPI_ADDRESS_24_BITS;
	cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	cmd.AddressMode = QSPI_ADDRESS_NONE;
	cmd.DataMode = QSPI_DATA_NONE;
	cmd.DummyCycles = 0;
	cmd.NbData = 0;
	cmd.Instruction = ENTER_4_BYTE_ADDR_CMD;

	//	Enable 4Byte Addressing
	result = HAL_QSPI_Command(&hqspi, &cmd, 1000);
}

void QSPI_Disable_4Byte_Addressing(void)
{
	HAL_StatusTypeDef result = HAL_ERROR;
	QSPI_CommandTypeDef cmd;

	//	Command settings
	cmd.AddressSize = QSPI_ADDRESS_32_BITS;
	cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DdrMode = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
	cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
	cmd.AddressMode = QSPI_ADDRESS_NONE;
	cmd.DataMode = QSPI_DATA_NONE;
	cmd.DummyCycles = 0;
	cmd.NbData = 0;
	cmd.Instruction = EXIT_4_BYTE_ADDR_CMD;

	//	Enable 4Byte Addressing
	result = HAL_QSPI_Command(&hqspi, &cmd, 1000);
}

/* USER CODE END 1 */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
