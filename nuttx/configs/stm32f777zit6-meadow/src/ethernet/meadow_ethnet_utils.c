/****************************************************************************
 * /nuttx/configs/stm32f777zit6-meadow/src/ethernet/meadow_meadow_eth_utils.c
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
#include <up_arch.h>
#include <chip.h>
#include <stm32_ethernet.h>

#if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
#include <meadow/meadow_ethnet_common.h>
#include "meadow_ethnet_local.h"

// Uncomment the #define below to turn on debug help macros.
#define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

// For timeout of certain read/write operation that need to wait for data to be
// available/delivered
#define LAN9355_PHY_READ_TIMEOUT  (0x0004ffff)
#define LAN9355_PHY_WRITE_TIMEOUT (0x0004ffff)

/****************************************************************************
 * Private Data
 ****************************************************************************/
static char *thisFile = __FILE__;

/****************************************************************************
 * Public Functions
 ****************************************************************************/
void meadow_eth_utils_syslog_ip_mac(void)
{
  uint8_t macAddr[IFHWADDRLEN];
  struct in_addr ipaddr;
  ipaddr.s_addr = 0;

  meadow_eth_utils_get_ipv4(MEADOW_ETHMAC_DEVICENAME, &ipaddr);
  meadow_eth_utils_get_mac(MEADOW_ETHMAC_DEVICENAME, macAddr);

  syslog(LOG_INFO, "Ethernet active MAC:%02x:%02x:%02x:%02x:%02x:%02x, IP:%d.%d.%d.%d\n",
            ((uint8_t*)macAddr)[0], ((uint8_t*)macAddr)[1], ((uint8_t*)macAddr)[2],
            ((uint8_t*)macAddr)[3], ((uint8_t*)macAddr)[4], ((uint8_t*)macAddr)[5],
            (ipaddr.s_addr       ) & 0xff,
            (ipaddr.s_addr >> 8  ) & 0xff,
            (ipaddr.s_addr >> 16 ) & 0xff,
            (ipaddr.s_addr >> 24 ) & 0xff);
}

//=======================================================
// Read and test the LAN9355 chip's ID to verify that it is indeed a LAN9355
int meadow_eth_utils_verify_lan9355(void)
{
  int ret;
  uint32_t lanChipId;

  // Verify this is a LAN9355 chip
  ret = meadow_lan9355_phyread_32(LAN9355_CHIP_ID_REVISION_REGISTER,
            &lanChipId);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_lan9355_phyread_32 failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  MEADOW_TRACE_INFORMATION("%s@%d-mon - Chip Id:0x%08x\n", thisFile, __LINE__, lanChipId);

  if((lanChipId & 0xffff0000) != 0x93550000)
  {
    syslog(LOG_ERR, "%s@%d-Ethernet chip must be LAN9355\n",
                thisFile, __LINE__);
    return -ENOTSUP;
  }
  return OK;
}

//=======================================================
int meadow_eth_utils_get_hw_mac(const char *interfaceName, uint8_t *macAddr)
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
int meadow_eth_utils_exec_ifup(const char *interfaceName)
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
int meadow_eth_utils_exec_ifdown(const char *interfaceName)
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

      /* Perform the ioctl to ifdown flag */

      req.ifr_flags |= IFF_DOWN;

      ret = ioctl(sockfd, SIOCSIFFLAGS, (unsigned long)&req);
      close(sockfd);
    }
  }

  return ret;
}

//==========================================================================
int meadow_eth_utils_set_mac(const char *interfaceName,
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
int meadow_eth_utils_get_mac(const char *interfaceName,
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
int meadow_eth_utils_set_ipv4(const char *interfaceName,
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
int meadow_eth_utils_get_ipv4(const char *interfaceName, struct in_addr *addr)
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
int meadow_eth_utils_set_ipv4_mask(const char *interfaceName,
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
int meadow_eth_utils_set_dns(const struct in_addr *inaddr)
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
    // Adds the dns address to the dns.conf file
    ret = dns_add_nameserver((FAR const struct sockaddr *)&addr,
                              sizeof(struct sockaddr_in));
  }

  return ret;
}

//==========================================================================
int meadow_eth_utils_set_router(const char *interfaceName,
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


//==========================================================================
// Same code at configs/stm32f777zit6-meadow/src/hcom_nx/hcom_nx_config_manager.c,
// hcom_nx_config_parse_ip_address()
uint32_t meadow_eth_utils_parse_ip_str(const char *address)
{
    uint32_t ip = 0;
    if (address != NULL)
    {
        struct sockaddr_in sa;
        if (inet_pton(AF_INET, address, &(sa.sin_addr)) == 1)
        {
            ip = (uint32_t ) sa.sin_addr.s_addr;
        }
    }
    return(ip);
}

//=============================================================
// It took me a bit to understand the LAN9355 data sheet and how to communicate
// with its 32-bit registers and how to configure it to generate an IRQ when
// the link status changed for either PHY. Once understood I created the
// following to hide the complexity. Since the MII protocol only deals with
// 16-bit values. When accessing the 32-bit registers of the LAN9355 two 2
// 16-bit reads or writes are necessary. So, a 32-bit register read and write
// are also provided.
int meadow_lan9355_phyread_16(uint16_t phyAddr, uint16_t regAddr, uint16_t *value)
{
  int regval;
  volatile uint32_t timeout;

  // Preserve CSR Clock Range CR[2:0] bits
  regval  = getreg32(STM32_ETH_MACMIIAR);
  regval &= ETH_MACMIIAR_CR_MASK;

  // Set the PHY device address, PHY register address, also set the busy bit.
  // Note: internally the LAN9355 chip takes the phyAddr and regAddr values,,
  // which are meaningless to other LAN chips, and converts them into meaningful
  // information to access it's non-MII compliant 32-bit registers.
  regval |= (((uint32_t)phyAddr << ETH_MACMIIAR_PA_SHIFT) & ETH_MACMIIAR_PA_MASK);
  regval |= (((uint32_t)regAddr << ETH_MACMIIAR_MR_SHIFT) & ETH_MACMIIAR_MR_MASK);
  regval |= ETH_MACMIIAR_MB;

  // This is 32-bit F7 register designed to pass the information to the LAN
  // chip for processing. The F7 takes care of all the communications timing etc.
  putreg32(regval, STM32_ETH_MACMIIAR);

  // Wait for the transfer to complete
  for (timeout = 0; timeout < LAN9355_PHY_READ_TIMEOUT; timeout++)
  {
    // When the ETH_MACMIIAR_MW is clear, the read has completed.
    if ((getreg32(STM32_ETH_MACMIIAR) & ETH_MACMIIAR_MB) == 0)
    {
      // Read the register value from data register
      *value = (uint16_t)(getreg32(STM32_ETH_MACMIIDR) & 0xffff);
      return OK;
    }
  }

  MEADOW_TRACE_INFORMATION("%s:%s()@%d-MII transfer timed out: phyAddr: %04x regAddr: %04x\n",
        thisFile, __func__, __LINE__, phyAddr, regAddr);

  return -ETIMEDOUT;
}

//=============================================================
// This is a simple wrapper to hide the complexity of writing to a 16-bit register
int meadow_lan9355_phywrite_16(uint16_t phyAddr, uint16_t regAddr, uint16_t value)
{
  volatile uint32_t timeout;
  uint32_t regval;

  // Preserve CSR Clock Range CR[2:0] bits
  regval = getreg32(STM32_ETH_MACMIIAR);
  regval &= ETH_MACMIIAR_CR_MASK;

  // To conform to the MII protocol assemble the phyAddr and regAddr bits in
  // the proper field locations. Also, set the busy bit and the bit indicating
  // a write operation.
  regval |= (((uint32_t)phyAddr << ETH_MACMIIAR_PA_SHIFT) & ETH_MACMIIAR_PA_MASK);
  regval |= (((uint32_t)regAddr << ETH_MACMIIAR_MR_SHIFT) & ETH_MACMIIAR_MR_MASK);
  regval |= (ETH_MACMIIAR_MB | ETH_MACMIIAR_MW);

  // Write the value to the data register
  putreg32((uint32_t)value, STM32_ETH_MACMIIDR);

  // Write the destination register address in the MACIIDR register
  putreg32(regval, STM32_ETH_MACMIIAR);

  // When the ETH_MACMIIAR_MW is clear, the write has completed.
  for (timeout = 0; timeout < LAN9355_PHY_WRITE_TIMEOUT; timeout++)
  {
    if ((getreg32(STM32_ETH_MACMIIAR) & ETH_MACMIIAR_MB) == 0)
    {
      return OK;
    }
  }

  MEADOW_TRACE_INFORMATION("%s:%s()@%d-MII Transfer timed out: phyAddr: %04x regAddr: %04x value: %04x\n",
            thisFile, __func__, __LINE__, regAddr, phyAddr, value);

  return -ETIMEDOUT;
}

//=============================================================
// This is a special 32-bit write that is needed by the LAN9355 to access its
// Control and Status Registers (CSRs)
int meadow_lan9355_phywrite_32(uint16_t csrAddr, uint32_t value)
{
  int ret;
  uint8_t phyAddr;
  uint8_t regAddr;

  // Break the csrAddr down into it's component parts so it can ride on the
  // MII protocol. See LAN9355 data sheet section 14.2.
  // phyAddr is bit 4 set plus bits 9:6 of csrAddr as bits 3:0 of phyAddr.
  phyAddr = 0x10 | ((csrAddr >> 6) & 0x0f);

  // regAddr is bits 5:1 of csrAddr. Note: bit 0 of csrAddr is ignored.
  // However, bit 0 (which was csrAddr bit 1) determines if the upper or
  // lower 16-bits of the CSR register is being accessed
  regAddr = (csrAddr >> 1) & 0x1f;

  // Write the lower 16-bits
  ret = meadow_lan9355_phywrite_16(phyAddr, regAddr, value & 0xffff);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_lan9355_phywrite_16-1 failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Write the upper 16-bits by setting regAddr bit 0 to 1 and writing the
  // upper 16-bits
  ret = meadow_lan9355_phywrite_16(phyAddr, regAddr + 1, value >> 16);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_lan9355_phywrite_16-2 failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  return OK;
}

//=============================================================
// This is a special 32-bit read that is needed by the LAN9355 to access its
// Control and Status Registers (CSRs). See LAN9355 section 5.1.
int meadow_lan9355_phyread_32(uint16_t csrAddr, uint32_t *value)
{
  int ret;
  uint8_t phyAddr;
  uint8_t regAddr;
  uint16_t temp16;

  // Break the csrAddr down into it's component parts. See LAN9355 data sheet
  // section 14.2.
  // phyAddr is bit 4 set plus bits 9:6 of csrAddr.
  phyAddr = 0x10 | ((csrAddr >> 6) & 0x0f);

  // regAddr is bits 5:1 of csrAddr without. Note: bit 0 of csrAddr is ignored.
  // However, bit 0 (which was csrAddr bit 1) determines if the upper or
  // lower 16-bits of the CSR register is being accessed
  regAddr = (csrAddr >> 1) & 0x1f;

  // Read the lower 16-bits
  ret = meadow_lan9355_phyread_16(phyAddr, regAddr, &temp16);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_lan9355_phyread_16-1 failed, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Lower 16-bits
  *value = temp16;

  // Now read the upper 16-bits by setting regAddr bit 0 to 1
  ret = meadow_lan9355_phyread_16(phyAddr, regAddr + 1, &temp16);
  if (ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-meadow_lan9355_phyread_16-2, ret:%d, errno:%d\n",
                thisFile, __LINE__, ret, errno);
    return ret;
  }

  // Add the upper 16-bits
  *value |= temp16 << 16;

  return OK;
}

#endif    // #if defined(CONFIG_MEADOW_ETHNET_INCLUDE_IN_BUILD)
