include $(top_srcdir)/mk/common.mk

SUBDIRS = .

noinst_LTLIBRARIES = libmono-mbedtls.la

AM_CFLAGS = $(WERROR_CFLAGS)

libmono_mbedtls_la_SOURCES = \
	mono-mbedtls.c

CFLAGS := $(filter-out @CXX_REMOVE_CFLAGS@, @CFLAGS@) @CXX_ADD_CFLAGS@

AM_CFLAGS = -I$(srcdir) -I$(srcdir)/../../../mbedtls/include -I$(srcdir)/.. -I$(srcdir)/../eglib

MAINTAINERCLEANFILES = Makefile.in
