# This file is part of the Astrometry.net suite.
# Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.
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

#NO_QFITS := 1
#export NO_QFITS

.PHONY: all
all:

include $(COMMON)/makefile.common
include $(COMMON)/makefile.cfitsio

LDFLAGS := $(LDFLAGS_DEF)
LDFLAGS += -lm

CFLAGS := $(CFLAGS_DEF)
CFLAGS += $(CFITS_INC)
# To get ver.h:
CFLAGS += -I..
# For Keir's fancy Python bindings:
CFLAGS += -fPIC

SHAREDLIBFLAGS := $(SHAREDLIBFLAGS_DEF)

SIMPLEXY := src/dallpeaks.o src/dcen3x3.o src/dfind.o \
	src/dmedsmooth.o src/dobjects.o src/dpeaks.o src/dselip.o \
	src/dsigma.o src/dsmooth.o src/fits2xy.o src/simplexy.o \
	src/radix.o

SIMPLEXY_SHARED := src/simplexy.o src/dsigma.o src/dmedsmooth.o \
	src/dallpeaks.o src/dpeaks.o src/dobjects.o src/dselip.o \
	src/dsmooth.o src/dcen3x3.o src/dfind.o src/radix.o

FIXAXIS := src/fixaxis.o

TEST_DFIND := src/dfind.o src/test_dfind.o

all: $(REMAKE_CFITSIO) simplexy fixaxis

simplexy: fits2xy
.PHONY: simplexy

test_dfind: $(TEST_DFIND)

fits2xy: $(SIMPLEXY) $(CFITS_LIB)
	$(CC) $(LDFLAGS) -o $@ $^

fixaxis: $(FIXAXIS) $(CFITS_LIB)
	$(CC) $(LDFLAGS) -o $@ $^

simplexy_shared: _simplexy.so
.PHONY: simplexy_shared

_simplexy.so: $(SIMPLEXY_SHARED)
	$(CC) $(SHAREDLIBFLAGS) -o $@ $^

clean :
	-rm $(SIMPLEXY) $(FIXAXIS) fits2xy fixaxis _simplexy.so


DEP_OBJ := $(SIMPLEXY) $(SIMPLEXY_SHARED) $(FIXAXIS) $(TEST_DFIND)

include $(COMMON)/makefile.deps
