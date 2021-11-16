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
	#ifdef ENABLE_BL_CDC
	CDC_Transmit_FS((uint8_t*)string, (uint16_t)(size - 1));
	#endif
	#ifdef ENABLE_BL_UART
	HAL_UART_Transmit(&huart4, (uint8_t*)string, (size - 1), 1000);
	#endif

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
	char update_flag_str[4] = {0};
	char rollback_flag_str[4]= {0};
	char backup_flag_str[4]= {0};
	char update_failure_flag_str[4]= {0};
	char rollback_failure_flag_str[4]= {0};
	char backup_failure_flag_str[4]= {0};
	char rollback_on_fail_flag_str[4]= {0};

	sprintf(update_flag_str, "%d", getQspiOTAFlagState(update_flag));
	sprintf(rollback_flag_str, "%d", getQspiOTAFlagState(rollback_flag));
	sprintf(backup_flag_str, "%d", getQspiOTAFlagState(backup_flag));
	sprintf(update_failure_flag_str, "%d", getQspiOTAFlagState(update_failure_flag));
	sprintf(rollback_failure_flag_str, "%d", getQspiOTAFlagState(rollback_failure_flag));
	sprintf(backup_failure_flag_str, "%d", getQspiOTAFlagState(backup_failure_flag));
	sprintf(rollback_on_fail_flag_str, "%d", getQspiOTAFlagState(rollback_on_fail_flag));

	LogConsole("Update Flag: ", SIZEOF("Update Flag: "));
	LogConsole(update_flag_str, SIZEOF(update_flag_str));
	LogConsole("  Update Fail Flag: ", SIZEOF("  Update Fail Flag: "));
	LogConsole(update_failure_flag_str, SIZEOF(update_failure_flag_str));
	LogConsole("\r\n", SIZEOF("\r\n"));
	LogConsole("Rollback Flag: ", SIZEOF("Rollback Flag: "));
	LogConsole(rollback_flag_str, SIZEOF(rollback_flag_str));
	LogConsole("  Rollback Fail Flag: ", SIZEOF("  Rollback Fail Flag: "));
	LogConsole(rollback_failure_flag_str, SIZEOF(rollback_failure_flag_str));
	LogConsole("\r\n", SIZEOF("\r\n"));
	LogConsole("Backup Flag: ", SIZEOF("Backup Flag: "));
	LogConsole(backup_flag_str, SIZEOF(backup_flag_str));
	LogConsole("  Backup Fail Flag: ", SIZEOF("  Backup Fail Flag: "));
	LogConsole(backup_failure_flag_str, SIZEOF(backup_failure_flag_str));
	LogConsole("\r\n", SIZEOF("\r\n"));
	LogConsole("Rollback on fail boot Flag: ", SIZEOF("Rollback on fail boot Flag: "));
	LogConsole(rollback_on_fail_flag_str, SIZEOF(rollback_on_fail_flag_str));
	LogConsole("\r\n", SIZEOF("\r\n"));
}
