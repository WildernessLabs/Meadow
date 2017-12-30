/************************************************************************************
 * drivers/mtd/s25fl5.c
 * Driver for QuadSPI-based S25FL516K, S25FL532K, and S25L164K
 *
 *   Copyright (C) 2015 Gregory Nutt. All rights reserved.
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
 ************************************************************************************/

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <nuttx/config.h>

#include <sys/types.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/kmalloc.h>
#include <nuttx/signal.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/spi/qspi.h>
#include <nuttx/mtd/mtd.h>

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/
/* Configuration ********************************************************************/
/* QuadSPI Mode.  Per data sheet, either Mode 0 or Mode 3 may be used. */

#ifndef CONFIG_S25FL5_QSPIMODE
#  define CONFIG_S25FL5_QSPIMODE QSPIDEV_MODE0
#endif

/* QuadSPI Frequency per data sheet::
 *
 * – Normal Read (Serial):
 *   50 MHz clock rate
 * – Fast Read (Serial):
 *   133 MHz clock rate
 * – Dual Read:
 *   104 MHz clock rate
 * – Quad Read:
 *   104 MHz clock rate

 * In this implementation, only "Quad" reads are performed.
 */

#ifndef CONFIG_S25FL5_QSPI_FREQUENCY
#  define CONFIG_S25FL5_QSPI_FREQUENCY 104000000
#endif

/* S25FL5 Commands ******************************************************************/
/* Configuration, Status, Erase, Program Commands ***********************************/
/*      Command                    Value    Description:                            */
/*                                            Data sequence                         */
#define S25FL5_READ_STATUS1        0x05  /* Read status register 1:                 *
                                          *   0x05 | SR1                            */
#define S25FL5_READ_STATUS2        0x07  /* Read status register 2:                 *
                                          *   0x07 | SR2                            */
#define S25FL5_READ_CONFIG1        0x35  /* Read status register 3:                 *
                                          *   0x35 | SR3                            */
#define S25FL5_WRITE_ENABLE        0x06  /* Write enable:                           *
                                          *   0x06                                  */
#define S25FL5_WRITE_DISABLE       0x04  /* Write disable command code:             *
                                          *   0x04                                  */
#define S25FL5_WRITE_STATUS        0x01  /* Write status register:                  *
                                          *   0x01 | SR1 | CR1                      */
#define S25FL5_PAGE_PROGRAM        0x02  /* Page Program:                           *
                                          *   0x02 | ADDR(MS) | ADDR(MID) |         *
                                          *   ADDR(LS) | data                       */
#define S25FL5_SECTOR_ERASE        0xd8  /* Sector Erase (4 kB)                     *
                                          *   0xd8 | ADDR(MS) | ADDR(MID) |         *
                                          *   ADDR(LS)                              */
#define S25FL5_CHIP_ERASE_1        0x60  /* Chip Erase 1:                           *
                                          *   0x60                                  */
#define S25FL5_CHIP_ERASE_2        0xc7  /* Chip Erase 2:                           *
                                          *   0xc7                                  */
#define S25FL5_ERASE_PROG_SUSPEND  0x75  /* Erase / Program Suspend:                *
                                          *   0x75                                  */
#define S25FL5_ERASE_PROG_RESUME   0x7a  /* Erase / Program Resume:                 *
                                          *   0x7a                                  */

/* Read Commands ********************************************************************/
/*      Command                    Value    Description:                            */
/*                                            Data sequence                         */
#define S25FL5_READ_DATA           0x03  /* Read Data:                              *
                                          *   0x03 | ADDR(MS) | ADDR(MID) |         *
                                          *   ADDR(LS) | data...                    */
#define S25FL5_FAST_READ           0x0b  /* Fast Read:                              *
                                          *   0x0b | ADDR(MS) | ADDR(MID) |         *
                                          *   ADDR(LS) | dummy | data...            */
#define S25FL5_FAST_READ_DUAL      0x3b  /* Fast Read Dual Output:                  *
                                          *   0x3b | ADDR(MS) | ADDR(MID) |         *
                                          *   ADDR(LS) | dummy | data...            */
#define S25FL5_FAST_READ_QUAD      0x6b  /* Fast Read Dual Output:                  *
                                          *   0x6b | ADDR(MS) | ADDR(MID) |         *
                                          *   ADDR(LS) | dummy | data...            */
#define S25FL5_FAST_READ_DUALIO    0xbb  /* Fast Read Dual I/O:                     *
                                          *   0xbb | ADDR(MS) | ADDR(LS) | data...  */
#define S25FL5_FAST_READ_QUADIO    0xeb  /* Fast Read Quad I/O:                     *
                                          *   0xeb | ADDR | data...                 */

/* Reset Commands *******************************************************************/
/*      Command                    Value    Description:                            */
/*                                            Data sequence                         */
#define S25FL5_SOFT_RESET          0xf0  /* Software Reset:                         *
                                          *   0xf0                                  */

/* ID/Security Commands *************************&***********************************/
/*      Command                    Value    Description:                            */
/*                                            Data sequence                         */
#define S25FL5_MANUFACTURER        0x90  /* Manufacturer / Device ID:               *
                                          *   0x90 | dummy | dummy | 0x00 |         *
                                          *   Manufacturer | DeviceID               */
#define S25FL5_JEDEC_ID            0x9f  /* JEDEC ID:                               *
                                          *   0x9f | Manufacturer | MemoryType |    *
                                          *   Capacity                              */
#define S25FL5_READ_SFDP           0x5a  /* Read SFDP Register / Read Unique ID     *
                                          * Number:                                 *
                                          *   0x5a | 0x00 | 0x00 | ADDR | dummy |   *
                                          *   data...                               */


/* Flash Manufacturer JEDEC IDs */

#define S25FL5_JEDEC_ID_SPANSION   0x01

/* S25FL5 JEDIC IDs */
#define S25FL5_JEDEC_DEVICE_TYPE   0x02
#define S25FL512S_JEDEC_CAPACITY   0x20

/* S25FL5 Registers ****************************************************************/
/* Status register bit definitions                                                  */

#define STATUS1_BUSY_MASK          (1 << 0) /* Bit 0: Device ready/busy status      */
#  define STATUS1_READY            (0 << 0) /*   0 = Not Busy                       */
#  define STATUS1_BUSY             (1 << 0) /*   1 = Busy                           */
#define STATUS1_WEL_MASK           (1 << 1) /* Bit 1: Write enable latch status     */
#  define STATUS1_WEL_DISABLED     (0 << 1) /*   0 = Not Write Enabled              */
#  define STATUS1_WEL_ENABLED      (1 << 1) /*   1 = Write Enabled                  */
#define STATUS1_BP_SHIFT           (2)      /* Bits 2-4: Block protect bits         */
#define STATUS1_BP_MASK            (7 << STATUS1_BP_SHIFT)
#  define STATUS1_BP_NONE          (0 << STATUS1_BP_SHIFT)
#  define STATUS1_BP_ALL           (7 << STATUS1_BP_SHIFT)
#define STATUS1_SRP0_MASK          (1 << 7) /* Bit 7: Status register protect 0     */
#  define STATUS1_SRP0_UNLOCKED    (0 << 7) /*   0 = WP# no effect / PS Lock Down   */
#  define STATUS1_SRP0_LOCKED      (1 << 7) /*   1 = WP# protect / OTP Lock Down    */

#define CONFIG1_QUAD_ENABLE_MASK   (1 << 1) /* Bit 1: Quad Enable                   */
#  define CONFIG1_QUAD_DISABLE     (0 << 1) /*   0 = Quad Mode Not Enabled          */
#  define CONFIG1_QUAD_ENABLE      (1 << 1) /*   1 = Quad Mode Enabled              */
#define CONFIG1_TB_MASK            (1 << 5) /* Bit 5: Top / Bottom Protect          */
#  define CONFIG1_TB_TOP           (0 << 5) /*   0 = BP2-BP0 protect Top down       */
#  define CONFIG1_TB_BOTTOM        (1 << 5) /*   1 = BP2-BP0 protect Bottom up      */

/* Chip Geometries ******************************************************************/
/* All members of the family support uniform 4K-byte sectors  */

#define S25FL512S_SECTOR_SIZE      (4*1024)
#define S25FL512S_SECTOR_SHIFT     (12)
#define S25FL512S_SECTOR_COUNT     (16384)
#define S25FL512S_PAGE_SIZE        (256)
#define S25FL512S_PAGE_SHIFT       (8)

/* Cache flags **********************************************************************/

#define S25FL5_CACHE_VALID         (1 << 0)  /* 1=Cache has valid data */
#define S25FL5_CACHE_DIRTY         (1 << 1)  /* 1=Cache is dirty */
#define S25FL5_CACHE_ERASED        (1 << 2)  /* 1=Backing FLASH is erased */

#define IS_VALID(p)                ((((p)->flags) & S25FL5_CACHE_VALID) != 0)
#define IS_DIRTY(p)                ((((p)->flags) & S25FL5_CACHE_DIRTY) != 0)
#define IS_ERASED(p)               ((((p)->flags) & S25FL5_CACHE_ERASED) != 0)

#define SET_VALID(p)               do { (p)->flags |= S25FL5_CACHE_VALID; } while (0)
#define SET_DIRTY(p)               do { (p)->flags |= S25FL5_CACHE_DIRTY; } while (0)
#define SET_ERASED(p)              do { (p)->flags |= S25FL5_CACHE_ERASED; } while (0)

#define CLR_VALID(p)               do { (p)->flags &= ~S25FL5_CACHE_VALID; } while (0)
#define CLR_DIRTY(p)               do { (p)->flags &= ~S25FL5_CACHE_DIRTY; } while (0)
#define CLR_ERASED(p)              do { (p)->flags &= ~S25FL5_CACHE_ERASED; } while (0)

/************************************************************************************
 * Private Types
 ************************************************************************************/

/* This type represents the state of the MTD device.  The struct mtd_dev_s must
 * appear at the beginning of the definition so that you can freely cast between
 * pointers to struct mtd_dev_s and struct s25fl5_dev_s.
 */

struct s25fl5_dev_s
{
  struct mtd_dev_s       mtd;         /* MTD interface */
  FAR struct qspi_dev_s *qspi;        /* Saved QuadSPI interface instance */
  uint16_t               nsectors;    /* Number of erase sectors */
  uint8_t                sectorshift; /* Log2 of sector size */
  uint8_t                pageshift;   /* Log2 of page size */
  FAR uint8_t           *cmdbuf;      /* Allocated command buffer */
  FAR uint8_t           *readbuf;     /* Allocated status read buffer */
};

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/

/* Locking */

static void s25fl5_lock(FAR struct qspi_dev_s *qspi);
static inline void s25fl5_unlock(FAR struct qspi_dev_s *qspi);

/* Low-level message helpers */

static int  s25fl5_command(FAR struct qspi_dev_s *qspi, uint8_t cmd);
static int  s25fl5_command_address(FAR struct qspi_dev_s *qspi, uint8_t cmd,
              off_t addr, uint8_t addrlen);
static int  s25fl5_command_read(FAR struct qspi_dev_s *qspi, uint8_t cmd,
              FAR void *buffer, size_t buflen);
static int  s25fl5_command_write(FAR struct qspi_dev_s *qspi, uint8_t cmd,
              FAR const void *buffer, size_t buflen);
static uint8_t sf25fl5_read_status1(FAR struct s25fl5_dev_s *priv);
static uint8_t sf25fl5_read_config1(FAR struct s25fl5_dev_s *priv);
static void s25fl5_write_enable(FAR struct s25fl5_dev_s *priv);
static void s25fl5_write_disable(FAR struct s25fl5_dev_s *priv);

static int  s25fl5_readid(FAR struct s25fl5_dev_s *priv);
static int  s25fl5_protect(FAR struct s25fl5_dev_s *priv,
              off_t startblock, size_t nblocks);
static int  s25fl5_unprotect(FAR struct s25fl5_dev_s *priv,
              off_t startblock, size_t nblocks);
static bool s25fl5_isprotected(FAR struct s25fl5_dev_s *priv,
              uint8_t status, off_t address);
static int  s25fl5_erase_sector(FAR struct s25fl5_dev_s *priv, off_t offset);
static int  s25fl5_erase_chip(FAR struct s25fl5_dev_s *priv);
static int  s25fl5_read_byte(FAR struct s25fl5_dev_s *priv, FAR uint8_t *buffer,
              off_t address, size_t nbytes);
static int  s25fl5_write_page(FAR struct s25fl5_dev_s *priv,
              FAR const uint8_t *buffer, off_t address, size_t nbytes);

/* MTD driver methods */

static int  s25fl5_erase(FAR struct mtd_dev_s *dev, off_t startblock,
              size_t nblocks);
static ssize_t s25fl5_bread(FAR struct mtd_dev_s *dev, off_t startblock,
              size_t nblocks, FAR uint8_t *buf);
static ssize_t s25fl5_bwrite(FAR struct mtd_dev_s *dev, off_t startblock,
              size_t nblocks, FAR const uint8_t *buf);
static ssize_t s25fl5_read(FAR struct mtd_dev_s *dev, off_t offset, size_t nbytes,
              FAR uint8_t *buffer);
static int  s25fl5_ioctl(FAR struct mtd_dev_s *dev, int cmd, unsigned long arg);

/************************************************************************************
 * Private Data
 ************************************************************************************/

/************************************************************************************
 * Private Functions
 ************************************************************************************/

/************************************************************************************
 * Name: s25fl5_lock
 ************************************************************************************/

static void s25fl5_lock(FAR struct qspi_dev_s *qspi)
{
  /* On QuadSPI busses where there are multiple devices, it will be necessary to
   * lock QuadSPI to have exclusive access to the busses for a sequence of
   * transfers.  The bus should be locked before the chip is selected.
   *
   * This is a blocking call and will not return until we have exclusiv access to
   * the QuadSPI buss.  We will retain that exclusive access until the bus is unlocked.
   */

  (void)QSPI_LOCK(qspi, true);

  /* After locking the QuadSPI bus, the we also need call the setfrequency, setbits, and
   * setmode methods to make sure that the QuadSPI is properly configured for the device.
   * If the QuadSPI buss is being shared, then it may have been left in an incompatible
   * state.
   */

  QSPI_SETMODE(qspi, CONFIG_S25FL5_QSPIMODE);
  QSPI_SETBITS(qspi, 8);
  (void)QSPI_SETFREQUENCY(qspi, CONFIG_S25FL5_QSPI_FREQUENCY);
}

/************************************************************************************
 * Name: s25fl5_unlock
 ************************************************************************************/

static inline void s25fl5_unlock(FAR struct qspi_dev_s *qspi)
{
  (void)QSPI_LOCK(qspi, false);
}

/************************************************************************************
 * Name: s25fl5_command
 ************************************************************************************/

static int s25fl5_command(FAR struct qspi_dev_s *qspi, uint8_t cmd)
{
  struct qspi_cmdinfo_s cmdinfo;

  finfo("CMD: %02x\n", cmd);

  cmdinfo.flags   = 0;
  cmdinfo.addrlen = 0;
  cmdinfo.cmd     = cmd;
  cmdinfo.buflen  = 0;
  cmdinfo.addr    = 0;
  cmdinfo.buffer  = NULL;

  return QSPI_COMMAND(qspi, &cmdinfo);
}

/************************************************************************************
 * Name: s25fl5_command_address
 ************************************************************************************/

static int s25fl5_command_address(FAR struct qspi_dev_s *qspi, uint8_t cmd,
                                  off_t addr, uint8_t addrlen)
{
  struct qspi_cmdinfo_s cmdinfo;

  finfo("CMD: %02x Address: %04lx addrlen=%d\n", cmd, (unsigned long)addr, addrlen);

  cmdinfo.flags   = QSPICMD_ADDRESS;
  cmdinfo.addrlen = addrlen;
  cmdinfo.cmd     = cmd;
  cmdinfo.buflen  = 0;
  cmdinfo.addr    = addr;
  cmdinfo.buffer  = NULL;

  return QSPI_COMMAND(qspi, &cmdinfo);
}

/************************************************************************************
 * Name: s25fl5_command_read
 ************************************************************************************/

static int s25fl5_command_read(FAR struct qspi_dev_s *qspi, uint8_t cmd,
                               FAR void *buffer, size_t buflen)
{
  struct qspi_cmdinfo_s cmdinfo;

  finfo("CMD: %02x buflen: %lu\n", cmd, (unsigned long)buflen);

  cmdinfo.flags   = QSPICMD_READDATA;
  cmdinfo.addrlen = 0;
  cmdinfo.cmd     = cmd;
  cmdinfo.buflen  = buflen;
  cmdinfo.addr    = 0;
  cmdinfo.buffer  = buffer;

  return QSPI_COMMAND(qspi, &cmdinfo);
}

/************************************************************************************
 * Name: s25fl5_command_write
 ************************************************************************************/

static int s25fl5_command_write(FAR struct qspi_dev_s *qspi, uint8_t cmd,
                                FAR const void *buffer, size_t buflen)
{
  struct qspi_cmdinfo_s cmdinfo;

  finfo("CMD: %02x buflen: %lu\n", cmd, (unsigned long)buflen);

  cmdinfo.flags   = QSPICMD_WRITEDATA;
  cmdinfo.addrlen = 0;
  cmdinfo.cmd     = cmd;
  cmdinfo.buflen  = buflen;
  cmdinfo.addr    = 0;
  cmdinfo.buffer  = (FAR void *)buffer;

  return QSPI_COMMAND(qspi, &cmdinfo);
}

/************************************************************************************
 * Name: sf25fl5_read_status1
 ************************************************************************************/

static uint8_t sf25fl5_read_status1(FAR struct s25fl5_dev_s *priv)
{
  DEBUGVERIFY(s25fl5_command_read(priv->qspi, S25FL5_READ_STATUS1,
                                  (FAR void *)&priv->readbuf[0], 1));
  return priv->readbuf[0];
}

/************************************************************************************
 * Name: sf25fl5_read_config1
 ************************************************************************************/

static uint8_t sf25fl5_read_config1(FAR struct s25fl5_dev_s *priv)
{
  DEBUGVERIFY(s25fl5_command_read(priv->qspi, S25FL5_READ_CONFIG1,
                                  (FAR void *)&priv->readbuf[0], 1));
  return priv->readbuf[0];
}

/************************************************************************************
 * Name:  s25fl5_write_enable
 ************************************************************************************/

static void s25fl5_write_enable(FAR struct s25fl5_dev_s *priv)
{
  uint8_t status;

  do
    {
      s25fl5_command(priv->qspi, S25FL5_WRITE_ENABLE);
      status = sf25fl5_read_status1(priv);
    }
  while ((status & STATUS1_WEL_MASK) != STATUS1_WEL_ENABLED);
}

/************************************************************************************
 * Name:  s25fl5_write_disable
 ************************************************************************************/

static void s25fl5_write_disable(FAR struct s25fl5_dev_s *priv)
{
  uint8_t status;

  do
    {
      s25fl5_command(priv->qspi, S25FL5_WRITE_DISABLE);
      status = sf25fl5_read_status1(priv);
    }
  while ((status & STATUS1_WEL_MASK) != STATUS1_WEL_DISABLED);
}

/************************************************************************************
 * Name:  s25fl5_write_status
 ************************************************************************************/

static void s25fl5_write_status(FAR struct s25fl5_dev_s *priv)
{
  s25fl5_write_enable(priv);
  s25fl5_command_write(priv->qspi, S25FL5_WRITE_STATUS,
                       (FAR const void *)priv->cmdbuf, 2);
  s25fl5_write_disable(priv);
}

/************************************************************************************
 * Name: s25fl5_readid
 ************************************************************************************/

static inline int s25fl5_readid(struct s25fl5_dev_s *priv)
{
  /* Lock the QuadSPI bus and configure the bus. */

  s25fl5_lock(priv->qspi);

  /* Read the JEDEC ID */

  s25fl5_command_read(priv->qspi, S25FL5_JEDEC_ID, priv->cmdbuf, 3);

  /* Unlock the bus */

  s25fl5_unlock(priv->qspi);

  finfo("Manufacturer: %02x Device Type %02x, Capacity: %02x",
        priv->cmdbuf[0], priv->cmdbuf[1], priv->cmdbuf[2]);

  /* Check for a recognized memory device type */

  if (priv->cmdbuf[1] != S25FL5_JEDEC_DEVICE_TYPE && priv->cmdbuf[1] != S25FL5_JEDEC_DEVICE_TYPE)
    {
      ferr("ERROR: Unrecognized device type: %02x\n", priv->cmdbuf[1]);
      return -ENODEV;
    }

  /* Check for a supported capacity */

  switch (priv->cmdbuf[2])
    {
      case S25FL512S_JEDEC_CAPACITY:
        priv->sectorshift = S25FL512S_SECTOR_SHIFT;
        priv->pageshift   = S25FL512S_PAGE_SHIFT;
        priv->nsectors    = S25FL512S_SECTOR_COUNT;
        break;

      /* Support for this part is not implemented yet */

      default:
        ferr("ERROR: Unsupported memory capacity: %02x\n", priv->cmdbuf[2]);
        return -ENODEV;
    }

  return OK;
}

/************************************************************************************
 * Name: s25fl5_protect
 ************************************************************************************/

static int s25fl5_protect(FAR struct s25fl5_dev_s *priv,
                          off_t startblock, size_t nblocks)
{
  /* Get the status register value to check the current protection */

  priv->cmdbuf[0] = sf25fl5_read_status1(priv);
  priv->cmdbuf[1] = sf25fl5_read_config1(priv);
  
  if ((priv->cmdbuf[0] & STATUS1_BP_MASK) == STATUS1_BP_NONE)
    {
      /* Protection already disabled */

      return 0;
    }

  /* Check if sector protection registers are locked */

  if ((priv->cmdbuf[0] & STATUS1_SRP0_MASK) == STATUS1_SRP0_LOCKED)
    {
      /* Yes.. unprotect section protection registers */

      priv->cmdbuf[0] &= ~STATUS1_SRP0_MASK;
      s25fl5_write_status(priv);
    }

  /* Set the protection mask to zero.
   * REVISIT:  This logic should really just set the BP bits as
   * necessary to protect the range of sectors.
   */

  priv->cmdbuf[0] |= STATUS1_BP_MASK;
  s25fl5_write_status(priv);

  /* Check the new status */

  priv->cmdbuf[0] = sf25fl5_read_status1(priv);
  if ((priv->cmdbuf[0] & STATUS1_BP_MASK) != STATUS1_BP_MASK)
    {
      return -EACCES;
    }

  return OK;
}

/************************************************************************************
 * Name: s25fl5_unprotect
 ************************************************************************************/

static int s25fl5_unprotect(FAR struct s25fl5_dev_s *priv,
                            off_t startblock, size_t nblocks)
{
  /* Get the status register value to check the current protection */

  priv->cmdbuf[0] = sf25fl5_read_status1(priv);
  priv->cmdbuf[1] = sf25fl5_read_config1(priv);

  if ((priv->cmdbuf[0] & STATUS1_BP_MASK) == STATUS1_BP_NONE)
    {
      /* Protection already disabled */

      return 0;
    }

  /* Check if sector protection registers are locked */

  if ((priv->cmdbuf[0] & STATUS1_SRP0_MASK) == STATUS1_SRP0_LOCKED)
    {
      /* Yes.. unprotect section protection registers */

      priv->cmdbuf[0] &= ~STATUS1_SRP0_MASK;
      s25fl5_write_status(priv);
    }

  /* Set the protection mask to zero (and not complemented).
   * REVISIT:  This logic should really just re-write the BP bits as
   * necessary to unprotect the range of sectors.
   */

  priv->cmdbuf[0] &= ~STATUS1_BP_MASK;
  s25fl5_write_status(priv);

  /* Check the new status */

  priv->cmdbuf[0] = sf25fl5_read_status1(priv);
  if ((priv->cmdbuf[0] & (STATUS1_SRP0_MASK | STATUS1_BP_MASK)) != 0)
    {
      return -EACCES;
    }

  return OK;
}

/************************************************************************************
 * Name: s25fl5_isprotected
 ************************************************************************************/

static bool s25fl5_isprotected(FAR struct s25fl5_dev_s *priv, uint8_t status,
                               off_t address)
{
  off_t protstart;
  off_t protend;
  off_t protsize;
  unsigned int bp;
  uint8_t config;

  config = sf25fl5_read_config1(priv);
  protsize = 1024;
  
  bp = (status & STATUS1_BP_MASK) >> STATUS1_BP_SHIFT;
  switch (bp)
    {
      case 0:
        return false;

      case 7:
        return true;

       default:
        protsize <<= (protsize << (bp - 1));
        break;
    }

  /* The final protection range then depends on if the protection region is
   * configured top-down or bottom up  (assuming CMP=0).
   */

  if ((config & CONFIG1_TB_MASK) != 0)
    {
      protstart = 0x00000000;
      protend   = protstart + protsize;
    }
  else
    {
      protend   = 0x04000000;
      protstart = protend - protsize;
    }

  return (address >= protstart && address < protend);
}

/************************************************************************************
 * Name:  s25fl5_erase_sector
 ************************************************************************************/

static int s25fl5_erase_sector(struct s25fl5_dev_s *priv, off_t sector)
{
  off_t address;
  uint8_t status;

  finfo("sector: %08lx\n", (unsigned long)sector);

  /* Check that the flash is ready and unprotected */

  status = sf25fl5_read_status1(priv);
  if ((status & STATUS1_BUSY_MASK) != STATUS1_READY)
    {
      ferr("ERROR: Flash busy: %02x", status);
      return -EBUSY;
    }

  /* Get the address associated with the sector */

  address = (off_t)sector << priv->sectorshift;

  if ((status & STATUS1_BP_MASK) != 0 &&
      s25fl5_isprotected(priv, status, address))
    {
      ferr("ERROR: Flash protected: %02x", status);
      return -EACCES;
    }

  /* Send the sector erase command */

  s25fl5_write_enable(priv);
  s25fl5_command_address(priv->qspi, S25FL5_SECTOR_ERASE, address, 3);

  /* Wait for erasure to finish */

  while ((sf25fl5_read_status1(priv) & STATUS1_BUSY_MASK) != 0);
  return OK;
}

/************************************************************************************
 * Name:  s25fl5_erase_chip
 ************************************************************************************/

static int s25fl5_erase_chip(struct s25fl5_dev_s *priv)
{
  uint8_t status;

  /* Check if the FLASH is protected */

  status = sf25fl5_read_status1(priv);
  if ((status & STATUS1_BP_MASK) != 0)
    {
      ferr("ERROR: FLASH is Protected: %02x", status);
      return -EACCES;
    }

  /* Erase the whole chip */

  s25fl5_write_enable(priv);
  s25fl5_command(priv->qspi, S25FL5_CHIP_ERASE_2);

  /* Wait for the erasure to complete */

  status = sf25fl5_read_status1(priv);
  while ((status & STATUS1_BUSY_MASK) != 0)
    {
      nxsig_usleep(200*1000);
      status = sf25fl5_read_status1(priv);
    }

  return OK;
}

/************************************************************************************
 * Name: s25fl5_read_byte
 ************************************************************************************/

static int s25fl5_read_byte(FAR struct s25fl5_dev_s *priv, FAR uint8_t *buffer,
                            off_t address, size_t buflen)
{
  struct qspi_meminfo_s meminfo;

  finfo("address: %08lx nbytes: %d\n", (long)address, (int)buflen);

#ifdef CONFIG_S25FL5_SCRAMBLE
  meminfo.flags   = QSPIMEM_READ | QSPIMEM_QUADIO | QSPIMEM_SCRAMBLE;
#else
  meminfo.flags   = QSPIMEM_READ | QSPIMEM_QUADIO;
#endif
  meminfo.addrlen = 3;
  meminfo.dummies = 6;
  meminfo.buflen  = buflen;
  meminfo.cmd     = S25FL5_FAST_READ_QUADIO;
  meminfo.addr    = address;
#ifdef CONFIG_S25FL5_SCRAMBLE
  meminfo.key     = CONFIG_S25FL5_SCRAMBLE_KEY;
#endif
  meminfo.buffer  = buffer;

  return QSPI_MEMORY(priv->qspi, &meminfo);
}

/************************************************************************************
 * Name:  s25fl5_write_page
 ************************************************************************************/

static int s25fl5_write_page(struct s25fl5_dev_s *priv, FAR const uint8_t *buffer,
                             off_t address, size_t buflen)
{
  struct qspi_meminfo_s meminfo;
  unsigned int pagesize;
  unsigned int npages;
  int ret;
  int i;

  finfo("address: %08lx buflen: %u\n", (unsigned long)address, (unsigned)buflen);

  npages   = (buflen >> priv->pageshift);
  pagesize = (1 << priv->pageshift);

  /* Set up non-varying parts of transfer description */

#ifdef CONFIG_S25FL5_SCRAMBLE
  meminfo.flags   = QSPIMEM_WRITE | QSPIMEM_SCRAMBLE;
#else
  meminfo.flags   = QSPIMEM_WRITE;
#endif
  meminfo.cmd     = S25FL5_PAGE_PROGRAM;
  meminfo.addrlen = 3;
  meminfo.buflen  = pagesize;
#ifdef CONFIG_S25FL5_SCRAMBLE
  meminfo.key     = CONFIG_S25FL5_SCRAMBLE_KEY;
#endif
  meminfo.dummies = 0;

  /* Then write each page */

  for (i = 0; i < npages; i++)
    {
      /* Set up varying parts of the transfer description */

      meminfo.addr   = address;
      meminfo.buffer = (void *)buffer;

      /* Write one page */

      s25fl5_write_enable(priv);
      ret = QSPI_MEMORY(priv->qspi, &meminfo);
      s25fl5_write_disable(priv);

      if (ret < 0)
        {
          ferr("ERROR: QSPI_MEMORY failed writing address=%06x\n",
               address);
          return ret;
        }

      /* Update for the next time through the loop */

      buffer  += pagesize;
      address += pagesize;
      buflen  -= pagesize;
    }

  /* The transfer should always be an even number of sectors and hence also
   * pages.  There should be no remainder.
   */

  DEBUGASSERT(buflen == 0);
  return OK;
}

/************************************************************************************
 * Name: s25fl5_erase
 ************************************************************************************/

static int s25fl5_erase(FAR struct mtd_dev_s *dev, off_t startblock, size_t nblocks)
{
  FAR struct s25fl5_dev_s *priv = (FAR struct s25fl5_dev_s *)dev;
  size_t blocksleft = nblocks;

  finfo("startblock: %08lx nblocks: %d\n", (long)startblock, (int)nblocks);

  /* Lock access to the SPI bus until we complete the erase */

  s25fl5_lock(priv->qspi);

  while (blocksleft-- > 0)
    {
      /* Erase each sector */

      s25fl5_erase_sector(priv, startblock);

      startblock++;
    }

  s25fl5_unlock(priv->qspi);
  return (int)nblocks;
}

/************************************************************************************
 * Name: s25fl5_bread
 ************************************************************************************/

static ssize_t s25fl5_bread(FAR struct mtd_dev_s *dev, off_t startblock,
                            size_t nblocks, FAR uint8_t *buffer)
{
  FAR struct s25fl5_dev_s *priv = (FAR struct s25fl5_dev_s *)dev;
  ssize_t nbytes;

  finfo("startblock: %08lx nblocks: %d\n", (long)startblock, (int)nblocks);

  /* On this device, we can handle the block read just like the byte-oriented read */

  nbytes = s25fl5_read(dev, startblock << priv->sectorshift,
                       nblocks << priv->sectorshift, buffer);
  if (nbytes > 0)
    {
      nbytes >>= priv->sectorshift;
    }

  return nbytes;
}

/************************************************************************************
 * Name: s25fl5_bwrite
 ************************************************************************************/

static ssize_t s25fl5_bwrite(FAR struct mtd_dev_s *dev, off_t startblock,
                             size_t nblocks, FAR const uint8_t *buffer)
{
  FAR struct s25fl5_dev_s *priv = (FAR struct s25fl5_dev_s *)dev;
  int ret = (int)nblocks;

  finfo("startblock: %08lx nblocks: %d\n", (long)startblock, (int)nblocks);

  /* Lock the QuadSPI bus and write all of the pages to FLASH */

  s25fl5_lock(priv->qspi);

  ret = s25fl5_write_page(priv, buffer, startblock << priv->sectorshift,
                          nblocks << priv->sectorshift);
  if (ret < 0)
    {
      ferr("ERROR: s25fl5_write_page failed: %d\n", ret);
    }

  s25fl5_unlock(priv->qspi);

  return ret < 0 ? ret : nblocks;
}

/************************************************************************************
 * Name: s25fl5_read
 ************************************************************************************/

static ssize_t s25fl5_read(FAR struct mtd_dev_s *dev, off_t offset, size_t nbytes,
                           FAR uint8_t *buffer)
{
  FAR struct s25fl5_dev_s *priv = (FAR struct s25fl5_dev_s *)dev;
  int ret;

  finfo("offset: %08lx nbytes: %d\n", (long)offset, (int)nbytes);

  /* Lock the QuadSPI bus and select this FLASH part */

  s25fl5_lock(priv->qspi);
  ret = s25fl5_read_byte(priv, buffer, offset, nbytes);
  s25fl5_unlock(priv->qspi);

  if (ret < 0)
    {
      ferr("ERROR: s25fl5_read_byte returned: %d\n", ret);
      return (ssize_t)ret;
    }

  finfo("return nbytes: %d\n", (int)nbytes);
  finfo("    header: %x %x %x %x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
  return (ssize_t)nbytes;
}

/************************************************************************************
 * Name: s25fl5_ioctl
 ************************************************************************************/

static int s25fl5_ioctl(FAR struct mtd_dev_s *dev, int cmd, unsigned long arg)
{
  FAR struct s25fl5_dev_s *priv = (FAR struct s25fl5_dev_s *)dev;
  int ret = -EINVAL; /* Assume good command with bad parameters */

  finfo("cmd: %d \n", cmd);

  switch (cmd)
    {
      case MTDIOC_GEOMETRY:
        {
          FAR struct mtd_geometry_s *geo =
            (FAR struct mtd_geometry_s *)((uintptr_t)arg);

          if (geo)
            {
              /* Populate the geometry structure with information need to know
               * the capacity and how to access the device.
               *
               * NOTE: that the device is treated as though it where just an array
               * of fixed size blocks.  That is most likely not true, but the client
               * will expect the device logic to do whatever is necessary to make it
               * appear so.
               */

              geo->blocksize    = (1 << priv->sectorshift);
              geo->erasesize    = (1 << priv->sectorshift);
              geo->neraseblocks = priv->nsectors;
              ret               = OK;

              finfo("blocksize: %d erasesize: %d neraseblocks: %d\n",
                    geo->blocksize, geo->erasesize, geo->neraseblocks);
            }
        }
        break;

      case MTDIOC_BULKERASE:
        {
          /* Erase the entire device */

          s25fl5_lock(priv->qspi);
          ret = s25fl5_erase_chip(priv);
          s25fl5_unlock(priv->qspi);
        }
        break;

      case MTDIOC_PROTECT:
        {
          FAR const struct mtd_protect_s *prot =
            (FAR const struct mtd_protect_s *)((uintptr_t)arg);

          DEBUGASSERT(prot);
          ret = s25fl5_protect(priv, prot->startblock, prot->nblocks);
        }
        break;

      case MTDIOC_UNPROTECT:
        {
          FAR const struct mtd_protect_s *prot =
            (FAR const struct mtd_protect_s *)((uintptr_t)arg);

          DEBUGASSERT(prot);
          ret = s25fl5_unprotect(priv, prot->startblock, prot->nblocks);
        }
        break;

      default:
        ret = -ENOTTY; /* Bad/unsupported command */
        break;
    }

  finfo("return %d\n", ret);
  return ret;
}

/************************************************************************************
 * Public Functions
 ************************************************************************************/

/************************************************************************************
 * Name: s25fl5_initialize
 *
 * Description:
 *   Create an initialize MTD device instance for the QuadSPI-based ST24FL1
 *   FLASH part.
 *
 *   MTD devices are not registered in the file system, but are created as instances
 *   that can be bound to other functions (such as a block or character driver front
 *   end).
 *
 ************************************************************************************/

FAR struct mtd_dev_s *s25fl5_initialize(FAR struct qspi_dev_s *qspi, bool unprotect)
{
  FAR struct s25fl5_dev_s *priv;
  int ret;

  finfo("qspi: %p\n", qspi);
  DEBUGASSERT(qspi != NULL);

  /* Allocate a state structure (we allocate the structure instead of using
   * a fixed, static allocation so that we can handle multiple FLASH devices.
   * The current implementation would handle only one FLASH part per QuadSPI
   * device (only because of the QSPIDEV_FLASH(0) definition) and so would have
   * to be extended to handle multiple FLASH parts on the same QuadSPI bus.
   */

  priv = (FAR struct s25fl5_dev_s *)kmm_zalloc(sizeof(struct s25fl5_dev_s));
  if (priv)
    {
      /* Initialize the allocated structure (unsupported methods were
       * nullified by kmm_zalloc).
       */

      priv->mtd.erase  = s25fl5_erase;
      priv->mtd.bread  = s25fl5_bread;
      priv->mtd.bwrite = s25fl5_bwrite;
      priv->mtd.read   = s25fl5_read;
      priv->mtd.ioctl  = s25fl5_ioctl;
      priv->qspi       = qspi;

      /* Allocate a 4-byte buffer to support DMA command data */

      priv->cmdbuf = (FAR uint8_t *)QSPI_ALLOC(qspi, 4);
      if (priv->cmdbuf == NULL)
        {
          ferr("ERROR Failed to allocate command buffer\n");
          goto errout_with_priv;
        }

      /* Allocate a one-byte buffer to support DMA status read data */

      priv->readbuf = (FAR uint8_t *)QSPI_ALLOC(qspi, 1);
      if (priv->readbuf == NULL)
        {
          ferr("ERROR Failed to allocate read buffer\n");
          goto errout_with_cmdbuf;
        }

      /* Identify the FLASH chip and get its capacity */

      ret = s25fl5_readid(priv);
      if (ret != OK)
        {
          /* Unrecognized! Discard all of that work we just did and return NULL */

          ferr("ERROR Unrecognized QSPI device\n");
          goto errout_with_readbuf;
        }

      /* Enable quad mode */

      priv->cmdbuf[0] = sf25fl5_read_status1(priv);
      priv->cmdbuf[1] = sf25fl5_read_config1(priv);

      while ((priv->cmdbuf[1] & CONFIG1_QUAD_ENABLE_MASK) == 0)
        {
          priv->cmdbuf[1] |= CONFIG1_QUAD_ENABLE;
          s25fl5_write_status(priv);
          priv->cmdbuf[1] = sf25fl5_read_config1(priv);
          nxsig_usleep(50*1000);
        }

      /* Unprotect FLASH sectors if so requested. */

      if (unprotect)
        {
          ret = s25fl5_unprotect(priv, 0, priv->nsectors - 1);
          if (ret < 0)
            {
              ferr("ERROR: Sector unprotect failed\n");
            }
        }
    }

#ifdef CONFIG_MTD_REGISTRATION
  /* Register the MTD with the procfs system if enabled */

  mtd_register(&priv->mtd, "s25fl5");
#endif

  /* Return the implementation-specific state structure as the MTD device */

  finfo("Return %p\n", priv);
  return (FAR struct mtd_dev_s *)priv;

errout_with_readbuf:
  QSPI_FREE(qspi, priv->readbuf);

errout_with_cmdbuf:
  QSPI_FREE(qspi, priv->cmdbuf);

errout_with_priv:
  kmm_free(priv);
  return NULL;
}
