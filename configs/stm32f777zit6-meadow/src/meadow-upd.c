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
#include <sys/ioctl.h>
#include "stm32_tim.h"
#include "meadow-upd.h"
#include <meadow/hcom_nuttx_shared.h>
#include "stm32_uid.h" // stm32_get_uniqueid()

#include "espcp/espcp_common.h"

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


struct upd_dir_enum_cmd
{
  char* root; // folder to enumerate
  char* result; // data back to app
  uint32_t resultLength; // length of data buffer
};


/*
 *  Information about the function that should be requested to
 *  be performed by the ESP32.
 */
struct upd_esp32_command
{
  uint8_t interface;          // Interface (WiFi, System etc.) to perform the request.
  uint32_t function;          // Function number to be executed.
  uint32_t status_code;       // Status code returned by the ESP32.
  uint8_t *payload;           // Pointer to the data required by the function.
  uint32_t payload_length;    // Length of the data block.
  uint8_t *result;            // Pointer to the result.
  uint32_t result_length;     // Length of the result data block.
  uint8_t block;              // Is this a blocking call?
};

/*
 *  Command data that relates to a block of memory previously allocated
 *  by the unmanaged code.
 */
struct upd_esp32_free_memory
{
  uint8_t *memory;
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

static int upd_handle_esp32_command(struct upd_esp32_command *);
static int upd_handle_dev_info_request(struct upd_device_info *);

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

#define MEADOW_I2C_PORT     1
#define MEADOW_SPI_PORT3    3  // external
#define MEADOW_SPI_PORT2    2  // EXP32

static struct i2c_master_s *g_i2c1 = NULL;
static struct i2c_config_s g_i2c_cfg;

static struct spi_dev_s *g_spi3 = NULL; // external
static struct spi_dev_s *g_spi2 = NULL; // to ESP32

static int upd_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  struct upd_register_value *register_val;
  struct upd_register_update *register_update;
  struct upd_gpio_int_config *interrupt_cfg;

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
        interrupt_cfg = (struct upd_gpio_int_config *)arg;
        return upd_config_interrupt(interrupt_cfg);

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
      return upd_handle_esp32_command((struct upd_esp32_command *) arg);

    case MUPD_GET_DEVICE_INFO:
      return upd_handle_dev_info_request((struct upd_device_info *) arg);
  }
  return ERROR;
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

  if(g_i2c1 == NULL)
  {
    // the only I2C port Meadow supports is #1 - just initialize it
    g_i2c1 = stm32_i2cbus_initialize(MEADOW_I2C_PORT);
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
  attr.mq_maxmsg = QUEUE_MAX_MSGS;
  attr.mq_msgsize = QUEUE_MSG_SIZE;
  attr.mq_curmsgs = 0;

  if(s_int_queue == 0)
  {
    s_int_queue = mq_open(QUEUE_NAME, O_WRONLY | O_CREAT, 0660, &attr);
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
  int result = OK;
  uint8_t *payload = NULL;

  espcp_configuration_t *config = espcp_get_configuration();
  if (config == NULL)
  {
    result = ERROR;
  }
  else
  {
    if (config->esp_not_responding)
    {
      data->status_code = espcp_status_codes_coprocessor_not_responding;
    }
    else
    {
      if (data->payload_length != 0)
      {
        //
        //  TODO: This may not be required, it may be possible to use the original payload pointer.
        //
        payload = (uint8_t *) malloc(data->payload_length);
        if (payload == NULL)
        {
          return ERROR;
        }
        memcpy(payload, data->payload, data->payload_length);
      }
      
      espcp_message_t *message = espcp_create_message_on_heap(espcp_message_types_header,
        data->interface, data->function, 0, espcp_get_next_message_id(),
        payload, data->payload_length);
      if (message == NULL)
      {
        return ERROR;
      }

      result = espcp_queue_message(message, data->block != 0);

      if (result == espcp_status_codes_completed_ok)
      {
        result = OK;
        if (message->payload_length > 0)
        {
          if (message->payload_length <= data->result_length)
          {
            memcpy(data->result, message->payload, message->payload_length);
            data->result_length = message->payload_length;
          }
          else
          {
            data->result_length = 0;
            result = ERROR;
          }
        }
        else
        {
          data->result_length = 0;
        }
      }
      else
      {
        result = ERROR;
      }
      //
      //  TODO: Is this an error?
      //
      // espcp_delete_message_and_payload(message);
    }
  }

  return(result);
}

//==============================================================
// Returns null terminated string containing device information with
// elements separated by 0x03
static int upd_handle_dev_info_request(struct upd_device_info *devInfo)
{
  int ret;
  int stringLen;
  char strMcuSn[16];
  uint8_t mcuId[12];
  char returnValueBuf[MEADOW_DEFAULT_INI_CFG_BUF_LEN];

  stm32_get_uniqueid(mcuId);  // 96 bit unique chip id as 12 bytes
  char strChipId[64];
  snprintf(strChipId, 64, "%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x-%02x", 
     mcuId[0],  mcuId[1],  mcuId[2],  mcuId[3],  mcuId[4],  mcuId[5],
     mcuId[6],  mcuId[7],  mcuId[8],  mcuId[9],  mcuId[10],  mcuId[11]);

  ret = hcom_nx_common_utils_calculate_serial_numb(NULL, strMcuSn);
  if(ret < 0)
  {
    strcpy(strMcuSn, "<calc error>");
  }
  
  ret = meadow_config_find_value_from_key(NULL, "operation", "DeviceName", returnValueBuf, 
          MEADOW_DEFAULT_INI_CFG_BUF_LEN);
  if(ret != OK)
  {
    strcpy(returnValueBuf, "MeadowF7");
  }

  // Build a ETX (0x03) delimited string
  stringLen = snprintf(devInfo->infoBuf, devInfo->infoBufLen,
                "%s%c"      // Device Name - user defined or MeadowF7
                "%s%c"      // Product Info - Meadow by Wilderness Labs
                "%s%c"      // Model - F7Micro
                "%s%c"      // Meadow OS ver - 0.4.0
                "%s @ %s%c" // build date & time - Dec  5 2020 @ 09:04:51
                "%s%c"      // Processor type - STM32F777IIK6
                "%s%c"      // MCU Id - 19-00-27-00-0e-51-38-32-37-35-36-30
                "%s%c"      // MCU S/N - 305D355A3238
                "%s%c"      // Co processor type - ESP32
                "%s%c"      // Co processor version - 0.4.1.6
                "%s",       // Mono version - 0.0.0.1
                returnValueBuf, 0x03,
                HCOM_DEVICE_INFO_PRODUCT, 0x03,
                HCOM_DEVICE_INFO_MODEL, 0x03,
                HCOM_DEVICE_INFO_MEADOW_OS_VERSION, 0x03,
                __DATE__, __TIME__, 0x03,
                HCOM_DEVICE_INFO_PROCESSOR_TYPE, 0x03,
                strChipId, 0x03,
                strMcuSn, 0x03,
                HCOM_DEVICE_INFO_COPROCESSOR_TYPE, 0x03,
                HCOM_DEVICE_INFO_COPROCESSOR_OS_VERSION, 0x03,
                HCOM_DEVICE_INFO_MONO_VERSION);
  devInfo->infoRetLen = stringLen;
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
  syslog(LOG_INFO, "+meadow_upd_initialize\n");
  
  // register the driver, passing in our entry points
  int ret = register_driver("/dev/upd", &g_driver_operations, 0666, NULL);
  if (ret)
  {
    return ERROR;
  }

  return OK;
}

