/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/ethernet/meadow_ethnet_dhcp.c
 * 
 *   Copyright (C) 2021, 2023 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
 *
 ****************************************************************************/

/****************************************************************************
 * netutils/dhcpc/dhcpc.c
 *
 *   Copyright (C) 2007, 2009, 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Based heavily on portions of uIP:
 *
 *   Author: Adam Dunkels <adam@dunkels.com>
 *   Copyright (c) 2005, Swedish Institute of Computer Science
 *   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ****************************************************************************/

// The following was originally copied from /apps/netutils/dhcpc/dhcpc.c and
// modifed as needed to function in Nuttxland within our protected build.
// dhcpc.h was not copied but integrated into meadow_ethnet_common.h
// Peter Moody Sept 2021.

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <debug.h>

#include <arpa/inet.h>
#include <netinet/udp.h>
#include <nuttx/wqueue.h>
#include <meadow/hcom_shared_common.h>

#include <meadow/meadow_ethnet_common.h>

#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
#include "meadow_ethnet_local.h"

// Uncomment the #define below to turn on debug help macros.
#define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* DHCP Definitions */

#define STATE_INITIAL 0
#define STATE_HAVE_OFFER 1
#define STATE_HAVE_LEASE 2

#define BOOTP_BROADCAST 0x8000

#define DHCP_REQUEST 1
#define DHCP_REPLY 2
#define DHCP_HTYPE_ETHERNET 1
#define DHCP_HLEN_ETHERNET 6
#define DHCP_MSG_LEN 236

#define DHCPC_SERVER_PORT 67
#define DHCPC_CLIENT_PORT 68

#define DHCPDISCOVER 1
#define DHCPOFFER 2
#define DHCPREQUEST 3
#define DHCPDECLINE 4
#define DHCPACK 5
#define DHCPNAK 6
#define DHCPRELEASE 7

#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_DNS_SERVER 6
#define DHCP_OPTION_REQ_IPADDR 50
#define DHCP_OPTION_LEASE_TIME 51
#define DHCP_OPTION_MSG_TYPE 53
#define DHCP_OPTION_SERVER_ID 54
#define DHCP_OPTION_REQ_LIST 55
#define DHCP_OPTION_END 255

#define BUFFER_SIZE 256

#define MEADOW_ETHNET_DHCP_TIMEOUT_RETRY_COUNT (3)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct dhcp_msg
{
  uint8_t op;
  uint8_t htype;
  uint8_t hlen;
  uint8_t hops;
  uint8_t xid[4];
  uint16_t secs;
  uint16_t flags;
  uint8_t ciaddr[4];
  uint8_t yiaddr[4];
  uint8_t siaddr[4];
  uint8_t giaddr[4];
  uint8_t chaddr[16];
#ifndef CONFIG_NET_DHCP_LIGHT
  uint8_t sname[64];
  uint8_t file[128];
#endif
  uint8_t options[312];
};

// Only used in this file
struct meadow_eth_dhcp_state_s
{
  FAR const char *interface;
  FAR const void *ds_macaddr;
  int ds_maclen;
  int sockfd;
  struct in_addr ipaddr;
  struct in_addr serverid;
  struct dhcp_msg packet;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

static const uint8_t xid[4] = {0xad, 0xde, 0x12, 0x23};
static const uint8_t magic_cookie[4] = {99, 130, 83, 99};

/****************************************************************************
 * Private Functions
 ****************************************************************************/


/****************************************************************************
 * Name: meadow_eth_diag_show_dhcp_info
 ****************************************************************************/
static void meadow_eth_diag_show_dhcp_info(struct dhcp_info_s *dhcp_info)
{
  MEADOW_TRACE_INFORMATION("  Got IP address %d.%d.%d.%d\n",
        (dhcp_info->ipaddr.s_addr) & 0xff,
        (dhcp_info->ipaddr.s_addr >> 8) & 0xff,
        (dhcp_info->ipaddr.s_addr >> 16) & 0xff,
        (dhcp_info->ipaddr.s_addr >> 24) & 0xff);
  MEADOW_TRACE_INFORMATION("  Got netmask %d.%d.%d.%d\n",
        (dhcp_info->netmask.s_addr) & 0xff,
        (dhcp_info->netmask.s_addr >> 8) & 0xff,
        (dhcp_info->netmask.s_addr >> 16) & 0xff,
        (dhcp_info->netmask.s_addr >> 24) & 0xff);
  MEADOW_TRACE_INFORMATION("  Got DNS server %d.%d.%d.%d\n",
        (dhcp_info->dnsaddr.s_addr) & 0xff,
        (dhcp_info->dnsaddr.s_addr >> 8) & 0xff,
        (dhcp_info->dnsaddr.s_addr >> 16) & 0xff,
        (dhcp_info->dnsaddr.s_addr >> 24) & 0xff);
  MEADOW_TRACE_INFORMATION("  Got default router %d.%d.%d.%d\n",
        (dhcp_info->default_router.s_addr) & 0xff,
        (dhcp_info->default_router.s_addr >> 8) & 0xff,
        (dhcp_info->default_router.s_addr >> 16) & 0xff,
        (dhcp_info->default_router.s_addr >> 24) & 0xff);
  MEADOW_TRACE_INFORMATION("  Lease expiration time %d seconds\n", dhcp_info->lease_time);
}

/****************************************************************************
 * Name: meadow_eth_dhcp_add<option>
 ****************************************************************************/

static FAR uint8_t *meadow_eth_dhcp_addmsgtype(FAR uint8_t *optptr, uint8_t type)
{
  *optptr++ = DHCP_OPTION_MSG_TYPE;
  *optptr++ = 1;
  *optptr++ = type;
  return optptr;
}

static FAR uint8_t *meadow_eth_dhcp_addserverid(FAR struct in_addr *serverid,
                                      FAR uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_SERVER_ID;
  *optptr++ = 4;
  memcpy(optptr, &serverid->s_addr, 4);
  return optptr + 4;
}

static FAR uint8_t *meadow_eth_dhcp_addreqipaddr(FAR struct in_addr *ipaddr,
                                       FAR uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_REQ_IPADDR;
  *optptr++ = 4;
  memcpy(optptr, &ipaddr->s_addr, 4);
  return optptr + 4;
}

static FAR uint8_t *meadow_eth_dhcp_addreqoptions(FAR uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_REQ_LIST;
  *optptr++ = 3;
  *optptr++ = DHCP_OPTION_SUBNET_MASK;
  *optptr++ = DHCP_OPTION_ROUTER;
  *optptr++ = DHCP_OPTION_DNS_SERVER;
  return optptr;
}

static FAR uint8_t *meadow_eth_dhcp_addend(FAR uint8_t *optptr)
{
  *optptr++ = DHCP_OPTION_END;
  return optptr;
}

/****************************************************************************
 * Name: meadow_eth_dhcp_sendmsg
 ****************************************************************************/

static int meadow_eth_dhcp_sendmsg(FAR struct meadow_eth_dhcp_state_s *pdhcpc,
                         FAR struct dhcp_info_s *presult, int msgtype)
{
  struct sockaddr_in addr;
  FAR uint8_t *pend;
  in_addr_t serverid = INADDR_BROADCAST;
  int len;

  /* Create the common message header settings */

  memset(&pdhcpc->packet, 0, sizeof(struct dhcp_msg));
  pdhcpc->packet.op = DHCP_REQUEST;
  pdhcpc->packet.htype = DHCP_HTYPE_ETHERNET;
  pdhcpc->packet.hlen = pdhcpc->ds_maclen;
  memcpy(pdhcpc->packet.xid, xid, 4);
  memcpy(pdhcpc->packet.chaddr, pdhcpc->ds_macaddr, pdhcpc->ds_maclen);
  memset(&pdhcpc->packet.chaddr[pdhcpc->ds_maclen], 0, 16 - pdhcpc->ds_maclen);
  memcpy(pdhcpc->packet.options, magic_cookie, sizeof(magic_cookie));

  /* Add the common header options */

  pend = &pdhcpc->packet.options[4];
  pend = meadow_eth_dhcp_addmsgtype(pend, msgtype);

  /* Handle the message specific settings */

  switch (msgtype)
  {
    /* Broadcast DISCOVER message to all servers */

  case DHCPDISCOVER:
    /* REVISIT: We don't need the broadcast flag since we can receive
         * unicast traffic before being fully configured.
         */
    pdhcpc->packet.flags = HTONS(BOOTP_BROADCAST); /*  Broadcast bit. */
    pend = meadow_eth_dhcp_addreqoptions(pend);
    break;

    /* Send REQUEST message to the server that sent the *first* OFFER */

  case DHCPREQUEST:
    /* REVISIT: We don't need the broadcast flag since we can receive
         * unicast traffic before being fully configured.
         */

    pdhcpc->packet.flags = HTONS(BOOTP_BROADCAST); /*  Broadcast bit. */
    pend = meadow_eth_dhcp_addserverid(&pdhcpc->serverid, pend);
    pend = meadow_eth_dhcp_addreqipaddr(&pdhcpc->ipaddr, pend);
    break;

    /* Send DECLINE message to the server that sent the *last* OFFER */

  case DHCPDECLINE:
    memcpy(pdhcpc->packet.ciaddr, &presult->ipaddr.s_addr, 4);
    pend = meadow_eth_dhcp_addserverid(&presult->serverid, pend);
    serverid = presult->serverid.s_addr;
    break;

  default:
    return ERROR;
  }

  pend = meadow_eth_dhcp_addend(pend);
  len = pend - (uint8_t *)&pdhcpc->packet;

  /* Send the request */

  addr.sin_family = AF_INET;
  addr.sin_port = HTONS(DHCPC_SERVER_PORT);
  addr.sin_addr.s_addr = serverid;

  return sendto(pdhcpc->sockfd, &pdhcpc->packet, len, 0,
                (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
}

/****************************************************************************
 * Name: meadow_eth_dhcp_parseoptions
 ****************************************************************************/

static uint8_t meadow_eth_dhcp_parseoptions(FAR struct dhcp_info_s *presult,
                                  FAR uint8_t *optptr, int len)
{
  FAR uint8_t *end = optptr + len;
  uint8_t type = 0;

  while (optptr < end)
  {
    switch (*optptr)
    {
    case DHCP_OPTION_SUBNET_MASK:
      /* Get subnet mask in network order */

      memcpy(&presult->netmask.s_addr, optptr + 2, 4);
      break;

    case DHCP_OPTION_ROUTER:
      /* Get the default router address in network order */

      memcpy(&presult->default_router.s_addr, optptr + 2, 4);
      break;

    case DHCP_OPTION_DNS_SERVER:
      /* Get the DNS server address in network order */

      memcpy(&presult->dnsaddr.s_addr, optptr + 2, 4);
      break;

    case DHCP_OPTION_MSG_TYPE:
      /* Get message type */

      type = *(optptr + 2);
      break;

    case DHCP_OPTION_SERVER_ID:
      /* Get server address in network order */

      memcpy(&presult->serverid.s_addr, optptr + 2, 4);
      break;

    case DHCP_OPTION_LEASE_TIME:
    {
      /* Get lease time (in seconds) in host order */

      uint16_t tmp[2];
      memcpy(tmp, optptr + 2, 4);
      presult->lease_time = ((uint32_t)ntohs(tmp[0])) << 16 |
                            (uint32_t)ntohs(tmp[1]);
    }
    break;

    case DHCP_OPTION_END:
      return type;
    }

    optptr += optptr[1] + 2;
  }

  return type;
}

/****************************************************************************
 * Name: meadow_eth_dhcp_parsemsg
 ****************************************************************************/

static uint8_t meadow_eth_dhcp_parsemsg(FAR struct meadow_eth_dhcp_state_s *pdhcpc, int buflen,
                              FAR struct dhcp_info_s *presult)
{
  if (pdhcpc->packet.op == DHCP_REPLY &&
      memcmp(pdhcpc->packet.xid, xid, sizeof(xid)) == 0 &&
      memcmp(pdhcpc->packet.chaddr, pdhcpc->ds_macaddr, pdhcpc->ds_maclen) == 0)
  {
    memcpy(&presult->ipaddr.s_addr, pdhcpc->packet.yiaddr, 4);
    return meadow_eth_dhcp_parseoptions(presult, &pdhcpc->packet.options[4], buflen);
  }

  return 0;
}

/****************************************************************************
 * Name: meadow_eth_dhcp_open
 ****************************************************************************/

static void *meadow_eth_dhcp_open(FAR const char *interface, FAR const void *macaddr,
                     int maclen)
{
  FAR struct meadow_eth_dhcp_state_s *pdhcpc;
  struct sockaddr_in addr;
  struct timeval tv;
  int ret;

  /* Allocate an internal DHCP structure */

  pdhcpc = (FAR struct meadow_eth_dhcp_state_s *)malloc(sizeof(struct meadow_eth_dhcp_state_s));
  if (pdhcpc)
  {
    /* Initialize the allocated structure */

    memset(pdhcpc, 0, sizeof(struct meadow_eth_dhcp_state_s));
    pdhcpc->interface = interface;
    pdhcpc->ds_macaddr = macaddr;
    pdhcpc->ds_maclen = maclen;

    /* Create a UDP socket */

    pdhcpc->sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (pdhcpc->sockfd < 0)
    {
      MEADOW_TRACE_INFORMATION("socket handle %d\n", ret);
      free(pdhcpc);
      return NULL;
    }

    /* Bind the socket */

    addr.sin_family = AF_INET;
    addr.sin_port = HTONS(DHCPC_CLIENT_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(pdhcpc->sockfd, (struct sockaddr *)&addr,
               sizeof(struct sockaddr_in));
    if (ret < 0)
    {
      MEADOW_TRACE_INFORMATION("bind status %d\n", ret);
      close(pdhcpc->sockfd);
      free(pdhcpc);
      return NULL;
    }

    /* Configure for read timeouts */

    tv.tv_sec = 10;
    tv.tv_usec = 0;

    ret = setsockopt(pdhcpc->sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv,
                     sizeof(struct timeval));
    if (ret < 0)
    {
      MEADOW_TRACE_INFORMATION("setsockopt(RCVTIMEO) status %d\n", ret);
      close(pdhcpc->sockfd);
      free(pdhcpc);
      return NULL;
    }

#ifdef CONFIG_NET_UDP_BINDTODEVICE
    // Bind socket to interface, because UDP packets have to be sent to the
    // broadcast address at a moment when it is not possible to decide the
    // target network device using the local or remote address (which is, by
    // definition and purpose of DHCP, undefined yet).
    ret = setsockopt(pdhcpc->sockfd, SOL_UDP, UDP_BINDTODEVICE,
                     pdhcpc->interface, strlen(pdhcpc->interface));
    if (ret < 0)
    {
      MEADOW_TRACE_INFORMATION("setsockopt(BINDTODEVICE) status %d\n", ret);
      close(pdhcpc->sockfd);
      free(pdhcpc);
      return NULL;
    }
#endif
  }

  return (FAR void *)pdhcpc;
}

/****************************************************************************
 * Name: meadow_eth_dhcp_close
 ****************************************************************************/

static void meadow_eth_dhcp_close(FAR void *handle)
{
  struct meadow_eth_dhcp_state_s *pdhcpc = (struct meadow_eth_dhcp_state_s *)handle;

  if (pdhcpc)
  {
    if (pdhcpc->sockfd)
    {
      close(pdhcpc->sockfd);
      pdhcpc->sockfd = 0;
    }

    free(pdhcpc);
  }
}

/****************************************************************************
 * Name: meadow_eth_dhcp_request
 ****************************************************************************/
// This function is only for dhcp. When called it will broadcast a dhcp
// discovery message to any listening dhcp server.
static int meadow_eth_dhcp_request(FAR void *handle, FAR struct dhcp_info_s *presult)
{
  int ret;
  FAR struct meadow_eth_dhcp_state_s *pdhcpc = (FAR struct meadow_eth_dhcp_state_s *)handle;
  struct in_addr oldaddr;
  struct in_addr newaddr;
  ssize_t result;
  uint8_t msgtype;
  int retries;
  int state;

  // Save the currently assigned IP address (should be INADDR_ANY)
  oldaddr.s_addr = 0;
  ret = meadow_eth_utils_get_ipv4(pdhcpc->interface, &oldaddr);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-failed ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Loop until we receive the lease (or an error occurs)
  // Note: a outer do while loop with 2 inner do while loops.
  do
  {
    // Set the IP address to INADDR_ANY.
    newaddr.s_addr = INADDR_ANY;
    (void)meadow_eth_utils_set_ipv4(pdhcpc->interface, &newaddr);

    // Loop sending DISCOVER until we receive an OFFER from a DHCP server. We
    // will lock on to the first OFFER and decline any subsequent offers
    // (which will happen if there are more than one DHCP servers on the
    // network.

    state = STATE_INITIAL;
    do
    {
      // Send the DISCOVER command
      MEADOW_TRACE_INFORMATION("Broadcast DISCOVER\n"); 
      ret = meadow_eth_dhcp_sendmsg(pdhcpc, presult, DHCPDISCOVER);
      if(ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-failed ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);
        return ret;
      }

      // Get the DHCPOFFER response
      result = recv(pdhcpc->sockfd, &pdhcpc->packet,
                    sizeof(struct dhcp_msg), 0);
      if (result >= 0)
      {
        msgtype = meadow_eth_dhcp_parsemsg(pdhcpc, result, presult);
        if (msgtype == DHCPOFFER)
        {
          // Save the serverid from the presult so that it is not clobbered by a
          // new OFFER.
          MEADOW_TRACE_INFORMATION("Received OFFER from %08x, offered:%08x\n",
                ntohl(presult->serverid.s_addr),
                ntohl(presult->ipaddr.s_addr));

          pdhcpc->ipaddr.s_addr = presult->ipaddr.s_addr;
          pdhcpc->serverid.s_addr = presult->serverid.s_addr;

          // Temporarily use the address offered by the server and break out
          // of the loop.
          (void)meadow_eth_utils_set_ipv4(pdhcpc->interface,
                                      &presult->ipaddr);
          state = STATE_HAVE_OFFER;
        }
      }
      else if (errno != EAGAIN)
      {
        // An error has occurred.
        syslog(LOG_ERR, "%s@%d-failed ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);
        return ret;   // Let caller decide what to do
      }
      else
      {
        // This is a timeout error (meaning that nothing was received on this
        // socket for a period of time). We loop and send the DISCOVER command
        // again.
      }
    } while (state == STATE_INITIAL);

    // Loop sending the REQUEST up to three times (if there is no response)
    retries = 0;
    do
    {
      // Send the REQUEST acceptance message to obtain the lease offered
      MEADOW_TRACE_INFORMATION("Send Lease offer acceptance message\n");
      ret = meadow_eth_dhcp_sendmsg(pdhcpc, presult, DHCPREQUEST);
      if(ret < 0)
      {
        syslog(LOG_ERR, "%s@%d-failed ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);
        return ret;
      }

      retries++;

      // Get the ACK/NAK response to the REQUEST (or timeout)
      result = recv(pdhcpc->sockfd, &pdhcpc->packet,
                    sizeof(struct dhcp_msg), 0);
      if (result >= 0)
      {
        // Parse the response
        msgtype = meadow_eth_dhcp_parsemsg(pdhcpc, result, presult);

        if (msgtype == DHCPACK)
        {
          // The ACK response means that the server has accepted our request and
          // we have the lease.
          MEADOW_TRACE_INFORMATION("Received Lease offer ACK\n");
          state = STATE_HAVE_LEASE;
        }
        else if (msgtype == DHCPNAK)
        {
          // NAK means the server has refused our request. Break out of this loop
          // with state == STATE_HAVE_OFFER and send DISCOVER again.
          MEADOW_TRACE_INFORMATION("Received Lease offer NAK\n");
          break;
        }
        else if (msgtype == DHCPOFFER)
        {
          // If we get any OFFERs from other servers, then decline them now
          // and continue waiting for the ACK from the server that we
          // requested from.
          MEADOW_TRACE_INFORMATION("Received another OFFER, send DECLINE\n");
          ret = meadow_eth_dhcp_sendmsg(pdhcpc, presult, DHCPDECLINE);
          if(ret > 0)
          {
            syslog(LOG_ERR, "%s@%d-failed ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);
            return ret;
          }
        }
        else
        {
          // Otherwise, it is something that we do not recognize
          MEADOW_TRACE_INFORMATION("Ignoring msgtype=%d\n", msgtype);
        }
      }
      else if (errno != EAGAIN)
      {
        // A non-EAGAIN error occurred. EAGAIN means the that nothing was
        // received on this socket for a long of time. Note:if this was the
        // EAGAIN error we will simply loop and send DISCOVER again
        syslog(LOG_ERR, "%s@%d-failed ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);

        ret = meadow_eth_utils_set_ipv4(pdhcpc->interface, &oldaddr);
        if(ret > 0)
        {
          syslog(LOG_ERR, "%s@%d-failed ret:%d errno:%d\n", thisFile, __LINE__, ret, errno);
        }
        return errno;
      }

      // Loop on errno of EAGAIN
    } while (state == STATE_HAVE_OFFER && retries < MEADOW_ETHNET_DHCP_TIMEOUT_RETRY_COUNT);

  } while (state != STATE_HAVE_LEASE);

#if defined (USE_MEADOW_DEBUG_HELPERS)
  meadow_eth_diag_show_dhcp_info(presult);
#endif

  return OK;
}

/****************************************************************************
 * Name: meadow_eth_dhcp_get_dhcp_info
 ****************************************************************************/
// Use DHCP to get and set the ip address and related parameters
int meadow_eth_dhcp_get_dhcp_info(struct dhcp_info_s *dhcp_info,
          const char *interfaceName, const uint8_t *macAddr)
{
  int ret;
  FAR void *handle;

  // Allocates memory for handle
  handle = meadow_eth_dhcp_open(interfaceName, macAddr, IFHWADDRLEN);
  if (handle == NULL)
  {
    syslog(LOG_ERR, "%s@%d-failed, handle == NULL, errno:%d\n",
           thisFile, __LINE__, errno);
    return -errno;
  }

  MEADOW_TRACE_INFORMATION("%s@%d- REQUESTING IP via DHCP\n", thisFile, __LINE__);

  // This call sets all the dhcp_info fields
  ret = meadow_eth_dhcp_request(handle, dhcp_info);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_dhcp_request() failed:%d, errno:%d\n",
           thisFile, __LINE__, ret, errno);
    meadow_eth_dhcp_close(handle);
    return -errno;
  }

  // Save our new IP address
  ret = meadow_eth_utils_set_ipv4(interfaceName, &dhcp_info->ipaddr);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_ipv4() failed:%d, errno:%d\n",
           thisFile, __LINE__, ret, errno);
    meadow_eth_dhcp_close(handle);
    return -errno;
  }

  if (dhcp_info->netmask.s_addr != 0)
  {
    // netlib_set_ipv4netmask
    ret = meadow_eth_utils_set_ipv4_mask(interfaceName, &dhcp_info->netmask);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_ipv4_mask() failed:%d, errno:%d\n",
             thisFile, __LINE__, ret, errno);
      meadow_eth_dhcp_close(handle);
      return -errno;
    }
  }

  if (dhcp_info->default_router.s_addr != 0)
  {
    // netlib_set_dripv4addr
    ret = meadow_eth_utils_set_router(interfaceName, &dhcp_info->default_router);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_router() failed:%d, errno:%d\n",
             thisFile, __LINE__, ret, errno);
      meadow_eth_dhcp_close(handle);
      return -errno;
    }
  }

  if (dhcp_info->dnsaddr.s_addr != 0)
  {
    // netlib_set_ipv4dnsaddr
    ret = meadow_eth_utils_set_dns(&dhcp_info->dnsaddr);
    if (ret < 0)
    {
      syslog(LOG_ERR, "%s@%d-meadow_eth_utils_set_dns() failed:%d, errno:%d\n",
             thisFile, __LINE__, ret, errno);
      meadow_eth_dhcp_close(handle);
      return -errno;
    }
  }

  meadow_eth_dhcp_close(handle);
  return OK;
}

#endif // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
