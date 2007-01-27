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
include $(COMMON)/makefile.anfiles
include $(COMMON)/makefile.qfits
include $(COMMON)/makefile.libkd

LDFLAGS := $(LDFLAGS_DEF)
LDFLAGS += -lm

CFLAGS := $(CFLAGS_DEF)

CFLAGS += $(QFITS_INC)
CFLAGS += $(LIBKD_INC)
CFLAGS += $(ANUTILS_INC)
CFLAGS += $(ANFILES_INC)

MISC_EXECS := ppmnormrgb whitebalance zoomout

USNOBMAP_EXECS := usnobcount usnobtile sdsstile sdssfieldtile \
	indexquad sdssquad render-mercator make-merctree rdlstile \
	indextile

USNOBMAP_MAIN_OBJS := $(addsuffix .o,$(USNOBMAP_EXECS))
MISC_MAIN_OBJS := $(addsuffix .o,$(MISC_EXECS))

NETPBM_I :=
NETPBM_L := -L. -lnetpbm

#CFLAGS += $(NETPBM_I)
#LFLAGS += $(NETPBM_L)

all: $(USNOBMAP_EXECS) $(MISC_EXECS)

rdlstile: rdlstile.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

render-mercator: render-mercator.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

make-merctree: make-merctree.o merctree.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

usnobcount: usnobcount.o 

usnobtile: usnobtile.o merctree.o \
		$(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

sdsstile: sdsstile.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

sdssfieldtile: sdssfieldtile.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

indextile: indextile.o \
		$(ANFILES_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

indexquad: indexquad.o \
		$(ANFILES_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

sdssquad: sdssquad.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

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

ALL_OBJS := $(KDTREE_OBJS) $(USNOBMAP_MAIN_OBJS) $(MISC_MAIN_OBJS)

DEP_OBJ := $(ALL_OBJS)
DEP_PREREQS := $(QFITS_LIB)

CFLAGS += $(NETPBM_I)
include $(COMMON)/makefile.deps

allclean: clean
	rm -f *.o *~ *.dep 

clean:
	rm -f $(USNOBMAP_EXECS) $(MISC_EXECS) $(ALL_OBJS) \
		$(DEPS) deps


