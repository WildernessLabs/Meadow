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
#include <nuttx/drivers/pwm.h>

#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include "chip.h"
#include "fcntl.h"
#include "stm32_pwm.h"
#include "stm32_i2c.h"
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

struct upd_pwm_cmd
{
  uint32_t timer_id;
  uint32_t frequency;
  uint32_t duty;
};

struct upd_i2c_cmd
{
  uint32_t address;
  uint32_t frequency;
  uint8_t* txBuffer; // in to driver (so tx)
  uint32_t txLength;
  uint8_t* rxBuffer; // back out to app, so rx
  uint32_t rxLength;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int upd_ioctl(FAR struct file *filep, int cmd, unsigned long arg);
static int upd_open(struct file *filep);
static int upd_close(struct file *filep);

static int upd_gpio_interrupt(int irq, void *context, void *arg);

static int upd_handle_pwm(int cmd, unsigned long arg);
static int upd_handle_i2c(int cmd, struct upd_i2c_cmd*);

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

    case MUPD_PWM_SETUP:
    case MUPD_PWM_SHUTDOWN:
    case MUPD_PWM_START:
    case MUPD_PWM_STOP:
      return upd_handle_pwm(cmd, arg);

    case MUPD_I2C_SHUTDOWN:
    case MUPD_I2C_DATA:
      return upd_handle_i2c(cmd, (struct upd_i2c_cmd*)arg);     
  }
  return ERROR;
}

static struct i2c_master_s *g_i2c1 = NULL;
static struct i2c_config_s g_i2c_cfg;

static int upd_handle_i2c(int cmd, struct upd_i2c_cmd* data)
{
  if(cmd == MUPD_I2C_SHUTDOWN)
  {
    if(g_i2c1 != NULL)
    {
      stm32_i2cbus_uninitialize(g_i2c1);
      g_i2c1 = NULL;
    }
    return OK;
  }

  if(g_i2c1 == NULL)
  {
    // the only I2C port Meadow supports is #1 - just initialize it
    g_i2c1 = stm32_i2cbus_initialize(1);
  }

  g_i2c_cfg.address = data->address;
  g_i2c_cfg.addrlen = 7; // we currently are supporting only 7-bit address devices
  g_i2c_cfg.frequency = data->frequency;

  int result = OK;

  // if we have only outbuffer, it's a write
  if(data->txLength > 0)
  {
    if(data->rxLength > 0)
    {
      // writeread
      result = i2c_writeread(g_i2c1, &g_i2c_cfg, data->txBuffer, data->txLength, data->rxBuffer, data->rxLength);
    }
    else
    {
      //write
      result = i2c_write(g_i2c1, &g_i2c_cfg, data->txBuffer, data->txLength);
    }
  }
  else if(data->rxLength > 0)
  {
    // read
    result = i2c_read(g_i2c1, &g_i2c_cfg, data->rxBuffer, data->rxLength);
  }
  else
  {
    // no read or write buffer
    result = EINVAL;
  }
  
  return result;
}

static int upd_handle_pwm(int cmd, unsigned long arg)
{
  struct upd_pwm_cmd *_upd_pwm_cmd = (struct upd_pwm_cmd *)arg;
  struct pwm_lowerhalf_s *pwm;

  /* Call stm32_pwminitialize() to get an instance of the PWM interface */
  pwm = stm32_pwminitialize(_upd_pwm_cmd->timer_id);
  if (!pwm)
  {
    aerr("ERROR: Failed to get the STM32 PWM lower half\n");
    return -ENODEV;
  }

  switch(cmd)
  {
  case MUPD_PWM_SETUP:
  {
      pwm->ops->setup(pwm);
      return OK;
  }
  case MUPD_PWM_SHUTDOWN:
  {
      pwm->ops->shutdown(pwm);
      return OK;
  }
  case MUPD_PWM_START:
  {
      struct pwm_info_s info;
      info.frequency = _upd_pwm_cmd->frequency;
      info.duty = _upd_pwm_cmd->duty;

      pwm->ops->start(pwm, &info);
      return OK;
  }
  case MUPD_PWM_STOP:
  {
      pwm->ops->stop(pwm);
      return OK;
  }
  }

  return OK;
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
  syslog(0, "+meadow_upd_initialize");
  
  // register the driver, passing in our entry points
  int ret = register_driver("/dev/upd", &g_driver_operations, 0666, NULL);
  if (ret)
  {
    return ERROR;
  }

  return OK;
}

