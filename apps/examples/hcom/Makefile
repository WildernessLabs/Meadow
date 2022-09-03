############################################################################
# apps/examples/hcom/Makefile
#
#   Copyright (C) 2008, 2010-2013 Gregory Nutt. All rights reserved.
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

# hcom built-in application info

CONFIG_EXAMPLES_HCOM_PRIORITY ?= SCHED_PRIORITY_DEFAULT
CONFIG_EXAMPLES_HCOM_STACKSIZE ?= 2048

APPNAME = hcom

PRIORITY  = $(CONFIG_EXAMPLES_HCOM_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_HCOM_STACKSIZE)

# hcom example

HCOM_DIR = ../../../hcom/
ASRCS =
HCOM_SRCS = comms/hcom_host_encode.c \
	comms/hcom_host_process.c \
	comms/hcom_host_receive.c \
	comms/hcom_host_route.c \
	comms/hcom_host_send.c \
	diag/hcom_diag_gpio.c \
	diag/hcom_diag_logging.c \
	diag/hcom_diag_misc.c \
	diag/hcom_diag_nsh_support.c \
	diag/hcom_diag_decode_protocol.c \
	diag/hcom_diag_trace_to_cli.c \
	diag/hcom_diag_host_text_transport.c \
	file/hcom_file_dnld_esp32.c \
	file/hcom_file_dnld_stm32f7.c \
	file/hcom_file_write_stm32f7.c \
	file/hcom_file_delete_stm32f7.c \
	file/hcom_file_upld_proc.c \
	file/hcom_file_lists.c \
	file/hcom_file_misc.c \
	misc/meadow_cirbuf.c \
	misc/hcom_common_utils.c \
	misc/hcom_config_manager.c \
	mono/hcom_mono_control.c \
	mono/hcom_remote_vs_debug.c \
	mono/hcom_stdout_redirect.c \
	mono/hcom_stderr_redirect.c \
	nx_rqsts/hcom_bbreg_access.c \
	nx_rqsts/hcom_misc_requests.c \
	nx_rqsts/hcom_via_nx_access.c \
	tests/bbreg_tests.c \
	tests/developer_tests.c \
	tests/vs_debug_tests.c \
	tests/diag_gpio_tests.c \
	tests/hcom_execute_espcp_tests.c \
	tests/diag_misc_tests.c \
	tests/diag_ethernet_chat_test.c

ifeq ($(CONFIG_HCOM_ESP32_COMMS),y)
 HCOM_SRCS += esp32/hcom_esp32_execute.c \
 esp32/hcom_esp32_proc_recv.c \
 esp32/hcom_esp32_proc_util.c \
 esp32/hcom_esp32_proc_xmit.c \
 esp32/hcom_esp32_uart_comms.c
endif

MAINSRC = hcom_startup_manager.c

CSRCS += $(HCOM_SRCS) 

CONFIG_EXAMPLES_HCOM_PROGNAME ?= hcom$(EXEEXT)
PROGNAME = $(CONFIG_EXAMPLES_HCOM_PROGNAME)

include $(APPDIR)/Application.mk
