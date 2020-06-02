############################################################################
# configs/stm32f777zit6-meadow/src/Makefile
#
#   Copyright (C) 2015 Gregory Nutt. All rights reserved.
#   Author: Gregory Nutt <gnutt@nuttx.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name NuttX nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

-include $(TOPDIR)/Make.defs

ASRCS =
CSRCS = stm32_boot.c stm32_spi.c stm32_extmem.c stm32_i2c.c \
 meadow-upd.c meadow-upd-interrupt.c

ifeq ($(CONFIG_ARCH_LEDS),y)
CSRCS += stm32_autoleds.c
else
CSRCS += stm32_userleds.c
endif

ifeq ($(CONFIG_ARCH_BUTTONS),y)
CSRCS +=  stm32_buttons.c
endif

ifeq ($(CONFIG_LIB_BOARDCTL),y)
CSRCS += stm32_appinitialize.c
endif

ifeq ($(CONFIG_ARCH_FPU),y)
CSRCS += stm32_ostest.c
endif

ifeq ($(CONFIG_PWM),y)
CSRCS += stm32_pwm.c
endif

ifeq ($(CONFIG_SPORADIC_INSTRUMENTATION),y)
CSRCS += stm32_sporadic.c
endif

ifeq ($(CONFIG_STM32F7_OTGFS),y)
CSRCS += stm32_usb.c
endif

ifeq ($(CONFIG_USBMSC),y)
CSRCS += stm32_usbmsc.c
endif

ifeq ($(CONFIG_ARCH_IDLE_CUSTOM),y)
CSRCS += stm32_idle.c
endif

ifeq ($(CONFIG_MEADOW_HCOM),y)
HCOM_SRCS = hcom/hcom_startup_manager.c \
 hcom/comms/hcom_comms_send.c \
 hcom/comms/hcom_comms_receive.c \
 hcom/comms/hcom_comms_support.c \
 hcom/commands/hcom_save_parse_request.c \
 hcom/commands/hcom_file_commands.c \
 hcom/commands/hcom_exec_rqst_flash_fs.c \
 hcom/commands/hcom_exec_rqst_misc.c \
 hcom/commands/hcom_exec_rqst_download.c \
 hcom/hcom_common_utils.c \
 hcom/hcom_mono_pipe_interface.c \
 hcom/hcom_remote_debugging.c \
 hcom/hcom_host_trace_ramlog.c
 
ifeq ($(CONFIG_HCOM_ESP32_COMMS),y)
 HCOM_SRCS += hcom/esp32/hcom_esp32_uart_comms.c \
 hcom/esp32/hcom_esp32_proc_xmit.c \
 hcom/esp32/hcom_esp32_proc_recv.c \
 hcom/esp32/hcom_esp32_execute.c \
 hcom/esp32/hcom_esp32_proc_util.c
endif

ifeq ($(CONFIG_HCOM_MTD_STRESS_TEST),y)
HCOM_SRCS += hcom/commands/hcom_exec_rqst_testing.c
endif

HCOM_SRCS += hcom/fs/hcom_fs.c \

ifeq ($(CONFIG_FS_SMARTFS),y)
HCOM_SRCS += hcom/fs/hcom_fs_smartfs.c
endif

ifeq ($(CONFIG_FS_LITTLEFS),y)
HCOM_SRCS += hcom/fs/hcom_fs_littlefs.c
endif

CSRCS += $(HCOM_SRCS)
endif

ifeq ($(CONFIG_MEADOW_ESPCP_MANAGER),y)
CSRCS += espcp/espcp_thread.c \
		 espcp/espcp_queue.c \
		 espcp/espcp_encoders.c \
		 espcp/espcp_message.c \
		 espcp/espcp_message_dispatcher.c \
		 espcp/generic_list.c \
		 espcp/espcp_coprocessor.c \
		 espcp/espcp_posix.c \
		 espcp/espcp_wifi.c \
		 espcp/espcp_system.c \
		 espcp/espcp_common.c
endif

include $(TOPDIR)/configs/Board.mk
