# This file is part of the Astrometry.net suite.
# Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.
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

all:

include $(COMMON)/makefile.common
include $(COMMON)/makefile.utils
include $(COMMON)/makefile.libwcs
include $(COMMON)/makefile.libkd
include $(COMMON)/makefile.cfitsio
include $(COMMON)/makefile.qfits

TWEAK_C_OBJ := ezfits.o sip.o sip_util.o tweak_internal.o
TWEAK_F_OBJ := dgelsd.o
TWEAK_OBJ := $(TWEAK_C_OBJ) $(TWEAK_F_OBJ)

CFLAGS := $(CFLAGS_DEF)
CFLAGS += $(ANUTILS_INC)
CFLAGS += $(LIBKD_INC)
CFLAGS += $(CFITS_INC)
CFLAGS += $(LIBWCS_INC)
CFLAGS += $(QFITS_INC)

LDFLAGS := $(LDFLAGS_DEF)
#LDFLAGS += -lm

LIBTWEAK := libtweak.a

all: $(LIBTWEAK)

$(LIBTWEAK): $(TWEAK_OBJ)
	$(AR) rc $@ $^
	$(RANLIB) $@

.PHONY: clean
clean:
	-rm $(LIBTWEAK) $(TWEAK_OBJ) \
		deps $(DEPS)

realclean: clean
	-rm *~ *.dep

.PHONY: tags
tags:
	etags `find . -name "*.c" -o -name "*.h"`

DEP_OBJ := $(TWEAK_C_OBJ)

include $(COMMON)/makefile.deps
