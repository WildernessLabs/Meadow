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

#include <pty.h>

#include <meadow/hcom_protocol.h>
#include <meadow/meadow_os.h>

#include "hcom_cmux.h"

#define BIT_0 (0)
#define BIT_1 (1)
#define BIT_2 (2)
#define BIT_3 (3)
#define BIT_4 (4)
#define BIT_5 (5)
#define BIT_6 (6)
#define BIT_7 (7)

#define PATH_TO_UART1              "/dev/ttyS1"
#define CMD_ATTEMPS  (10)
#define CHANNEL_NAME_SIZE (64)

#define FRAME_PREFIX (5)
#define FRAME_POSFIX (2)

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

static char *thisFile = __FILE__;

static int fd_uart = 0;
static cmux_channel_t *cmux_status = NULL;
static int number_of_ports = 5;

static char *cmux_modem_at_cmd[] =
{ 
  {"ATE0\r\n"},
  {"AT+IFC=2,2\r\n"},
  {"AT+IPR=115200\r\n"},
  {"AT+CMUX=0,0,5,127,10,3,30,10,2\r\n"}
};

static int hcom_send_attention_cmd(unsigned char* cmd, size_t cmd_size, uint32_t cmd_timeout)
{
  fd_set rfds;
  int ret = 0;
  struct timeval timeout;
  static char buffer[125] = {0x00};

  if (fd_uart)
  {
    ret = write(fd_uart, cmd, cmd_size);
  }

  if (cmd_timeout)
  {
    tcdrain(fd_uart);
    sleep(1);

    timeout.tv_sec = 0;
		timeout.tv_usec = cmd_timeout;

    for (int attemps = 0; attemps < CMD_ATTEMPS; attemps ++)
    {
        FD_ZERO(&rfds);
	    FD_SET(fd_uart, &rfds);
        ret = select((fd_uart+1), &rfds, NULL, NULL, &timeout);
        if (ret > 0)
        {
            if (FD_ISSET(fd_uart, &rfds))
            {
                memset (buffer, 0, sizeof(buffer));
                ret = read(fd_uart, buffer, sizeof(buffer));
                if (ret > 0)
                {
                    break;
                }
            }
        }
    }
  }
  return ret;
}

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
  char pty_name[64];
  struct termios options;

  // set raw input
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
  options.c_iflag &= ~(INLCR | ICRNL | IGNCR);

  // set raw output
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

static int open_channels(cmux_channel_t * ch, int num_of_ports)
{
  int ret = 0;
  for (int i = 0; i < num_of_ports; i++)
  {
    ret = send_cmux_raw_frame(fd_uart, i, (FRAME_TYPE_SABM | CONTROL_FIELD_BIT_PF));
    if(ret != OK)
    {
      printf("Failed to open channel\n");
      break;
    }
    sleep(1);
  }
  return ret;
}

static int hcom_setup_multiplex_mode()
{
    int ret = 0;

    ret = hcom_send_attention_cmd(cmux_modem_at_cmd[0], strlen(cmux_modem_at_cmd[0]), 1000);      
    if (ret <=0)
    {
        return ERROR;
    }

    ret = hcom_send_attention_cmd(cmux_modem_at_cmd[1], strlen(cmux_modem_at_cmd[1]), 1000);
    if (ret <=0)
    {
        printf("Failed to send,ret=%d\n", ret);
        return ERROR;
    }
    sleep(1);

    ret = hcom_send_attention_cmd(cmux_modem_at_cmd[2], strlen(cmux_modem_at_cmd[2]), 1000);
    if (ret <=0)
    {
        printf("Failed to send,ret=%d\n", ret);
        return ERROR;
    }
    sleep(1);

    ret = hcom_send_attention_cmd(cmux_modem_at_cmd[3], strlen(cmux_modem_at_cmd[3]), 1000);
    if (ret <=0)
    {
        printf("Failed to send,ret=%d\n", ret);
        return ERROR;
    }
    return OK;
}

static int hcom_cmux_daemon(int argc, FAR char *argv[])
{
  fd_set rfds;
  struct timeval timeout;
  int channel = 0;
  int ret = 0;
  unsigned char buffer[512];

    for (;;)
    {
        FD_ZERO(&rfds);
        FD_SET(fd_uart, &rfds);

        int max_fd = fd_uart;
        for (int i = 0; i < number_of_ports; i++) 
        {
            if (cmux_status[i].active) 
            {
                FD_SET(cmux_status[i].master_fd, &rfds);
                FD_SET(cmux_status[i].slave_fd, &rfds);

                if (cmux_status[i].master_fd > max_fd) 
                    max_fd = cmux_status[i].master_fd;
                if (cmux_status[i].slave_fd > max_fd) 
                    max_fd = cmux_status[i].slave_fd;
            }
        }

        timeout.tv_usec = 100;
        timeout.tv_sec = 0;

        ret = select(max_fd + 1, &rfds, NULL, NULL, &timeout);
        if (ret > 0)
        {
            if (FD_ISSET(fd_uart, &rfds))
            {
                int bytes_read = read(fd_uart, buffer, sizeof(buffer) - 1);
                if (bytes_read > 0)
                {
                    ;
                }
            }

            for (int i = 0; i < number_of_ports; i++) 
            {
                if (cmux_status[i].active && FD_ISSET(cmux_status[i].master_fd, &rfds))
                {
                    memset(buffer, 0, sizeof(buffer));
                    int bytes_read = read(cmux_status[i].master_fd, buffer, sizeof(buffer) - 1);
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

                if (cmux_status[i].active && FD_ISSET(cmux_status[i].slave_fd, &rfds))
                {
					memset(buffer, 0, sizeof(buffer));
					if (read(cmux_status[i].slave_fd, buffer, sizeof(buffer)-1) > 0 )
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
    int ret = ERROR;
    fd_uart = open(PATH_TO_UART1, O_RDWR | O_NONBLOCK);
    if (fd_uart)
    {
        cmux_status = (cmux_channel_t *) malloc(sizeof(cmux_channel_t) * number_of_ports);
        if (cmux_status == NULL)
        {
            hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to allocate cmux channels\n", thisFile, __LINE__);
            return (-ENOMEM);
        }

        ret = hcom_create_multiplex_channel(cmux_status, number_of_ports);
        if (ret < 0)
        {
            hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to create multiplex channels\n", thisFile, __LINE__);
            return ret;
        }

        ret = hcom_setup_multiplex_mode();
        if (ret < 0)
        {
            hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to setup multiplex mode\n", thisFile, __LINE__);
            return ret;
        }

        ret = task_create(HCOM_CMUX_TASK_NAME,
                         HCOM_CMUX_TASK_PRIORITY,
                         HCOM_CMUX_TASK_STACKSIZE,
                         hcom_cmux_daemon, NULL);
        if (ret < 0)
        {
            hcom_logging_syslog(LOG_ERR, "%s-%d-Failed to create cmux task\n", thisFile, __LINE__);
            return ret;
        }
        ret = OK;
    }

    return ret;
}