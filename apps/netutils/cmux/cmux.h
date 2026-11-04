/****************************************************************************
 * apps/netutils/cmux/cmux.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __APPS_NETUTILS_CMUX_H
#define __APPS_NETUTILS_CMUX_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <sys/time.h>
#include <stdbool.h>
#include <debug.h>
#include <errno.h>

#ifndef MIN
#  define MIN(a,b)                 (((a) < (b)) ? (a) : (b))
#endif

#define CMUX_BIT0 (0)
#define CMUX_BIT1 (1)
#define CMUX_BIT2 (2)
#define CMUX_BIT3 (3)
#define CMUX_BIT4 (4)
#define CMUX_BIT5 (5)
#define CMUX_BIT6 (6)
#define CMUX_BIT7 (7)

#define CMUX_BUFFER_SZ (1024)
#define CMUX_CHANNEL_NAME_SZ (64)
#define CMUX_FRAME_MAX_SIZE (127)

/**
 * Mux Frame
 *
 * |      Open flag     |
 * |      1 octed       |
 * |       0xF9         |
 * ______________________
 *        Address
 * |      1 octet       |
 * |      ----------    |
 * ______________________
 *        Control
 * |     1 octet        |
 * |    ----------      |
 * ______________________
 *        Length
 * |     1-2 octet      |
 * |      ---------     |
 * ______________________
 *      Information
 * |  Multiples octets  |
 * |  ----------------  |
 * ______________________
 * |      FCS           |
 * |      1 octed       |
 * |    ---------       |
 * ______________________
 * |    Close flag      |
 * |    1 octed         |
 * |    0xF9            |
 *
 */

/* Flag Field - Each frame begins and ends with a flag sequence octet. */

#define CMUX_OPEN_FLAG       (0xF9)
#define CMUX_CLOSE_FLAG (0xF9)

/**
 * Address Field
 *
 * |Bit 1 |Bit 2 |Bit 3 |Bit 4 |Bit 5 |Bit 6 |Bit 7 |Bit 8
 * |ADDR_FIELD_BIT_EA    | C/R  |      |      |      | DLCI |      |
 */

/* EA bit extends the range of the address field.
 * When the EA bit is set to 1 in an octet, it signifies that this octet
 * is the last octet of the length field.
 * When the EA bit is set to 0, it signifies that another
 * octet of the address field follows.
 */

#define CMUX_ADDR_FIELD_BIT_EA (CMUX_BIT1)
#define CMUX_ADDR_FIELD_OPERATOR (0x3F)
#define CMUX_ADDR_FIELD_CHECK    (0xFC)

/**
 * The C/R (command/response) bit identifies the frame as
 * either a command or a response.
 * ______________________________
 * ________| Direction| CR Value
 * Command | T.E -> U.E | 1
 * Command | T.E <- U.E | 0
 * ______________________________
 * Response | T.E -> U.E | 1
 * Response | T.E <- U.E | 0
 */

#define CMUX_ADDR_FIELD_BIT_CR (CMUX_BIT2)

/* Control field
 * |Bit 1 |Bit 2| Bit 3 |Bit 4 |Bit 5 |Bit 6 |Bit 7 |Bit 8
 *    -     -      -      -      PF     -      -      -
 * P/F (Poll/Final)
 * - The Poll bit set to 1 shall be used by one station to solicit poll
 * a response or sequence of responses from the other station.
 * - The final bit set to 1 shall be used by a station to indicate
 * the response frame transmitted as the result of a
 * soliciting (poll) command.
 */

/* Poll/Final */

#define CMUX_CONTROL_FIELD_BIT_PF (0x10)

/* Set Asynchronous Balanced Mode :  establish DLC between T.E and U.E */

#define CMUX_FRAME_TYPE_SABM (0x2F)

/* Unnumbered Acknowledgement:  is a response to SABM or DISC frame */

#define CMUX_FRAME_TYPE_UA (0x63)

/* Disconnected Mode :  frame is used to report a status where the station
 * is logically disconnected from the data link. When in disconnected mode,
 * no commands are accepted until the disconnected mode is terminated by
 * the receipt of a SABM command. If a DISC command is received while
 * in disconnected mode, a DM response is sent
 */

#define CMUX_FRAME_TYPE_DM (0x0F)

/* Disconnect : is a command frame and is used to close down DLC. */

#define CMUX_FRAME_TYPE_DISC (0x43)

/* Unnumbered Information with Header check :
 * command/response sends user data at either station
 */

#define CMUX_FRAME_TYPE_UIH (0xEF)

/* Unnumbered Information */

#define CMUX_FRAME_TYPE_UI (0x03)

#define CMUX_FRAME_TYPE(type, frame) ((frame->control & ~CMUX_CONTROL_FIELD_BIT_PF) == type)

/* | U.E         | <---------   SABM (DLC 1)         -------------  | T.E
 * |             | ----------   U.A (Response)        ------------> |
 * | Multiplexer | <----------  DISC (Close DLC 1)   ------------   |  Recv
 * |             | <----------- UA(Response)         -----------    |
 */

/* Note : Some manufactures doesn't support UI frame. */

/* Length Field
 * |Bit 1 |Bit 2| Bit 3 |Bit 4 |Bit 5 |Bit 6 |Bit 7 |Bit 8
 *    E/A    L1    L2     L3      L4     L5     L6     L7
 * - L1 - L7 : The L1 to L7 bits indicate the length of the
 *   following data field for the information field less than 128 bytes
 * - EA bit = 1 in an octet, it signifies that this octet
 *   is the last octet of the length field.
 * - EA bit = 0, it signifies that a second octet of
 *   the length field follows.
 * The total length of the length field is 15 bits in that case.
 */

#define CMUX_LENGTH_FIELD_MAX_VALUE (0x7F)
#define CMUX_LENGTH_FIELD_OPERATOR  (0xFE)

/* Information Field
 * The information field is the payload of the frame and carries the
 * user data and any convergence layer information.
 * The field is octet structured and only presents in UIH frames.
 */

/* FSC field
 * In the case of the UIH frame, the contents of the information field shall
 * not be included in the FCS calculation. FCS is calculated on the contents
 * of the address, control and length fields only. This means that only the
 * delivery to the correct DLCI is protected, but not the information.
 */

#define CMUX_FCS_MAX_VALUE  (0xFF)
#define CMUX_FCS_OPERATOR   (0xCF)

/* reversed, 8-bit, poly=0x07 */
#define CMUX_CRC_TABLE { \
    0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75, \
    0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B, \
    0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69, \
    0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67, \
    0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D, \
    0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43, \
    0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51, \
    0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F, \
    0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05, \
    0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B, \
    0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19, \
    0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17, \
    0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D, \
    0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33, \
    0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21, \
    0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F, \
    0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95, \
    0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B, \
    0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89, \
    0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87, \
    0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD, \
    0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3, \
    0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1, \
    0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF, \
    0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5, \
    0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB, \
    0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9, \
    0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7, \
    0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD, \
    0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3, \
    0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1, \
    0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF \
}

struct cmux_parse_s
{
  unsigned char address;              /* Reserved to address filed */
  unsigned char control;              /* Reserved to control field */
  int data_length;                    /* Reserved to data length field */
  unsigned char data[CMUX_BUFFER_SZ]; /* Reserved to information field */
};

struct cmux_stream_buffer_s
{
  unsigned char data[CMUX_BUFFER_SZ]; /* Buffer to hold incoming packets. */
  unsigned char *readp;               /* Pointer to read buffer */
  unsigned char *writep;              /* Pointer to write buffer */
  unsigned char *endp;                /* Pointer to end of buffer */
  int flag_found;                     /* Detected open flag */
  unsigned long received_count;       /* Counter to received packets */
  unsigned long dropped_count;        /* Counter to dropped packets */
};

struct cmux_channel_s
{
  int master_fd;                         /* Master pseudo terminal */
  int slave_fd;                          /* Slave pseudo terminal */
  int dlci;                              /* Data Link Connection Identifier */
  char slave_path[CMUX_CHANNEL_NAME_SZ]; /* Path do slave (/dev/pts/X) */
  bool active;                           /* Flag to check if the channel is active */
  time_t last_activity;                  /* Timestamp to last packet sent/received */
};

#endif /* __APPS_NETUTILS_CMUX_H */