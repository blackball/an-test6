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
include $(COMMON)/makefile.cfitsio
include $(COMMON)/makefile.libkd

LDFLAGS := $(LDFLAGS_DEF)
LDFLAGS += -lm

CFLAGS := $(CFLAGS_DEF)

# check out:
#   __attribute__ ((nothrow))
#   __attribute__ ((nonnull))
#   -fforce-addr
#   -fprefetch-loop-arrays

CFLAGS += $(QFITS_INC)
CFLAGS += $(CFITS_INC)
CFLAGS += $(LIBKD_INC)
CFLAGS += $(ANUTILS_INC)

QFITS_UTILS := fitsgetext subtable tabsort

CFITS_UTILS := tablist modhead fitscopy tabmerge fitstomatlab liststruc listhead

FITS_UTILS := $(QFITS_UTILS) $(CFITS_UTILS)

MISC_EXECS := ppmnormrgb whitebalance 2masstofits 

NETPBM_I :=
NETPBM_L := -L. -lnetpbm

#CFLAGS += $(NETPBM_I)
#LFLAGS += $(NETPBM_L)

UTIL_OBJS := ioutils.o fileutil.o boilerplate.o svn.o

OTHER_OBJS := usnob.o usnob_fits.o tycho2.o tycho2_fits.o \
	bt.o an_catalog.o catalog.o idfile.o starkd.o quadfile.o codefile.o \
	rdlist.o xylist.o histogram.o tic.o permutedsort.o codekd.o \
	donuts.o verify.o dualtree_max.o \
	solver.o matchfile.o solvedclient.o solvedfile.o hitlist.o handlehits.o \
	intmap.o qidxfile.o pnpoly.o blind_wcs.o

2MASSOBJ := 2masstofits.o 2mass_catalog.o 2mass.o

PIPELINE := usnobtofits tycho2tofits build-an-catalog hpowned cut-an startree \
	hpquads codetree unpermute-quads unpermute-stars homer blind agreeable \
	solvedserver printsolved xylist2fits printmatch

PROSPECTUS := plotcat quadidx codeprojections quadlocations \
	quadscales quadsperstar
# HACK - codedensity is broken at the moment (and also not used)...
# codedensity 

# old and miscellaneous executables that aren't part of the pipeline.
OLDEXECS := whynot certifiable findquad findstar matchstats plotmatch \
	plotquads matchtordls tunematch overlapstats render_mercator \
	make-merctree rawstartree 

OLDEXECS_OBJS := starkd.o catalog.o qidxfile.o histogram.o rdlist.o \
	xylist.o verify.o intmap.o tic.o matchfile.o \
	quadfile.o an_catalog.o merctree.o codekd.o idfile.o \
	$(UTIL_OBJS)

PIPELINE_MAIN_OBJ := $(addsuffix .o,$(PIPELINE))
PROSPECTUS_MAIN_OBJ := $(addsuffix .o,$(PROSPECTUS))
FITS_UTILS_MAIN_OBJ := $(addsuffix .o,$(FITS_UTILS))

ALL_OBJ := $(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_OBJ) \
	$(PIPELINE_MAIN_OBJ) $(PROSPECTUS_MAIN_OBJ) $(FITS_UTILS_MAIN_OBJ) \
	$(OTHER_OBJS)
ALL_OBJ += $(2MASSOBJ)

all: $(QFITS_LIB) $(CFITS_LIB) $(LIBKD_LIB) \
	$(REMAKE_LIBKD) \
	$(PIPELINE) $(PROSPECTUS) $(FITS_UTILS)

.PHONY: pipeline
pipeline: $(PIPELINE)

.PHONY: prospectus_progs
prospectus_progs: $(PROSPECTUS)

.PHONY: fitsutils
fitsutils: $(FITS_UTILS)

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

rdlstohealpix: rdlstohealpix.o rdlist.o xylist.o ioutils.o \
		$(ANUTILS_LIB) $(QFITS_LIB)

xylist2fits: xylist2fits.o xylist.o ioutils.o \
		$(QFITS_LIB) $(ANUTILS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) 

.PHONY: tags
tags:
	etags `find . -name "*.h" -o -name "*.c"`

#homer2: homer2.o label.o xylist.o fitsioutils.o ioutils.o bl.o $(QFITS_LIB)
#	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^ -lm

#badfields: badfields.o solvedfile.o bl.o 
#	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^

#badfields.o: badfields.c badfields.inc
#	$(CC) $(CFLAGS) -c $< -o $@

#badfields.inc: opBadfields.par badfields.awk
#	awk -f badfields.awk $< > $@

WCSLIB_LIB ?= /usr/local/wcslib-4.2/lib/libwcs.a
WCSLIB_INC ?= /usr/local/wcslib-4.2/include/wcslib
WCSLIB_LIB := $(WCSLIB_LIB)
WCSLIB_INC := $(WCSLIB_INC)

wcscheck.o: wcscheck.c
	$(CC) $(CFLAGS) -c $< -o $@ -I$(WCSLIB_INC)

wcscheck: wcscheck.o xylist.o rdlist.o \
		$(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(WCSLIB_LIB)

noisesim: noisesim.o noise.o $(ANUTILS_LIB)

noisesim2: noisesim2.o noise.o $(ANUTILS_LIB)

noisesim3: noisesim3.o noise.o $(ANUTILS_LIB)

hpgrid: hpgrid.o $(ANUTILS_LIB)

get-healpix: get-healpix.o $(ANUTILS_LIB)

$(CFITS_UTILS) :: %: %.o $(CFITS_LIB)

$(OLDEXECS) :: %: %.o $(OLDEXECS_OBJS) \
		$(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

catalog_analysis: catalog_analysis.o pnpoly.o $(ANUTILS_LIB)

nomadtofits: nomadtofits.o nomad_fits.o nomad.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

read_nomad: read_nomad.o nomad.o

2masstofits: $(2MASSOBJ) $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lz

usnobtofits: usnobtofits.o usnob_fits.o usnob.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

tycho2tofits: tycho2tofits.o tycho2_fits.o tycho2.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

hpowned: hpowned.o boilerplate.o svn.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

build-an-catalog: build-an-catalog.o an_catalog.o usnob_fits.o tycho2_fits.o \
		tycho2.o usnob.o 2mass_catalog.o 2mass.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

cut-an: cut-an.o an_catalog.o catalog.o idfile.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

startree: startree.o starkd.o catalog.o $(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

hpquads: hpquads.o starkd.o quadfile.o codefile.o bt.o rdlist.o xylist.o \
		histogram.o tic.o permutedsort.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

codetree: codetree.o codekd.o codefile.o $(UTIL_OBJS) \
		$(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

unpermute-stars: unpermute-stars.o starkd.o quadfile.o idfile.o $(UTIL_OBJS) \
		$(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

unpermute-quads: unpermute-quads.o quadfile.o codekd.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

homer: homer.o donuts.o xylist.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

blind: blind.o solver.o starkd.o codekd.o quadfile.o matchfile.o tic.o \
		solvedclient.o solvedfile.o idfile.o verify.o svd.o \
		xylist.o hitlist.o handlehits.o intmap.o pnpoly.o blind_wcs.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

agreeable: agreeable.o matchfile.o hitlist.o handlehits.o \
		solvedfile.o solvedclient.o intmap.o verify.o pnpoly.o \
		blind_wcs.o svd.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

printmatch: printmatch.o matchfile.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

printsolved: printsolved.o solvedfile.o $(UTIL_OBJS) \
		$(ANUTILS_LIB) $(QFITS_LIB) 
	$(CC) $(CFLAGS) -o $@ $^ -lm

solvedserver: solvedserver.o solvedfile.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) -o $@ $^ -lm

plotcat: plotcat.o an_catalog.o catalog.o tycho2_fits.o tycho2.o rdlist.o \
		xylist.o usnob_fits.o usnob.o starkd.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

quadidx: quadidx.o quadfile.o qidxfile.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

codedensity: codedensity.o codekd.o dualtree_max.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

codeprojections: codeprojections.o codefile.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

quadlocations: quadlocations.o quadfile.o catalog.o starkd.o $(UTIL_OBJS) $(ANUTILS_LIB) \
		$(LIBKD_LIB) $(QFITS_LIB)

quadscales: quadscales.o histogram.o starkd.o quadfile.o $(UTIL_OBJS) $(ANUTILS_LIB) \
		$(LIBKD_LIB) $(QFITS_LIB)

quadsperstar: quadsperstar.o qidxfile.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

fitsgetext: fitsgetext.o $(ANUTILS_LIB) $(QFITS_LIB)

subtable: subtable.o $(UTIL_OBJS) $(ANUTILS_LIB) ioutils.o $(QFITS_LIB)

tabsort: tabsort.o $(UTIL_OBJS) $(ANUTILS_LIB) ioutils.o permutedsort.o fileutil.o $(QFITS_LIB)

usnobmap: usnobcount usnobtile sdsstile sdssfieldtile indexquad sdssquad
# indextile    (currently broken)

usnobcount: usnobcount.o 
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

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
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

zoomout: zoomout.o pnmutils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

tycho2tostellarium: tycho2tostellarium.o tycho2_fits.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

DEP_OBJ := $(ALL_OBJ)
DEP_PREREQS := $(QFITS_LIB)

CFLAGS += $(CFITSIO_CFLAGS) $(NETPBM_I)
include $(COMMON)/makefile.deps

allclean: clean
	rm -f *.o *~ *.dep 

clean:
	rm -f $(EXECS) $(EXTRA_EXECS) $(SOLVER_EXECS) $(MISC_EXECS) \
		$(PIPELINE) $(PROSPECTUS) $(DEPS) deps $(FITS_UTILS) $(ALL_OBJ)

