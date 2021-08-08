/*
 * console.c
 *
 *  Created on: 1 Aug 2021
 *      Author: ioann
 */

#include "console.h"

void LogConsole(char* string, uint16_t size)
{
	// size - 1 to omit NULL terminator
	CDC_Transmit_FS((uint8_t*)string, (uint16_t)(size - 1));
	HAL_UART_Transmit(&huart4, (uint8_t*)string, (size - 1), 1000);

}

void PrintVersion(void)
{
	char bl_major_version[5];
	char bl_minor_version[5];
	char board_major_version[5];

	memset(bl_major_version, 0 , SIZEOF(bl_major_version));
	memset(bl_minor_version, 0 , SIZEOF(bl_minor_version));
	sprintf(bl_major_version, "%d", *(uint8_t*)BL_MAJOR_VERSION_LOC);
	sprintf(bl_minor_version, "%d", *(uint8_t*)BL_MINOR_VERSION_LOC);
	LogConsole("Bootloader Version: v", SIZEOF("Bootloader Version: v"));
	LogConsole(bl_major_version, SIZEOF(bl_major_version));
	LogConsole(".", SIZEOF("."));
	LogConsole(bl_minor_version, SIZEOF(bl_minor_version));
	LogConsole("\r\n", SIZEOF("\r\n"));

	memset(board_major_version, 0 , SIZEOF(board_major_version));
	sprintf(board_major_version, "%d", board_version);
	LogConsole("Board Version: v", SIZEOF("Board Version: v"));
	LogConsole(board_major_version, SIZEOF(board_major_version));
	LogConsole("\r\n", SIZEOF("\r\n"));
}
void PrintOtaFlags(void)
{
	char update_flag[4];
	char rollback_flag[4];
	char backup_flag[4];
	char rollback_on_fail_flag[4];

	memset(update_flag, 0, SIZEOF(update_flag));
	memset(rollback_flag, 0, SIZEOF(rollback_flag));
	memset(backup_flag, 0, SIZEOF(backup_flag));
	memset(rollback_on_fail_flag, 0, SIZEOF(rollback_on_fail_flag));

	sprintf(update_flag, "%d", *(uint8_t*)UPDATE_FLAG_LOC);
	sprintf(rollback_flag, "%d", *(uint8_t*)ROLLBACK_FLAG_LOC);
	sprintf(backup_flag, "%d", *(uint8_t*)BACKUP_FLAG_LOC);
	sprintf(rollback_on_fail_flag, "%d", *(uint8_t*)ROLLBACK_ON_FAIL_BOOT_FLAG_LOC);

	LogConsole("Update Flag: ", SIZEOF("Update Flag: "));
	LogConsole(update_flag, SIZEOF(update_flag));
	LogConsole("\r\n", SIZEOF("\r\n"));
	LogConsole("Rollback Flag: ", SIZEOF("Rollback Flag: "));
	LogConsole(rollback_flag, SIZEOF(rollback_flag));
	LogConsole("\r\n", SIZEOF("\r\n"));
	LogConsole("Backup Flag: ", SIZEOF("Backup Flag: "));
	LogConsole(backup_flag, SIZEOF(backup_flag));
	LogConsole("\r\n", SIZEOF("\r\n"));
	LogConsole("Rollback on fail boot Flag: ", SIZEOF("Backup on fail boot Flag: "));
	LogConsole(rollback_on_fail_flag, SIZEOF(rollback_on_fail_flag));
	LogConsole("\r\n", SIZEOF("\r\n"));
}
