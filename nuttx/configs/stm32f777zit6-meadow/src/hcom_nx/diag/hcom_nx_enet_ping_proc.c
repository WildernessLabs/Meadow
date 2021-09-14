/****************************************************************************
 * \apps\examples\hcom\tests\ethernet_tests\enet_ping_test.c
 *   Copyright (C) 2021 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
 ****************************************************************************/

// Copied by Peter Moody from .../apps/system/ping/ping.c and 
// .../apps/netutils/ping/icmp_ping_m.c because NSH Built-in apps are
// not generally available on a protected build.
// Note: all copied function names, structures and #defines have had '_m'
// (for meadow) postpended to their names to prevent build/linker issues.

/****************************************************************************
 * apps/netutils/ping/icmp_ping_m.c
 *
 *   Copyright (C) 2018 Pinecone Inc. All rights reserved.
 *   Author: Guiding Li<liguiding@pinecone.net>
 *
 * Extracted from logic originally written by:
 *
 *   Copyright (C) 2017-2018 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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
#include <nuttx/config.h>

#include <sys/socket.h>

#include <unistd.h>   // getopt() - parses command line args
#include <stdlib.h>
#include <time.h>
#include <poll.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdio.h>

#include <arpa/inet.h>
#include <nuttx/clock.h>
#include <nuttx/net/icmp.h>

#include "../hcom_nx_common.h"
#include <meadow/hcom_nuttx_shared.h>
#include <meadow/hcom_shared_common.h>
#include <meadow/hcom_protocol.h>

#if defined(HCOM_INCLUDE_ETHERNET_IN_HCOM_IN_BUILD)

#if defined(CONFIG_LIBC_NETDB) && defined(CONFIG_NETDB_DNSCLIENT)
#  include <netdb.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define ICMP_M_IOBUFFER_SIZE(x) (sizeof(struct icmp_hdr_s) + (x))

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* NOTE: This will not work in the kernel build where there will be a
 * separate instance of g_pingid in every process space.
 */

static uint16_t g_pingid = 0;

/****************************************************************************
 * Private Types
 ****************************************************************************/
// Copied from icmp_ping_m.h and modified
/* Positive number represent information */

#define ICMP_M_I_BEGIN       0   /* extra: not used      */
#define ICMP_M_I_ROUNDTRIP   1   /* extra: packet delay  */
#define ICMP_M_I_FINISH      2   /* extra: elapsed time  */

/* Negative odd number represent error(unrecoverable) */

#define ICMP_M_E_HOSTIP      -1  /* extra: not used      */
#define ICMP_M_E_MEMORY      -3  /* extra: not used      */
#define ICMP_M_E_SOCKET      -5  /* extra: error code    */
#define ICMP_M_E_SENDTO      -7  /* extra: error code    */
#define ICMP_M_E_SENDSMALL   -9  /* extra: sent bytes    */
#define ICMP_M_E_POLL        -11 /* extra: error code    */
#define ICMP_M_E_RECVFROM    -13 /* extra: error code    */
#define ICMP_M_E_RECVSMALL   -15 /* extra: recv bytes    */

/* Negative even number represent warning(recoverable) */

#define ICMP_M_W_TIMEOUT     -2  /* extra: timeout value */
#define ICMP_M_W_IDDIFF      -4  /* extra: recv id       */
#define ICMP_M_W_SEQNOBIG    -6  /* extra: recv seqno    */
#define ICMP_M_W_SEQNOSMALL  -8  /* extra: recv seqno    */
#define ICMP_M_W_RECVBIG     -10 /* extra: recv bytes    */
#define ICMP_M_W_DATADIFF    -12 /* extra: not used      */
#define ICMP_M_W_TYPE        -14 /* extra: recv type     */

struct ping_result_s_m;

struct ping_info_s_m
{
  FAR const char *hostname; /* Host name to ping */
  uint16_t count;           /* Number of pings requested */
  uint16_t datalen;         /* Number of bytes to be sent */
  uint16_t delay;           /* Deciseconds to delay between pings */
  uint16_t timeout;         /* Deciseconds to wait response before timeout */
  FAR void *priv;           /* Private context for callback */
  void (*callback)(FAR const struct ping_result_s_m *result);
};

struct ping_result_s_m
{
  int linenumb;
  int code;                 /* Notice code ICMP_I/E/W_XXX */
  int extra;                /* Extra information for code */
  struct in_addr dest;      /* Target address to ping */
  uint16_t nrequests;       /* Number of ICMP ECHO requests sent */
  uint16_t nreplies;        /* Number of matching ICMP ECHO replies received */
  uint16_t outsize;         /* Bytes(include ICMP header) to be sent */
  uint16_t id;              /* ICMP_ECHO id */
  uint16_t seqno;           /* ICMP_ECHO seqno */
  FAR const struct ping_info_s_m *info;
};

/****************************************************************************
 * From: apps/system/ping/ping.c
 *
 * Pre-processor Definitions
 ****************************************************************************/
#define ICMP_PING_DATALEN  56
#define ICMP_NPINGS        10    /* Default number of pings */
#define ICMP_POLL_DELAY    1000  /* 1 second in milliseconds */

/****************************************************************************
 * Private Functions
 ****************************************************************************/
// Help prints this
// Usage: ping [-c <count>] [-i <interval>] [-W <timeout>] [-s <size>] <hostname>
//   ping -h
// Where:
//   <hostname> is either an IPv4 address or the name of the remote host
//    that is requested the ICMPv4 ECHO reply.
//   -c <count> determines the number of pings.  Default 10.
//   -i <interval> is the default delay between pings (milliseconds).
//     Default 1000.
//   -W <timeout> is the timeout for wait response (milliseconds).
//     Default 1000.
//   -s <size> specifies the number of data bytes to be sent.  Default 56.
//   -h shows this text and exits.

/****************************************************************************
 * Name: ping_newid_m
 ****************************************************************************/

static inline uint16_t ping_newid_m(void)
{
  /* Revisit:  No thread safe */

  return ++g_pingid;
}

//===========================================================================
// All ping_text_to_host_m and fprintf(stderr) message now come here for routing to host
static void ping_text_to_host(int priority, FAR const IPTR char *fmt, ...)
{
  size_t maxStringLen = 256;
  char * finalString = malloc(maxStringLen);
  uint16_t requestType;
  
  switch (priority)
  {
  case LOG_INFO:
    requestType = HCOM_HOST_REQUEST_TEXT_INFORMATION;
    break;
  
  case LOG_ERR:
    requestType = HCOM_HOST_REQUEST_TEXT_ERROR;
    break;
  
  default:
    requestType = HCOM_HOST_REQUEST_TEXT_TRACE_MSG;
    break;
  }
  
  va_list args;
  va_start(args, fmt);

  // Create the complete message with prefix
  // The Nuttx version of snprintf will truncate the string based on the buffer
  // size but will always place a terminating NULL at the end.
  int stringLen = vsnprintf(finalString, maxStringLen - 1, fmt, args);

  hcom_nx_route_text_to_host(requestType, finalString, stringLen);

  // Only needed to see all text on syslog too
  // PeterM
  syslog(priority, finalString);

  va_end(args);
}

// /****************************************************************************
//  * Name: show_usage_m
//  ****************************************************************************/

static void show_usage_m(FAR const char *progname, int exitcode) noreturn_function;
static void show_usage_m(FAR const char *progname, int exitcode)
{
#if defined(CONFIG_LIBC_NETDB) && defined(CONFIG_NETDB_DNSCLIENT)
  ping_text_to_host(LOG_INFO, "\nUsage: %s [-c <count>] [-i <interval>] [-W <timeout>] [-s <size>] <hostname>\n", progname);
  ping_text_to_host(LOG_INFO, "       %s -h\n", progname);
  ping_text_to_host(LOG_INFO, "\nWhere:\n");
  ping_text_to_host(LOG_INFO, "  <hostname> is either an IPv4 address or the name of the remote host\n");
  ping_text_to_host(LOG_INFO, "   that is requested the ICMPv4 ECHO reply.\n");
#else
  ping_text_to_host(LOG_INFO, "\nUsage: %s [-c <count>] [-i <interval>] [-W <timeout>] [-s <size>] <ip-address>\n", progname);
  ping_text_to_host(LOG_INFO, "       %s -h\n", progname);
  ping_text_to_host(LOG_INFO, "\nWhere:\n");
  ping_text_to_host(LOG_INFO, "  <ip-address> is the IPv4 address request the ICMP ECHO reply.\n");
#endif
  ping_text_to_host(LOG_INFO, "  -c <count> determines the number of pings.  Default %u.\n",
         ICMP_NPINGS);
  ping_text_to_host(LOG_INFO, "  -i <interval> is the default delay between pings (milliseconds).\n");
  ping_text_to_host(LOG_INFO, "    Default %d.\n", ICMP_POLL_DELAY);
  ping_text_to_host(LOG_INFO, "  -W <timeout> is the timeout for wait response (milliseconds).\n");
  ping_text_to_host(LOG_INFO, "    Default %d.\n", ICMP_POLL_DELAY);
  ping_text_to_host(LOG_INFO, "  -s <size> specifies the number of data bytes to be sent.  Default %u.\n",
         ICMP_PING_DATALEN);
  ping_text_to_host(LOG_INFO, "  -h shows this text and exits.\n");
  exit(exitcode);
}

/****************************************************************************
 * Name: ping_gethostip_m
 *
 * Description:
 *   Call gethostbyname() to get the IP address associated with a hostname.
 *
 * Input Parameters
 *   hostname - The host name to use in the nslookup.
 *   ipv4addr - The location to return the IPv4 address.
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

static int ping_gethostip_m(FAR const char *hostname, FAR struct in_addr *dest)
{
#if defined(CONFIG_LIBC_NETDB) && defined(CONFIG_NETDB_DNSCLIENT)
  /* Netdb DNS client support is enabled */

  FAR struct hostent *he;
  he = gethostbyname(hostname);
  if (he == NULL)
    {
      return -ENOENT;
    }
  else if (he->h_addrtype == AF_INET)
    {
       memcpy(dest, he->h_addr, sizeof(in_addr_t));
    }
  else
    {
      return -ENOEXEC;
    }

  return OK;

#else /* CONFIG_LIBC_NETDB */

  /* No host name support */
  /* Convert strings to numeric IPv6 address */

  int ret = inet_pton(AF_INET, hostname, dest);

  /* The inet_pton() function returns 1 if the conversion succeeds. It will
   * return 0 if the input is not a valid IPv4 dotted-decimal string or -1
   * with errno set to EAFNOSUPPORT if the address family argument is
   * unsupported.
   */

  return (ret > 0) ? OK : ERROR;

#endif /* CONFIG_LIBC_NETDB */
}

/****************************************************************************
 * Name: icmp_callback_m
 ****************************************************************************/

static void icmp_callback_m(FAR struct ping_result_s_m *result, int code, int extra, int linenumb)
{
  result->code = code;
  result->extra = extra;
  result->linenumb = linenumb;
  
  result->info->callback(result);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: icmp_ping_m
 ****************************************************************************/
// Processing starts here
static void icmp_ping_m(FAR const struct ping_info_s_m *info)
{
  struct ping_result_s_m result;
  struct sockaddr_in destaddr;
  struct sockaddr_in fromaddr;
  struct icmp_hdr_s outhdr;
  FAR struct icmp_hdr_s *inhdr;
  struct pollfd recvfd;
  FAR uint8_t *iobuffer;
  FAR uint8_t *ptr;
  int32_t elapsed;
  clock_t kickoff;
  clock_t start;
  socklen_t addrlen;
  ssize_t nsent;
  ssize_t nrecvd;
  bool retry;
  int sockfd;
  int ret;
  int ch;
  int i;

  /* Initialize result structure */

  memset(&result, 0, sizeof(result));
  result.info = info;
  result.id = ping_newid_m();
  result.outsize = ICMP_M_IOBUFFER_SIZE(info->datalen);
  if (ping_gethostip_m(info->hostname, &result.dest) < 0)
    {
      icmp_callback_m(&result, ICMP_M_E_HOSTIP, 0, __LINE__);
      return;
    }
    // result.dest is "backward"
  ping_text_to_host(LOG_ERR, "==>The host name is:'%s', result.dest:0x%08x\n", info->hostname, result.dest);

  /* Allocate memory to hold ping buffer */
 
  iobuffer = (FAR uint8_t *)malloc(result.outsize);
  if (iobuffer == NULL)
    {
      icmp_callback_m(&result, ICMP_M_E_MEMORY, 0, __LINE__);
      return;
    }

  sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
  if (sockfd < 0)
    {
      icmp_callback_m(&result, ICMP_M_E_SOCKET, errno, __LINE__);
      free(iobuffer);
      return;
    }

  kickoff = clock();

  memset(&destaddr, 0, sizeof(struct sockaddr_in));
  destaddr.sin_family      = AF_INET;
  destaddr.sin_port        = 0;
  destaddr.sin_addr.s_addr = result.dest.s_addr;

  memset(&outhdr, 0, sizeof(struct icmp_hdr_s));
  outhdr.type              = ICMP_ECHO_REQUEST;
  outhdr.id                = htons(result.id);
  outhdr.seqno             = htons(result.seqno);

  icmp_callback_m(&result, ICMP_M_I_BEGIN, 0, __LINE__);

  while (result.nrequests < info->count)
    {
      /* Copy the ICMP header into the I/O buffer */

      memcpy(iobuffer, &outhdr, sizeof(struct icmp_hdr_s));

     /* Add some easily verifiable payload data */

      ptr = &iobuffer[sizeof(struct icmp_hdr_s)];
      ch  = 0x20;

      for (i = 0; i < info->datalen; i++)
        {
          *ptr++ = ch;
          if (++ch > 0x7e)
            {
              ch = 0x20;
            }
        }

      start = clock();
      nsent = sendto(sockfd, iobuffer, result.outsize, 0,
                     (FAR struct sockaddr*)&destaddr,
                     sizeof(struct sockaddr_in));
      if (nsent < 0)
        {
          icmp_callback_m(&result, ICMP_M_E_SENDTO, errno, __LINE__);
          goto done;
        }
      else if (nsent != result.outsize)
        {
          icmp_callback_m(&result, ICMP_M_E_SENDSMALL, nsent, __LINE__);
          goto done;
        }

      result.nrequests++;

      elapsed = 0;
      do
        {
          retry           = false;

          recvfd.fd       = sockfd;
          recvfd.events   = POLLIN;
          recvfd.revents  = 0;

          ret = poll(&recvfd, 1, info->timeout - elapsed);
          if (ret < 0)
            {
              icmp_callback_m(&result, ICMP_M_E_POLL, errno, __LINE__);
              goto done;
            }
          else if (ret == 0)
            {
              icmp_callback_m(&result, ICMP_M_W_TIMEOUT, info->timeout, __LINE__);
              continue;
            }

          /* Get the ICMP response (ignoring the sender) */

          addrlen = sizeof(struct sockaddr_in);
          nrecvd  = recvfrom(sockfd, iobuffer, result.outsize, 0,
                             (FAR struct sockaddr *)&fromaddr, &addrlen);
          if (nrecvd < 0)
            {
              icmp_callback_m(&result, ICMP_M_E_RECVFROM, errno, __LINE__);
              goto done;
            }
          else if (nrecvd < sizeof(struct icmp_hdr_s))
            {
              icmp_callback_m(&result, ICMP_M_E_RECVSMALL, nrecvd, __LINE__);
             goto done;
            }

          elapsed = (unsigned int)TICK2MSEC(clock() - start);
          inhdr   = (FAR struct icmp_hdr_s *)iobuffer;

          if (inhdr->type == ICMP_ECHO_REPLY)
            {
              if (ntohs(inhdr->id) != result.id)
                {
                  icmp_callback_m(&result, ICMP_M_W_IDDIFF, ntohs(inhdr->id), __LINE__);
                  retry = true;
                }
              else if (ntohs(inhdr->seqno) > result.seqno)
                {
                  icmp_callback_m(&result, ICMP_M_W_SEQNOBIG, ntohs(inhdr->seqno), __LINE__);
                  retry = true;
                }
              else
                {
                  bool verified = true;
                  int32_t pktdelay = elapsed;

                  if (ntohs(inhdr->seqno) < result.seqno)
                    {
                      icmp_callback_m(&result, ICMP_M_W_SEQNOSMALL, ntohs(inhdr->seqno), __LINE__);
                      pktdelay += info->delay;
                      retry     = true;
                    }

                  icmp_callback_m(&result, ICMP_M_I_ROUNDTRIP, pktdelay, __LINE__);

                  /* Verify the payload data */

                  if (nrecvd != result.outsize)
                    {
                      icmp_callback_m(&result, ICMP_M_W_RECVBIG, nrecvd, __LINE__);
                      verified = false;
                    }
                  else
                    {
                      ptr = &iobuffer[sizeof(struct icmp_hdr_s)];
                      ch  = 0x20;

                      for (i = 0; i < info->datalen; i++, ptr++)
                        {
                          if (*ptr != ch)
                            {
                              icmp_callback_m(&result, ICMP_M_W_DATADIFF, 0, __LINE__);
                              verified = false;
                              break;
                            }

                          if (++ch > 0x7e)
                            {
                              ch = 0x20;
                            }
                        }
                    }

                  /* Only count the number of good replies */

                  if (verified)
                    {
                      result.nreplies++;
                    }
                }
            }
          else
            {
              icmp_callback_m(&result, ICMP_M_W_TYPE, inhdr->type, __LINE__);
            }
        }
      while (retry && info->delay > elapsed && info->timeout > elapsed);

      /* Wait if necessary to preserved the requested ping rate */

      elapsed = (unsigned int)TICK2MSEC(clock() - start);
      if (elapsed < info->delay)
        {
          struct timespec rqt;
          unsigned int remaining;
          unsigned int sec;
          unsigned int frac;  /* In deciseconds */

          remaining   = info->delay - elapsed;
          sec         = remaining / MSEC_PER_SEC;
          frac        = remaining - MSEC_PER_SEC * sec;

          rqt.tv_sec  = sec;
          rqt.tv_nsec = frac * NSEC_PER_MSEC;

          (void)nanosleep(&rqt, NULL);
        }

      outhdr.seqno = htons(++result.seqno);
    }

done:
  icmp_callback_m(&result, ICMP_M_I_FINISH, TICK2MSEC(clock() - kickoff), __LINE__);
  close(sockfd);
  free(iobuffer);
}

/****************************************************************************
 * From: apps/system/ping/ping.c
 *
 ****************************************************************************/

/****************************************************************************
 * Name: ping_result_m
 ****************************************************************************/

static void ping_result_m(FAR const struct ping_result_s_m *result)
{
  switch (result->code)
    {
      case ICMP_M_E_HOSTIP:
        ping_text_to_host(LOG_ERR, "ERROR: ping_gethostip_m(%s) failed @%d\n",
                result->info->hostname, result->linenumb);
        break;

      case ICMP_M_E_MEMORY:
        ping_text_to_host(LOG_ERR, "ERROR: Failed to allocate memory @%d\n", result->linenumb);
        break;

      case ICMP_M_E_SOCKET:
        ping_text_to_host(LOG_ERR, "ERROR: socket() failed: %d @%d\n", result->extra, result->linenumb);
        break;

      case ICMP_M_I_BEGIN:
        ping_text_to_host(LOG_ERR, "PING %u.%u.%u.%u %u bytes of data @%d\n",
               (result->dest.s_addr      ) & 0xff,
               (result->dest.s_addr >> 8 ) & 0xff,
               (result->dest.s_addr >> 16) & 0xff,
               (result->dest.s_addr >> 24) & 0xff,
               result->info->datalen, result->linenumb);
        break;

      case ICMP_M_E_SENDTO:
        ping_text_to_host(LOG_ERR, "ERROR: sendto failed at seqno %u: %d @%d\n",
                result->seqno, result->extra, result->linenumb);
        break;

      case ICMP_M_E_SENDSMALL:
        ping_text_to_host(LOG_ERR, "ERROR: sendto returned %d, expected %u @%d\n",
                result->extra, result->outsize, result->linenumb);
        break;

      case ICMP_M_E_POLL:
        ping_text_to_host(LOG_ERR, "ERROR: poll failed: %d @%d\n", result->extra, result->linenumb);
        break;

      case ICMP_M_W_TIMEOUT:
        ping_text_to_host(LOG_INFO, "No response from %u.%u.%u.%u: icmp_seq=%u time=%d ms @%d\n",
               (result->dest.s_addr      ) & 0xff,
               (result->dest.s_addr >> 8 ) & 0xff,
               (result->dest.s_addr >> 16) & 0xff,
               (result->dest.s_addr >> 24) & 0xff,
               result->seqno, result->extra, result->linenumb);
        break;

      case ICMP_M_E_RECVFROM:
        ping_text_to_host(LOG_ERR, "ERROR: recvfrom failed: %d @%d\n", result->extra, result->linenumb);
        break;

      case ICMP_M_E_RECVSMALL:
        ping_text_to_host(LOG_ERR, "ERROR: short ICMP packet: %d @%d\n", result->extra, result->linenumb);
        break;

      case ICMP_M_W_IDDIFF:
        ping_text_to_host(LOG_ERR,
                "WARNING: Ignoring ICMP reply with ID %d.  "
                "Expected %u @%d\n",
                result->extra, result->id, result->linenumb);
        break;

      case ICMP_M_W_SEQNOBIG:
        ping_text_to_host(LOG_ERR,
                "WARNING: Ignoring ICMP reply to sequence %d.  "
                "Expected <= %u @%d\n",
                result->extra, result->seqno, result->linenumb);
        break;

      case ICMP_M_W_SEQNOSMALL:
        ping_text_to_host(LOG_ERR, "WARNING: Received after timeout @%d\n", result->linenumb);
        break;

      case ICMP_M_I_ROUNDTRIP:
        ping_text_to_host(LOG_ERR, "%u bytes from %u.%u.%u.%u: icmp_seq=%u time=%d ms @%d\n",
               result->info->datalen,
               (result->dest.s_addr      ) & 0xff,
               (result->dest.s_addr >> 8 ) & 0xff,
               (result->dest.s_addr >> 16) & 0xff,
               (result->dest.s_addr >> 24) & 0xff,
               result->seqno, result->extra, result->linenumb);
        break;

      case ICMP_M_W_RECVBIG:
        ping_text_to_host(LOG_ERR,
                "WARNING: Ignoring ICMP reply with different payload "
                "size: %d vs %u @%d\n",
                result->extra, result->outsize, result->linenumb);
        break;

      case ICMP_M_W_DATADIFF:
        ping_text_to_host(LOG_ERR, "WARNING: Echoed data corrupted @%d\n", result->linenumb);
        break;

      case ICMP_M_W_TYPE:
        ping_text_to_host(LOG_ERR, "WARNING: ICMP packet with unknown type: %d @%d\n",
                result->extra, result->linenumb);
        break;

      case ICMP_M_I_FINISH:
        if (result->nrequests > 0)
          {
            unsigned int tmp;

            /* Calculate the percentage of lost packets */

            tmp = (100 * (result->nrequests - result->nreplies) +
                  (result->nrequests >> 1)) /
                   result->nrequests;

            ping_text_to_host(LOG_ERR, "%u packets transmitted, %u received, %u%% packet loss, time %d ms, @%d\n",
                   result->nrequests, result->nreplies, tmp, result->extra, result->linenumb);
          }
        break;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/
static int ping_parse_entry(int argc, char **argv)
{
  struct ping_info_s_m info;
  FAR char *endptr;
  int exitcode;
  int option;

  info.count     = ICMP_NPINGS;
  info.datalen   = ICMP_PING_DATALEN;
  info.delay     = ICMP_POLL_DELAY;
  info.timeout   = ICMP_POLL_DELAY;
  info.callback  = ping_result_m;

  /* Parse command line options */

  exitcode = EXIT_FAILURE;

  while ((option = getopt(argc, argv, ":c:i:W:s:h")) != ERROR)
    {
      switch (option)
        {
          case 'c':
            {
              long count = strtol(optarg, &endptr, 10);
              if (count < 1 || count > UINT16_MAX)
                {
                  ping_text_to_host(LOG_ERR, "ERROR: <count> out of range: %ld\n", count);
                  goto errout_with_usage;
                }

              info.count = (uint16_t)count;
            }
            break;

          case 'i':
            {
              long delay = strtol(optarg, &endptr, 10);
              if (delay < 1 || delay > UINT16_MAX)
                {
                  ping_text_to_host(LOG_ERR, "ERROR: <interval> out of range: %ld\n", delay);
                  goto errout_with_usage;
                }

              info.delay = (int16_t)delay;
            }
            break;

          case 'W':
            {
              long timeout = strtol(optarg, &endptr, 10);
              if (timeout < 1 || timeout > UINT16_MAX)
                {
                  ping_text_to_host(LOG_ERR, "ERROR: <timeout> out of range: %ld\n", timeout);
                  goto errout_with_usage;
                }

              info.timeout = (int16_t)timeout;
            }
            break;

          case 's':
            {
              long datalen = strtol(optarg, &endptr, 10);
              if (datalen < 1 || datalen > UINT16_MAX)
                {
                  ping_text_to_host(LOG_ERR, "ERROR: <size> out of range: %ld\n", datalen);
                  goto errout_with_usage;
                }

              info.datalen = (uint16_t)datalen;
            }
            break;

          case 'h':
            exitcode = EXIT_SUCCESS;
            goto errout_with_usage;

          case ':':
            ping_text_to_host(LOG_ERR, "ERROR: Missing required argument\n");
            goto errout_with_usage;

          case '?':
          default:
            ping_text_to_host(LOG_ERR, "ERROR: Unrecognized option\n");
            goto errout_with_usage;
        }
    }

  /* There should be one final parameters remaining on the command line */

  if (optind >= argc)
    {
      ping_text_to_host(LOG_INFO, "ERROR: Missing required <ip-address> argument\n");
      goto errout_with_usage;
    }

  info.hostname = argv[optind];
  icmp_ping_m(&info);
  return EXIT_SUCCESS;

errout_with_usage:
  optind = 0;
  show_usage_m(argv[0], exitcode);
  return exitcode;  /* Not reachable */
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

// This is called via CLI to execute ANY available application. Currently, there
// is one, ping.
int hcom_nx_diagnostic_app_execute(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t msgLen)
{
  int ret = EXIT_SUCCESS;

  HcomProtoDiagCmdMsg_t *diagAppCmd = (HcomProtoDiagCmdMsg_t *) hdrMsg;

  size_t argLen = diagAppCmd->argListLen;
  char *argText = diagAppCmd->argListText;

  if(argLen == 0 || argText == NULL)
    return -1;
  
  // Convert the char array into a NULL terminated string
  #define HCOM_PING_MAX_TOKEN (16)  // max tokens 
  char *argv[HCOM_PING_MAX_TOKEN];
  
  char* inputStr = malloc(argLen + 1);
  memcpy(inputStr, argText, argLen);
  inputStr[argLen] = '\0';

  // Build argc and argv so we can call the ping code written for NSH
  int argc = 0;
  int tokIndex = 0;
  argv[tokIndex] = strtok(inputStr, " ");

  // Replace spaces with NULL
  while(argv[tokIndex] != NULL && tokIndex < HCOM_PING_MAX_TOKEN - 1)
  {
    argc++;
    argv[++tokIndex] = strtok(NULL, " ");
  }

  // Last element must be NULL
  argv[tokIndex] = NULL;

  // Execute the right command
  if(strcasecmp(argv[0], "ping") == 0)
    ret = ping_parse_entry(argc, argv);
  else
    ret = -1;

  free(inputStr);
  return ret;
}

#else

int hcom_nx_diagnostic_app_execute(const HcomProtoHdrMsg_t *hdrMsg,
          const size_t msgLen)
{
  return EXIT_SUCCESS;
}

#endif //#if defined(HCOM_INCLUDE_ETHERNET_IN_HCOM_IN_BUILD)
