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
 meadow-upd.c meadow-upd-interrupt.c misc/meadow_cirbuf.c \
 meadow_hw_version.c misc/parse_iso8601_time.c

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

ifeq ($(CONFIG_STM32F7_SDMMC2),y)
CSRCS += meadow_sdmmc.c
endif

ifeq ($(CONFIG_MEADOW_HCOM),y)
CSRCS += hcom_nx/hcom_nx_upd.c \
hcom_nx/hcom_nx_startup_mgr.c \
hcom_nx/hcom_nx_common_utils.c \
hcom_nx/hcom_nx_bbreg_access.c \
hcom_nx/hcom_nx_upd_route_cmd.c \
hcom_nx/hcom_nx_config_manager.c \
hcom_nx/commands/hcom_nx_ex_flash.c \
hcom_nx/create_fs/hcom_nx_fs.c \
hcom_nx/create_fs/hcom_nx_fs_littlefs.c \
hcom_nx/diag/hcom_nx_route_text_to_host.c \
hcom_nx/diag/hcom_nx_trace_msg_proc.c \
hcom_nx/tests/hcom_nx_developer_3_tests.c \
hcom_nx/tests/hcom_nx_qspi_flash_tests.c \
hcom_nx/tests/hcom_nx_sdcard_tests.c \
hcom_nx/tests/hcom_nx_iso8601_time_tests.c

ifeq ($(CONFIG_MEADOW_PWR_MGMT_SUPPORT),y)
CSRCS += pwrmgmt/pwrmgmt_calib_lsi_clock.c \
pwrmgmt/pwrmgmt_config_wakeup_timer.c \
pwrmgmt/pwrmgmt_control.c \
pwrmgmt/pwrmgmt_enter_stop_mode.c \
pwrmgmt/pwrmgmt_misc.c \
pwrmgmt/pwrmgmt_switch_rtc_clock.c \
pwrmgmt/pwrmgmt_time_support.c \
hcom_nx/tests/hcom_nx_pwr_mgmt_tests.c
endif

ifeq ($(CONFIG_MEADOW_TIMER_SUPPORT),y)
CSRCS += timers/timer_manager.c \
timers/cpu_measure.c \
timers/pulse_width.c \
timers/freq_duty.c \
timers/rc_servo_decode.c \
timers/lsi_clk_freq_measure.c
endif

CSRCS += ethernet/meadow_ethnet_start.c \
ethernet/meadow_ethnet_utils.c \
ethernet/meadow_ethnet_monitor.c \
ethernet/meadow_ethnet_dhcp.c \
ethernet/meadow_ethnet_ping.c

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
		 espcp/espcp_common.c \
		 espcp/espcp_usrsock_sockif.c \
		 espcp/espcp_event_handlers.c \
		 espcp/espcp_tests.c
endif

#
#	Add the NTP client.
#
CSRCS += ntpclient/ntpclient.c misc/long_period_scheduler.c

#
#	The libyaml library is required by cyaml.
#
CSRCS += libyaml/api.c libyaml/dumper.c libyaml/emitter.c libyaml/loader.c \
         libyaml/parser.c libyaml/reader.c libyaml/scanner.c libyaml//writer.c
#
#	The cyaml library maps the items from the libyaml output to the structures used by the application.
#
CSRCS += libcyaml/mem.c libcyaml/free.c libcyaml/load.c libcyaml/save.c libcyaml/util.c libcyaml/utf8.c

endif

include $(TOPDIR)/configs/Board.mk
