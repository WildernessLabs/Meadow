/****************************************************************************
 * \apps\examples\hcom\ota\hcom_ota.c
 *
 *   Meadow Cloud Authentication and Update
 *   Copyright (C) Wilderness Labs. All rights reserved.
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_common.h"
#include <meadow/hcom_protocol.h>
#include <meadow/hcom_nuttx_shared.h>
#include <meadow/hcom_shared_common.h>

static char *thisFile = __FILE__;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>


/* OS & App updaters */

int update_file(const char *srcpath, const char *destpath, const char *rollbackpath)
{
  syslog(LOG_ERR, "%s -> %s\n", srcpath, destpath);
  struct stat statbuf;
  int ret;
  if (stat(destpath, &statbuf) == 0)
  {
    if (rollbackpath)
    {
      ret = update_file(destpath, rollbackpath, NULL);
      if (ret != 0)
        return ret;
    }
    else
    {
      ret = unlink(destpath);
      if (ret != 0)
        return ret;
    }
  }
  ret = rename(srcpath, destpath);
  return ret;
}

// TODO: We have limited stack, convert to iterative
static int deltree(const char *path)
{
  DIR *dir = opendir(path);
  struct dirent *entry;

  if (!dir)
    return 0;

  bool error = false;

  while ((entry = readdir(dir)) != NULL && !error)
  {
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
    if (DIRENT_ISDIRECTORY(entry->d_type) && strncmp(entry->d_name, ".", 1) && strncmp(entry->d_name, "..", 1))
    {
      deltree(full_path);
    }
    if (DIRENT_ISFILE(entry->d_type))
    {
      unlink(full_path);
    }
  }
  closedir(dir);
  rmdir(path);

  return 0;
}

int app_update(void)
{
  DIR *update_dir = opendir(UPDATE_APP_DIR);
  struct dirent *entry;

  if (!update_dir)
    return 0;

  bool error = false;
  int  __attribute__((unused)) ret;
  ret = mkdir(ROLLBACK_DIR, 0777);

  // TODO: Recursive copying
  while ((entry = readdir(update_dir)) != NULL && !error)
  {
    if (DIRENT_ISFILE(entry->d_type))
    {
      char source_path[PATH_MAX];
      char target_path[PATH_MAX];
      char rollback_path[PATH_MAX];
      snprintf(source_path, sizeof(source_path), "%s%s", UPDATE_APP_DIR, entry->d_name);
      snprintf(target_path, sizeof(target_path), "/meadow0/%s", entry->d_name);
      snprintf(rollback_path, sizeof(target_path), "%s/%s", ROLLBACK_DIR, entry->d_name);
      if (update_file(source_path, target_path, rollback_path) != 0)
        error = true;
    }
  }
  closedir(update_dir);
  if (error) // Invalid update; roll back
  {
    deltree(UPDATE_APP_DIR);
    DIR *rollback_dir = opendir(ROLLBACK_DIR);

    if (!rollback_dir)
      return 0;

    // TODO: Recursive copying
    while ((entry = readdir(rollback_dir)) != NULL && !error)
    {
      if (DIRENT_ISFILE(entry->d_type))
      {
        char source_path[PATH_MAX];
        char target_path[PATH_MAX];
        snprintf(source_path, sizeof(source_path), "%s/%s", ROLLBACK_DIR, entry->d_name);
        snprintf(target_path, sizeof(target_path), "/meadow0/%s", entry->d_name);
        if (update_file(source_path, target_path, NULL) != 0)
        { 
          syslog(LOG_ERR, "Error rolling back update, failed to restore %s to %s\n", source_path, target_path);
        }
      }
    }
    closedir(rollback_dir);
  }
  return 1;

}

#define OS_BINARY_SIGNATURE_EXT ".sig"

static int validate_signature(const char *path)
{
  // mbedtls_pk_verify ()
  return -1;
}

#define OS_PART1_BINARY_FILENAME HCOM_NX_FS_NUTTX_UPDATE_FILENAME
#define OS_PART2_BINARY_FILENAME HCOM_NX_FS_MONO_RUNTIME_FILENAME

int os_update(void)
{
  DIR *update_dir = opendir(UPDATE_OS_DIR);
  struct dirent *entry;

  if (!update_dir)
    return 0;

  bool part1_rollback_happening = false; // TODO: Check OTADATA for rollback

  if (part1_rollback_happening)
  {
    deltree(UPDATE_OS_DIR);
    return -1;
  }

  bool part1_update = false;
  bool part2_update = false;

  while ((entry = readdir(update_dir)) != NULL)
  {
    if (DIRENT_ISFILE(entry->d_type))
    {
      if (!strncmp(entry->d_name, OS_PART1_BINARY_FILENAME, strnlen(OS_PART1_BINARY_FILENAME, PATH_MAX)))
        part1_update = true;
      if (!strncmp(entry->d_name, OS_PART2_BINARY_FILENAME, strnlen(OS_PART2_BINARY_FILENAME, PATH_MAX)))
        part2_update = true;
    }
  }
  closedir(update_dir);

  if (part1_update && part2_update)
  {
    validate_signature(OS_PART1_BINARY_FILENAME);
    validate_signature(OS_PART2_BINARY_FILENAME);
    hcom_via_nx_update_OS1(); // should not return
  }

  if (!part1_update && part2_update)
  {
    return hcom_via_nx_update_OS2();
  }

  return -2;
}

int firmware_update(void)
{
  int result = hcom_nx_exec_ex_update_ESP32();
  if (result > 0) // successful update
  {
    deltree(UPDATE_FIRMWARE_DIR);
    hcom_via_nx_only_restart_meadow();
  }
  return result;
}



/* RSA+AES key generation data + functions */

#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/rsa.h"
#include "mbedtls/error.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "meadow/meadow_cloud.h"

static mbedtls_pk_context key;
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;

static const char *pers = "meadow_cloud_key_generator";
static mbedtls_pk_type_t key_type = MBEDTLS_PK_RSA;
static const int KEY_SIZE = 4096;
static const int PEM_SIZE = 4096;

#define DEV_URANDOM_THRESHOLD        32
#define DEV_RANDOM_THRESHOLD        32

// copied from mbedtls/programs/pkey/gen_key.c
static int dev_random_entropy_poll( void *data, unsigned char *output,
                             size_t len, size_t *olen )
{
    FILE *file;
    size_t ret, left = len;
    unsigned char *p = output;
    ((void) data);

    *olen = 0;

    file = fopen( "/dev/random", "rb" );
    if( file == NULL )
        return( MBEDTLS_ERR_ENTROPY_SOURCE_FAILED );

    while( left > 0 )
    {
        /* /dev/random can return much less than requested. If so, try again */
        ret = fread( p, 1, left, file );
        if( ret == 0 && ferror( file ) )
        {
            fclose( file );
            return( MBEDTLS_ERR_ENTROPY_SOURCE_FAILED );
        }

        p += ret;
        left -= ret;
        sleep( 1 );
    }
    fclose( file );
    *olen = len;

    return( 0 );
}

static void ota_rsa_init (void)
{
    int ret;
    mbedtls_mpi N, P, Q, D, E, DP, DQ, QP;
    mbedtls_mpi_init( &N ); mbedtls_mpi_init( &P ); mbedtls_mpi_init( &Q );
    mbedtls_mpi_init( &D ); mbedtls_mpi_init( &E ); mbedtls_mpi_init( &DP );
    mbedtls_mpi_init( &DQ ); mbedtls_mpi_init( &QP );

    mbedtls_pk_init( &key );
    mbedtls_ctr_drbg_init( &ctr_drbg );



    mbedtls_entropy_init(&entropy);

    if ((ret = mbedtls_entropy_add_source(&entropy, dev_random_entropy_poll,
                                          NULL, DEV_RANDOM_THRESHOLD,
                                          MBEDTLS_ENTROPY_SOURCE_STRONG)) != 0)
    {
        printf(" failed\n  ! adding /dev/random entropy returned -0x%04x\n", (unsigned int)-ret);
        goto exit;
    }

    if ((ret = mbedtls_entropy_add_source(&entropy, mbedtls_platform_entropy_poll,
                                          NULL, DEV_URANDOM_THRESHOLD,
                                          MBEDTLS_ENTROPY_SOURCE_STRONG)) != 0)
    {
        printf(" failed\n  ! adding /dev/urandom entropy returned -0x%04x\n", (unsigned int)-ret);
        goto exit;
    }

    if( ( ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
                               (const unsigned char *) pers,
                               strlen( pers ) ) ) != 0 )
    {
        printf( " failed\n  ! mbedtls_ctr_drbg_seed returned -0x%04x\n", (unsigned int) -ret );
        goto exit;
    }

    if( ( ret = mbedtls_pk_setup( &key,
           mbedtls_pk_info_from_type (key_type)) ) != 0 )
    {
        printf( " failed\n  !  mbedtls_pk_setup returned -0x%04x", (unsigned int) -ret );
        goto exit;
    }

exit:
    return;
}

static void ota_rsa_keygen (unsigned char *private_key_pem, unsigned char *public_key_pem)
{
    int ret;
    ota_rsa_init();

    ret = mbedtls_rsa_gen_key (mbedtls_pk_rsa (key),
                        mbedtls_ctr_drbg_random,
                        &ctr_drbg,
                        KEY_SIZE,
                        65537);
    assert (ret == 0);

    ret = mbedtls_pk_write_key_pem (&key, private_key_pem, PEM_SIZE);
    assert (ret == 0);
    ret = mbedtls_pk_write_pubkey_pem (&key, public_key_pem, PEM_SIZE);
    assert (ret == 0);
}

void hcom_ota_rqst_register_device(uint32_t userData)
{
    unsigned char private_key_pem[PEM_SIZE];
    unsigned char public_key_pem[PEM_SIZE];
    int private_key_len, public_key_len;

    ota_rsa_keygen(private_key_pem, public_key_pem);
    private_key_len = strlen((char*)private_key_pem);
    public_key_len = strlen((char*)public_key_pem);

    //Send out public key
    hcom_host_send_raw_string_msg(HCOM_HOST_REQUEST_DEVICE_PUBLIC_KEY, 0, public_key_pem, public_key_len, thisFile, __LINE__);

    meadow_cloud_provision((const char*)private_key_pem, private_key_len + 1, (const char*) public_key_pem, public_key_len + 1, NULL);
}

int meadow_cloud_decrypt_buf(const unsigned char *encrypted_buf, int encrypted_len, unsigned char *decrypted_buf)
{
    unsigned char *private_key;
    int len, ret;
    meadow_cloud_retrieve_private_key((const char**) &private_key, &len);

    mbedtls_pk_init( &key );
    mbedtls_ctr_drbg_init( &ctr_drbg );
    mbedtls_entropy_init( &entropy );

    if ((ret = mbedtls_entropy_add_source(&entropy, mbedtls_platform_entropy_poll,
                                          NULL, DEV_URANDOM_THRESHOLD,
                                          MBEDTLS_ENTROPY_SOURCE_STRONG)) != 0)
    {
        printf(" failed\n  ! mbedtls_entropy_add_source returned -0x%04x\n", (unsigned int)-ret);
        return -3;
    }
    if( ( ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
                               (const unsigned char *) pers,
                               strlen( pers ) ) ) != 0 )
    {
        printf( " failed\n  ! mbedtls_ctr_drbg_seed returned -0x%04x\n", (unsigned int) -ret );
        return -4;
    }
    if( ( ret = mbedtls_pk_parse_key(&key, private_key, len, NULL, 0,
                            mbedtls_ctr_drbg_random, &ctr_drbg) ) != 0 )
    {
        printf( " failed\n  ! mbedtls_pk_parse_key returned -0x%04x\n", -ret );
        return -1;
    }

    meadow_cloud_release_private_key((const char**) &private_key);

    unsigned char result[MBEDTLS_MPI_MAX_SIZE];
    size_t olen = 0;

    fflush( stdout );

    if( ( ret = mbedtls_pk_decrypt(&key, encrypted_buf, encrypted_len, result, &olen, sizeof(result),
                                    mbedtls_ctr_drbg_random, &ctr_drbg ) ) != 0 )
    {
        printf( " failed\n  ! mbedtls_pk_decrypt returned -0x%04x\n", -ret );
        return -2;
    }

    memcpy (decrypted_buf, result, olen);
    return olen;

}

int meadow_cloud_decrypt_buf_aes(const unsigned char *encrypted_buf, int encrypted_len, const unsigned char *key, unsigned char *iv, unsigned char *decrypted_buf)
{
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_dec(&aes, key, 256 );
    mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, encrypted_len, iv, encrypted_buf, decrypted_buf);
    mbedtls_aes_free( &aes );
    return encrypted_len;
}
