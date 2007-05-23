#  This file is part of libkd.
#  Copyright 2006, 2007 Dustin Lang and Keir Mierle.
#
#  libkd is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, version 2.
#
#  libkd is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with libkd; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

BASEDIR := ..
COMMON := $(BASEDIR)/an-common

all:

include $(COMMON)/makefile.common
include $(COMMON)/makefile.utils
include $(COMMON)/makefile.qfits

INTERNALS := \
	kdint_ddd.o \
	kdint_fff.o \
	kdint_ddu.o \
	kdint_duu.o \
	kdint_dds.o \
	kdint_dss.o

KD := kdtree.o kdtree_dim.o $(INTERNALS)
KD_FITS := kdtree_fits_io.o

CFLAGS := $(CFLAGS_DEF)
# CFLAGS += -DKDTREE_NO_FITS
CFLAGS += $(QFITS_INC)
CFLAGS += $(ANUTILS_INC)

LDFLAGS := $(LDFLAGS_DEF)
LDFLAGS += -lm

LIBKD := libkd.a

all: $(LIBKD) checktree

$(INTERNALS): kdtree_internal.c kdtree_internal_fits.c

$(LIBKD): $(KD) $(KD_FITS)
	$(AR) rc $@ $^
	$(RANLIB) $@

checktree: checktree.o $(LIBKD) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ -lm

.PHONY: clean
clean:
	-rm $(LIBKD) checktree $(KD) $(KD_FITS) deps $(DEPS) \
		checktree.o

realclean: clean
	-rm *~ *.dep

.PHONY: tags
tags:
	etags `find . -name "*.c" -o -name "*.h"`

DEP_OBJ := $(KD)

include $(COMMON)/makefile.deps
