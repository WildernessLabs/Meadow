/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <string.h>

#include <nuttx/config.h>

#include <nuttx/fs/fs.h>
#include <nuttx/kmalloc.h>
#include <arch/board/board.h>
#include <nuttx/mqueue.h>
#include <nuttx/signal.h>

#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include "chip.h"
#include "fcntl.h"
#include "stm32f777zit6-meadow.h"

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct upd_register_value
{
  uint32_t address;
  uint32_t value;
};

struct upd_register_update
{
  uint32_t address;
  uint32_t clearBits;
  uint32_t setBits;
};

struct upd_gpio_int_config
{
  uint32_t irq;
  uint32_t port;
  uint32_t pin;
  bool enable;
  bool risingEdge;
  bool fallingEdge;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int upd_ioctl(FAR struct file *filep, int cmd, unsigned long arg);
static int upd_open(struct file *filep);
static int upd_close(struct file *filep);

static int upd_gpio_interrupt(int irq, void *context, void *arg);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct file_operations g_driver_operations =
{
  .open  = upd_open,
  .close = upd_close,
  .ioctl = upd_ioctl
};

static pid_t s_meadow_pid;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int upd_gpio_interrupt(int irq, void *context, void *arg)
{
  // create a signal value struct
  union sigval value;

  // we'll pass the IRQ to the app
  value.sival_int = irq;

  // dispatch the data
  int result = nxsig_queue(s_meadow_pid, 1, value);

  return result;
}

static int upd_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  struct upd_register_value *register_val;
  struct upd_register_update *register_update;
  struct upd_gpio_int_config *interruptRequest;

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
    case MUPD_UPDATE_REGISTER:
        // this does an atomic read/set/write of a register
        register_update = (struct upd_register_update *)arg;
        modifyreg32(register_update->address, register_update->clearBits, register_update->setBits);
        return OK;
    case MUPD_REGISTER_GPIO_IRQ:
      interruptRequest = (FAR struct upd_gpio_int_config*)arg;
      // we need to PID for signalling.  Nicely Meadow only has one app process, so we just store it
      s_meadow_pid = getpid();

      // determine a pin designator
      uint32_t designator = interruptRequest->port << 4 | interruptRequest->pin;

      // the app will give us the signal number.  
      // This is expected to remain constant for the entire app, so we store the first one we get
      if(interruptRequest->enable)
      {
        return stm32_gpiosetevent(
          designator,
          interruptRequest->risingEdge,
          interruptRequest->fallingEdge,
          0,
          upd_gpio_interrupt,
          NULL); // probably need to pass a pointer to a number to tell the ISR what the source was        
      }
      // disable the interrupt
      return stm32_gpiosetevent(
          designator,
          false, false, 0, NULL, NULL);
      break;
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

