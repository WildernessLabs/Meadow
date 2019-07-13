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
  int enable;
  int risingEdge;
  int fallingEdge;
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

#define QUEUE_NAME          "/mdw_int"
#define QUEUE_MSG_SIZE      16
static pid_t s_meadow_pid;
static mqd_t s_int_queue = 0;
static char queue_buffer[QUEUE_MSG_SIZE];

// the interrupt designator needs to be stored since we pass an address to the interrupt handler
// this array is our "map"
static int s_interruptPinMap[26];

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static int upd_gpio_interrupt(int irq, void *context, void *arg)
{
  // arg here will be the port/pin designator passed in during the register ioctl
  memset(queue_buffer, 0, QUEUE_MSG_SIZE);
  memcpy(queue_buffer, arg, 4);

  int result = mq_send(s_int_queue, queue_buffer, QUEUE_MSG_SIZE, 0);

  return result;
}

static int upd_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  struct upd_register_value *register_val;
  struct upd_register_update *register_update;
  //struct upd_gpio_int_config *interruptRequest;

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
      // we need to PID for signalling.  Nicely Meadow only has one app process, so we just store it
      s_meadow_pid = getpid();

      struct upd_gpio_int_config cfg;
      memset(&cfg, 0, sizeof(cfg));
      memcpy(&cfg, (void*)arg, sizeof(cfg));

      // determine a pin designator
      uint32_t designator = cfg.port << 4 | cfg.pin;

      // the app will give us the signal number.  
      // This is expected to remain constant for the entire app, so we store the first one we get
      if(cfg.enable)
      {
        int index = 0;
        // find the first empty (== 0) map index
        for(int i = 0 ; i < 26 ; i++)
        {
          if(s_interruptPinMap[i] == 0)
          {
            s_interruptPinMap[i] = cfg.irq;
            index = i;
            break;
          }
        }

        return stm32_gpiosetevent(
          designator,
          cfg.risingEdge,
          cfg.fallingEdge,
          0,
          upd_gpio_interrupt,
          &s_interruptPinMap[index]);        
      }

      // remove designator from interrupt map
      for(int i = 0 ; i < 26 ; i++)
      {
        if(s_interruptPinMap[i] == cfg.irq)
        {
          s_interruptPinMap[i] = 0;
          break;
        }
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
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = 10;
  attr.mq_msgsize = QUEUE_MSG_SIZE;
  attr.mq_curmsgs = 0;

  if(s_int_queue == 0)
  {
    s_int_queue = mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, 0660, &attr);
  }
  return OK;
}

static int upd_close(struct file *filep)
{
  mq_close(s_int_queue);

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
  syslog(0, "+meadow_upd_initialize\n");
  
  // register the driver, passing in our entry points
  int ret = register_driver("/dev/upd", &g_driver_operations, 0666, NULL);
  if (ret)
  {
    return ERROR;
  }

  return OK;
}

