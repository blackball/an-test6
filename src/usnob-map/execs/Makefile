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

BASEDIR := .
COMMON := an-common

.PHONY: all
all:

include $(COMMON)/makefile.common
include $(COMMON)/makefile.utils
include $(COMMON)/makefile.qfits
include $(COMMON)/makefile.libkd

LDFLAGS := $(LDFLAGS_DEF)
LDFLAGS += -lm

CFLAGS := $(CFLAGS_DEF)

CFLAGS += $(QFITS_INC)
CFLAGS += $(LIBKD_INC)
CFLAGS += $(ANUTILS_INC)

MISC_EXECS := ppmnormrgb whitebalance

USNOBMAP_EXECS := usnobcount usnobtile sdsstile sdssfieldtile \
	indexquad sdssquad
# indextile    (currently broken)

#UTIL_OBJS := ioutils.o fileutil.o boilerplate.o svn.o

USNOBMAP_MAIN_OBJS := $(addsuffix .o,$(USNOBMAP_EXECS))

NETPBM_I :=
NETPBM_L := -L. -lnetpbm

#CFLAGS += $(NETPBM_I)
#LFLAGS += $(NETPBM_L)

all: $(USNOBMAP_EXECS)

usnobcount: usnobcount.o 
#	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

usnobtile: usnobtile.o merctree.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

sdsstile: sdsstile.o rdlist.o xylist.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

sdssfieldtile: sdssfieldtile.o xylist.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

indextile: indextile.o starkd.o \
		$(LIBKD_LIB) $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

indexquad: indexquad.o starkd.o permutedsort.o \
		$(LIBKD_LIB) $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

sdssquad: sdssquad.o rdlist.o xylist.o permutedsort.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
#	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

zoomout: zoomout.o pnmutils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

ppmnormrgb: ppmnormrgb.o
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^ $(NETPBM_L) -lm

ppmnormrgb.o: ppmnormrgb.c
	$(CC) $(CFLAGS) -c $< -o $@ $(NETPBM_I)

whitebalance: whitebalance.o pnmutils.o
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^ $(NETPBM_L) -lm

whitebalance.o: whitebalance.c
	$(CC) $(CFLAGS) -c $< -o $@ $(NETPBM_I)

pnmutils.o: pnmutils.c
	$(CC) $(CFLAGS) -c $< -o $@ $(NETPBM_I)


.PHONY: tags
tags:
	etags `find . -name "*.h" -o -name "*.c"`

ALL_OBJS := $(UTIL_OBJS) $(KDTREE_OBJS) $(USNOBMAP_MAIN_OBJS)

DEP_OBJ := $(ALL_OBJS)
DEP_PREREQS := $(QFITS_LIB)

CFLAGS += $(NETPBM_I)
include $(COMMON)/makefile.deps

allclean: clean
	rm -f *.o *~ *.dep 

clean:
	rm -f $(USNOBMAP_EXECS) $(ALL_OBJS)
		$(DEPS) deps


