/****************************************************************************
 * long_period_timer.c
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

#include <nuttx/config.h>

#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <errno.h>
#include <debug.h>
#include <sys/types.h>

#include <ctype.h>
#include <nuttx/semaphore.h>
#include <nuttx/kthread.h>

#include "long_period_scheduler.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/**
 *  @brief Work out the LPS_PERIOD
 */
#if LPS_DEFAULT_PERIOD < 60
#define LPS_PERIOD 60
#else
#define LPS_PERIOD LPS_DEFAULT_PERIOD
#endif

/****************************************************************************
 * Local type defintions.
 ****************************************************************************/

struct lps_configuration_s
{
    /**
     *  @brief ID of the task that will process the timer.
     */
    pid_t pid;

    /**
     *  @brief How many seconds between checks.
     */
    uint32_t period;
};
typedef struct lps_configuration_s lps_configuration_t;

/*
 *  Forward declarations for the linked list to stop the compiler complaining.
 */
struct lps_registered_handlers_s;
typedef struct lps_registered_handlers_s lps_registered_handlers_t;

/**
 *  @brief Linked list of methods that should be run by the scheduler.
 *  
 *  The linked ist contains:
 *      - A pointer to the hander to be executed
 *      - How long before the next invocation of the handler
 *      - Period between invocations (used to reset the timer)
 */
struct lps_registered_handlers_s
{
    /**
     *  @brief Method to be executed.
     */
    lps_handler_t handler;

    /**
     *  @brief How long (in seconds) between invocations of the handler.
     */
    uint32_t period;

    /**
     *  @brief How much time is left before the handler should be run.
     */
    uint32_t ttl;

    /**
     *  @brief Next item in the lined list.
     */
    lps_registered_handlers_t *next;
};
// typedef struct lps_registered_handlers_s lps_registered_handlers_t;

/****************************************************************************
 * Private Data
 ****************************************************************************/

/**
 * @brief Configuration for the long period timer.
 */
static lps_configuration_t lps_configuration = { };

/**
 *  Mutex to be used by any code that wants access to the long period timer.
 */
static sem_t lps_mutex = { };

/**
 *  @brief Linked list of registered handlers.
 */
static lps_registered_handlers_t *lps_registered_handlers = NULL;

/****************************************************************************
 * private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lps_lock
 *
 * Description:
 *  Lock the long period scheduler configuration.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static void lps_lock(void)
{
    sem_wait(&lps_mutex);
}

/****************************************************************************
 * Name: lps_unlock
 *
 * Description:
 *  Unlock the long period scheduler configuration.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static void lps_unlock(void)
{
    sem_post(&lps_mutex);
}

/****************************************************************************
 * Name: lps_daemon
 *
 * Description:
 *  Scheduler task.  This implements the scheduler.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  This method will never return, the NuttX threading system determines the
 *  function prototype.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int lps_daemon(int argc, char **argv)
{
    lps_lock();
    uint32_t period = lps_configuration.period;
    lps_unlock();
    while (1)
    {
        sleep(period);
        lps_lock();
        lps_registered_handlers_t *rh = lps_registered_handlers;
        while (rh != NULL)
        {
            if ((rh->ttl == 0) || (rh->ttl < period))
            {
                (rh->handler)();
                rh->ttl = rh->period;
            }
            else
            {
                rh->ttl -= period;
            }
            rh = rh->next;
        }
        lps_unlock();
    }
    return(OK);
}

/****************************************************************************
 * Name: lps_start
 *
 * Description:
 *  Start the long term scheduler daemon.
 *
 * Input Parameters:
 *  None.
 *
 * Returned Value:
 *  OK if successful, ENOMEM if memory allocation fails, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
static int lps_start(void)
{
    int result = OK;
    //
    //  Initialise the mutex so that it is already locked.
    //
    sem_init(&lps_mutex, 0, 0);
    lps_configuration.period = LPS_PERIOD;
    lps_configuration.pid = kthread_create("LPS Daemon", CONFIG_LPSDAEMON_SERVERPRIO,
                                           CONFIG_LPSDAEMON_STACKSIZE, (main_t) lps_daemon, (char *const *) NULL);
    if (lps_configuration.pid < 0)
    {
        result = errno;
        syslog(LOG_ERR, "ERROR: Failed to start the Long Period Timer daemon\n", result);
    }
    lps_unlock();
    return(result);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lps_add_handler
 *
 * Description:
 *  Add a handler to be executed at the specified period.
 * 
 *  Note that the exact period between executions cannot be guaranteed as the
 *  list of registered handlers will only be checked based upon the timer
 *  period.
 * 
 *  For instance, requesting an execution period of 90 seconds when the timer
 *  is running with a period of 60 seconds means that the handler will get
 *  called every 120 seconds.
 *
 * Input Parameters:
 *  handler - Method to be executed when the timer expires.
 *  period - number of seconds between execution of the handler.
 *
 * Returned Value:
 *  OK if successful, ERROR otherwise.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int lps_add_handler(lps_handler_t handler, uint32_t period)
{
    if ((handler == NULL) || (period == 0))
    {
        return(ERROR);
    }

    int result = OK;
    if (lps_configuration.period == 0)
    {
        result = lps_start();
    }
    if (result == OK)
    {
        lps_registered_handlers_t *rh = (lps_registered_handlers_t *) malloc(sizeof(lps_registered_handlers_t));
        if (rh == NULL)
        {
            result = ERROR;
        }
        else
        {
            rh->handler = handler;
            rh->period = period;
            rh->ttl = period;
            lps_lock();
            rh->next = lps_registered_handlers;
            lps_registered_handlers = rh;
            lps_unlock();
        }
    }
    return(result);
}

/****************************************************************************
 * Name: lps_remove_handler
 *
 * Description:
 *  Remove a handler from the list of known handlers.
 * 
 * Input Parameters:
 *  handler - Method to be removed from the list of registered handlers.
 *
 * Returned Value:
 *  None.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
void lps_remove_handler(lps_handler_t handler)
{
    lps_lock();
    lps_registered_handlers_t *current = lps_registered_handlers;
    lps_registered_handlers_t *previous = NULL;
    while (current != NULL)
    {
        if (current->handler == handler)
        {
            if (previous == NULL)
            {
                lps_registered_handlers = current->next;
            }
            else
            {
                previous->next = current->next;
            }
            free(current);
            current = NULL;     // Force the loop to terminate.
        }
        else
        {
            current = current->next;
        }
    }
    lps_unlock();
}