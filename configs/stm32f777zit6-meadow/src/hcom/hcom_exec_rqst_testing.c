/****************************************************************************
 * configs/stm32f777-zit6-meadow/src/hcom/hcom_exec_rqst_testing.c
 * 
 *   Copyright (C) 2019 Wilderness Labs. All rights reserved.
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2017 Alan Carvalho de Assis. All rights reserved.
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

#include "hcom_common.h"

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include <crc8.h>

#include "stm32_qspi.h"
#include <nuttx/spi/qspi.h>
#include <dirent.h>
//#include <nuttx/mm/mm.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Configuration ************************************************************/

// Tried to align terminology with the s25fl256 docs. Page(256), Sector (4K)
// and Block (64k)
#define FLASH_TEST_DISPLAY_INTERVAL 1024

/****************************************************************************
 * Private Data
 ****************************************************************************/

static FAR struct mtd_dev_s *_test_mtd;
static FAR struct mtd_geometry_s _test_geo;
static uint32_t _flash_test_write_page_size;
static uint32_t _flash_test_total_mtd_bytes;
static uint32_t _flash_test_total_write_pages;
static uint32_t _flash_test_pages_per_4k_sector;

//static   void * memTest;
/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static void hcom_exec_flash_fs_flash_test_erase_1_4k_sector(uint32_t sectorOffset);
static void hcom_exec_flash_test_find_display_used_pages(bool eraseUsedPages, bool displayErasedPages);

/****************************************************************************
 * Public Functions
 ****************************************************************************/
int hcom_exec_rqst_testing_setup(FAR struct mtd_dev_s *mtd)
{
  _test_mtd = mtd;
  return OK;
}

//=====================================================================
static int hcom_exec_flash_initialize_mtd_for_testing(void)
{
  if(_test_mtd != NULL)
    return OK;

  int ret = _test_mtd->ioctl(_test_mtd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&_test_geo));
  DEBUGASSERT(ret == OK);

  _flash_test_write_page_size = _test_geo.blocksize;
  _flash_test_total_mtd_bytes = _test_geo.neraseblocks * _test_geo.erasesize;
  _flash_test_total_write_pages = _flash_test_total_mtd_bytes / _test_geo.blocksize;
  _flash_test_pages_per_4k_sector = _test_geo.erasesize / _test_geo.blocksize;

  // syslog(0, "MTD Geo info - nerasesectors %u, erasesize %u pagesize %u, total pages %u, pages/sector %u\n",
  //          _test_geo.neraseblocks, _test_geo.erasesize, _test_geo.blocksize,
  //           _flash_test_total_write_pages, _flash_test_pages_per_4k_sector);
  return OK;
}

//=====================================================================
static void hcom_exec_flash_populate_buffer(uint32_t pageNumber, uint8_t *pageBuffer)
{
  off_t off;
  uint8_t 
  tempBuffer[3];

  // Pattern will be - Each 256 byte page will be divided into 64, 32-bit words. Each 32-bit
  // word will contain the page number 0 - 131072 (0x20000) (bits 0-17), the page offset
  // (bits 18-23) and the crc8 checksum of bytes 0-2 (bits 24-31)

  tempBuffer[0] = pageNumber & 0x000000ff;
  tempBuffer[1] = (pageNumber & 0x0000ff00) >> 8;
  tempBuffer[2] = (pageNumber & 0x00030000) >> 16;

  // syslog(0, "pageNumber = %d buff[0] 0x%02x, buff[1] 0x%02x, buff[2] 0x%02x\n",
  //     pageNumber, tempBuffer[0], tempBuffer[1], tempBuffer[2]);

  for(off = 0; off < _flash_test_write_page_size; off += 4)
  {
    pageBuffer[off]     = tempBuffer[0];
    pageBuffer[off + 1] = tempBuffer[1];
    pageBuffer[off + 2] = tempBuffer[2];
    pageBuffer[off + 2] |= off;  // use offset / 4 (0 - 64)
    pageBuffer[off + 3] = crc8(pageBuffer + off, 3);

    // syslog(0, "  offset = %04d (0x%02x) buff[2] 0x%02x, buff[3] 0x%02x\n", off, ((off >> 2) & 0x3f) << 2,
    //   pageBuffer[off+2], pageBuffer[off+3]);
  }
}

//=====================================================================
// Takes a populated page buffer and verifies that its contents match
// what should be in it.
static bool hcom_exec_flash_verify_buffered_data(uint32_t pageNumber, uint8_t *pageBuffer)
{
  uint8_t testBuffer[_flash_test_write_page_size];

  hcom_exec_flash_populate_buffer(pageNumber, testBuffer);

  // syslog(0, "\n--------- data read ----------\n");
  // hcom_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 0);
  // syslog(0, "\n--------- data calculated ----------\n");
  // hcom_diag_print_buffer(testBuffer, _flash_test_write_page_size, 0);

  if(memcmp(pageBuffer, testBuffer, _flash_test_write_page_size) == 0)
    return true;

  return false;
}

//===========================================================
static void hcom_exec_flash_fs_flash_test_erase_used_4k_sectors(void)
{
  hcom_exec_flash_test_find_display_used_pages(true, false);
}

//===========================================================
static void hcom_exec_flash_test_find_display_erased_pages(void)
{
  hcom_exec_flash_test_find_display_used_pages(false, true);
}

//===========================================================
// This function can display or erase the used sectors
static void hcom_exec_flash_test_find_display_used_pages(bool eraseUsedPages, bool displayErasedPages)
{
  off_t pageOff;
  int nread;
  int numbUsed = 0;
  bool patternFailed;
  bool eraseFailed;

  uint8_t pageBuffer[_flash_test_write_page_size];
  uint8_t eraseBuffer[_flash_test_write_page_size];
  memset(eraseBuffer, 0xff, _flash_test_write_page_size);

  if(eraseUsedPages && displayErasedPages)
  {
    syslog(0, "Unsupported request\n");
    return;
  }
  else if(eraseUsedPages)
    syslog(0, "Checking %d pages to erase those used\n", _flash_test_total_write_pages);
  else if(displayErasedPages)
    syslog(0, "Checking %d pages to display those erased\n", _flash_test_total_write_pages);
  else
    syslog(0, "Checking %d pages to display those used\n", _flash_test_total_write_pages);

  // Now read and test that the entire qspi flash is correct
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    patternFailed = false;
    eraseFailed = false;

    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nread == 1);

    // We are only interested in pages that do not have the
    // normal test pattern and are not erased.
    if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
      patternFailed = true;

    if(memcmp(eraseBuffer, pageBuffer, _flash_test_write_page_size) != 0)
      eraseFailed = true;

    // Erase sectir if it's not erased
    if(eraseUsedPages && (eraseFailed || patternFailed))
    {
      // Note this erases multiple pages
      uint32_t sectorOff = pageOff / _flash_test_pages_per_4k_sector;
      syslog(0, "Page offset %u (0x%08x) used. Erasing associated 4k sector %u (0x%08x)\n",
          pageOff, pageOff, sectorOff, sectorOff);
      hcom_exec_flash_fs_flash_test_erase_1_4k_sector(sectorOff);
      numbUsed++;
    }
    else if(displayErasedPages && patternFailed && !eraseFailed)
    {
      // Assumes pattern written to entire flash device except where erased.
      // Used to verify erase functionality.
      // Display erased page
      uint32_t sectorOff = pageOff / _flash_test_pages_per_4k_sector;
      syslog(0, "Page offset %u (0x%08x) erased. Associated 4k erase sector %u (0x%08x)\n",
          pageOff, pageOff, sectorOff, sectorOff);
      numbUsed++;
    }
    else if(patternFailed && eraseFailed)
    {
      // Just display if not pattern nor erased
      syslog(0, "\n--------- data read from page# %d----------\n", pageOff);
      hcom_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 0);
      numbUsed++;
    }
  }

  if(eraseUsedPages)
    syslog(0, "Erased %d used pages\n", numbUsed);
  else if(displayErasedPages)
    syslog(0, "Found %d erased pages\n", numbUsed);
  else
    syslog(0, "Found %d used pages\n", numbUsed);
}

//=====================================================================
static bool hcom_exec_flash_test_qspi_data_rw(bool verifyPages)
{
  uint8_t pageBuffer[_flash_test_write_page_size];
  off_t pageOff;
  int nread;
  int nfailed = 0;
  
  syslog(0, "QSPI Flash data testing %d pages has begun.\n", _flash_test_total_write_pages);

#if 1 // Disable to allow retesting after power cycle etc.
  int nwrite;
  syslog(0, "MTD initialized to %p. Bulk erasing QSPI flash next.\n", _test_mtd);
  int ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  DEBUGASSERT(ret == OK);
  syslog(0, "Bulk erase completed. Writing data to %d pages\n", _flash_test_total_write_pages);

  // Fill the entire QSPI flash with data
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    hcom_exec_flash_populate_buffer(pageOff, pageBuffer);

    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
      syslog(0, "Writing to page %d of %d\n", pageOff, _flash_test_total_write_pages);

    nwrite = MTD_BWRITE(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nwrite == 1);
  }
#endif

  if(!verifyPages)
  {
    syslog(0, "Pattern written to %d pages\n", _flash_test_total_write_pages);
    return true;
  }
  
  syslog(0, "Data written. Verifying %d pages\n", _flash_test_total_write_pages);
  
  // Now read and test that the entire qspi files is correct
  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
      syslog(0, "Verifying page %d of %d\n", pageOff, _flash_test_total_write_pages);

    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nread == 1);

    if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
    {
      if(nfailed == 0)
      {
        syslog(0, "Page %d first to fail to compare\n", pageOff);
      }
      else
      {
        if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
          syslog(0, "Page %d also failed to compare\n", pageOff);
      }
      nfailed++;
    }
  }
  syslog(0, "Write / read test completed with %d errors\n", nfailed);

  return nfailed ? true : false;
}

//=====================================================================
static void hcom_exec_flash_qspi_comprehensive_test(void)
{
  int ret;
  off_t pageOff;
  int nwrite, nread;
  off_t beforeOff, afterOff;

  uint8_t pageBuffer[_flash_test_write_page_size];
  uint8_t eraseBuffer[_flash_test_write_page_size];
  memset(eraseBuffer, 0xff, _flash_test_write_page_size);

  syslog(0, "Comprehensive testing %d pages has begun.\n", _flash_test_total_write_pages);
  syslog(0, "Bulk erasing QSPI flash\n");
  ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  DEBUGASSERT(ret == OK);

  for(pageOff = 0; pageOff < _flash_test_total_write_pages; pageOff++)
  {
    if(pageOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
      syslog(0, "Testing page %d of %d\n", pageOff, _flash_test_total_write_pages);

    // Write next page
    hcom_exec_flash_populate_buffer(pageOff, pageBuffer);
    nwrite = MTD_BWRITE(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nwrite == 1);

    // And verify that this page has been written correctly
    nread = MTD_BREAD(_test_mtd, pageOff, 1, pageBuffer);
    DEBUGASSERT(nread == 1);
    if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
    {
      syslog(0, "Just written page %d failed to compare\n", pageOff);
    }

    // Next verify that all proceeding and subsequent pages are
    // correct. Those before should have data and those following
    // should be erased (0xff).
    for(beforeOff = 0; beforeOff < pageOff; beforeOff++)
    {
      if(beforeOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
          syslog(0, "Testing before page %d of %d\n", beforeOff, _flash_test_total_write_pages);

      nread = MTD_BREAD(_test_mtd, beforeOff, 1, pageBuffer);
      DEBUGASSERT(nread == 1);
      if(!hcom_exec_flash_verify_buffered_data(pageOff, pageBuffer))
      {
        syslog(0, "Proceeding page %d failed to compare\n", beforeOff);
      }
    }

    for(afterOff = pageOff + 1; afterOff < _flash_test_total_write_pages; afterOff++)
    {
      if(afterOff % FLASH_TEST_DISPLAY_INTERVAL == 0)
          syslog(0, "Testing after page %d of %d\n", afterOff, _flash_test_total_write_pages);

      nread = MTD_BREAD(_test_mtd, afterOff, 1, pageBuffer);
      DEBUGASSERT(nread == 1);
      if(memcmp(eraseBuffer, pageBuffer, _flash_test_write_page_size) != 0)
      {
        syslog(0, "Following page %d failed to compare\n", afterOff);
      }
    }
  }
  return;
}

//=======================================================================================
static void hcom_exec_flash_test_read_display_1_page(uint32_t pageOffset)
{
  uint8_t pageBuffer[_flash_test_write_page_size];

  syslog(0, "Reading 256 bytes from QSPI flash at page# %d\n", pageOffset);
  int nread = MTD_BREAD(_test_mtd, pageOffset, 1, pageBuffer);
  DEBUGASSERT(nread == 1);

  syslog(0, "\n--------- data read from page# %d----------\n", pageOffset);
  hcom_diag_print_buffer(pageBuffer, _flash_test_write_page_size, 0);
}

//=======================================================================================
// Erase secctors are 4096 bytes each
static void hcom_exec_flash_fs_flash_test_erase_1_4k_sector(uint32_t sectorOffset)
{
  int ret;

  syslog(0, "Erasing QSPI flash erase sector# %d\n", sectorOffset);
  ret = MTD_ERASE(_test_mtd, sectorOffset, 1);
  DEBUGASSERT(ret >= 0);
  syslog(0, "Sector erase of QSPI flash completed\n");
}

//=======================================================================================
static void hcom_exec_flash_fs_flash_test_erase_entire_flash(void)
{
  int ret;

  syslog(0, "Bulk erasing QSPI flash\n");
  ret = _test_mtd->ioctl(_test_mtd, MTDIOC_BULKERASE, 0);
  DEBUGASSERT(ret == OK);
  syslog(0, "Bulk erase of QSPI flash completed\n");
}

//=======================================================================================
// This function is only used to test the qspi flash as it is not working properly
void hcom_exec_rqst_testing_flash_qspi_write(uint32_t userData)
{
  uint8_t pageBuffer[_flash_test_write_page_size];
  int ret;

  if(_test_mtd == NULL)
  {
    ret = hcom_exec_flash_initialize_mtd_for_testing();
    DEBUGASSERT(ret == OK);
  }

  switch((int32_t)userData)
  {
    case -1:
      hcom_exec_flash_test_qspi_data_rw(true);
      break;

    case -2:
      hcom_exec_flash_test_qspi_data_rw(false);
      break;

    case -3:
      hcom_exec_flash_qspi_comprehensive_test();
      break;

    default:
      // Populate the buffer with one page number and write it to MTD
      hcom_exec_flash_populate_buffer(userData, pageBuffer);
      ret = MTD_BWRITE(_test_mtd, userData, 1, pageBuffer);
      DEBUGASSERT(ret == 1);
      break;
  }
  syslog(0, "Testing command for flash completed\a\n");

  return;
}
//=======================================================================================
// This function is only used to test the qspi flash as it is not working properly
void hcom_exec_rqst_testing_flash_qspi_init(uint32_t userData)
{
  int ret;

  if(_test_mtd == NULL)
  {
    ret = hcom_exec_flash_initialize_mtd_for_testing();
    DEBUGASSERT(ret == OK);
  }
  
  switch((int32_t)userData)
  {
    case -1:
      hcom_exec_flash_fs_flash_test_erase_entire_flash();    
      break;

     case -2:
      hcom_exec_flash_fs_flash_test_erase_used_4k_sectors();    
      break;

   default:
      hcom_exec_flash_fs_flash_test_erase_1_4k_sector(userData);
      break;
  }
  
  syslog(0, "Erase command for flash completed\a\n");
}

//=======================================================================================
// This function is only used to test the qspi flash as it is not working properly
void hcom_exec_rqst_testing_flash_qspi_read(uint32_t userData)
{
  int ret;

  if(_test_mtd == NULL)
  {
    ret = hcom_exec_flash_initialize_mtd_for_testing();
    DEBUGASSERT(ret == OK);
  }

  switch((int32_t)userData)
  {
    case -1:
      hcom_exec_flash_test_find_display_used_pages(false, false);
      break;

    case -2:
      hcom_exec_flash_test_find_display_erased_pages();
      break;

    default:
      hcom_exec_flash_test_read_display_1_page(userData);
      break;
  }

  syslog(0, "Read command for flash completed\a\n");
}

//======================================================================
void hcom_exec_rqst_testing_developer_1(uint32_t userData)
{
  f7syslog(LOG_WARNING, "%s not implemented\n", __func__);

  //memTest = malloc(1024 * userData);
  //memTest = kmm_malloc(1024 * userData);

  // if(memTest == NULL)
  //   syslog(0, "****************Allocation failed\n");

  // DIR *dirp;

  // dirp = opendir(HCOM_FILE_MOUNT_POINT_TARGET);
  // if ( !dirp )
  // {
  //   f7syslog(LOG_ERR, "ERROR: opendir(\"%s\") failed with errno=%d\n", HCOM_FILE_MOUNT_POINT_TARGET, errno);
  // }

  // closedir(dirp);

// int ret;
  
//   syslog(0, "%s() - userData = %d\n", __func__, userData);
//   int argc = 1;
//   char *argv[1];
//   strcpy(argv[0], "TestStdoutBefore");

// // This may never return
//   int ret = (*USERSPACE->us_entrypoint)((int)argc, argv);
//   syslog(0, "%s() - TestStdoutBefore exited ret = %d\n", __func__, ret);=
}

// //=============================================================
// // This is all related to developer_2
// static uint32_t dev2_user_data;
// //static int dev2_previous_thread_pid;
// //-----------
// // Test code
// static int hcom_test_pipe_server(int argc, char *argv[])
// {
//   int ret;
//   syslog(0, "%s() - Pipe Test Thread passing argc = %d\n",
//       __func__, dev2_user_data); sleep(4);

//   char *myArgv[1];
//   myArgv[0] = "TestPipe";

//   // Now send the requested command
//   syslog(0, "%s() - Now requested being passed down argc = %d, argv = %s\n",
//       __func__, dev2_user_data, myArgv[0]);
    
//   ret = (*USERSPACE->us_entrypoint)((int)dev2_user_data, myArgv);
//   syslog(0, "%s() - Pipe Test thread terminated = %d\n", __func__, ret);
//   return 0;
// }
// //---------------
void hcom_exec_rqst_testing_developer_2(uint32_t userData)
{
  f7syslog(LOG_WARNING, "%s not implemented\n", __func__);

  //free(memTest);
  
  //kmm_free(memTest);

// int ret;
  
//   syslog(0, "%s() - userData = %d\n", __func__, userData);

//   // Set up a call so the pipe code can be tested
//   dev2_user_data = userData;

//   int pid = kthread_create("pipeTester",
//     100, 1024, (main_t)hcom_test_pipe_server,
//     (FAR char * const *)  NULL);
//   if(pid <= 0)
//   {
//     syslog(0, "%s() - thread create failed = %d\n", __func__, pid);
//     return;
//   }

}

//=============================================================
void hcom_exec_rqst_testing_developer_3(uint32_t userData)
{
  // int ret;
  int i;

  for(i = 1; i <= userData; i++)
  {
    if((i % 50) == 0)
      syslog(0, "Number is %d\n", i);
    f7syslog_host(0, "This Message is from %s. The number is %d\n", __func__, i);
  }
  syslog(0, "Sent all requested %d\n", i);

  // syslog(0, "%s() - userData = %d\n", __func__, userData);
  // int argc = 1;
  // char *myArgv[1];
  // myArgv[0] = "RedirectStdout";

  // // Now send the requested command    
  // ret = (*USERSPACE->us_entrypoint)((int)argc, myArgv);
  // syslog(0, "%s() - RedirectStdout exited ret = %d\n", __func__, ret);
}

//=============================================================
void hcom_exec_rqst_testing_developer_4(uint32_t userData)
{
  f7syslog(LOG_WARNING, "%s not implemented\n", __func__);

// int ret;
  
//   syslog(0, "%s() - userData = %d\n", __func__, userData);
//   int argc = 1;
//   char *argv[1];
//   strcpy(argv[0], "TestStdoutAfter");

// // This may never return
//   int ret = (*USERSPACE->us_entrypoint)((int)argc, argv);
//   syslog(0, "%s() - TestStdoutAfter exited ret = %d\n", __func__, ret);

}
