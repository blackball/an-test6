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

CFLAGS := $(CFLAGS_DEF)
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

ANUTILS_OBJ := starutil.o mathutil.o bl.o healpix.o permutedsort.o ioutils.o
ifndef NO_QFITS
ANUTILS_OBJ := $(ANUTILS_OBJ) fitsioutils.o
endif
ifndef NO_KDTREE
ANUTILS_OBJ := $(ANUTILS_OBJ) dualtree.o dualtree_rangesearch.o
endif

ANFILES_OBJ := 
ifndef NO_QFITS
ANFILES_OBJ := $(ANFILES_OBJ) starkd.o rdlist.o xylist.o an_catalog.o \
	qidxfile.o quadfile.o
endif

$(ANUTILS_LIB): $(ANUTILS_OBJ)
	$(AR) rc $@ $^
	$(RANLIB) $@

$(ANFILES_LIB): $(ANFILES_OBJ)
	$(AR) rc $@ $^
	$(RANLIB) $@

DEP_OBJ := $(ANUTILS_OBJ) $(ANFILES_OBJ)
include $(COMMON)/makefile.deps

clean:
	rm -f $(ANUTILS_LIB) $(ANUTILS_OBJ) $(ANFILES_LIB) $(ANFILES_OBJ) \
		$(DEPS) deps
