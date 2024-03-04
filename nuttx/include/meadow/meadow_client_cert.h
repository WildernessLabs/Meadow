/****************************************************************************
 * meadow_client_cert.h
 *
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
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

#ifndef __MEADOW_CLIENT_CERT_H__
#define __MEADOW_CLIENT_CERT_H__

#include <nuttx/config.h>

#include <meadow/hcom_shared_common.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 *  @brief Default client credentials file paths.
 */
#define CLIENT_CERT_FILE_PATH MONO_MEADOW_EXECUTABLE_PARTITION_NAME "/" CLIENT_CERT_FILE
#define CLIENT_CERT_PRIVATE_KEY_FILE_PATH MONO_MEADOW_EXECUTABLE_PARTITION_NAME "/" CLIENT_CERT_PRIVATE_KEY_FILE
#define CLIENT_CERT_PRIVATE_KEY_PASS_FILE_PATH MONO_MEADOW_EXECUTABLE_PARTITION_NAME "/" CLIENT_CERT_PRIVATE_KEY_PASS_FILE

#define CLIENT_CERT_FILE "client_cert.pem"
#define CLIENT_CERT_PRIVATE_KEY_FILE "private_key.pem"
#define CLIENT_CERT_PRIVATE_KEY_PASS_FILE "private_key_pass.txt"

static const int KEY_SIZE = 4096;
static const int PEM_SIZE = 4096;
static const int PASSWORD_SIZE = 512;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int meadow_client_cert_retrieve_credentials(FAR const char **client_cert_buf_ptr, int *client_cert_buf_len, FAR const char **private_key_buf_ptr, int *private_key_buf_len, FAR const char **private_key_pass_buf_ptr, int *private_key_pass_buf_len);
int meadow_client_cert_release_credentials(FAR const char **client_cert_buf_ptr, FAR const char **private_key_buf_ptr, FAR const char **private_key_pass_buf_ptr);
int meadow_client_cert_initialize(void);

#endif // __MEADOW_CLIENT_CERT_H__