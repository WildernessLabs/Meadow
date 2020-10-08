/****************************************************************************
 * \apps\examples\hcom\esp32\hcom_esp32_comms.h
 * 
 *   Copyright (C) 2020 Wilderness Labs. All rights reserved.
 *   Author:  Wilderness Labs
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

#ifndef __CONFIGS_MEADOW_SRC_HCOM_ESP32_COMMON__H
#define __CONFIGS_MEADOW_SRC_HCOM_ESP32_COMMON__H

/****************************************************************************
 * Included Files
 ****************************************************************************/
#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <fcntl.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef __ASSEMBLY__
#include <stdint.h>
#endif
#include <stdbool.h>
#include <sched.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <crc32.h>
#include <errno.h>
#include <debug.h>

#include <meadow/hcom_upd_shared.h>

#define HCOM_ESP32_ALLOW_BOOT_PIN_TO_BE_INPUT 1

// GPIO for controlling ESP32 enable and boot pins
#define HCOM_ESP32_DIGITAL_OUTPUT_STATE_LOW false   // For open drain this is N-MOS on
#define HCOM_ESP32_DIGITAL_OUTPUT_STATE_HIGH true   // For open drain this is N-MOS off

struct hcom_esp32_cir_buffer_s
{
  uint8_t *bottom; // bottom of buffer
  uint8_t *top;    // top end of buffer
  uint8_t *head;   // add data here
  uint8_t *tail;   // remove from here
};

struct HcomEsp32XmitHeader_s
{
  uint8_t direction;    // should alsways be 0x00
  uint8_t command;
  uint16_t size;
  uint32_t checksum;
} __attribute__((packed));
#define HCOM_ESP32_PROTOCOL_PRI_HDR_LENGTH (sizeof(struct HcomEsp32XmitHeader_s))

struct HcomEsp32RecvHeader_s
{
  uint8_t direction;    // should always be 0x01
  uint8_t command;
  uint16_t size;
  uint32_t value;
} __attribute__((packed));
#define HCOM_ESP32_PROTOCOL_RECV_HDR_LENGTH (sizeof(struct HcomEsp32RecvHeader_s))

// Message Queue data
#define HCOM_ESP32_MQ_DATA_FIELD_SIZE 32  // largest allowable size defined by Nuttx config
// This structure contains the message received
struct HcomEsp32UserRecvdData_s
{
  struct HcomEsp32RecvHeader_s espHdr;
  uint8_t esp32Status;
  uint8_t esp32Error;
  uint32_t recvdDataLen;
  uint8_t recvdData[HCOM_ESP32_MQ_DATA_FIELD_SIZE + 1];
} __attribute__((packed));
#define HCOM_ESP32_RECVD_DATA_STRUCT_LENGTH (sizeof(struct HcomEsp32UserRecvdData_s))

// This structure contains the data passed between receiver and transmitter
// via MQ. It is a qnique structure because MQ has a limited size. This
// structure contains a pointer to allocated memory. The memory is copied
// into the above structure by MQ receiver (the transmitter). So, memory is
// allocated by the receiver and freed by the transmitter, so each consumer
// need not worry about freeing the memory.
struct HcomEsp32MqRecvdData_s
{
  struct HcomEsp32RecvHeader_s espMqHdr;
  uint8_t esp32Status;
  uint8_t esp32Error;
  uint32_t recvdMqDataLen;
  uint8_t *recvdMqData;
} __attribute__((packed));
#define HCOM_ESP32_MQ_RECVD_DATA_STRUCT_LENGTH (sizeof(struct HcomEsp32MqRecvdData_s))

// Used by FLASH_BEGIN, MEM_BEGIN, FLASH_DEFL_BEGIN
struct HcomEsp32SecHdrBegin_s
{
  uint32_t eraseSize;
  uint32_t numbBlocks;
  uint32_t downloadWriteSize;
  uint32_t downloadOffset;
} __attribute__((packed));
#define HCOM_ESP32_PROTOCOL_BEGIN_HDR_LENGTH (sizeof(struct HcomEsp32SecHdrBegin_s))

// Use by FLASH_DATA, MEM_DATA, FLASH_DEFL_DATA
struct HcomEsp32SecHdrData_s
{
  uint32_t dataSize;
  uint32_t sequence;
  uint32_t zero1;
  uint32_t zero2;
} __attribute__((packed));
#define HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH (sizeof(struct HcomEsp32SecHdrData_s))

// Used by FLASH_END
struct HcomEsp32SecHdrFlashEnd_s
{
  uint32_t execFlag;
  uint32_t zero1;
  uint32_t zero2;
  uint32_t zero3;

} __attribute__((packed));
#define HCOM_ESP32_PROTOCOL_FLASH_END_HDR_LENGTH (sizeof(struct HcomEsp32SecHdrFlashEnd_s))

// Used by SPI_FLASH_MD5
struct HcomEsp32SecHdrFlashMD5_s
{
  uint32_t address;
  uint32_t size;
  uint32_t zero1;
  uint32_t zero2;
} __attribute__((packed));
#define HCOM_ESP32_PROTOCOL_FLASH_MD5_HDR_LENGTH (sizeof(struct HcomEsp32SecHdrFlashMD5_s))

// Used by MEM_END
struct HcomEsp32SecHdrMemEnd_s
{
  uint32_t execFlag;
  uint32_t entryPt;
} __attribute__((packed));
#define HCOM_ESP32_PROTOCOL_MEM_END_HDR_LENGTH (sizeof(struct HcomEsp32SecHdrMemEnd_s))

// Used by SPI_ATTACH
struct HcomEsp32SecHdrSpiAttach_s
{
  uint32_t spiPins;
  uint32_t legacyFlag;
} __attribute__((packed));

struct HcomEsp32SecHdrSpiParms_s
{
  uint32_t flId;
  uint32_t sizeInBytes;
  uint32_t blockSize;
  uint32_t sectorSize;
  uint32_t pageSize;
  uint32_t statusMask;
} __attribute__((packed));

#define HCOM_ESP_COMMS_MSG_QUEUE_NAME "/EspMQ"
#define HCOM_ESP_COMMS_MSG_QUEUE_MAX_MSGS     4

// This is just a guess and so far it's been big enough.
// Originally, it was based on HCOM_SAFE_PACKET_BUF_SIZE
// which was about 750. 
#define HCOM_ESP_COMMS_MAX_ESP_PACKET_SIZE 768

#define HCOM_ESP32_PICO_D4_FLASH_ID 0
#define HCOM_ESP32_PICO_D4_FLASH_SIZE (4 * 1024 * 1024)
#define HCOM_ESP32_PICO_D4_FLASH_BLOCK_SIZE (64 * 1024)
#define HCOM_ESP32_PICO_D4_FLASH_SECTOR_SIZE (4 * 1024)
#define HCOM_ESP32_PICO_D4_FLASH_PAGE_SIZE 256
#define HCOM_ESP32_PICO_D4_FLASH_STATUS_MASK 0xffff
#define HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE 0x400

// This combines all three sections of the data packet
#define HCOM_ESP32_PROTOCOL_LONGEST_BOOT_LOADER HCOM_ESP32_PROTOCOL_PRI_HDR_LENGTH + \
                                                HCOM_ESP32_PROTOCOL_DATA_HDR_LENGTH + \
                                                HCOM_ESP32_BOOT_LOADER_PAYLOAD_SIZE

#define HCOM_ESP_XMIT_TYPICAL_DELAY_MS      1000
#define HCOM_ESP_XMIT_FLASH_DELAY_MS        3000
#define HCOM_ESP_XMIT_CONNECT_DELAY_MS      100
#define HCOM_ESP32_ERASE_TIME_PER_MEGA_BYTE 3000

enum hcom_esp32_recv_buffer_return
{
  HCOM_ESP32_BUF_INIT_OK,
  HCOM_ESP32_BUF_INIT_FAILED,

  HCOM_ESP32_BUF_ADD_SUCCESS,
  HCOM_ESP32_BUF_ADD_WONT_FIT,
  HCOM_ESP32_BUF_ADD_BAD_ARG,

  HCOM_ESP32_BUF_GET_FOUND_BIN,
  HCOM_ESP32_BUF_GET_FOUND_TEXT,
  HCOM_ESP32_BUF_GET_NONE_FOUND,
  HCOM_ESP32_BUF_GET_DEST_NO_ROOM
};

enum EspCommands
{
    Esp32CommandUndefined = 0xff,
    Esp32CommandFlashBegin = 0x02,
    Esp32CommandFlashData = 0x03,
    Esp32CommandFlashEnd = 0x04,
    Esp32CommandMemBegin = 0x05,
    Esp32CommandMemEnd = 0x06,
    Esp32CommandMemData = 0x07,
    Esp32CommandSynchronise = 0x08,
    Esp32CommandWriteRegister = 0x09,
    Esp32CommandReadRegister = 0x0A,
    Esp32CommandSpiSetParams = 0x0B,
    Esp32CommandSpiAttach = 0x0D,
    Esp32CommandChangeBaudrate = 0x0F,
    Esp32CommandFlashDeflBegin = 0x10,
    Esp32CommandFlashDeflData = 0x11,
    Esp32CommandFlashDeflEnd = 0x12,
    Esp32CommandSpiFlashMd5 = 0x13,
    Esp32CommandEraseFlash = 0xD0,
    Esp32CommandEraseRegion = 0xD1,
    Esp32CommandReadFlash = 0xD2,
    Esp32CommandRunUserCode = 0xD3,
};

enum Esp32Registers
{
  Esp32RegAddrUART_DATE_REG_ADDR = 0x60000078,    // used to differentiate ESP8266 vs ESP32
  Esp32RegValueDATE_REG_VALUE_ESP32 = 0x15122500, // used to differentiate ESP8266 vs ESP32
  Esp32RegAddrEFUSE_REG_BASE = 0x6001a000,

};

  // ESP32 SPI comms
  int hcom_esp32_spi_comms_setup(void);
  void hcom_esp32_spi_comms_shutdown(void);
  int hcom_esp32_spi_comms_read_loop(void);
  typedef void (*hcom_esp32_spi_comms_callback)(int irq, void *context);
  void hcom_esp32_spi_comms_set_callback(hcom_esp32_spi_comms_callback cb);

  // ESP32 comms
  int hcom_esp32_uart_comms_setup(void);
  void hcom_esp32_uart_comms_shutdown(void);
  int hcom_esp32_uart_lazy_initialization(void);  
  int hcom_esp32_uart_comms_write_serial(uint8_t* espWriteBuf, size_t espWriteSize);

  // ESP32 execute
  int hcom_esp32_exec_setup_lazy(void);
  void hcom_esp32_exec_shutdown(void);
  int hcom_esp32_exec_download_flash_start(const size_t entireFileSize, const uint32_t targetAddr);
  int hcom_esp32_exec_add_flash_data(const uint8_t *packet, const size_t packetSize, uint16_t seqNumb);
  int hcom_esp32_exec_add_flash_end(void);
  void hcom_esp32_util_read_esp32_mac(uint32_t userData);
  void hcom_esp32_util_restart_esp32(uint32_t userData);
  char *hcom_esp32_exec_get_md5_file_hash(void);

  // ESP32 Received data processing
  int hcom_esp32_recv_setup_lazy(void);
  void hcom_esp32_recv_shutdown(void);
  int hcom_esp32_recv_handle_data(uint8_t *esp32_read_buffer, ssize_t readReturn);
  void hcom_esp32_recv_expect_command_type(uint8_t expectCommand);

  // ESP32 Transmit data processing
  int hcom_esp32_xmit_setup_lazy(void);
  void hcom_esp32_xmit_shutdown(void);
  bool hcom_esp32_xmit_is_command_expected(uint8_t espCmd);
  int hcom_esp32_xmit_build_and_send_msg(uint8_t *msgBody, ssize_t msgBodyLen,
        uint8_t espCommand, long millisecDelay, struct HcomEsp32UserRecvdData_s *recvdData);

  // ESP32 Utility functions
  int hcom_esp32_util_setup_lazy(void);
  void hcom_esp32_util_shutdown(void);
  int hcom_esp32_util_init_comms_enter_boot_mode(void);
  int hcom_esp32_util_read_register(uint32_t regAddr, uint32_t *regValue);
  int hcom_esp32_util_write_register(uint32_t regAddr, uint32_t regValue);
  char *hcom_esp32_util_convert_esp32_cmd_to_string(uint8_t cmd);
  int hcom_esp32_util_hardware_restart(void);

#endif // __CONFIGS_MEADOW_SRC_HCOM_ESP32_COMMON__H
