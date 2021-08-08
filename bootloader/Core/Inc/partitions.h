/**
	******************************************************************************
	* @file           : partitions.h
	* @brief          : Header file containing device partition information.
	******************************************************************************
	*/

/*	Recursive Inclusion Guard	*/
#ifndef __PARTITIONS_H
#define __PARTITIONS_H

#ifdef __cplusplus
extern "C" {
#endif

/*	Includes */

/*	Definitions	*/

//	NUTTX DEFINITIONS
#define NUTTX_PRI_LOC	0x08040000
#define NUTTX_SEC_QSPI_LOC	0x200000	// 2MB offset from QSPI base addr
#define NUTTX_SIZE	0x1C0000	//	(2MB - 256KB)
#define NUTTX_PRI_CRC_LOC	(NUTTX_PRI_LOC + NUTTX_SIZE - 4)

//	SDRAM DEFINITIONS
#define SDRAM_LOC 0xC0000000
#define SDRAM_SIZE 0x2000000

//	QSPI DEFINITIONS
#define QSPI_FLASH_LOC_INTERNAL 0x0000000
#define QSPI_FLASH_SIZE 0x2000000

//	OTA DATA
#define OTA_DATA_LOC 0x08008000
#define OTA_DATA_SIZE 0x08000


#define IO_BLOCK_SIZE 0x40000

#ifdef __cplusplus
}
#endif


#endif
