/**
    ******************************************************************************
    * @file           : partitions.h
    * @brief          : Header file containing device partition information.
    ******************************************************************************
    */

/*    Recursive Inclusion Guard    */
#ifndef __PARTITIONS_H
#define __PARTITIONS_H

#ifdef __cplusplus
extern "C" {
#endif

//
//  The partitioning information is required by both the OS and the bootloader.
//  Keep one copy of the layout and share this between the two systems.
//
//  The definitive copy of this information is stored in the OS as the bootloader
//  needs to know about the OS but the OS does not need to know about the
//  bootloader.
//
#include "../../../nuttx/include/meadow/bootloader/meadow_os_partitions.h"

#ifdef __cplusplus
}
#endif


#endif  // __PARTITIONS_H
