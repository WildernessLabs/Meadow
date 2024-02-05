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
#include <nuttx/spi/spi.h>

#include <stdbool.h>
#include <assert.h>
#include <debug.h>
#include <errno.h>

#include "chip.h"
#include "fcntl.h"
#include "stm32_pwm.h"
#include "stm32_i2c.h"
#include "stm32f777zit6-meadow.h"
#include "stm32_spi.h"

#include <dirent.h>

#include <nuttx/timers/timer.h>
#include <nuttx/timers/watchdog.h>
#include <sys/ioctl.h>
#include "stm32_tim.h"
#include "meadow-upd.h"
#include <meadow/hcom_nuttx_shared.h>
#include <meadow/hcom_shared_common.h>
#include "stm32_uid.h" // stm32_get_uniqueid()
#include "hcom_nx/hcom_nx_common.h"

#include "espcp/espcp_common.h"
#include "espcp/espcp_encoders.h"
#include "hcom_nx/hcom_nx_config_manager.h"
// #include "pwrmgmt/pwrmgmt_local.h"

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

struct upd_pwm_cmd
{
  uint32_t timer;
  uint32_t channel;
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
  uint32_t busNumber; // bus number is at the end to enable backward-compat
};

struct upd_spi_data_cmd
{
  uint8_t* txBuffer; // in to driver (so tx)
  uint8_t* rxBuffer; // back out to app, so rx
  uint32_t length;
  uint32_t busNumber;
};

struct upd_spi_speed_cmd
{
  uint32_t busNumber;
  uint64_t frequency;
};

struct upd_spi_mode_cmd
{
  uint32_t busNumber;
  uint32_t mode;
};

struct upd_spi_bits_cmd
{
  uint32_t busNumber;
  uint32_t bits;
};

struct upd_sleep_cmd
{
  uint32_t secondsToSleep;
};

struct upd_dir_enum_cmd
{
  char* root; // folder to enumerate
  char* result; // data back to app
  uint32_t resultLength; // length of data buffer
};


struct upd_device_info
{
  char *infoBuf;
  int infoBufLen;
  int infoRetLen;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
static int upd_ioctl(FAR struct file *filep, int cmd, unsigned long arg);
static int upd_open(struct file *filep);
static int upd_close(struct file *filep);

// static int upd_gpio_interrupt(int irq, void *context, void *arg);

static int upd_handle_pwm(int cmd, unsigned long arg);
static int upd_handle_i2c(int cmd, struct upd_i2c_cmd*);
static struct spi_dev_s * get_spi_bus(int busNumber);
static int upd_handle_spi_data(int cmd, struct upd_spi_data_cmd*);
static int upd_handle_spi_speed(int cmd, struct upd_spi_speed_cmd*);
static int upd_handle_spi_mode(int cmd, struct upd_spi_mode_cmd*);
static int upd_handle_spi_bits(int cmd, struct upd_spi_bits_cmd* data);
static int upd_handle_dir_enum(struct upd_dir_enum_cmd*);
static int upd_handle_sleep_command(struct upd_sleep_cmd* cmd);

// static int upd_handle_watchdog_set(unsigned long cmd);
// static int upd_handle_watchdog_pet(void);

static int upd_get_set_configuration_value(upd_get_set_configuration_value_t *);

/****************************************************************************
 * Private Data
 ****************************************************************************/

mqd_t s_int_queue;

static const struct file_operations g_driver_operations =
{
  .open  = upd_open,
  .close = upd_close,
  .ioctl = upd_ioctl
};

#define MEADOW_I2C_PORT1    1
#define MEADOW_I2C_PORT3    3
#define MEADOW_SPI_PORT5    5  // external on CCM
#define MEADOW_SPI_PORT3    3  // external
#define MEADOW_SPI_PORT2    2  // EXP32

static struct i2c_master_s *g_i2c1 = NULL;
static struct i2c_master_s *g_i2c3 = NULL;

static struct i2c_config_s g_i2c1_cfg;
static struct i2c_config_s g_i2c3_cfg;

static struct spi_dev_s *g_spi5 = NULL; // external
static struct spi_dev_s *g_spi3 = NULL; // external
static struct spi_dev_s *g_spi2 = NULL; // to ESP32

// static int s_wd_fd = -1;

static int upd_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  struct upd_register_value *register_val;
  struct upd_register_update *register_update;
  struct mint_gpio_int_config *interrupt_cfg;

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
        interrupt_cfg = (struct mint_gpio_int_config *)arg;
        return mint_config_interrupt(interrupt_cfg);

    case MUPD_PWM_SETUP:
    case MUPD_PWM_SHUTDOWN:
    case MUPD_PWM_START:
    case MUPD_PWM_STOP:
        return upd_handle_pwm(cmd, arg);

    case MUPD_I2C_SHUTDOWN:
    case MUPD_I2C_DATA:
        return upd_handle_i2c(cmd, (struct upd_i2c_cmd*)arg);
      
    case MUPD_SPI_DATA:
        return upd_handle_spi_data(cmd, (struct upd_spi_data_cmd*)arg);
    case MUPD_SPI_SPEED:
        return upd_handle_spi_speed(cmd, (struct upd_spi_speed_cmd*)arg);
    case MUPD_SPI_MODE:
        return upd_handle_spi_mode(cmd, (struct upd_spi_mode_cmd*)arg);
    case MUPD_SPI_BITS:
        return upd_handle_spi_bits(cmd, (struct upd_spi_bits_cmd*)arg);

    case MUPD_DIR_ENUM:
      return upd_handle_dir_enum((struct upd_dir_enum_cmd*)arg);
      break;
    case MUPD_GET_LAST_ERROR:
      *((int*)arg) = errno;
      return OK;

    case MUPD_ESP32_COMMAND:
      return upd_handle_esp32_command((upd_esp32_command_t *) arg);
    case MUPD_ESP32_GET_EVENT_RESULT:
      return upd_handle_esp32_get_event_result((espcp_event_data_payload_t *) arg);

    case MUPD_PWR_RESET:
      up_systemreset();
      break;

    case MUPD_GET_SET_CONFIGURATION_VALUE:
      return(upd_get_set_configuration_value((upd_get_set_configuration_value_t *) arg));
      break;

    case MUPD_PWR_SLEEP1:
    case MUPD_PWR_SLEEP2:
      return upd_handle_sleep_command((struct upd_sleep_cmd *)arg);
      return EINVAL;
  }
  return ERROR;
}

// Allow the CLI to initiate Meadow entering the stop mode for a time period.
static int upd_handle_sleep_command(struct upd_sleep_cmd* cmd)
{
  return pwrmgmt_enter_stm32f7_stop_mode(cmd->secondsToSleep);
}

static int upd_handle_dir_enum(struct upd_dir_enum_cmd* cmd)
{
  DIR *d;
  struct dirent *dir;
  int len = 0;

  d = opendir(cmd->root);
  if(!d) return ENOTDIR;
  while((dir = readdir(d)) != NULL)
  {
    if(len + strlen(dir->d_name) + 1 > cmd->resultLength)
    {
      // this isn't really safe, as the user could always send in fake length data, but for now we assume they are nice users
      break;
    }
    strcat(cmd->result, dir->d_name);
    strcat(cmd->result, "\n");
  }
  closedir(d);
  return OK;
}

static struct spi_dev_s * get_spi_bus(int busNumber)
{
  switch (busNumber)
  {
    case 2:
      if(g_spi2 == NULL)
      {
        g_spi2 = stm32_spibus_initialize(MEADOW_SPI_PORT2);
      }
      return g_spi2;
    case 3:
      if(g_spi3 == NULL)
      {
        g_spi3 = stm32_spibus_initialize(MEADOW_SPI_PORT3);
      }
      return g_spi3;
    case 5:
      if(g_spi5 == NULL)
      {
        g_spi5 = stm32_spibus_initialize(MEADOW_SPI_PORT5);
      }
      return g_spi5;
  }

  return NULL;
}

static int upd_handle_spi_bits(int cmd, struct upd_spi_bits_cmd* data)
{
  struct spi_dev_s *target = get_spi_bus(data->busNumber);

  if(target == NULL)
  {
    return ENODEV;
  }

  SPI_SETBITS(target, data->bits);

  return OK;
}

static int upd_handle_spi_mode(int cmd, struct upd_spi_mode_cmd* data)
{
  struct spi_dev_s *target = get_spi_bus(data->busNumber);

  if(target == NULL)
  {
    return ENODEV;
  }

  SPI_SETMODE(target, data->mode);

  return OK;
}

static int upd_handle_spi_speed(int cmd, struct upd_spi_speed_cmd* data)
{
  struct spi_dev_s *target = get_spi_bus(data->busNumber);

  if(target == NULL)
  {
    return ENODEV;
  }

  SPI_SETFREQUENCY(target, data->frequency);

  return OK;
}

static int upd_handle_spi_data(int cmd, struct upd_spi_data_cmd* data)
{
  struct spi_dev_s *target = get_spi_bus(data->busNumber);

  if(target == NULL)
  {
    return ENODEV;
  }

#if defined (CONFIG_STM32F7_SPI_DMA)
  // The STM32F777 used in Meadow has a DMA transfer size limit of 65535
  // bytes. To workaround this limitation we'll do multiple 
  if(data->txBuffer || data->rxBuffer)
  {
    if(data->length <= 0xffff)
    {
      SPI_EXCHANGE(target, data->txBuffer, data->rxBuffer, data->length);
    }
    else
    {
      uint32_t numbToSend = data->length;
      uint8_t *txTempBuf = data->txBuffer;
      uint8_t *rxTempBuf = data->rxBuffer;

      // There is also a DMA requirement that the number be mulitple of 4 or 2,
      // in some cases.
      while(numbToSend > 65532)
      {
        // syslog(1, "->Send Loop-to send %lu bytes, tx:%p->rx:%p\n",
        //           numbToSend, txTempBuf, rxTempBuf);
        SPI_EXCHANGE(target, txTempBuf, rxTempBuf, 65532);
        if(txTempBuf) txTempBuf += 65532;
        if(rxTempBuf) rxTempBuf += 65532;
        numbToSend -= 65532;
      }

      // syslog(1, "->Send Last-%lu bytes, tx:%p->rx:%p\n",
      //             numbToSend, txTempBuf, rxTempBuf);
      SPI_EXCHANGE(target, txTempBuf, rxTempBuf, numbToSend);
    }
  }
  else
  {
    // no read or write buffer
    return EINVAL;
  }
#else
  // if we have only outbuffer, it's a write
  if(data->txBuffer)
  {
    if(data->rxBuffer)
    {
      // writeread
      SPI_EXCHANGE(target, data->txBuffer, data->rxBuffer, data->length);
    }
    else
    {
      //write
      SPI_SNDBLOCK(target, data->txBuffer, data->length);
    }
  }
  else if(data->rxBuffer > 0)
  {
    // read
    SPI_RECVBLOCK(target, data->rxBuffer, data->length);
  }
  else
  {
    // no read or write buffer
    return EINVAL;
  }
#endif
  return OK;
}

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

  struct i2c_config_s *pCfg;
  struct i2c_master_s *pBus;

  if(data->busNumber == 0 || data->busNumber == 1)
  {
    if(g_i2c1 == NULL)
    {
      g_i2c1 = stm32_i2cbus_initialize(MEADOW_I2C_PORT1);
    }
    pBus = g_i2c1;
    pCfg = &g_i2c1_cfg;
  }
  else if(data->busNumber == 3)
  {
    if(g_i2c3 == NULL)
    {
      g_i2c3 = stm32_i2cbus_initialize(MEADOW_I2C_PORT3);
    }
    pBus = g_i2c3;
    pCfg = &g_i2c3_cfg;
  }
  else
  {
    return ENODEV;
  }

  pCfg->address = data->address;
  pCfg->addrlen = 7; // we currently are supporting only 7-bit address devices
  pCfg->frequency = data->frequency;

  int result = OK;

  // if we have only outbuffer, it's a write
  if(data->txLength > 0)
  {
    if(data->rxLength > 0)
    {
      // writeread
      result = i2c_writeread(pBus, pCfg, data->txBuffer, data->txLength, data->rxBuffer, data->rxLength);
    }
    else
    {
      //write
      result = i2c_write(pBus, pCfg, data->txBuffer, data->txLength);
    }
  }
  else if(data->rxLength > 0)
  {
    // read
    result = i2c_read(pBus, pCfg, data->rxBuffer, data->rxLength);
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
  pwm = stm32_pwminitialize(_upd_pwm_cmd->timer);
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
#ifdef CONFIG_PWM_MULTICHAN
      pwm->ops->ioctl(pwm, 0, (unsigned long)&info);
      for (int i = 0; i < CONFIG_PWM_NCHANNELS; i++)
      {
        if (info.channels[i].channel == _upd_pwm_cmd->channel)
        {
          info.channels[i].duty = _upd_pwm_cmd->duty;
        }
      }
#else
      info.duty = _upd_pwm_cmd->duty;
#endif

      pwm->ops->start(pwm, &info);
      return OK;
  }
  case MUPD_PWM_STOP:
  {
      struct pwm_info_s info;
#ifdef CONFIG_PWM_MULTICHAN
      pwm->ops->ioctl(pwm, 0, (unsigned long)&info);
      for (int i = 0; i < CONFIG_PWM_NCHANNELS; i++)
      {
        if (info.channels[i].channel == _upd_pwm_cmd->channel)
        {
          info.channels[i].duty = 0;
        }
      }
#else
      info.duty = 0;
#endif

      pwm->ops->start(pwm, &info);
      return OK;
  }
  }

  return OK;
}

static int upd_open(struct file *filep)
{
  extern mqd_t s_int_queue;
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = MINT_MSG_QUEUE_MAX_MSGS;
  attr.mq_msgsize = MINT_MSG_QUEUE_MSG_SIZE;
  attr.mq_curmsgs = 0;

  if(s_int_queue == 0)
  {
    s_int_queue = mq_open(MINT_MSG_QUEUE_NAME, O_WRONLY | O_CREAT, 0660, &attr);
    if (s_int_queue == (mqd_t)-1)
    {
      int errcode = get_errno();
      syslog(LOG_ERR, "%s@%d-mq_open failed: %d\n", __FILE__, __LINE__, errcode);
      return -errcode;
    }
  }
  return OK;
}

static int upd_close(struct file *filep)
{
  extern mqd_t s_int_queue;
  mq_close(s_int_queue);

  return OK;
}

/****************************************************************************
 * Name: upd_handle_esp32_command
 *
 * Description:
 *  Take the command for the ESP32 and package this up into a message to
 *  send to the ESP32.
 *
 * Input Parameters:
 *  data - structure containing a pointer to the structure containing the
 *         message information.
 *
 * Returned Value:
 *  OK if the command was executed or ERROR if there was a problem.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int upd_handle_esp32_command(struct upd_esp32_command *data)
{
  int result = ERROR;
  uint8_t *payload = NULL;
  espcp_message_t *message = NULL;

  data->status_code = espcp_status_codes_failure;
  
  espcp_configuration_t *config = espcp_get_configuration();
  if (config == NULL)
  {
    return ERROR;
  }
  if (config->esp_not_responding)
  {
    data->status_code = espcp_status_codes_coprocessor_not_responding;
    return(ERROR);
  }
  if (data->payload_length != 0)
  {
    /*
      *  Note that we must take a copy of the message payload here as
      *  the process of sending the message that will be created next
      *  will dispose of the message and the payload after the message
      *  has been dealt with.  We do not want to mess with the managed
      *  memory so we take copies.
      */
    payload = (uint8_t *) malloc(data->payload_length);
    if (payload == NULL)
    {
      return ERROR;
    }
    memcpy(payload, data->payload, data->payload_length);
  }
  else
  {
    payload = NULL;
  }

  message = espcp_create_message_on_heap(espcp_message_types_header,
    data->interface, data->function, 0, espcp_get_next_message_id(),
    payload, data->payload_length);
  if (message == NULL)
  {
    if (payload != NULL)
    {
      free(payload);
    }
    return ERROR;
  }

  if (espcp_queue_message(message, data->block != 0) == espcp_status_codes_completed_ok)
  {
    result = OK;
    if (data->block != 0)
    {
      data->status_code = message->status_code;
      if (message->payload_length > 0)
      {
        if (message->payload_length <= data->result_length)
        {
          memcpy(data->result, message->payload, message->payload_length);
          data->result_length = message->payload_length;
        }
        else
        {
          result = ERROR;
        }
      }
    }

    else
    {
      //
      //  Non-blocking message should appear to succeed immediately
      //  as success or failure is normally indicated by an event.
      //
      data->status_code = espcp_status_codes_completed_ok;
    }
  }
  if (data->block != 0)
  {
    //
    //  We must free blocking messages here as they have served their purpose,
    //  non-blocking messages will be deleted by the messaging system when
    //  they have been sent to the ESP32.
    //
    espcp_delete_message_and_payload(message);
  }
  return(result);
}

/****************************************************************************
 * Name: upd_handle_esp32_get_event_result
 * 
 * Description:
 *  Get any payload and status information for events returning more than
 *  a trivial amount of data.
 * 
 *  This method is necessary as message queues can only hold 22 bytes of data.
 *  After overheads are taken into considertion this only leaves 13 bytes.  It
 *  was decided to use the payload and payload_length concept used with
 *  messages to transfer any data other than interface, function and status
 *  code.
 *
 * Input Parameters:
 *  data - pointer to a structure to hold the event data.  The memory should
 *         be allocated in the managed code.
 *
 * Returned Value:
 *  OK if the command was executed or ERROR if there was a problem.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int upd_handle_esp32_get_event_result(espcp_event_data_payload_t *data)
{
  int result = OK;

  espcp_message_t *message = espcp_get_event_data(data->message_id);
  if (message != NULL)
  {
      if (data->payload_length >= message->payload_length)
      {
        if (data->payload_length > 0)
        {
          memcpy(data->payload, message->payload, message->payload_length);
        }
        data->payload_length = message->payload_length;
      }
      else
      {
        data->payload_length = 0;
        result = ERROR;
      }

      espcp_delete_message_and_payload(message);
  }
  else
  {
      result = ERROR;
  }
  
  return(result);
}

/****************************************************************************
 * Name: upd_get_set_configuration_value
 * 
 * Description:
 *  Process the request to read or write a configuration value.
 * 
 * Input Parameters:
 *  data - pointer to a structure holding the request.
 *
 * Returned Value:
 *  OK if the command was executed or ERROR if there was a problem.
 *
 * Assumptions/Limitations:
 *  None
 *
 ****************************************************************************/
int upd_get_set_configuration_value(upd_get_set_configuration_value_t *data)
{
  data->returned_data_length = hcom_nx_config_get_set_config_value(data->item, data->direction, data->buffer, data->buffer_length);
  return((data->returned_data_length >= 0) ? OK : ERROR);
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
  syslog(LOG_INFO, "meadow_upd_initialize\n");
  
  // register the driver, passing in our entry points
  int ret = register_driver("/dev/upd", &g_driver_operations, 0666, NULL);
  if (ret)
  {
    return ERROR;
  }

  return OK;
}

