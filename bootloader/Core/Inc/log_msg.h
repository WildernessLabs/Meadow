/**
	******************************************************************************
	* @file           : log_msg.h
	* @brief          : Header file containing device partition information.
	******************************************************************************
	*/

/*	Recursive Inclusion Guard	*/
#ifndef __LOG_MSG_H
#define __LOG_MSG_H

#ifdef __cplusplus
extern "C" {
#endif

/*	Includes */

/*	Definitions	*/

#define INIT_MSG "Bootloader Init...\r\n"

#define NUTTX_IMG_CHK_MSG "Checking for valid Meadow.OS image.\r\n"

#define PRIMARY_IMG_VERIFY_SUCCESS_MSG "Valid image found in Primary slot.\r\n"

#define PRIMARY_IMG_VERIFY_FAIL_MSG "Valid image not found in Primary slot...\r\n"

#define SECONDARY_IMG_VERIFY_SUCCESS_MSG "Valid image found in Secondary slot.\r\n"

#define SECONDARY_IMG_VERIFY_FAIL_MSG "Valid image not found in Secondary slot...\r\n"

#define JUMP_MSG "Performing Jump.\r\n"

#define BACKUP_START_MSG "Starting OS Backup\r\n"

#define BACKUP_COMPLETE_MSG "OS Backup Complete!\r\n"

#define UPDATE_START_MSG "Starting OS Update\r\n"

#define UPDATE_COMPLETE_MSG "OS Update Complete!\r\n"

#define UPDATE_STAGE_1_START_MSG "Update Stage 1 of 3.\r\n"

#define UPDATE_STAGE_2_START_MSG "Update Stage 2 of 3.\r\n"

#define UPDATE_STAGE_3_START_MSG "Update Stage 3 of 3.\r\n"

#define ROLLBACK_START_MSG "Starting OS Rollback\r\n"

#define ROLLBACK_COMPLETE_MSG "OS Rollback Complete!\r\n"

#define VERIFY_QSPI_SUCCESS_MSG_ "QSPI comms functional.\r\n"

#define VERIFY_QSPI_FAIL_MSG "QSPI comms error.\r\n"

#define V1_BOARD_DETECT_MSG "Detected V1 Board.\r\n"

#define V2_BOARD_DETECT_MSG "Detected V2 Board.\r\n"

#define ERASE_PRIMARY_START_MSG "Starting Primary Image Erase\r\n"

#define ERASE_PRIMARY_COMPLETE_MSG "Primary Image Erase Complete!\r\n"

#define ERASE_SECONDARY_START_MSG "Starting Secondary Image Erase\r\n"

#define ERASE_SECONDARY_COMPLETE_MSG "Secondary Image Erase Complete!\r\n"

#define CONSOLE_HELP_MSG "v? - Display bootloader & board version \r\n \
						  c1 - Perform CRC on primary image \r\n \
						  c2 - Perform CRC on secondary image \r\n \
						  e1 - Erase primary image \r\n \
						  e2 - Erase secondary image \r\n \
						  u  - Perform update \r\n \
						  r  - Perform rollback \r\n \
						  b  - Perform primary image backup \r\n \
						  f  - Display status of OTA flags \r\n \
						  s  - Continue normal bootloader operation \r\n"
#ifdef __cplusplus
}
#endif


#endif
