/****************************************************************************
 * configs\stm32f777zit6-meadow\src\hcom_nx\tests\hcom_nx_qspi_flash_tests.c
 * 
 *   Copyright (C) 2019 - 2023 Wilderness Labs. All rights reserved.
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

// Available tests based on provided user data
// QspiWrite -1:  Bulk erases and writes data pattern to entire flash and reads to verify
// QspiWrite -2:  Bulk erases and writes data pattern to entire flash and stops
// QspiWrite -3:  Bulk erases and writes data pattern to one page and then verifies that all
//                 pages are correct, before and after the page written, repeats for each page.
//                 Note: This test will take about 26.4 years to complete for 512 MBit flash.
// QspiWrite -4:  Bulk erases and writes, flashes and reads to verify 0x55 repeats with 0xaa.
//                 Note: does everything twice to check each flash cell, take about 10 minutes.
// QspiWrite 0-n: Fill buffer with test pattern. Page data determined by developerValue 0-n
//
// QspiInit -1:   Bulk entire flash
// QspiInit -2:   Finds any non-erased sectors and erases them (faster than bulk erase)
// QspiInit 0-n:  Erases one 4k sector as determined by developerValue 0-n
//
// QspiRead -1:   Displays via syslog the data in any non-erased pages
// QspiRead -2:   Displays via syslog the data in erased pages (not very useful)
// QspiRead 0-n:  Displays the data in the page determined by developerValue 0-n

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "../hcom_nx_common.h"
#include <meadow/hcom_upd_shared.h>

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include <crc8.h>

#include "stm32_qspi.h"
#include <nuttx/spi/qspi.h>
#include <dirent.h>

#if HCOM_INCLUDE_QSPI_FLASH_TESTS_IN_BUILD > 0

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

// Tried to align terminology with Nuttx. Page(256), Sector (4K)
// and Block (64k)

// How often to output syslog information?
#define FLASH_TEST_DISPLAY_INTERVAL (16 * 1024)

/****************************************************************************
 * Private Data
 ****************************************************************************/

static char *thisFile = __FILE__;

static FAR struct mtd_dev_s *_test_mtd = NULL;
static FAR struct mtd_geometry_s _test_geo;
static uint32_t _flash_test_write_page_size;
static uint32_t _flash_test_total_mtd_bytes;
static uint32_t _flash_test_total_write_pages;
static uint32_t _flash_test_pages_per_4k_sector;
static bool _mtdGeoShown;

//static   void * memTest;
/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int hcom_exec_flash_fs_flash_test_erase_1_4k_sector(uint32_t sectorOffset);
static int hcom_exec_flash_test_find_display_used_pages(bool eraseUsedPages, bool displayErasedPages);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_nx_exec_test_qspi_flash_setup(FAR struct mtd_dev_s *mtd)
{
  _test_mtd = mtd;
  return OK;
}

//===================================================================
// Returns the current time as a 64-bit number representing nanosec.
// Used for testing. Note: Only millisecond resolution due to
// Nuttx's clock resolution.
static uint64_t hcom_nx_utils_get_current_time64(void)
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
static void hcom_exec_flash_convert_ms_to_time(uint32_t elapsedTimeMs, char* timeStr, int bufLen)
{
  uint32_t hours = 0;
  if(elapsedTimeMs >= 3600000)
  {
    hours = elapsedTimeMs/3600000;
    elapsedTimeMs = elapsedTimeMs - (3600000 * hours);
  }

  uint32_t minutes = elapsedTimeMs/60000;
  elapsedTimeMs = elapsedTimeMs - (60000 * minutes);
  uint32_t seconds = elapsedTimeMs/1000;
  elapsedTimeMs = elapsedTimeMs - (1000 * seconds);

  if(hours > 0)
    snprintf_chk(timeStr, bufLen, "%02lu:%02lu:%02lu.%03lu", hours, minutes, seconds, elapsedTimeMs);
  else
    snprintf_chk(timeStr, bufLen, "%02lu:%02lu.%03lu", minutes, seconds, elapsedTimeMs);
}

//=====================================================================
// Use this if the actual mtd is not known
static int hcom_exec_flash_initialize_mtd_for_testing(void)
{
  int ret;
  
  if(_test_mtd == NULL)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR MTD offset is NULL\n",
      thisFile, __LINE__);
    usleep(50 * 1000);
    return -1;
  }

  ret = _test_mtd->ioctl(_test_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&_test_geo));
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    usleep(50 * 1000);
    return ret;
  }

  _flash_test_write_page_size = _test_geo.blocksize;
  _flash_test_total_mtd_bytes = _test_geo.neraseblocks * _test_geo.erasesize;
  _flash_test_total_write_pages = _flash_test_total_mtd_bytes / _test_geo.blocksize;
  _flash_test_pages_per_4k_sector = _test_geo.erasesize / _test_geo.blocksize;

  if(!_mtdGeoShown)
  {
    syslog(LOG_MTEST, "MTD Geo-Numb Erase Sectors:%lu, Erase Size:%u Page Size:%lu, Total Pages:%lu, Pages/Sector %lu\n",
            _test_geo.neraseblocks, _test_geo.erasesize, _test_geo.blocksize,
            _flash_test_total_write_pages, _flash_test_pages_per_4k_sector);
    _mtdGeoShown = true;
  }
  return OK;
}

//=====================================================================
// Populate the buffer with a complex pattern
static void hcom_exec_flash_populate_buffer(uint32_t pageNumber, uint8_t *pageBuffer)
{
  off_t off;
  uint8_t 
  tempBuffer[3];

  // Pattern - Each 256 byte page will be divided into 64, 32-bit words. Each 32-bit
  // word will contain the page number 0 - 131072 (0x20000) (bits 0-17), the page offset
  // (bits 18-23) and the crc8 checksum of bytes 0-2 (bits 24-31)

  tempBuffer[0] = pageNumber & 0x000000ff;
  tempBuffer[1] = (pageNumber & 0x0000ff00) >> 8;
  tempBuffer[2] = (pageNumber & 0x00030000) >> 16;

  // syslog(LOG_MTEST, "pageNumber = %d buff[0] 0x%02x, buff[1] 0x%02x, buff[2] 0x%02x\n",
  //     pageNumber, tempBuffer[0], tempBuffer[1], tempBuffer[2]);

  for(off = 0; off < _flash_test_write_page_size; off += 4)
  {
    pageBuffer[off]     = tempBuffer[0];
    pageBuffer[off + 1] = tempBuffer[1];
    pageBuffer[off + 2] = tempBuffer[2];
    pageBuffer[off + 2] |= off;  // use offset / 4 (0 - 64)
    pageBuffer[off + 3] = crc8(pageBuffer + off, 3);

    // syslog(LOG_MTEST, "  offset = %04d (0x%02x) buff[2] 0x%02x, buff[3] 0x%02x\n",
    // off, ((off >> 2) & 0x3f) << 2, pageBuffer[off+2], pageBuffer[off+3]);
  }
}


//=====================================================================
// Takes a populated page buffer and verifies that its contents match
// what should be in it.
static bool hcom_exec_flash_verify_buffered_data(uint32_t pageNumber, uint8_t *pageBuffer)
{
  uint8_t testBuffer[_flash_test_write_page_size];

  hcom_exec_flash_populate_buffer(pageNumber, testBuffer);

  // syslog(LOG_MTEST, "\n--------- data read ----------\n");
  // hcom_nx_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 1);
  // syslog(LOG_MTEST, "\n--------- data calculated ----------\n");
  // hcom_nx_diag_print_buffer(testBuffer, _flash_test_write_page_size, 1);

  if(memcmp(pageBuffer, testBuffer, _flash_test_write_page_size) == 0)
    return true;

  return false;
}

//=====================================================================
// Populate the buffer with the user provided information
static void hcom_exec_flash_populate_buffer_user_pat(uint8_t userPattern, uint8_t *pageBuffer)
{
  off_t off;
  for(off = 0; off < _flash_test_write_page_size; off++)
  {
    pageBuffer[off] = userPattern;
  }
}

//=====================================================================
// 
static bool hcom_exec_flash_verify_buffered_data_2(uint8_t userPattern,
          uint32_t pageNumber, uint8_t *pageBuffer)
{
  uint8_t testBuffer[_flash_test_write_page_size];

  hcom_exec_flash_populate_buffer_user_pat(userPattern, testBuffer);

  // syslog(LOG_MTEST, "\n--------- data read ----------\n");
  // hcom_nx_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 1);
  // syslog(LOG_MTEST, "\n--------- data calculated ----------\n");
  // hcom_nx_diag_print_buffer(testBuffer, _flash_test_write_page_size, 1);

  if(memcmp(pageBuffer, testBuffer, _flash_test_write_page_size) == 0)
    return true;

  return false;
}

//===========================================================
static int hcom_exec_flash_fs_flash_test_erase_used_4k_sectors(void)
{
  return hcom_exec_flash_test_find_display_used_pages(true, false);
}

//===========================================================
static int hcom_exec_flash_test_find_display_erased_pages(void)
{
  return hcom_exec_flash_test_find_display_used_pages(false, true);
}

//===========================================================
// This function can display or erase the sectors used
static int hcom_exec_flash_test_find_display_used_pages(bool eraseUsedPages, bool displayErasedPages)
{
  off_t pageOff;
  int nread;
  int ret;
  int numbUsed = 0;
  bool patternFailed;
  bool eraseFailed;

  uint8_t pageBuffer[_flash_test_write_page_size];
  uint8_t eraseBuffer[_flash_test_write_page_size];
  memset(eraseBuffer, 0xff, _flash_test_write_page_size);

  if(eraseUsedPages && displayErasedPages)
  {
    syslog(LOG_MTEST, "Unsupported request\n");
    usleep(50 * 1000);
    return -1;
  }

  else if(eraseUsedPages)
    syslog(LOG_MTEST, "Checking %d pages to erase those used\n",
      _flash_test_total_write_pages);
  else if(displayErasedPages)
    syslog(LOG_MTEST, "Checking %d pages to display those erased\n",\
      _flash_test_total_write_pages);
  else
    syslog(LOG_MTEST, "Checking %d pages to display those not erased\n",
      _flash_test_total_write_pages);

  // Now read and test that the entire qspi flash is correct
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    patternFailed = false;
    eraseFailed = false;

    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    if(nread != 1)
    {
      syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nread:%d\n",
        thisFile, __LINE__, __func__, nread);
      usleep(50 * 1000);
      return nread;
    }

    // We are only interested in pages that do not have the
    // normal test pattern and are not erased.
    if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
      patternFailed = true;

    if(memcmp(eraseBuffer, pageBuffer, _flash_test_write_page_size) != 0)
      eraseFailed = true;

    // Erase sector if it's not erased
    if(eraseUsedPages && (eraseFailed || patternFailed))
    {
      // Note this erases multiple pages
      uint32_t sectorOff = pageOff / _flash_test_pages_per_4k_sector;
      syslog(LOG_MTEST, "Page offset %u (0x%08x) used. Erasing associated 4k sector %u (0x%08x)\n",
          pageOff, pageOff, sectorOff, sectorOff);
      ret = hcom_exec_flash_fs_flash_test_erase_1_4k_sector(sectorOff);
      if(ret < 0)
      {
        syslog(LOG_MTEST, "Unsupported request\n");
        usleep(50 * 1000);
        return ret;
      }
      
      numbUsed++;
    }
    else if(displayErasedPages && patternFailed && !eraseFailed)
    {
      // Assumes pattern written to entire flash device except where erased.
      // Used to verify erase functionality.
      // Display erased page
      uint32_t sectorOff = pageOff / _flash_test_pages_per_4k_sector;
      syslog(LOG_MTEST, "Page offset %u (0x%08x) erased. Associated 4k erase sector %u (0x%08x)\n",
          pageOff, pageOff, sectorOff, sectorOff);
      numbUsed++;
    }
    else if(patternFailed && eraseFailed)
    {
      // Just display if not pattern nor erased
      syslog(LOG_MTEST, "\n--------- data read from page# %d----------\n", pageOff);
      hcom_nx_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 1);
      numbUsed++;
    }
  }

  if(eraseUsedPages)
    syslog(LOG_MTEST, "Erased %d used pages\n", numbUsed);
  else if(displayErasedPages)
    syslog(LOG_MTEST, "Found %d erased pages\n", numbUsed);
  else
    syslog(LOG_MTEST, "Found %d pages not erased\n", numbUsed);
  
  return OK;
}

//=====================================================================
// Returns 0 for 100% success, 1-n = number failed, -value = error code
static int hcom_exec_flash_test_qspi_data_rw(bool verifyPages)
{
  uint8_t pageBuffer[_flash_test_write_page_size];
  off_t pageOff;
  int nread;
  int nfailed = 0;
  
  syslog(LOG_MTEST, "QSPI Flash data testing %d pages has begun.\n",
    _flash_test_total_write_pages);
  uint64_t testTimeEraStart = hcom_nx_utils_get_current_time64();

#if 1 // Disable to allow retesting after power cycle etc.
  int nwrite;
  syslog(LOG_MTEST, "MTD initialized to %p. Bulk erasing QSPI flash (2-3 minutes)\n",
    _test_mtd);
  int ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    return ret;
  }

  uint64_t testTimeEraEnd = hcom_nx_utils_get_current_time64();
  syslog(LOG_MTEST, "Bulk erase of %lu completed.\n",
    _flash_test_total_write_pages);
  if(verifyPages)
    syslog(LOG_MTEST, "Write all %lu pages then verify them.\n",
      _flash_test_total_write_pages);
  else
    syslog(LOG_MTEST, "Write all %lu pages then exit.\n",
      _flash_test_total_write_pages);

  usleep(10);

  // Verify 100% erased
  syslog(LOG_MTEST, "Verifying %lu pages erased.\n",
    _flash_test_total_write_pages);
  ret = hcom_exec_flash_test_find_display_used_pages(false, false);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s(). Verifying erased pages - ret:%d\n",
              thisFile, __LINE__, __func__, ret);
    return ret;
  }
  usleep(10);
  uint64_t testTimeVerEnd = hcom_nx_utils_get_current_time64();

  // Fill the entire QSPI flash with data
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    hcom_exec_flash_populate_buffer(pageOff, pageBuffer);

    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
    {
      syslog(LOG_MTEST, "INFO-Writing to page %d of %d\n",
        pageOff, _flash_test_total_write_pages);
      usleep(10);
    }

    nwrite = MTD_BWRITE(_test_mtd, pageOff, 1, pageBuffer);
    if(nwrite != 1)
    {
      syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nwrite:%d\n",
        thisFile, __LINE__, __func__, nwrite);
      return ret;
    }
  }
#endif

  if(!verifyPages)
  {
    syslog(LOG_MTEST, "Pattern written to %d pages\n",
      _flash_test_total_write_pages);
    return OK;
  }
  
  syslog(LOG_MTEST, "Data written. Verifying %d pages\n",
    _flash_test_total_write_pages);
  
  // Now read and test that the entire qspi flash is correct
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
    {
      syslog(LOG_MTEST, "INFO-Verifying page %d of %d\n",
        pageOff, _flash_test_total_write_pages);
      usleep(10);
    }

    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    if(nread != 1)
    {
      syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nread:%d\n",
        thisFile, __LINE__, __func__, nread);
      return ret;
    }

    if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
    {
      if(nfailed == 0)
      {
        syslog(LOG_MTEST, "Page %d was first to fail to compare\n", pageOff);
      }
      else
      {
        syslog(LOG_MTEST, "Page %d also failed to compare\n", pageOff);
      }
      nfailed++;
    }
  }
  uint64_t testTimeTestEnd = hcom_nx_utils_get_current_time64();

  syslog(LOG_MTEST, "Write / read test completed with %d errors\n", nfailed);
  
  char timeBufEra[16];
  char timeVerTest[16];
  char timeBufTest[16];
  char timeTotTest[16];

  hcom_exec_flash_convert_ms_to_time((testTimeEraEnd - testTimeEraStart)/1000000, timeBufEra, 16);
  hcom_exec_flash_convert_ms_to_time((testTimeVerEnd - testTimeEraEnd)/1000000, timeVerTest, 16);
  hcom_exec_flash_convert_ms_to_time((testTimeTestEnd - testTimeVerEnd)/1000000, timeBufTest, 16);
  hcom_exec_flash_convert_ms_to_time((testTimeTestEnd - testTimeEraStart)/1000000, timeTotTest, 16);
  syslog(LOG_MTEST, "Bulk erase took:%s, verify erase: %s and fill & verify took:%s total:%s\n",
            timeBufEra, timeVerTest, timeBufTest, timeTotTest);

  return nfailed == 0 ? OK : nfailed;
}

//=====================================================================
// Returns 0 for 100% success, 1-n = number failed, -value = error code
// Erases, writes 0x55 to every location & verifies. Erases, writes 0xaa
// to every location & verifies.
static int hcom_exec_flash_test_qspi_data_rw_2(void)
{
  uint8_t pageBuffer[_flash_test_write_page_size];
  off_t pageOff;
  int nread;
  int nfailed = 0;

  syslog(LOG_MTEST, "QSPI Flash data testing %d pages has begun.\n",
    _flash_test_total_write_pages);
  uint64_t testTimeTestStart = hcom_nx_utils_get_current_time64();

  int nwrite;
  syslog(LOG_MTEST, "MTD initialized to %p. Bulk erasing QSPI flash (2-3 minutes)\n",
    _test_mtd);
  int ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    return ret;
  }

  syslog(LOG_MTEST, "Bulk erase #1 of %lu completed.\n",
    _flash_test_total_write_pages);
  syslog(LOG_MTEST, "Write all %lu pages then verify them.\n",
    _flash_test_total_write_pages);
  usleep(10);

  // Verify 100% erased
  syslog(LOG_MTEST, "Verifying %lu pages erased.\n",
    _flash_test_total_write_pages);
  ret = hcom_exec_flash_test_find_display_used_pages(false, false);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s(). Verifying erased pages - ret:%d\n",
              thisFile, __LINE__, __func__, ret);
    return ret;
  }
  usleep(10);

  // Fill the entire QSPI flash with data
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    hcom_exec_flash_populate_buffer_user_pat(0x55, pageBuffer);

    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
    {
      syslog(LOG_MTEST, "INFO-Writing to page %d of %d\n",
        pageOff, _flash_test_total_write_pages);
      usleep(10);
    }

    nwrite = MTD_BWRITE(_test_mtd, pageOff, 1, pageBuffer);
    if(nwrite != 1)
    {
      syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nwrite:%d\n",
        thisFile, __LINE__, __func__, nwrite);
      return ret;
    }
  }

  syslog(LOG_MTEST, "Data written. Verifying %d pages\n",
    _flash_test_total_write_pages);
  
  // Now read and test that the entire qspi flash is correct
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
    {
      syslog(LOG_MTEST, "INFO-Verifying page %d of %d\n",
        pageOff, _flash_test_total_write_pages);
      usleep(10);
    }

    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    if(nread != 1)
    {
      syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nread:%d\n",
        thisFile, __LINE__, __func__, nread);
      return ret;
    }

    if(!hcom_exec_flash_verify_buffered_data_2(0x55, pageOff, pageBuffer))
    {
      if(nfailed == 0)
      {
        syslog(LOG_MTEST, "Page %d was first to fail to compare\n",
          pageOff);
      }
      else
      {
        syslog(LOG_MTEST, "Page %d also failed to compare\n",
          pageOff);
      }
      nfailed++;
    }
  }

  // Now repeat the process using a different pattern

  syslog(LOG_MTEST, "MTD initialized to %p. Bulk erasing QSPI flash again (2-3 minutes)\n",
    _test_mtd);
  ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    return ret;
  }

  syslog(LOG_MTEST, "Bulk erase #2 of %lu completed.\n",
    _flash_test_total_write_pages);
  syslog(LOG_MTEST, "Write all %lu pages then verify them.\n",
    _flash_test_total_write_pages);
  usleep(10);

  // Verify 100% erased
  syslog(LOG_MTEST, "Verifying %lu pages erased again.\n",
    _flash_test_total_write_pages);
  ret = hcom_exec_flash_test_find_display_used_pages(false, false);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s(). Verifying erased pages, ret:%d\n",
              thisFile, __LINE__, __func__, ret);
    return ret;
  }
  usleep(10);

  // Fill the entire QSPI flash with data
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    hcom_exec_flash_populate_buffer_user_pat(0xaa, pageBuffer);

    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
    {
      syslog(LOG_MTEST, "INFO-Writing to page %d of %d\n",
        pageOff, _flash_test_total_write_pages);
      usleep(10);
    }

    nwrite = MTD_BWRITE(_test_mtd, pageOff, 1, pageBuffer);
    if(nwrite != 1)
    {
      syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nwrite:%d\n",
        thisFile, __LINE__, __func__, nwrite);
      return ret;
    }
  }

  syslog(LOG_MTEST, "Data written. Verifying %d pages again\n",
    _flash_test_total_write_pages);
  
  // Now read and test that the entire qspi flash is correct
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
    {
      syslog(LOG_MTEST, "INFO-Verifying page %d of %d\n",
        pageOff, _flash_test_total_write_pages);
      usleep(10);
    }

    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    if(nread != 1)
    {
      syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nread:%d\n",
        thisFile, __LINE__, __func__, nread);
      return ret;
    }

    if(!hcom_exec_flash_verify_buffered_data_2(0xaa, pageOff, pageBuffer))
    {
      if(nfailed == 0)
      {
        syslog(LOG_MTEST, "Page %d was first to fail to compare\n", pageOff);
      }
      else
      {
        syslog(LOG_MTEST, "Page %d also failed to compare\n", pageOff);
      }
      nfailed++;
    }
  }
  uint64_t testTimeTestEnd = hcom_nx_utils_get_current_time64();

  syslog(LOG_MTEST, "Write / read test completed with %d errors\n", nfailed);
  
  char timeTotTest[16];
  hcom_exec_flash_convert_ms_to_time((testTimeTestEnd - testTimeTestStart)/1000000, timeTotTest, 16);

  syslog(LOG_MTEST, "Total test time:%s\n", timeTotTest);

  return nfailed == 0 ? OK : nfailed;
}

//=====================================================================
// This test takes years to complete. Nice test but not practical
static int hcom_exec_flash_qspi_comprehensive_test(void)
{
  int ret;
  off_t pageOff;
  int nwrite, nread;
  off_t beforeOff, afterOff;
  int errCount = 0;

  uint8_t pageBuffer[_flash_test_write_page_size];
  uint8_t eraseBuffer[_flash_test_write_page_size];
  memset(eraseBuffer, 0xff, _flash_test_write_page_size);

  syslog(LOG_MTEST, "Comprehensive testing %d pages has begun.\n",
    _flash_test_total_write_pages);
  syslog(LOG_MTEST, "This checks that the driver only writes to 1 page and the correct page\n");
  syslog(LOG_MTEST, "Bulk erasing QSPI flash (2-3 minutes)\n");

  uint64_t testTimeEraStart = hcom_nx_utils_get_current_time64();
  ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    usleep(50 * 1000);
    return ret;
  }

  uint64_t testTimeEraEnd = hcom_nx_utils_get_current_time64();
  syslog(LOG_MTEST, "Bulk erase completed\n");
  usleep(10);

  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    // Write next page
    hcom_exec_flash_populate_buffer(pageOff, pageBuffer);
    nwrite = MTD_BWRITE(_test_mtd, pageOff, 1, pageBuffer);
    if(nwrite != 1)
    {
      syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nwrite:%d\n",
        thisFile, __LINE__, __func__, nwrite);
      usleep(50 * 1000);
      return nwrite;
    }

    // Clear pageBuffer (probably a waste of time...) and read
    memset(pageBuffer, 0x00, _flash_test_write_page_size);
    // And verify that this page has been written correctly
    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    if(nread != 1)
    {
      syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nread:%d\n",
        thisFile, __LINE__, __func__, nread);
      usleep(50 * 1000);
      return nread;
    }

    if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
    {
      syslog(LOG_MTEST, "Just written page %d failed to compare\n",
        pageOff);
    }

    // Next verify that all proceeding and subsequent pages are
    // correct. Those before should have data and those following
    // should be erased (0xff).
    for(beforeOff = 0; beforeOff < pageOff; beforeOff++)
    {
      if(beforeOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
      {
        syslog(LOG_MTEST, "INFO-Testing:%d Previous page %d of %d. Errors:%lu\n",
          pageOff, beforeOff, _flash_test_total_write_pages, errCount);
        usleep(10);
      }

      nread = MTD_BREAD(_test_mtd, beforeOff, 1, pageBuffer);
      if(nread != 1)
      {
        syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nread:%d\n",
          thisFile, __LINE__, __func__, nread);
        usleep(50 * 1000);
        return nread;
      }

      // Check for expected data
      if(!hcom_exec_flash_verify_buffered_data(beforeOff, pageBuffer))
      {
        errCount++;
        syslog(LOG_MTEST, "Proceeding page %d failed to compare\n",
          beforeOff);
      }
    }

    for(afterOff = pageOff + 1; afterOff < _flash_test_total_write_pages; afterOff++)
    {
      if(afterOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
      {
        syslog(LOG_MTEST, "INFO-Testing:%d After page %d of %d. Errors:%lu\n",
          pageOff, afterOff, _flash_test_total_write_pages, errCount);
        usleep(10);
      }

      // Clear pageBuffer (probably a waste of time...) and read
      memset(pageBuffer, 0x00, _flash_test_write_page_size);
      nread = MTD_BREAD(_test_mtd, afterOff, 1, pageBuffer);
      if(nread != 1)
      {
        syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nread:%d\n",
          thisFile, __LINE__, __func__, nread);
        usleep(50 * 1000);
        return nread;
      }

      // Find any non-zero values
      if(memcmp(eraseBuffer, pageBuffer, _flash_test_write_page_size) != 0)
      {
        errCount++;
        syslog(LOG_MTEST, "Following page %d failed to compare\n", afterOff);
        usleep(10);
      }
    }
  }

  uint64_t testTimeTestEnd = hcom_nx_utils_get_current_time64();

  syslog(LOG_MTEST, "Bulk erase took:%llu mSec and testing took:%llu mSec\n",
            (testTimeEraEnd - testTimeEraStart)/1000000,
            (testTimeTestEnd - testTimeEraEnd) /1000000);

  return errCount;
}

//=======================================================================================
static int hcom_exec_flash_test_read_display_1_page(uint32_t pageOffset)
{
  uint8_t pageBuffer[_flash_test_write_page_size];

  syslog(LOG_MTEST, "Reading 256 bytes from QSPI flash at page# %d\n",
    pageOffset);
  int nread = MTD_BREAD(_test_mtd, pageOffset, 1, pageBuffer);
  if(nread != 1)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - nread:%d\n",
      thisFile, __LINE__, __func__, nread);
    usleep(50 * 1000);
    return nread;
  }

  syslog(LOG_MTEST, "\n--------- data read from page# %d----------\n", pageOffset);
  hcom_nx_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 1);
  return OK;
}

//=======================================================================================
// Erase sectors are 4096 bytes each
static int hcom_exec_flash_fs_flash_test_erase_1_4k_sector(uint32_t sectorOffset)
{
  int ret;

  syslog(LOG_MTEST, "Erasing QSPI flash erase sector# %d\n",
    sectorOffset);
  ret = MTD_ERASE(_test_mtd, sectorOffset, 1);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    usleep(50 * 1000);
    return ret;
  }

  syslog(LOG_MTEST, "Sector erase of QSPI flash completed\n");
  return OK;
}

//=======================================================================================
static int hcom_exec_flash_fs_flash_test_erase_entire_flash(void)
{
  int ret;

  syslog(LOG_MTEST, "Bulk erasing QSPI flash begun\n");
  usleep(10);

  ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    usleep(50 * 1000);
    return ret;
  }

  syslog(LOG_MTEST, "Bulk erase of QSPI flash completed\n");
  return OK;
}

//=======================================================================================
// This function is used to test the qspi flash
int hcom_nx_exec_test_qspi_flash_write(struct hcom_nx_cmd_data *cmdData)
{
  uint8_t pageBuffer[_flash_test_write_page_size];
  int userData = (int)cmdData->userData;
  int ret;

  _mtdGeoShown = false;

  ret = hcom_exec_flash_initialize_mtd_for_testing();
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    usleep(50 * 1000);
    return ret;
  }

  switch(userData)
  {
    case -1:
      ret = hcom_exec_flash_test_qspi_data_rw(/*verifyPages=*/true);
      break;

    case -2:
      ret = hcom_exec_flash_test_qspi_data_rw(/*verifyPages=*/false);
      break;

    case -3:
      ret = hcom_exec_flash_qspi_comprehensive_test();
      break;

    case -4:
      ret = hcom_exec_flash_test_qspi_data_rw_2();
      break;

    default:
      // Populate the buffer with userData for a page
      hcom_exec_flash_populate_buffer(userData, pageBuffer);

      // Write buffer to requested page
      ret = MTD_BWRITE(_test_mtd, userData, 1, pageBuffer);
      if(ret != 1)    // Check number written
      {
        syslog(LOG_MTEST, "%s@%d-ERROR in %s() - number written:%d. Must be 1\n",
                  thisFile, __LINE__, __func__, ret);
        usleep(50 * 1000);
        ret = -1;
      }
      break;
  }

  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR: Write test with ret:%d (command:%d)\n",
              thisFile, __LINE__, ret, userData);
  }
  else if (ret > 0)
  {
    syslog(LOG_MTEST, "%s@%d-Write test reported %d flash write errors (command:%d)\n",
              thisFile, __LINE__, ret, userData);
  }
  else
  {
    syslog(LOG_MTEST, "Success-Write test passed 100%% (command:%d)\n", userData);
  }

  return ret;
}

//=======================================================================================
// This function is used to test the qspi flash
int hcom_nx_exec_test_qspi_flash_init(struct hcom_nx_cmd_data *cmdData)
{
  int userData = (int)cmdData->userData;
  int ret;

  _mtdGeoShown = false;

  ret = hcom_exec_flash_initialize_mtd_for_testing();
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    usleep(50 * 1000);
    return ret;
  }
  
  switch(userData)
  {
    case -1:
      ret = hcom_exec_flash_fs_flash_test_erase_entire_flash();
      break;

     case -2:
      ret = hcom_exec_flash_fs_flash_test_erase_used_4k_sectors();
      break;

   default:
      ret = hcom_exec_flash_fs_flash_test_erase_1_4k_sector(userData);
      break;
  }
  
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-Init test command:%d ERROR with ret:%d\n",
      thisFile, __LINE__, userData, ret);
  }
  else
  {
    syslog(LOG_MTEST, "%s@%d-Init test command:%d Succeeded\n",
      thisFile, __LINE__, userData);
  }
  return ret;
}

//=======================================================================================
// This function is used to test the qspi flash
int hcom_nx_exec_test_qspi_flash_read(struct hcom_nx_cmd_data *cmdData)
{
  int userData = (int)cmdData->userData;
  int ret;

  _mtdGeoShown = false;

  ret = hcom_exec_flash_initialize_mtd_for_testing();
  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-ERROR in %s() - ret:%d\n",
      thisFile, __LINE__, __func__, ret);
    usleep(50 * 1000);
    return ret;
  }

  switch((int32_t)userData)
  {
    case -1:
      ret = hcom_exec_flash_test_find_display_used_pages(false, false);
      break;

    case -2:
      ret = hcom_exec_flash_test_find_display_erased_pages();
      break;

    default:
      ret = hcom_exec_flash_test_read_display_1_page(userData);
      break;
  }

  if(ret < 0)
  {
    syslog(LOG_MTEST, "%s@%d-Read test command:%d ERROR with ret:%d\n",
      thisFile, __LINE__, userData, ret);
  }
  else
  {
    syslog(LOG_MTEST, "%s@%d-Read test command:%d Succeeded\n",
      thisFile, __LINE__, userData);
  }
  return ret;
}

#endif
