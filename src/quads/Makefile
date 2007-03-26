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
include $(COMMON)/makefile.cfitsio
include $(COMMON)/makefile.libkd
include $(COMMON)/makefile.libtweak
include $(COMMON)/makefile.libwcs

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
CFLAGS += $(ANFILES_INC)
CFLAGS += $(LIBTWEAK_INC)
CFLAGS += $(LIBWCS_INC)

CAIRO_INC := $(shell pkg-config --cflags cairo)
CAIRO_LIB := $(shell pkg-config --libs cairo)

QFITS_UTILS := fitsgetext subtable tabsort

CFITS_UTILS := tablist modhead fitscopy tabmerge fitstomatlab liststruc listhead

FITS_UTILS := $(QFITS_UTILS) $(CFITS_UTILS)

MISC_EXECS := 2masstofits project-usnob diffractionFlag_check \
	fits-guess-scale

#	vossetal2006toan

UTIL_OBJS := fileutil.o boilerplate.o svn.o

OTHER_OBJS := usnob.o usnob_fits.o tycho2.o tycho2_fits.o \
	bt.o catalog.o idfile.o codefile.o \
	histogram.o tic.o codekd.o svd.o \
	donuts.o verify.o dualtree_max.o \
	solver.o matchfile.o solvedclient.o solvedfile.o hitlist.o handlehits.o \
	intmap.o pnpoly.o blind_wcs.o

2MASSOBJ := 2masstofits.o 2mass_catalog.o 2mass.o

PIPELINE := usnobtofits tycho2tofits build-an-catalog hpowned cut-an \
	startree hpquads codetree unpermute-quads unpermute-stars homer blind \
	agreeable solvedserver printsolved xylist2fits printmatch mergesolved \
	rdlsinfo xylsinfo wcsinfo

PROSPECTUS := plotcat quadidx codeprojections quadlocations \
	quadscales quadsperstar plotxy wcscheck wcs-xy2rd wcs-rd2xy plotxy2 \
	plotquad
# HACK - codedensity is broken at the moment (and also not used)...
# codedensity 

# old and miscellaneous executables that aren't part of the pipeline.
OLDEXECS := whynot certifiable findquad findstar matchstats plotmatch \
	plotquads matchtordls tunematch overlapstats rawstartree

OLDEXECS_OBJS := catalog.o histogram.o \
	verify.o intmap.o tic.o matchfile.o codekd.o idfile.o \
	$(UTIL_OBJS)

PIPELINE_MAIN_OBJ := $(addsuffix .o,$(PIPELINE))
PROSPECTUS_MAIN_OBJ := $(addsuffix .o,$(PROSPECTUS))
FITS_UTILS_MAIN_OBJ := $(addsuffix .o,$(FITS_UTILS))

ALL_OBJ := $(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_OBJ) \
	$(PIPELINE_MAIN_OBJ) $(PROSPECTUS_MAIN_OBJ) $(FITS_UTILS_MAIN_OBJ) \
	$(OTHER_OBJS)
ALL_OBJ += $(2MASSOBJ)

all: $(QFITS_LIB) $(CFITS_LIB) $(LIBKD_LIB) \
	$(REMAKE_LIBKD) $(REMAKE_ANUTILS) $(REMAKE_ANFILES) $(REMAKE_LIBTWEAK) \
	$(PIPELINE) $(PROSPECTUS) $(FITS_UTILS) fitsverify

.PHONY: pipeline
pipeline: $(PIPELINE)

.PHONY: prospectus_progs
prospectus_progs: $(PROSPECTUS)

.PHONY: fitsutils
fitsutils: $(FITS_UTILS)

rdlstohealpix: rdlstohealpix.o \
		$(ANUTILS_LIB) $(QFITS_LIB)

xylist2fits: xylist2fits.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC)  $(CFLAGS) $^ -o $@ $(LDFLAGS) 

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

an-fitstopnm: an-fitstopnm.o \
		$(ANUTILS_LIB) $(QFITS_LIB)

fits-guess-scale: fits-guess-scale.o \
		$(LIBTWEAK_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

vossetal2006toan: vossetal2006toan.o boilerplate.o svn.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

wcscheck: wcscheck.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB) $(LIBWCS_LIB)

wcs-xy2rd: wcs-xy2rd.o $(LIBTWEAK_LIB) $(UTIL_OBJS) \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

wcs-rd2xy: wcs-rd2xy.o $(LIBTWEAK_LIB) $(UTIL_OBJS) \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

noisesim: noisesim.o noise.o $(ANUTILS_LIB)

noisesim2: noisesim2.o noise.o $(ANUTILS_LIB)

noisesim3: noisesim3.o noise.o $(ANUTILS_LIB)

noisesim4: noisesim4.o blind_wcs.o noise.o svd.o \
		$(UTIL_OBJS) $(LIBTWEAK_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

hpgrid: hpgrid.o $(ANUTILS_LIB)

get-healpix: get-healpix.o $(ANUTILS_LIB)

$(CFITS_UTILS) :: %: %.o $(CFITS_LIB)

fitsverify: ftverify.c fvrf_data.c fvrf_file.c fvrf_head.c fvrf_key.c fvrf_misc.c $(CFITS_LIB)
	$(CC) -DSTANDALONE -trigraphs $(CFITS_INC) -o $@ $^ -lm

$(OLDEXECS) :: %: %.o $(OLDEXECS_OBJS) \
		$(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

catalog_analysis: catalog_analysis.o pnpoly.o $(ANUTILS_LIB)

nomadtofits: nomadtofits.o nomad_fits.o nomad.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

read_nomad: read_nomad.o nomad.o

2masstofits: $(2MASSOBJ) $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lz

diffractionFlag_check: diffractionFlag_check.o usnob_fits.o usnob.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

project-usnob: project-usnob.o usnob_fits.o usnob.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

usnobtofits: usnobtofits.o usnob_fits.o usnob.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

tycho2tofits: tycho2tofits.o tycho2_fits.o tycho2.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

hpowned: hpowned.o boilerplate.o svn.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

build-an-catalog: build-an-catalog.o usnob_fits.o tycho2_fits.o \
		tycho2.o usnob.o 2mass_catalog.o 2mass.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

cut-an: cut-an.o catalog.o idfile.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

startree: startree.o catalog.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

hpquads: hpquads.o codefile.o bt.o histogram.o tic.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

codetree: codetree.o codekd.o codefile.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

unpermute-stars: unpermute-stars.o idfile.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

unpermute-quads: unpermute-quads.o codekd.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

homer: homer.o donuts.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

blind: blind.o solver.o codekd.o matchfile.o tic.o \
		solvedclient.o solvedfile.o idfile.o verify.o svd.o \
		hitlist.o handlehits.o intmap.o pnpoly.o blind_wcs.o \
		$(UTIL_OBJS) \
		$(LIBTWEAK_LIB) $(LEVMAR_LIB) $(LIBWCS_LIB) $(LIBFORTRAN) \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

agreeable: agreeable.o matchfile.o hitlist.o handlehits.o \
		solvedfile.o solvedclient.o intmap.o verify.o pnpoly.o \
		blind_wcs.o svd.o \
		$(LIBTWEAK_LIB) $(UTIL_OBJS) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

rdlsinfo: rdlsinfo.o $(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

xylsinfo: xylsinfo.o $(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

wcsinfo: wcsinfo.o $(UTIL_OBJS) $(LIBTWEAK_LIB) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

printmatch: printmatch.o matchfile.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

printsolved: printsolved.o solvedfile.o $(UTIL_OBJS) \
		$(ANUTILS_LIB) $(QFITS_LIB) 

mergesolved: mergesolved.o solvedfile.o $(UTIL_OBJS) \
		$(ANUTILS_LIB) $(QFITS_LIB) 

solvedserver: solvedserver.o solvedfile.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) -o $@ $^ -lm

plotcat: plotcat.o catalog.o tycho2_fits.o tycho2.o \
		usnob_fits.o usnob.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

plotxy: plotxy.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

plotquad.o: plotquad.c
	$(CC) $(CFLAGS) $(CAIRO_INC) -c -o $@ $<

plotquad: plotquad.o boilerplate.o svn.o $(ANUTILS_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(QFITS_LIB) $(CAIRO_LIB)

plotxy2.o: plotxy2.c
	$(CC) $(CFLAGS) $(CAIRO_INC) -c -o $@ $<

plotxy2: plotxy2.o boilerplate.o svn.o $(ANFILES_LIB) $(ANUTILS_LIB)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(QFITS_LIB) $(CAIRO_LIB)

quadidx: quadidx.o $(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

codedensity: codedensity.o codekd.o dualtree_max.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

codeprojections: codeprojections.o codefile.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

quadlocations: quadlocations.o catalog.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

quadscales: quadscales.o histogram.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

quadsperstar: quadsperstar.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

fitsgetext: fitsgetext.o $(ANUTILS_LIB) $(QFITS_LIB)

subtable: subtable.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

tabsort: tabsort.o $(UTIL_OBJS) $(ANUTILS_LIB) fileutil.o $(QFITS_LIB)

tycho2tostellarium: tycho2tostellarium.o tycho2_fits.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

DEP_OBJ := $(ALL_OBJ)
DEP_PREREQS := $(QFITS_LIB)

CFLAGS += $(CFITSIO_CFLAGS)
include $(COMMON)/makefile.deps

allclean: clean
	rm -f *.o *~ *.dep 

clean:
	rm -f $(EXECS) $(EXTRA_EXECS) $(SOLVER_EXECS) $(MISC_EXECS) \
		$(PIPELINE) $(PROSPECTUS) $(DEPS) deps $(FITS_UTILS) $(ALL_OBJ)

