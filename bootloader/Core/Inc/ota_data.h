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

enum rollback_on_fail_flags{

	rollback_on_fail_disabled = 0,
	rollback_on_fail_enabled = 1,
};

enum update_flags{

	update_flag = 0,
	rollback_flag = 1,
	backup_flag = 2,
	update_failure_flag = 3,
	rollback_failure_flag = 4,
	backup_failure_flag = 5,
	rollback_on_fail_flag = 6,

};

enum update_state{
	no_update = 0,				//	Under normal operation
	update_nuttx_pending = 1,	//	Set by nuttx, read and cleared by BL
	update_nuttx_in_progress = 2,	//	Set and cleared by BL
	update_nuttx_complete = 3,	//	Set by BL, read and cleared by nuttx
	update_nuttx_failed = 4,	//	Set read and cleared by BL, cleared and read by nuttx

};

enum rollback_state{
	no_rollback = 0,				//	Under normal operation
	rollback_nuttx_pending = 1,		// 	Set, read and cleared by BL
	rollback_nuttx_in_progress = 2,	//	Set and cleared by BL
	rollback_nuttx_complete = 3,	//	Set by BL, read and cleared by nuttx
	rollback_nuttx_failed = 4,		//	Set read and cleared by BL, cleared and read by nuttx
};

enum backup_state{
	no_backup = 0,					//	Under normal operation
	backup_nuttx_pending = 1,		// 	Set, read and cleared by BL
	backup_nuttx_in_progress = 2,	//	Set, read and cleared by BL
	backup_nuttx_complete = 3,		//	Set, read and cleared by BL
	backup_nuttx_failed = 4,		//	Set read and cleared by BL, cleared and read by nuttx
};

enum update_failure{
	update_fail_none = 0,
	update_fail_stage_one = 1,
	update_fail_stage_two = 2,
	update_fail_stage_three = 3,
	update_fail_invalid_image = 4,
};

enum rollback_failure{
	rollback_fail_none = 0,
	rollback_fail = 1,
	rollback_fail_invalid_image = 2,
};

enum backup_failure{
	backup_fail_none = 0,
	backup_fail = 1,
};

#ifdef __cplusplus
}
#endif


#endif
