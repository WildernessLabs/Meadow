/**
	******************************************************************************
	* @file           : ota_data.h
	* @brief          : Header file containing ota data information.
	******************************************************************************
	*/

/*	Recursive Inclusion Guard	*/
#ifndef __OTA_DATA_H
#define __OTA_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

/*	Includes */

/*	Definitions	*/

#define UPDATE_FLAG_LOC OTA_DATA_LOC
#define UPDATE_FLAG_SIZE 1
#define	ROLLBACK_FLAG_LOC (OTA_DATA_LOC + UPDATE_FLAG_SIZE)
#define ROLLBACK_FLAG_SIZE 1
#define BACKUP_FLAG_LOC (ROLLBACK_FLAG_LOC + ROLLBACK_FLAG_SIZE)
#define BACKUP_FLAG_SIZE 1
#define ROLLBACK_ON_FAIL_BOOT_FLAG_LOC (BACKUP_FLAG_LOC + BACKUP_FLAG_SIZE)
#define ROLLBACK_ON_FAIL_BOOT_FLAG_SIZE 1


enum rollback_on_fail_flags{

	rollback_on_fail_disabled = 0,
	rollback_on_fail_enabled = 1,
};

enum update_flags{

	update_flag = 0,
	rollback_flag = 1,
	nuttx_backup_flag = 2,

};
//#define PRIMARY_OS_CRC_LOC (ROLLBACK_FLAG_LOC + 4)
//#define SECONDARY_OS_CRC_LOC (PRIMARY_OS_CRC_LOC + 4)

enum update_state{
	no_update = 0,				//	Under normal operation
	update_nuttx_pending = 1,	//	Set by nuttx, read and cleared by BL
	update_nuttx_in_progress = 2,	//	Set and cleared by BL
	update_nuttx_complete = 3,	//	Set by BL, read and cleared by nuttx

};

enum rollback_state{
	no_rollback = 0,				//	Under normal operation
	rollback_nuttx_pending = 1,		// 	Set, read and cleared by BL
	rollback_nuttx_in_progress = 2,	//	Set and cleared by BL
	rollback_nuttx_complete = 3,	//	Set by BL, read and cleared by nuttx
};

enum backup_state{
	no_backup = 0,					//	Under normal operation
	backup_nuttx_pending = 1,		// 	Set, read and cleared by BL
	backup_nuttx_in_progress = 2,	//	Set, read and cleared by BL
	backup_nuttx_complete = 3,		//	Set, read and cleared by BL
};

#ifdef __cplusplus
}
#endif


#endif
