/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/hcom_nx/ethernet/hcom_nx_ethnet_utils.c
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

// This module contains utility ethernet access, mostly via ioctl

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <ctype.h>
#include <stdint.h>

#include <sys/ioctl.h>
#include <nuttx/net/dns.h>
#include <net/route.h>
#include <net/if.h>

#include <meadow/hcom_shared_common.h>

#if defined(CONFIG_HCOM_INCLUDE_ETHNET_IN_BUILD)

#include "hcom_nx_ethnet_local.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int ethnet_utils_get_hw_mac(const char *interfaceName, uint8_t *macAddr)
{
  int ret = ERROR;
  if (interfaceName && macAddr)
  {
    struct ifreq req;
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd >= 0)
    {
      memset (&req, 0, sizeof(struct ifreq));

      /* Put the driver name into the request */

      strncpy(req.ifr_name, interfaceName, IFNAMSIZ);

      /* Perform the ioctl to get the MAC address */

      ret = ioctl(sockfd, SIOCGIFHWADDR, (unsigned long)&req);
      if (ret >= 0)
      {
        /* Return the MAC address */

        memcpy(macAddr, &req.ifr_hwaddr.sa_data, IFHWADDRLEN);
      }

      close(sockfd);
    }
  }
  return ret;

}

//==========================================================================
int ethnet_utils_exec_ifup(const char *interfaceName)
{
  int ret = ERROR;
  if (interfaceName)
  {
    struct ifreq req;

    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd >= 0)
    {
      memset (&req, 0, sizeof(struct ifreq));

      /* Put the driver name into the request */

      strncpy(req.ifr_name, interfaceName, IFNAMSIZ);

      /* Perform the ioctl to ifup flag */

      req.ifr_flags |= IFF_UP;

      ret = ioctl(sockfd, SIOCSIFFLAGS, (unsigned long)&req);
      close(sockfd);
    }
  }

  return ret;
}

//==========================================================================
int ethnet_utils_set_mac(const char *interfaceName,
          const uint8_t *macAddr)
{
  int ret = ERROR;

  if (interfaceName && macAddr)
  {
    struct ifreq req;

    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd >= 0)
    {
      /* Put the driver name into the request */

      strncpy(req.ifr_name, interfaceName, IFNAMSIZ);

      /* Put the new MAC address into the request */

      req.ifr_hwaddr.sa_family = AF_INET;
      memcpy(&req.ifr_hwaddr.sa_data, macAddr, IFHWADDRLEN);

      /* Perform the ioctl to set the MAC address */

      ret = ioctl(sockfd, SIOCSIFHWADDR, (unsigned long)&req);
      close(sockfd);
    }
  }

  return ret;
}

//==========================================================================
int ethnet_utils_get_mac(const char *interfaceName,
          uint8_t *macAddr)
{
  int ret = ERROR;
  if (interfaceName && macAddr)
    {
      /* Get a socket (only so that we get access to the INET subsystem) */

      int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
      if (sockfd >= 0)
        {
          struct ifreq req;
          memset (&req, 0, sizeof(struct ifreq));

          /* Put the driver name into the request */

          strncpy(req.ifr_name, interfaceName, IFNAMSIZ);

          /* Perform the ioctl to get the MAC address */

          ret = ioctl(sockfd, SIOCGIFHWADDR, (unsigned long)&req);
          if (!ret)
            {
              /* Return the MAC address */

              memcpy(macAddr, &req.ifr_hwaddr.sa_data, IFHWADDRLEN);
            }

          close(sockfd);
        }
    }
  return ret;
}

//==========================================================================
int ethnet_utils_set_ipv4(const char *interfaceName,
          const struct in_addr *addr)
{
  int ret = ERROR;

  if (interfaceName && addr)
  {
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd >= 0)
    {
      FAR struct sockaddr_in *inaddr;
      struct ifreq req;

      /* Add the device name to the request */

      strncpy(req.ifr_name, interfaceName, IFNAMSIZ);

      /* Add the INET address to the request */

      inaddr             = (FAR struct sockaddr_in *)&req.ifr_addr;
      inaddr->sin_family = AF_INET;
      inaddr->sin_port   = 0;
      memcpy(&inaddr->sin_addr, addr, sizeof(struct in_addr));

      ret = ioctl(sockfd, SIOCSIFADDR, (unsigned long)&req);
      close(sockfd);
    }
  }

  return ret;
}

//==========================================================================
int ethnet_utils_get_ipv4(const char *interfaceName, struct in_addr *addr)
{
  int ret = ERROR;

  if (interfaceName && addr)
    {
      int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
      if (sockfd >= 0)
        {
          struct ifreq req;

          strncpy(req.ifr_name, interfaceName, IFNAMSIZ);

          ret = ioctl(sockfd, SIOCGIFADDR, (unsigned long)&req);
          if (!ret)
            {
              FAR struct sockaddr_in *req_addr;

              req_addr = (FAR struct sockaddr_in*)&req.ifr_addr;
              memcpy(addr, &req_addr->sin_addr, sizeof(struct in_addr));
            }

          close(sockfd);
        }
    }

  return ret;
}

//==========================================================================
int ethnet_utils_set_ipv4_mask(const char *interfaceName,
      const struct in_addr *addr)
{
  int ret = ERROR;

  if (interfaceName && addr)
    {
      int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
      if (sockfd >= 0)
        {
          FAR struct sockaddr_in *inaddr;
          struct ifreq req;

          /* Add the device name to the request */

          strncpy(req.ifr_name, interfaceName, IFNAMSIZ);

          /* Add the INET address to the request */

          inaddr             = (FAR struct sockaddr_in *)&req.ifr_addr;
          inaddr->sin_family = AF_INET;
          inaddr->sin_port   = 0;
          memcpy(&inaddr->sin_addr, addr, sizeof(struct in_addr));

          ret = ioctl(sockfd, SIOCSIFNETMASK, (unsigned long)&req);
          close(sockfd);
        }
    }

  return ret;
}

//==========================================================================
int ethnet_utils_set_dns(const struct in_addr *inaddr)
{
  struct sockaddr_in addr;
  int ret = -EINVAL;

  if (inaddr)
  {
    /* Set the IPv4 DNS server address */

    addr.sin_family = AF_INET;
    addr.sin_port   = 0;
    memcpy(&addr.sin_addr, inaddr, sizeof(struct in_addr));

    // part of nuttx/libs/libc/netdb/lib_dnsaddserver.c
    ret = dns_add_nameserver((FAR const struct sockaddr *)&addr,
                              sizeof(struct sockaddr_in));
  }

  return ret;
}

//==========================================================================
int ethnet_utils_set_router(const char *interfaceName,
          const struct in_addr *addr)
{
  int ret = ERROR;

#ifdef CONFIG_NET_ROUTE
  struct sockaddr_in target;
  struct sockaddr_in netmask;
  struct sockaddr_in router;

  memset(&target, 0, sizeof(target));
  target.sin_family  = AF_INET;

  memset(&netmask, 0, sizeof(netmask));
  netmask.sin_family  = AF_INET;

  router.sin_addr    = *addr;
  router.sin_family  = AF_INET;
#endif

  if (interfaceName && addr)
  {
    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd >= 0)
    {
      FAR struct sockaddr_in *inaddr;
      struct ifreq req;

      /* Add the device name to the request */

      strncpy(req.ifr_name, interfaceName, IFNAMSIZ);

      /* Add the INET address to the request */

      inaddr             = (FAR struct sockaddr_in *)&req.ifr_addr;
      inaddr->sin_family = AF_INET;
      inaddr->sin_port   = 0;
      memcpy(&inaddr->sin_addr, addr, sizeof(struct in_addr));

      ret = ioctl(sockfd, SIOCSIFDSTADDR, (unsigned long)&req);

#ifdef CONFIG_NET_ROUTE
      if (OK == ret)
      {
        /* Delete the default route first */
        /* This call fails if no default route exists, but it's OK */

        (void)delroute(sockfd,
                      (FAR struct sockaddr_storage *)&target,
                      (FAR struct sockaddr_storage *)&netmask);

        /* Then add the new default route */

        ret = addroute(sockfd,
                      (FAR struct sockaddr_storage *)&target,
                      (FAR struct sockaddr_storage *)&netmask,
                      (FAR struct sockaddr_storage *)&router);
      }
#endif

      close(sockfd);
    }
  }

  return ret;
}

#endif    // #if defined(CONFIG_HCOM_INCLUDE_ETHNET_IN_BUILD)
