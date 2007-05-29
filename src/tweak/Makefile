# This file is part of the Astrometry.net suite.
# Copyright 2006, 2007 Keir Mierle, David W. Hogg, Sam Roweis and Dustin Lang.
#
# The Astrometry.net suite is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation, version 2.
#
# The Astrometry.net suite is distributed in the hope that it will be
# useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the Astrometry.net suite ; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

BASEDIR := ..
COMMON := $(BASEDIR)/an-common

.PHONY: all
all:

include $(COMMON)/makefile.common
include $(COMMON)/makefile.utils
include $(COMMON)/makefile.cfitsio
include $(COMMON)/makefile.qfits
include $(COMMON)/makefile.gsl
include $(COMMON)/makefile.levmar
include $(COMMON)/makefile.libwcs
include $(COMMON)/makefile.libkd
include $(COMMON)/makefile.libtweak

LDFLAGS := $(LDFLAGS_DEF)
LDFLAGS += -lm

CFLAGS := $(CFLAGS_DEF)

CFLAGS += $(CFITS_INC)
CFLAGS += $(QFITS_INC)
CFLAGS += $(GSL_INC)
CFLAGS += $(LEVMAR_INC)
CFLAGS += $(LIBWCS_INC)
CFLAGS += $(LIBKD_INC)
CFLAGS += $(LIBTWEAK_INC)
CFLAGS += $(ANUTILS_INC)
CFLAGS += -I. -g -O0

TWEAK_OBJS := tweak.o ezfits.o sip_util.o
TEST_SIP_OBJS := test_sip.o sip_util.o ezfits.o
SANITY_OBJS := sanity_check.o sip_util.o


all: $(REMAKE_LIBTWEAK) tweak test_sip sanity_check sanity_check2

# tweak.c depends on qfits.h existing, which is installed along with the library in the
# "staging" directory qfits-an/stage.
tweak.c: $(QFITS_LIB)

tweak: $(TWEAK_OBJS) \
	$(LIBTWEAK_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB) $(CFITS_LIB) \
	$(GSL_LIB) $(LEVMAR_LIB) $(LIBWCS_LIB) $(LIBFORTRAN)

sanity_check: $(SANITY_OBJS) \
	$(LIBTWEAK_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB) $(CFITS_LIB) \
	$(LIBWCS_LIB) $(LIBFORTRAN) $(GSL_LIB)

SC2 := sanity_check2.o \
	$(LIBTWEAK_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB) \
	$(LIBFORTRAN) $(GSL_LIB)

sanity_check2: $(REMAKE_LIBTWEAK) $(SC2)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SC2)

test_sip: $(TEST_SIP_OBJS) \
	$(LIBTWEAK_LIB) $(ANUTILS_LIB) $(CFITS_LIB) $(LIBWCS_LIB)

clean :
	-rm tweak test_sip $(TWEAK_OBJS) $(TEST_SIP_OBJS)
