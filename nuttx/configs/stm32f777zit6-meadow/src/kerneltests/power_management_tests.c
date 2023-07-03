/****************************************************************************
 * configs\stm32f777zit6-meadow\src\kerneltests\power_management_tests.c
 * 
 *   Copyright (C) 2019 - 2021 Wilderness Labs. All rights reserved.
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

#include "../hcom_nx/hcom_nx_common.h"

#if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)

#include <meadow/hcom_shared_common.h>

#include <nuttx/arch.h>
#include <nuttx/mtd/mtd.h>
#include <sys/mount.h>
#include <dirent.h>
#include <sys/stat.h>
#include "stm32_rtc.h"
#include "stm32_exti.h"
#include "stm32_gpio.h"                 // Needed for testing input gpio->event
#include <arch/board/board.h>           // Needed for testing getreg16
#include "chip/stm32f76xx77xx_pwr.h"    // Needed for testing
#include <nuttx/kthread.h>
#include <meadow/meadow_kernel_tests.h>

#include "../pwrmgmt/pwrmgmt_local.h"

#define USE_MEADOW_DEBUG_HELPERS
#include <meadow/meadow_debug_helpers.h>

#warning "(--) Peter here"
/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/************************************************************************************
 * Private Data
 ************************************************************************************/
// static char *thisFile = __FILE__;

/************************************************************************************
 * Public Data
 ************************************************************************************/

/************************************************************************************
 * Private Function Prototypes
 ************************************************************************************/
  
#if MEADOW_POWER_MANAGEMENT_SHOW_TIME_CALC > 0
// Used to verify the alarm is being properly configured
static int pwrmgmt_enter_test_alarm_timer_parsing(void);
#endif

static int pwrmgmt_enter_test_sleep_x_times_for_y_seconds(void);

// Needed for testing rtc alarm wakeup
static int pwmmgmt_test_timer_and_alarm_wakeup(time_t wakeupPeriod);
// static void *sleep_test_pthread_func(void *arg);
static void *sleep_test_kthread_func(int argc, char *argv[]);

/************************************************************************************
 * Private Functions
 ************************************************************************************/
#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
// ISR called when the RTC generates an alarm. Will indicate time to exit
// low-power mode.
static int pwmmgmt_test_rtc_alarm_isr_handler(int irq, FAR void *context, FAR void *arg)
{
  // Clear the EXTI Pending Register for the RTC alarm event
  putreg32(EXTI_RTC_ALARM, STM32_EXTI_PR);

  syslog(2, "RTC Alarm A - Interrupt Service Routine called\n");

  // Only called once so no more interrupts expected, needed or wanted
  // Note: in the non-test code this isn't done in the ISR
  up_disable_irq(STM32_IRQ_RTCALRM);
  irq_detach(STM32_IRQ_RTCALRM);

  return OK;
}
#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
// ISR called when wakeup timer reaches 0 indicating time to exit-power mode.
static int pwmmgmt_test_wakeup_timer_isr_handler(int irq, FAR void *context, FAR void *arg)
{
  // Clear the EXTI Pending Register for the wakeup event
  putreg32(EXTI_RTC_WAKEUP, STM32_EXTI_PR);
  
  syslog(2, "RTC Wakeup Timer ISR called\n");

  up_disable_irq(STM32_IRQ_RTC_WKUP);
  irq_detach(STM32_IRQ_RTC_WKUP);

  return OK;
}
#else
#error "Select Low-Power timing scheme"
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/
// Called from nuttx/configs/stm32f777zit6-meadow/src/hcom_nx/tests/hcom_nx_developer_3_tests.c
// These tests are for testing the power management implementation
void meadow_kt_power_management_tests(uint32_t userData)
{
  int ret = OK;
  struct tm tmNowRtc;

  switch(userData)
  {
#if MEADOW_POWER_MANAGEMENT_SHOW_TIME_CALC > 0
    case 1:
      syslog(2, "==>>power mgmt tests received %u - Verify Alarm timer parses correctly\n", userData);

      // Used to verify the alarm is being properly configured
      pwrmgmt_enter_test_alarm_timer_parsing();
      break;
#endif

    case 2:
      syslog(2, "==>>power mgmt tests received %u - Power sleep x times for y seconds\n", userData);
      usleep(20 * 1000);
      
      // Used to verify that multiple sleep events can succeed
      pwrmgmt_enter_test_sleep_x_times_for_y_seconds();
      break;

    case 52:
      // Enter Stop mode with max power savings & slowest restart
      syslog(2, "==>>power mgmt tests received %u - Stop mode MAX savings\n", userData);
      sleep(1);
      // Directly execute stop mode with no timer setup
      ret = pwrmgmt_enter_stop_mode();
      break;

    case 53:
      // Enter Stop mode with minimum power savings & fastest restart
      syslog(2, "==>>power mgmt tests received %u - Stop mode Min savings\n", userData);
      sleep(1);
      // Directly execute stop mode with no timer setup
      ret = pwrmgmt_enter_stop_mode();
      break;

    // case 54:
    //   // Enter Standby mode. This is the lowest possible power mode
    //   syslog(2, "==>>power mgmt tests received %u - Standby mode\n", userData);
    //   usleep(100 * 1000);
    //   ret = meadow_pwr_mgmt_enter_standby();
    //   break;

    case 55:
      // Set clock to HSE
      syslog(2, "==>>power mgmt tests received %u - HSE for clock\n", userData);
      usleep(20 * 1000);
      ret = meadow_pwr_mgmt_use_hse_for_rtc();
      break;

    case 56:
      // Set clock to LSI
      syslog(2, "==>>power mgmt tests received %u - LSI for clock\n", userData);
      usleep(20 * 1000);
      // The following function calls will result in the the F7 being put into sleep mode for 45 seconds.
      ret = meadow_pwr_mgmt_use_lsi_for_rtc();
      break;

    case 57:
      // Set for wakeup after X seconds. After this period a call to an ISR and
      // is used that the wakeup timer is working  as expected. No switching
      // clocks or going to sleep.
      syslog(2, "==>>power mgmt tests received %u - Testing wakeup timer, not sleep\n", userData);
      usleep(20 * 1000);
      // Should call alarm ISR in x seconds
      ret = pwmmgmt_test_timer_and_alarm_wakeup(15);
      break;

    case 58:
      // Set alarm for X sec, switch to LSI, enter Stop-mode, after alarm wake up switch to HSE.
      syslog(2, "==>>power mgmt tests received %u - Sleeping for 10 seconds\n", userData);
      up_rtc_getdatetime(&tmNowRtc);            // RTC Hardware time
      syslog(2, "Before Sleep:RTC-%4d-%02d-%02dT%02d:%02d:%02d\n",
                tmNowRtc.tm_year + 1900, tmNowRtc.tm_mon + 1, tmNowRtc.tm_mday,
                tmNowRtc.tm_hour, tmNowRtc.tm_min, tmNowRtc.tm_sec);
      usleep(20 * 1000);

      // Wakeup after x seconds
      ret = pwrmgmt_enter_stm32f7_stop_mode(10);

      up_rtc_getdatetime(&tmNowRtc);            // RTC Hardware time
      syslog(2, "After Sleep:RTC-%4d-%02d-%02dT%02d:%02d:%02d\n",
                tmNowRtc.tm_year + 1900, tmNowRtc.tm_mon + 1, tmNowRtc.tm_mday,
                tmNowRtc.tm_hour, tmNowRtc.tm_min, tmNowRtc.tm_sec);
      usleep(20 * 1000);
      break;

    default:
    syslog(2, "Unknown value %u passed to hcom_nx_exec_power_mgmt_tests()\n", userData);
    break;
  }

  if(ret < 0)
  {
    syslog(2, "==>>power mgmt tests received %u - Error ret:%d errno:%d\n", userData, ret, errno);
  }
}

//=========================================================
// This test will exercise the part of the alarm timer's configuration code to
// test if it is parsing the time in seconds correctly
#if MEADOW_POWER_MANAGEMENT_SHOW_TIME_CALC > 0

static time_t testTimeValArray[] = 
{
  17,                 // 17 seconds
  60,                 // 1 minute
  60 * 60,            // 1 hour
  24 * 60 * 60,       // 1 day
  5 * 24 * 60 * 60,   // 5 days
};

static char *testTimeStrArray[] = 
{
  "17 seconds",
  "1 minute",
  "1 hour",
  "1 day",
  "5 days"
};

int pwrmgmt_enter_test_alarm_timer_parsing()
{
  int ret = OK;

  for(int i = 0; i < 5; i++)
  {
    syslog(2, "Alarm Test Parsing '%s'\n", testTimeStrArray[i]);
    ret = pwrmgmt_config_rtc_alarm_wakeup_seconds(testTimeValArray[i]);
    if(ret < 0)
    {
      syslog(2, "Alarm Test Parsing ret:%d, errno:%d\n", ret, errno);
    }
  }
  return ret;
}
#endif

//=========================================================
// Verify that repeated sleeps sessions are possible.
// This thread allows the HCOM processor thread to return.
int pwrmgmt_enter_test_sleep_x_times_for_y_seconds()
{
  // DEBUG_CONFIGURE_PIN(DEBUG_PIN_V2_D04);
  // DEBUG_SET_HIGH(DEBUG_PIN_V2_D04);
  // task_create
  // pthread_create
  // kthread_create
 
  // pthread_t thread;
  // pthread_attr_t attr;
  // struct sched_param param;

  // param.sched_priority = 100;
  // (void)pthread_attr_init(&attr);
  // (void)pthread_attr_setschedparam(&attr, &param);
  // (void)pthread_attr_setstacksize(&attr, 4096);

  // int ret = pthread_create(&thread, &attr, sleep_test_pthread_func, NULL);
  // if (ret < 0)
  // {
  //   syslog(LOG_ERR, "%s@%d-create thread %s, ret:%d, errno:%d\n",
  //             __FILE__, __LINE__, "SleepTest", ret, errno);
  //   return ret;
  // }

  syslog(1, "%s:%s@%d-Creating kthread\n", __FILE__, __func__, __LINE__); usleep(20 * 1000);

  int thread_id = kthread_create("SleepTest",
                                100,
                                4096,
                                (main_t) sleep_test_kthread_func,
                                (char *const *) NULL);
  if (thread_id <= 0)
  {
    syslog(LOG_ERR, "%s@%d-Creation of %s kthread FAILED\n",
              __FILE__, __LINE__, PWRMGMT_CAL_LSI_THREAD_NAME);
    return -ENOEXEC;
  }
  return OK;
}

//---------------------------------------------------------------
// void *sleep_test_pthread_func(void *arg)
// Thread to run sleep test
void *sleep_test_kthread_func(int argc, char *argv[])
{
  int ret;
  int i;
  int seconds = 10;
  int count = 5;

  syslog(2, "%s:%s@%d-New thread [PID:%d],'%s'\n", __FILE__, __func__, __LINE__, getpid(), "SleepTest");

  for(i = 0; i < count; i++)
  {
    syslog(2, "%03d-Sleeping for %d seconds\n", i + 1, seconds);
    usleep(20 * 1000);

    // DEBUG_SET_LOW(DEBUG_PIN_V2_D04);
    ret = pwrmgmt_enter_stm32f7_stop_mode(seconds);
    if(ret < 0)
    {
      syslog(2, "%03d-Error:Alarm Test multi-sleep ret:%d, errno:%d, will continue\n", i, ret, errno);
    }

    // Now wait before sleeping again
    // DEBUG_SET_HIGH(DEBUG_PIN_V2_D04);
    syslog(2, "%03d-Awake for %d seconds\n", i + 1, seconds);
    sleep(seconds);
  }

  syslog(2, "%03d-Cycles were executed successfully\n", i);
  return NULL;
}

//=========================================================
// Set alarm for X sec, switch to LSI, enter Stop-mode, after alarm
// wake up switch to HSE.
int pwmmgmt_test_timer_and_alarm_wakeup(time_t wakeupPeriod)
{
  int ret;

#if 0   // FOR TESTING THE TESTING CODE
  struct timespec abstime;
  struct tm tmNowNx;
  struct tm tmNowRtc;

  up_rtc_getdatetime(&tmNowRtc);            // RTC Hardware time
  clock_gettime(CLOCK_REALTIME, &abstime);  // Nuttx internal time
  gmtime_r(&abstime.tv_sec, &tmNowNx);

  syslog(2, "Before Stop - RTC-%4d-%02d-%02dT%02d:%02d:%02d, Nuttx-%4d-%02d-%02dT%02d:%02d:%02d\n",
            tmNowRtc.tm_year + 1900, tmNowRtc.tm_mon + 1, tmNowRtc.tm_mday,
            tmNowRtc.tm_hour, tmNowRtc.tm_min, tmNowRtc.tm_sec,
            tmNowNx.tm_year + 1900, tmNowNx.tm_mon + 1, tmNowNx.tm_mday,
            tmNowNx.tm_hour, tmNowNx.tm_min, tmNowNx.tm_sec);
#endif   // FOR TESTING ONLY

#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // Configure the hardware
  // Set alarm wakeup period and wait for ISR to notify that time has elasped
  syslog(2, "==> Setting RTC alarm for %d seconds\n", wakeupPeriod);
  ret = pwrmgmt_config_rtc_alarm_wakeup_seconds(wakeupPeriod);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", __FILE__, __LINE__);
    return ret;
  }
#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
  // Set wakeup timer period and wait for ISR to notify time has elasped
  syslog(2, "==> Setting RTC wakeup timer for %d seconds\n", wakeupPeriod);
  ret = pwrmgmt_config_rtc_timer_wakeup_seconds(wakeupPeriod);
  if(ret < 0)
  {
    syslog(LOG_ERR, "%s@%d-Error:\n", __FILE__, __LINE__);
    return ret;
  }
#else
#error "Select Low-Power timing scheme"
#endif
  // Prepare for wakeup by configuring an ISR to be called when the time 
  // period has been reached.
#if defined (PWRMGMT_LOW_PWR_EXIT_USE_RTC_ALARM)
  // For using RTC Alarm timing
  irq_attach(STM32_IRQ_RTCALRM, pwmmgmt_test_rtc_alarm_isr_handler, NULL);
  up_enable_irq(STM32_IRQ_RTCALRM);
#elif defined (PWRMGMT_LOW_PWR_MODE_USE_WAKEUP_TIMER)
  // Setup the testing ISR for the RTC wakeup timer counting down to 0.
  irq_attach(STM32_IRQ_RTC_WKUP, pwmmgmt_test_wakeup_timer_isr_handler, NULL);
  up_enable_irq(STM32_IRQ_RTC_WKUP);
#else
#error "Select Low-Power timing scheme"
#endif

#if 0  // FOR TESTING. Use the HCOM thread to loop until it's time wakeup
  int countDown = wakeupPeriod;
  
  while(countDown > -2)
  {
    // Check ALRAF and EXTI_PR's EXTI_RTC_ALARM bit
    syslog(2, "==>%02d RTC Time:%08x, ALRAF:%d, EXTI PR:%d\n", countDown,
              getreg32(STM32_RTC_TR),
              getreg32(STM32_RTC_ISR) & RTC_ISR_ALRAF ? 1 : 0,
              getreg32(STM32_EXTI_PR) & EXTI_RTC_ALARM ? 1 : 0);
    sleep(1);
    countDown--;
  }

  syslog(2, "==>%d Final RTC Time:%08x\n", countDown, getreg32(STM32_RTC_TR));

#endif

  return OK;
}

#endif    // #if defined (CONFIG_MEADOW_PWR_MGMT_SUPPORT)
