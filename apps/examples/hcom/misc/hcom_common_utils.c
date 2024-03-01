/****************************************************************************
 * \apps\examples\hcom\misc\hcom_common_utils.c
 * 
 *   Copyright (C) 2019 - 2020 Wilderness Labs. All rights reserved.
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
#include <ctype.h>
#include "hcom_common.h"

#include <nuttx/config.h>
#include "syslog.h"

#include <meadow/hcom_shared_common.h>
#include <meadow/meadow_hw_version.h>
#include <arpa/inet.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/
// static char *thisFile = __FILE__;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_common_utils_setup()
{
  return OK;
}

//============================================================================
void hcom_common_utils_shutdown()
{
}

//===================================================================
// A bit simpler to use since it's defined in meadow/meadow_hw_version.h
uint32_t meadow_hw_version_get(void)
{
  return hcom_via_nx_get_hw_version();
}

//===================================================================
// Returns the current time as a 64-bit number representing nanosec.
// Used for testing. Note: Only millisecond resolution.
uint64_t hcom_utils_get_current_time64_ns(void)
{
  struct timespec ts;
#ifdef CONFIG_CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &ts);
#else
  clock_gettime(CLOCK_REALTIME, &ts);
#endif
  return (uint64_t)ts.tv_sec * NSEC_PER_SEC + (uint64_t)ts.tv_nsec;
}

//===================================================================
// Due to the number of places snprintf is called and the code required
// to determine success or failure. This function is designed so that
// users can generate less code and be confident that truncated are noted
// A macro that adds file name and line number exists
int hcom_common_utils_snprintf_chk(FAR char *buf, size_t size, char *fileName, int lineNumb,
          FAR const IPTR char *fmt, ...)
{
  int bufChk;
  va_list ap;

  va_start(ap, fmt);

  // Process the string
  bufChk = vsnprintf(buf, size, fmt, ap);
  va_end(ap);

  // Handle buffer overflow here
  if(bufChk >= size)
  {
    hcom_logging_syslog(LOG_WARNING, "%s@%d Host msg truncated, need:%d\n", fileName, lineNumb, bufChk + 1);
    // This modifies the standard Nuttx snprintf behavior which would normally
    // return the size of needed buffer.
    return -ENAMETOOLONG;
  }
  else if(bufChk < 0)
  {
    hcom_logging_syslog(LOG_ERR, "%s@%d snprintf returned an error, ret:%d\n", fileName, lineNumb, bufChk);
  }

  // Must be operations as usual
  return bufChk;
}

//===================================================================
// Erase the DNS resolver file, a.k.a. dns.conf file. 
void hcom_common_utils_erase_dns_resolver_file(void)
{
    // Open the file in write mode
    FILE *dns_file = fopen(CONFIG_NETDB_RESOLVCONF_PATH, "w");
    if (dns_file == NULL)
    {
        perror("Error opening file");
        return;
    }

    // Close the file to erase its contents
    fclose(dns_file);
}

//===================================================================
// Check if a IP address is valid.
bool hcom_common_utils_is_valid_ip_address(const char *address)
{
    struct sockaddr_in sa;
    return (inet_pton(AF_INET, address, &(sa.sin_addr)) == 1);
}

//===================================================================
// Add the user-defined DNS servers to the DNS resolver file, 
// a.k.a dns.conf file.
void hcom_common_utils_add_servers_to_dns_resolver_file(char **servers, uint32_t server_count)
{
    FILE *dns_file = fopen(CONFIG_NETDB_RESOLVCONF_PATH, "a");
    if (dns_file == NULL)
    {
        perror("Error opening file");
        return;
    }
    
    hcom_logging_syslog(LOG_INFO, "User-provided DNS servers count: %d\n", server_count);
    for (int index = 0; index < server_count; index++)
    {
        hcom_logging_syslog(LOG_INFO, "%d: %s\n", index + 1, servers[index]);
    }
    
    for (int index = 0; index < server_count; index++)
    {
        if (hcom_common_utils_is_valid_ip_address(servers[index]))
        {
            fputs("nameserver ", dns_file);
            fputs(servers[index], dns_file);
            fputs("\n", dns_file);
        }
    }

    fclose(dns_file);
}