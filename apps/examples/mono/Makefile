############################################################################
# apps/examples/mono/Makefile
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

# mono built-in application info

CONFIG_EXAMPLES_MONO_PRIORITY ?= SCHED_PRIORITY_DEFAULT
CONFIG_EXAMPLES_MONO_STACKSIZE ?= 2048

APPNAME = mono
PRIORITY = $(CONFIG_EXAMPLES_MONO_PRIORITY)
STACKSIZE = $(CONFIG_EXAMPLES_MONO_STACKSIZE)

# mono example

MONO_DIR = ../../../mono/
ASRCS =
CSRCS = symtab.c mono_nuttx_stubs.c
MAINSRC = mono_main.c

# ---------- Upstream System.Native PAL (from dotnet/runtime) ----------
# These compile the real .NET PAL implementations for NuttX instead of
# hand-writing stubs. Only the subset needed for threading/time/memory/etc.

RUNTIME_NATIVE = ../../../../runtime/src/native
PAL_SYSN_DIR   = $(RUNTIME_NATIVE)/libs/System.Native
PAL_COMMON_DIR = $(RUNTIME_NATIVE)/libs/Common
PAL_MINIPAL_DIR = $(RUNTIME_NATIVE)/minipal

# Add upstream PAL sources to VPATH so make can find them
VPATH += $(PAL_SYSN_DIR)

# PAL source files (basename only — VPATH resolves the directory)
CSRCS += pal_threading.c pal_time.c pal_memory.c pal_random.c
CSRCS += pal_errno.c pal_string.c pal_runtimeinformation.c pal_log.c
CSRCS += pal_datetime.c

# Include paths for PAL headers
CFLAGS += -I$(PAL_SYSN_DIR) -I$(PAL_COMMON_DIR) -I$(RUNTIME_NATIVE)
# -D flags needed by PAL code
CFLAGS += -DTARGET_UNIX=1 -D__NuttX__=1

CONFIG_EXAMPLES_MONO_PROGNAME ?= mono$(EXEEXT)
PROGNAME = $(CONFIG_EXAMPLES_MONO_PROGNAME)

include $(APPDIR)/Application.mk
