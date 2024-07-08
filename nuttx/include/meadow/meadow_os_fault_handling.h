/****************************************************************************
 * meadow_os_fault_handling.h
 * 
 *   Copyright (C) 2024 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs (Mark Stevens)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/
#ifndef __MEADOW_OS_FAULT_HANDLING_H
#define __MEADOW_OS_FAULT_HANDLING_H

#pragma once

#include <nuttx/config.h>

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 * @brief Meadow OS Component has errored
 */
#define FAULT_LOGGING_COMPONENT_ERRORED         (1 << 0)

/**
 * @brief Meadow OS Component has started phase 1 logging.
 */
#define FAULT_LOGGING_PHASE1_STARTED            (1 << 1)

/**
 * @brief Meadow OS Component has completed phase 1 logging.
 */
#define FAULT_LOGGING_PHASE1_COMPLETED          (1 << 2)

/**
 * @brief Meadow OS Component has started phase 2 logging.
 */
#define FAULT_LOGGING_PHASE2_STARTED            (1 << 3)

/**
 * @brief Meadow OS Component has completed phase 2 logging.
 */
#define FAULT_LOGGING_PHASE2_COMPLETED          (1 << 4)

/**
 * @brief Number of bits to shift the OS component fault logging flags in the status register.
 */
#define FAULT_LOGGING_OS_BIT_SHIFT              0

/**
 * @brief Number of bits to shift the runtime component fault logging flags in the status register.
 */
#define FAULT_LOGGING_RT_BIT_SHIFT              8

/**
 * @brief Meadow OS Component has errored
 */
#define FAULT_LOGGING_OS_COMPONENT_ERRORED      (FAULT_LOGGING_COMPONENT_ERRORED << FAULT_LOGGING_OS_BIT_SHIFT)

/**
 * @brief Meadow OS Component has started phase 1 logging.
 */
#define FAULT_LOGGING_OS_PHASE1_STARTED         (FAULT_LOGGING_PHASE1_STARTED << FAULT_LOGGING_OS_BIT_SHIFT)

/**
 * @brief Meadow OS Component has completed phase 1 logging.
 */
#define FAULT_LOGGING_OS_PHASE1_COMPLETED       (FAULT_LOGGING_PHASE1_COMPLETED << FAULT_LOGGING_OS_BIT_SHIFT)

/**
 * @brief Meadow OS Component has started phase 2 logging.
 */
#define FAULT_LOGGING_OS_PHASE2_STARTED         (FAULT_LOGGING_PHASE2_STARTED << FAULT_LOGGING_OS_BIT_SHIFT)

/**
 * @brief Meadow OS Component has completed phase 2 logging.
 */
#define FAULT_LOGGING_OS_PHASE2_COMPLETED       (FAULT_LOGGING_PHASE2_COMPLETED << FAULT_LOGGING_OS_BIT_SHIFT)

/**
 * @brief Meadow RT Component has errored
 */
#define FAULT_LOGGING_RT_COMPONENT_ERRORED      (FAULT_LOGGING_COMPONENT_ERRORED << FAULT_LOGGING_RT_BIT_SHIFT)

/**
 * @brief Meadow RT Component has started phase 1 logging.
 */
#define FAULT_LOGGING_RT_PHASE1_STARTED         (FAULT_LOGGING_PHASE1_STARTED << FAULT_LOGGING_RT_BIT_SHIFT)

/**
 * @brief Meadow RT Component has completed phase 1 logging.
 */
#define FAULT_LOGGING_RT_PHASE1_COMPLETED       (FAULT_LOGGING_PHASE1_COMPLETED << FAULT_LOGGING_RT_BIT_SHIFT)

/**
 * @brief Meadow RT Component has started phase 2 logging.
 */
#define FAULT_LOGGING_RT_PHASE2_STARTED         (FAULT_LOGGING_PHASE2_STARTED << FAULT_LOGGING_RT_BIT_SHIFT)

/**
 * @brief Meadow RT Component has completed phase 2 logging.
 */
#define FAULT_LOGGING_RT_PHASE2_COMPLETED       (FAULT_LOGGING_PHASE2_COMPLETED << FAULT_LOGGING_RT_BIT_SHIFT)

/****************************************************************************
 * Public type defintions.
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#endif /* __MEADOW_OS_FAULT_HANDLING_H */