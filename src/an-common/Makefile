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
COMMON := .

all:

# You can build stripped-down versions of the AN libraries that do not
# depend on libkd or libqfits by defining the following in your Makefile
# before calling make in the an-common directory:
#   NO_KDTREE := 1
#   export NO_KDTREE
#   NO_QFITS := 1
#   export NO_QFITS

include $(COMMON)/makefile.common
ifndef NO_QFITS
include $(COMMON)/makefile.qfits
endif
ifndef NO_KDTREE
include $(COMMON)/makefile.libkd
endif

include $(COMMON)/makefile.cairo
include $(COMMON)/makefile.jpeg
include $(COMMON)/makefile.netpbm

CFLAGS += $(CFLAGS_DEF)
CFLAGS += $(CAIRO_INC)
CFLAGS += $(NETPBM_INC)
CFLAGS += $(JPEG_INC)

ifndef NO_QFITS
CFLAGS += $(QFITS_INC)
endif
ifndef NO_KDTREE
CFLAGS += $(LIBKD_INC)
endif
CFLAGS += -I.

ANFILES_LIB := libanfiles.a
ANUTILS_LIB := libanutils.a

all: $(ANUTILS_LIB) $(ANFILES_LIB)

# Things that it depends on but which aren't linked in
ANUTILS_DEPS :=

ANUTILS_OBJ := starutil.o mathutil.o bl.o healpix.o permutedsort.o ioutils.o \
	sip.o sip-utils.o md5.o constellations.o
ifndef NO_QFITS
ANUTILS_OBJ += fitsbin.o fitsioutils.o sip_qfits.o
ANUTILS_DEPS += $(QFITS_LIB)
endif
ifndef NO_KDTREE
ANUTILS_OBJ += dualtree.o dualtree_rangesearch.o
endif

# Things that it depends on but which aren't linked in
ANFILES_DEPS :=

ANFILES_OBJ := 
ifndef NO_QFITS
ANFILES_OBJ += starkd.o rdlist.o xylist.o an_catalog.o \
	qidxfile.o quadfile.o
ANFILES_DEPS += $(QFITS_LIB)
endif

MISC_OBJ := grab-stellarium-constellations.o 

ALL_OBJ := $(ANUTILS_OBJ) $(ANFILES_OBJ) $(MISC_OBJ)

$(ANUTILS_LIB): $(ANUTILS_OBJ) $(ANUTILS_DEPS)
	$(AR) rc $@ $(ANUTILS_OBJ)
	$(RANLIB) $@

$(ANFILES_LIB): $(ANFILES_OBJ) $(ANFILES_DEPS)
	$(AR) rc $@ $(ANFILES_OBJ)
	$(RANLIB) $@

DEP_OBJ := $(ANUTILS_OBJ) $(ANFILES_OBJ) $(MISC_OBJ)
ifndef NO_QFITS
DEP_PREREQS := $(QFITS_LIB)
endif
include $(COMMON)/makefile.deps

# This is annoying for people who don't want to install the stellarium 
# data files...
#stellarium-constellations.c: grab-stellarium-constellations
stellarium-constellations.c:
	-./grab-stellarium-constellations > $@.tmp  &&  mv $@.tmp $@

grab-stellarium-constellations: grab-stellarium-constellations.o \
		starutil.o mathutil.o bl.o
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^ -lm

# we'll just tack this on here... It depends on cairo, libjpeg, etc, so it's not part
# of the anX.a libraries.
ifeq ($(MAKECMDGOALS),cairoutils.o)
  include cairoutils.dep
endif
#cairoutils.o: cairoutils.c
#	$(CC) $(CFLAGS) $(NETPBM_INC) $(CAIRO_INC) $(JPEG_INC) $(PNG_INC) -c $< -o $@

clean:
	rm -f $(ANUTILS_LIB) $(ANFILES_LIB) $(ALL_OBJ) \
		$(DEPS) deps cairoutils.o
