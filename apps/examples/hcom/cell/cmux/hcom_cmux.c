#include <nuttx/config.h>
#include <inttypes.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/select.h>

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pty.h>

#include <meadow/hcom_protocol.h>
#include <meadow/meadow_os.h>

#include "hcom_cmux.h"
#include "netutils/chat.h"

#define BIT_0 (0)
#define BIT_1 (1)
#define BIT_2 (2)
#define BIT_3 (3)
#define BIT_4 (4)
#define BIT_5 (5)
#define BIT_6 (6)
#define BIT_7 (7)

#define CMD_ATTEMPS  (10)
#define CHANNEL_NAME_SIZE (64)

#define FRAME_PREFIX (5)
#define FRAME_POSFIX (2)
#define HCOM_CMUX_TIMEOUT_SECONDS                     (30)
#define HCOM_CMUX_NUMBER_OF_PORTS                     (5)

#define HCOM_CMUX_TASK_PRIORITY                       150
#define HCOM_CMUX_TASK_NAME                           "CellCmux"
#define HCOM_CMUX_TASK_STACKSIZE                      3072

/**
 * CMux Frame
 * 
 * |Open flag |Address  |Control  |Length       |Infomation (payload) |FCS      |Close flag
   |1 octet   |1 octet  |1 octet  |1-2 octet    |Multiples octets     |1 octed  |1 octed
   | 0xF9     |-------  |-------  |---------    |----------------     |-------  |0xF9
*/

/* Flag Field */
#define F_FLAG (0xF9) /* Each frame begins and ends with a flag sequence octet. */

/**
 * Address Field
 * 
 * |Bit 1 |Bit 2 |Bit 3 |Bit 4 |Bit 5 |Bit 6 |Bit 7 |Bit 8
   |ADDR_FIELD_BIT_EA    | C/R  |      |      |      | DLCI |      |
 */
 #define ADDR_FIELD_BIT_CR (BIT_2)

/**
 * The C/R (command/response) bit identifies the frame as either a command or a response.
 * ______________________________
 * ________| Direction| CR Value
 * Command | TE -> UE | 1
 * Command | TE <- UE | 0
 * ______________________________
 * Response | TE -> UE | 1
 * Response | TE <- UE | 0
 */

#define ADDR_FIELD_BIT_EA (BIT_1)

/*
* EA bit extends the range of the address field. When the EA bit is set to 1 in an octet,
* it signifies that this octet is the last octet of the length field.
* When the EA bit is set to 0, it signifies that another octet of the address field follows.
*/

/* Control field 
* |Bit 1 |Bit 2| Bit 3 |Bit 4 |Bit 5 |Bit 6 |Bit 7 |Bit 8
*    -     -      -      -      PF     -      -      - 
* P/F (Poll/Final)
* - The Poll bit set to 1 shall be used by one station to solicit poll a response
    or sequence of responses from the other station.
* - The final bit set to 1 shall be used by a station to indicate the response frame
    transmitted as the result of a soliciting (poll) command.
*/
#define CONTROL_FIELD_BIT_PF 0x10    /* Poll/Final */

#define FRAME_TYPE_SABM    (0x2F) /* Set Asynchronous Balanced Mode :  establish DLC between TE and UE */

#define FRAME_TYPE_UA      (0x63) /* Unnumbered Acknowledgement:  is a response to SABM or DISC frame */

#define FRAME_TYPE_DM      (0x0F) /* Disconnected Mode :  frame is used to report a status where the station
                                    is logically disconnected from the data link. When in disconnected mode,
                                    no commands are accepted until the disconnected mode is terminated by 
                                    the receipt of a SABM command. If a DISC command is received while
                                    in disconnected mode, a DM response is sent*/

#define FRAME_TYPE_DISC    (0x43) /* Disconnect : is a command frame and is used to close down DLC. */

#define FRAME_TYPE_UIH     (0xEF) /* Unnumbered Information with Header check :  command/response sends user data at either station*/

#define FRAME_TYPE_UI      (0x03) /* Unnumbered Information */

/**
 * | UE          | <---------   SABM (DLC 1)         -------------  | TE
 * |             | ----------   UA (Response)        ------------>  |
 * | Multiplexer | <----------  DISC (Close DLC 1)   ------------   |  Receiver
 * |             | <----------- UA(Response)         -----------    |
 */

/*
* Note : Some manufactures doesn't support UI frame.
*/

/* Length Field 
* |Bit 1 |Bit 2| Bit 3 |Bit 4 |Bit 5 |Bit 6 |Bit 7 |Bit 8
*    E/A    L1    L2     L3      L4     L5     L6     L7 
* - L1 - L7 : The L1 to L7 bits indicate the length of the 
    following data field for the information field less than 128 bytes
* - EA bit = 1 in an octet, it signifies that this octet is the last octet of the length field.
* - EA bit = 0, it signifies that a second octet of the length field follows.
* The total length of the length field is 15 bits in that case.
*/
#define LENGTH_FIELD_MAX_VALUE (0x7F)

/**
 * Information Field
 * The information field is the payload of the frame and carries the user data and
 * any convergence layer information. The field is octet structured and only presents in UIH frames.
 */

 /**
  * FSC field
  * In the case of the UIH frame, the contents of the information field shall not be included in the FCS
  * calculation. FCS is calculated on the contents of the address, control and length fields only. This means
  * that only the delivery to the correct DLCI is protected, but not the information.
  */

#define CMUX_MAX_RETRIES (3)
#define FRAME_MAX_SIZE   (127) // Maximum frame size. Range: 1–32768. Default value: 127

// the types of the control channel commands
#define C_CLD 193
#define C_TEST 33
#define C_MSC 225
#define C_NSC 17

// V.24 signals: flow control, ready to communicate, ring indicator, data valid
// three last ones are not supported by Siemens TC_3x
#define S_FC 2
#define S_RTC 4
#define S_RTR 8
#define S_IC 64
#define S_DVframeReceiveTime 128

#define COMMAND_IS(command, type) ((type & ~ADDR_FIELD_BIT_CR) == command)
#define PF_ISSET(frame) ((frame->control & CONTROL_FIELD_BIT_PF) == CONTROL_FIELD_BIT_PF)
#define FRAME_IS(type, frame) ((frame->control & ~CONTROL_FIELD_BIT_PF) == type)

typedef struct {
    int master_fd;
    int slave_fd;
    int dlci;                               /* Data Link Connection Identifier */
    char slave_path[CHANNEL_NAME_SIZE];     /* Path do slave (/dev/pts/X) */
    bool active;
    time_t last_activity;
} cmux_channel_t;

typedef struct {
    int serial_fd;
    int number_of_ports;
    cmux_channel_t *channel;
    cell_settings_t *settings;
}hcom_cmux_config_t;

static char *thisFile = __FILE__;

static char g_cmux_script [] = 
  "ECHO ON "
  "TIMEOUT 30 "
  "\"\" ATE0 "
  "OK AT+IFC=2,2 "
  "OK AT+IPR=115200 "
  "OK AT+CMUX=0,0,5,127,10,3,30,10,2 "
  "OK \\c";

static int send_cmux_frame(int fd, int channel, char *buffer, int frame_size, unsigned char type)
{
    unsigned char frame_prefix[FRAME_PREFIX] = { F_FLAG, (ADDR_FIELD_BIT_EA | ADDR_FIELD_BIT_CR), 0x00, 0x00, 0x00};
    unsigned char frame_posfix[FRAME_POSFIX] = { 0xFF, F_FLAG};
    int prefix_len = 4;

    frame_prefix[BIT_1] = (frame_prefix[BIT_1] | ((0x3F & (unsigned char)channel ) << 2));
    frame_prefix[BIT_2] = type;

    if (frame_size <= FRAME_MAX_SIZE)
    {
        frame_prefix[BIT_3] = ADDR_FIELD_BIT_EA | (frame_size << 1);
        prefix_len = 4;
    }
    else
    {
        frame_prefix[BIT_3] = (frame_size << 1) & 0xFE;  // EA=0
        frame_prefix[BIT_4] = ADDR_FIELD_BIT_EA | ((frame_size >> 7) << 1);
        prefix_len = 5;
    }

    frame_posfix[BIT_0] = make_fcs(frame_prefix + 1, prefix_len - 1);

    int ret = write(fd, frame_prefix, prefix_len);
    if (ret != prefix_len)
    {
        printf("Failed to write prefix frame (wrote %d, expected %d)\n", ret, prefix_len);
        return ERROR;
    }

    if (frame_size > 0 && buffer != NULL)
    {
        ret = write(fd, buffer, frame_size);
        if (ret != frame_size)
        {
            printf("Failed to write buffer (wrote %d, expected %d)\n", ret, frame_size);
            return ERROR;
        }
		printf("Buffer written %s\n", buffer);
    }

    ret = write(fd, frame_posfix, FRAME_POSFIX);
    if (ret != FRAME_POSFIX)
    {
        printf("Failed to write posfix (wrote %d, expected %d)\n", ret, FRAME_POSFIX);
        return ERROR;
    }

    printf("PREFIX : [%02x %02x %02x %02x %02x]\n",
    frame_prefix[BIT_0], frame_prefix[BIT_1], frame_prefix[BIT_2],
    frame_prefix[BIT_3], frame_prefix[BIT_4]);

    if (buffer)
    {
        for (int i = 0; i < frame_size; i++)
            printf(" %02x ", buffer[i]);
        printf("\n");
    }

    printf("POSFIX : [%02x %02x]\n", frame_posfix[BIT_0], frame_posfix[BIT_1]);
    return OK;
}

static int send_cmux_raw_frame(int fd, int channel, unsigned char type)
{
  int ret = OK;
  ret = send_cmux_frame(fd, channel, NULL, 0x00, type);
  return ret;
}

static int hcom_create_multiplex_channel(cmux_channel_t * ch, int num_of_ports)
{
  int ret = 0;
  int master_fd, slave_fd = 0;
  struct termios options;

  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_iflag &= ~(INLCR | ICRNL | IGNCR);

  options.c_oflag &= ~OPOST;
  options.c_oflag &= ~OLCUC;
  options.c_oflag &= ~ONLRET;
  options.c_oflag &= ~ONOCR;
  options.c_oflag &= ~OCRNL;

  for (int i = 0; i < num_of_ports; i++)
  { 
    ret = openpty(&ch[i].master_fd, &ch[i].slave_fd, &ch[i].slave_path, &options, NULL);
    if (ret < 0)
    {
      printf("Failed to opent pseudo terminal \n");
    }
    else
    {
      printf("Open Serial  master = %d slave = %d name: %s\n", ch[i].master_fd, ch[i].slave_fd, ch[i].slave_path);
      ch[i].dlci = i + 1;
      ch[i].active = true;
      ch[i].last_activity = time(NULL);
    }
  }

  return ret;
}

static int hcom_open_virtual_channels(int fd, int num_of_ports)
{
  int ret = 0;
  for (int i = 0; i < num_of_ports; i++)
  {
    ret = send_cmux_raw_frame(fd, i, (FRAME_TYPE_SABM | CONTROL_FIELD_BIT_PF));
    if(ret != OK)
    {
      printf("Failed to open channel\n");
      break;
    }
    sleep(1);
  }
  return ret;
}

static int hcom_setup_multiplex_mode(int fd)
{
    int ret = 0;
    struct chat_ctl ctl;

    ctl.echo = false;
    ctl.verbose = false;
    ctl.fd = fd;
    ctl.timeout = HCOM_CMUX_TIMEOUT_SECONDS;

    ret = chat(&ctl, &g_cmux_script, NULL);    
    return ret;
}

static void *hcom_cmux_thread(void *cmux_configs)
{
    fd_set rfds;
    struct timeval timeout;
    int channel = 0;
    int ret = 0;
    unsigned char buffer[512];

    hcom_cmux_config_t *cmux = (hcom_cmux_config_t *)cmux_configs;
    if (!cmux)
    {
        return NULL;
    }

    for (;;)
    {
        FD_ZERO(&rfds);
        FD_SET(cmux->serial_fd, &rfds);

        int max_fd = cmux->serial_fd;
        for (int i = 0; i < cmux->number_of_ports; i++) 
        {
            if (cmux->channel[i].active) 
            {
                FD_SET(cmux->channel[i].master_fd, &rfds);
                FD_SET(cmux->channel[i].slave_fd, &rfds);

                if (cmux->channel[i].master_fd > max_fd) 
                    max_fd = cmux->channel[i].master_fd;
                if (cmux->channel[i].slave_fd > max_fd) 
                    max_fd = cmux->channel[i].slave_fd;
            }
        }

        timeout.tv_usec = 100;
        timeout.tv_sec = 0;

        ret = select(max_fd + 1, &rfds, NULL, NULL, &timeout);
        if (ret > 0)
        {
            if (FD_ISSET(cmux->serial_fd, &rfds))
            {
                int bytes_read = read(cmux->serial_fd, buffer, sizeof(buffer) - 1);
                if (bytes_read > 0)
                {
                    ;
                }
            }

            for (int i = 0; i < cmux->number_of_ports; i++) 
            {
                if (cmux->channel[i].active && FD_ISSET(cmux->channel[i].master_fd, &rfds))
                {
                    memset(buffer, 0, sizeof(buffer));
                    int bytes_read = read(cmux->channel[i].master_fd, buffer, sizeof(buffer) - 1);
                    if (bytes_read > 0)
                    {
                        buffer[bytes_read] = '\r';
						buffer[bytes_read+1] = '\0';
                        ret = hcom_incomming_data_pty(buffer, bytes_read+1, i);
                        if (ret < 0)
                        {
                            ;
                        }
                    }
                }

                if (cmux->channel[i].active && FD_ISSET(cmux->channel[i].slave_fd, &rfds))
                {
					memset(buffer, 0, sizeof(buffer));
					if (read(cmux->channel[i].slave_fd, buffer, sizeof(buffer)-1) > 0 )
					{
                        ;
					}
                }
            }
        }
    }
}

int hcom_cmux_start(void)
{
    int ret = -ENODATA;
    pthread_t cmux_thread_id;
    hcom_cmux_config_t hcom_cmux;
    meadow_configuration_t *config = meadow_os_deep_copy_config();

    if ((config != NULL) && (config->default_interface != NULL))
    {
        if (config->default_interface->interface_type != MEADOW_IFT_CELL)
        {
            hcom_cmux.number_of_ports = (int) HCOM_CMUX_NUMBER_OF_PORTS;

            hcom_cmux.serial_fd = open(hcom_cmux.settings->ttyname, O_RDWR | O_NONBLOCK);
            if (hcom_cmux.serial_fd < 0)
            {
                hcom_logging_syslog(LOG_ERR, "Unable to open file %s\n", hcom_cmux.settings->ttyname);
                return -ENODEV;
            }

            hcom_cmux.settings = malloc(sizeof(cell_settings_t));
            if (hcom_cmux.settings == NULL) 
            {
                hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate cell settings struct\n", thisFile, __LINE__);
                return -ENOMEM;
            }

            memcpy(hcom_cmux.settings, config->default_cell_settings, sizeof(cell_settings_t));

            hcom_cmux.channel = (cmux_channel_t *) malloc(sizeof(cmux_channel_t) * hcom_cmux.number_of_ports);
            if (hcom_cmux.channel == NULL)
            {
                hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate cmux channels\n", thisFile, __LINE__);
                return -ENOMEM;
            }

            ret = hcom_create_multiplex_channel(hcom_cmux.channel, hcom_cmux.number_of_ports);
            if (ret < 0)
            {
                hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to create multiplex channels\n", thisFile, __LINE__);
                return ret;
            }

            ret = hcom_setup_multiplex_mode(hcom_cmux.serial_fd);
            if (ret < 0)
            {
                hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to setup multiplex mode\n", thisFile, __LINE__);
                return ret;
            }
            hcom_open_virtual_channels(hcom_cmux.serial_fd,  hcom_cmux.number_of_ports);

            pthread_attr_t attr;
            struct sched_param param;

            pthread_attr_init(&attr);

            size_t stack_size = HCOM_CMUX_TASK_STACKSIZE;
            pthread_attr_setstacksize(&attr, stack_size);

            param.sched_priority = HCOM_CMUX_TASK_PRIORITY;
            pthread_attr_setschedparam(&attr, &param);

            ret = pthread_create(&cmux_thread_id, &attr, hcom_cmux_thread, (void *)&hcom_cmux);
            if (ret == OK)
            {
                /* TODO: Startup pppd thread.*/

                hcom_logging_syslog(LOG_INFO, "%s@%d-CMUX thread launched\n", thisFile, __LINE__);

                meadow_os_config_free_resources(config);
                return ret;
            }
            hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to create CMUX thread\n", thisFile, __LINE__);
        }
    }
    meadow_os_config_free_resources(config);

    return ret;
}