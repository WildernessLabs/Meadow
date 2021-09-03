/**
  ******************************************************************************
  * File Name          : QUADSPI.h
  * Description        : This file provides code for the configuration
  *                      of the QUADSPI instances.
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
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __quadspi_H
#define __quadspi_H
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern QSPI_HandleTypeDef hqspi;

/* USER CODE BEGIN Private defines */

// The following define the Manufacture, type and capacity of the flash
// memory chips used on the following models of the Meadow F7 Micro.
// F7v1 uses a spansion flash
// Spansion id 0x01, type 0x60, capacity 19 (256 bytes)
#define QSPI_FLASH_SPANSION_S25FL256L    (0x00016019)

// F7v2 uses a winbond 512 flash
// Winbond id 0xef, type 0x40 (Q) or 0x70 (M), capacity 20 (512 bytes)
#define QSPI_FLASH_WINBOND_W25Q512JVxxQ  (0x00EF4020) // 'Q' version, default QE = 1
#define QSPI_FLASH_WINBOND_W25Q512JVxxM  (0x00EF7020) // 'M' version, default QE = 0

#define CR2NV_ADDR 0x000003

#define SR1V_ADDR 0x800000
#define SR2V_ADDR 0x800001

#define READ_ID_CMD		0x9F
#define READ_STATUS_REGISTER_1 0x05
#define ENTER_QPI_CMD 	0x38
#define EXIT_QPI_CMD 	0xF5
#define WINBOND_EXIT_QPI_CMD 0xFF
#define ENTER_4_BYTE_ADDR_CMD 	0xB7
#define EXIT_4_BYTE_ADDR_CMD 	0xE9
#define READ_REG_CMD	0x65
#define READ_CONFIG_REG_1_CMD 0x35
#define READ_CONFIG_REG_2_CMD 0x15
#define READ_CONFIG_REG_3_CMD 0x33
#define QUADIO_FAST_READ 0x0C
#define QUADIO_READ 0xEC
#define WINBOND_FAST_READ_QPI_CMD 0xEB

#define WRITE_ENABLE_CMD 0x06
#define WRITE_DISABLE_CMD 0x04
#define PAGE_PROGRAM_CMD 0x02
#define SECTOR_ERASE_CMD 0x21
#define BLOCK_ERASE_CMD 0xDC


#define QSPI_PAGE_SIZE 0x100	//	256 Bytes
#define QSPI_SECTOR_SIZE 0x1000	//	4 kBytes
#define QSPI_BLOCK_SIZE 0x10000	//	64 kBytes

/* USER CODE END Private defines */

void MX_QUADSPI_Init(void);

/* USER CODE BEGIN Prototypes */

void QSPI_Get_Dev_ID(uint32_t* jedec_id);
void QSPI_Quad_Read(uint32_t start_addr, uint8_t* data_buff, uint32_t size);
void QSPI_Read_Config_Registers(uint8_t* data_buff);
void QSPI_Enable_QPI(void);
void QSPI_Disable_QPI(void);
void QSPI_Enable_4Byte_Addressing(void);
void QSPI_Disable_4Byte_Addressing(void);
void Backup_Primary_Nuttx(void);
void EraseSecondaryNuttx(void);
void QSPI_Quad_Write_Page(uint32_t page_start_addr, uint8_t* data_buff, uint32_t size);
void QSPI_Read_StatusRegisters(uint8_t* reg_data);
void QSPI_Read_StatusRegisterOne(uint8_t* reg_data);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ quadspi_H */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
