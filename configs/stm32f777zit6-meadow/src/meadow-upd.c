/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>

#include <nuttx/config.h>

#include <nuttx/fs/fs.h>
#include <nuttx/kmalloc.h>
#include <arch/board/board.h>
#include <nuttx/mqueue.h>

#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include <nuttx/clock.h>
#include <nuttx/wdog.h>

#include <arch/board/board.h>

#include "chip.h"
#include "fcntl.h"
#include "stm32f777zit6-meadow.h"

#define getreg32(address)          (*(volatile uint32_t *)(address))
#define putreg32(value,address)     (*(volatile uint32_t *)(address) = (value))

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct upd_register_value
{
  uint32_t address;
  uint32_t value;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int upd_ioctl(FAR struct file *filep, int cmd, unsigned long arg);
static int upd_open(struct file *filep);
static int upd_close(struct file *filep);


/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_driver_operations =
{
  .open  = upd_open,
  .close = upd_close,
  .ioctl = upd_ioctl
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int upd_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  struct upd_register_value *register_val;

  switch(cmd)
  {
    case MUPD_SET_REGISTER:
        register_val = (struct upd_register_value *)arg;
        putreg32(register_val->value, register_val->address);
        return OK;
    case MUPD_GET_REGISTER:
        register_val = (struct upd_register_value *)arg;
        register_val->value = getreg32(register_val->address);
        return OK;
    case MUPD_REGISTER_IRQ:
        return OK;
  }
  return ERROR;
}

static int upd_open(struct file *filep)
{
  return OK;
}

static int upd_close(struct file *filep)
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

int meadow_upd_initialize(void)
{
  syslog(0, "+meadow_upd_initialize");
  
  // register the driver, passing in our entry points
  int ret = register_driver("/dev/upd", &g_driver_operations, 0666, NULL);
  if (ret)
  {
    return ERROR;
  }

  return OK;
}

