/****************************************************************************
 * configs/stm32f103-minimum/src/stm32_gpio.c
 *
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Copyright (C) 2017 Alan Carvalho de Assis. All rights reserved.
 *   Author:  Alan Carvalho de Assis <acassis@gmail.com>
 *
 *   Based on: configs/sim/src/sim_gpio.c
 *   Author:  Gregory Nutt <gnutt@nuttx.org>
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

#include <nuttx/fs/fs.h>

#include <stdbool.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/clock.h>
#include <nuttx/wdog.h>
#include <nuttx/ioexpander/gpio.h>

#include <arch/board/board.h>

#include "chip.h"
//#include <arch/arm/src/stm32/stm32.h>

#include "stm32f777zit6-meadow.h"

#if defined(CONFIG_DEV_GPIO) && !defined(CONFIG_GPIO_LOWER_HALF)

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct gpio_pin_state
{
  int pinNumber;
  bool pinState;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int gpi_ioctl(FAR struct file *filep, int cmd, unsigned long arg);
static int gpi_open(struct file *filep);
static int gpi_close(struct file *filep);


/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_gpiops =
{
  .open  = gpi_open,
  .close = gpi_close,
  .ioctl = gpi_ioctl
};


/****************************************************************************
 * Private Functions
 ****************************************************************************/
static int gpi_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  struct gpio_pin_state stateRequest;
  uint32_t int32Request;

  switch(cmd)
  {
    case MGPIO_SET_CONFIG:
      int32Request = *(uint32_t*)arg;
      return stm32_configgpio(int32Request);
    break;
    case MGPIO_WRITE:
      // get the state request
      stateRequest = *(struct gpio_pin_state*)arg;
      stm32_gpiowrite(stateRequest.pinNumber & (GPIO_PIN_MASK | GPIO_PORT_MASK), stateRequest.pinState);
      return OK;
    case MGPIO_READ:
      int32Request = *(uint32_t*)arg;
      return stm32_gpioread(int32Request);
  }
  return ERROR;
}

static int gpi_open(struct file *filep)
{
  return OK;
}

static int gpi_close(struct file *filep)
{
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_gpio_initialize
 *
 * Description:
 *   Initialize GPIO drivers for use with /apps/examples/gpio
 *
 ****************************************************************************/

int meadow_gpio_initialize(void)
{
  syslog(0, "+meadow_gpio_initialize");

  // register the driver
  int ret = register_driver("/dev/gpio", &g_gpiops, 0666, NULL);
  if (ret)
  {
    return ERROR;
  }

  return OK;
}
#endif /* CONFIG_DEV_GPIO && !CONFIG_GPIO_LOWER_HALF */
