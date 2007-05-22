# This file is part of the Astrometry.net suite.
# Copyright 2006-2007, Keir Mierle, David W. Hogg, Sam Roweis and Dustin Lang.
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

BASEDIR := ../..
COMMON := $(BASEDIR)/an-common

all:

include $(COMMON)/makefile.common
include $(COMMON)/makefile.anfiles
include $(COMMON)/makefile.utils
include $(COMMON)/makefile.libwcs
include $(COMMON)/makefile.libkd
include $(COMMON)/makefile.cfitsio
include $(COMMON)/makefile.qfits
include $(COMMON)/makefile.gsl

TWEAK_C_OBJ := sip.o tweak_internal.o
TWEAK_C_OBJ += sip_qfits.o
TWEAK_F_OBJ := dgelsd.o
TWEAK_OBJ := $(TWEAK_C_OBJ) $(TWEAK_F_OBJ)

CFLAGS := $(CFLAGS_DEF)
CFLAGS += $(ANUTILS_INC)
CFLAGS += $(LIBKD_INC)
CFLAGS += $(CFITS_INC)
CFLAGS += $(QFITS_INC)
CFLAGS += $(LIBWCS_INC)
CFLAGS += $(GSL_INC)

LDFLAGS := $(LDFLAGS_DEF)

LIBTWEAK := libtweak.a

all: $(LIBTWEAK)

$(LIBTWEAK): $(TWEAK_OBJ)
	$(AR) rc $@ $^
	$(RANLIB) $@

TWEAK_COMMON := $(COMMON)/starutil.o
TWEAK_COMMON += $(COMMON)/mathutil.o
TWEAK_COMMON += $(COMMON)/bl.o
TWEAK_COMMON += $(COMMON)/dualtree_rangesearch.o
TWEAK_COMMON += $(COMMON)/dualtree.o
TWEAK_COMMON += $(COMMON)/healpix.o
TWEAK_COMMON += $(COMMON)/fitsioutils.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_header.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_table.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_tools.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_rw.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_card.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_memory.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_error.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_byteswap.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_time.o
TWEAK_COMMON += $(QFITS_DIR)/src/md5.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_md5.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_filename.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_float.o
TWEAK_COMMON += $(QFITS_DIR)/src/qfits_cache.o

libtweak.so: $(TWEAK_OBJ) $(TWEAK_COMMON) $(COMMON_LIB) $(LIBKD_LIB)
	gcc -shared -ff2c -o $@ $^

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
DEP_PREREQS := $(QFITS_LIB)

include $(COMMON)/makefile.deps
