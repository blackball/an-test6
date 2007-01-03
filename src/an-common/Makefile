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
COMMON := .

all:

include $(COMMON)/makefile.common
#include $(COMMON)/makefile.utils
#include $(COMMON)/makefile.anfiles
include $(COMMON)/makefile.qfits
include $(COMMON)/makefile.libkd

CFLAGS := $(CFLAGS_DEF)
CFLAGS += $(QFITS_INC)
CFLAGS += $(LIBKD_INC)

ANFILES_LIB := libanfiles.a
ANUTILS_LIB := libanutils.a

all: $(ANUTILS_LIB) $(ANFILES_LIB)

ANUTILS_OBJ := fitsioutils.o starutil.o mathutil.o bl.o healpix.o \
	dualtree.o dualtree_rangesearch.o permutedsort.o ioutils.o

ANFILES_OBJ := starkd.o rdlist.o xylist.o an_catalog.o

$(ANUTILS_LIB): $(ANUTILS_OBJ)
	$(AR) rc $@ $^

$(ANFILES_LIB): $(ANFILES_OBJ)
	$(AR) rc $@ $^

DEP_OBJ := $(ANUTILS_OBJ) $(ANFILES_OBJ)
include $(COMMON)/makefile.deps

clean:
	rm -f $(ANUTILS_LIB) $(ANUTILS_OBJ) $(ANFILES_LIB) $(ANFILES_OBJ) \
		$(DEPS) deps
