/*
 * bootloader.h
 *
 *  Created on: 1 Aug 2021
 *      Author: ioann
 */

#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

#ifdef __cplusplus
 extern "C" {
#endif

#include "stm32f7xx_hal.h"

#include "partitions.h"
#include "ota_data.h"
#include "stddef.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "console.h"

#define BL_MAJOR_VERSION_LOC 0x08010000
#define BL_MINOR_VERSION_LOC BL_MAJOR_VERSION_LOC + 1

#define SIZEOF(X) (sizeof(X) / sizeof(X[0]))


extern uint8_t board_version;
extern uint8_t bootloader_status;

enum crc_check{
	crc_fail = 0,
	crc_pass = 1,
};

enum bootloader_state{
	bootloader_no_op = 0,
	bootloader_update = 1,
	bootloader_rollback = 2,
	bootloader_backup = 3,
};

void Error_Handler(void);
uint8_t getOTAFlagState(uint8_t flag);

#ifdef __cplusplus
}
#endif

#endif /* BOOTLOADER_H_ */
