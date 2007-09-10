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

.PHONY: all
all:

include $(COMMON)/makefile.common
include $(COMMON)/makefile.utils
include $(COMMON)/makefile.anfiles
include $(COMMON)/makefile.qfits
include $(COMMON)/makefile.cfitsio
include $(COMMON)/makefile.libkd
include $(COMMON)/makefile.gsl

include $(COMMON)/makefile.cairo

LDFLAGS := $(LDFLAGS_DEF)
LDFLAGS += -lm

CFLAGS := $(CFLAGS_DEF)

CFLAGS += $(QFITS_INC)
CFLAGS += $(CFITS_INC)
CFLAGS += $(LIBKD_INC)
CFLAGS += $(ANUTILS_INC)
CFLAGS += $(ANFILES_INC)
CFLAGS += $(GSL_INC)

# For Keir's fancy Python bindings:
CFLAGS += -fPIC

SHAREDLIBFLAGS := $(SHAREDLIBFLAGS_DEF)

NETPBM_INC :=
NETPBM_LIB := -L. -lnetpbm

PNG_INC := $(shell pkg-config --cflags libpng 2>/dev/null)
PNG_LIB := $(shell pkg-config --libs libpng 2>/dev/null)

QFITS_UTILS := fitsgetext subtable tabsort column-merge

CFITS_UTILS := tablist modhead fitscopy tabmerge fitstomatlab liststruc \
		listhead imcopy imarith

FITS_UTILS := $(QFITS_UTILS) $(CFITS_UTILS)

MISC_EXECS := 2masstofits nomadtofits project-usnob diffractionFlag_check \
	hpgrid read_nomad tycho2tostellarium

UTIL_OBJS := fileutil.o boilerplate.o svn.o

OTHER_OBJS := usnob.o usnob_fits.o tycho2.o tycho2_fits.o bt.o catalog.o \
	idfile.o codefile.o histogram.o tic.o codekd.o svd.o verify.o \
	solver.o matchfile.o solvedclient.o solvedfile.o intmap.o pnpoly.o \
	blind_wcs.o index.o tweak.o log.o blind-main.o \
	plot-constellations.o constellations.o quadcenters.o startree2rdls.o \
	brightstars.o grab-stellarium-constellations.o cairoutils.o ngcic-accurate.o \
	ngc2000.o noisesim.o noisesim4.o noise.o scriptutils.o

2MASSOBJ := 2masstofits.o 2mass_catalog.o 2mass.o

PIPELINE := usnobtofits tycho2tofits build-an-catalog hpowned cut-an \
	startree hpquads codetree unpermute-quads unpermute-stars blind \
	solvedserver printsolved xylist2fits mergesolved rdlsinfo xylsinfo \
	wcsinfo fits-guess-scale an-fitstopnm agreeable new-wcs get-wcs \
	backend augment-xylist solve-field plotcat wcs-xy2rd wcs-rd2xy \
	certifiable wcs-annotate get-healpix

PROSPECTUS := quadidx codeprojections quadlocations quadscales quadsperstar \
	whynot quadcenters startree2rdls

# old and miscellaneous executables that aren't part of the pipeline.
OLDEXECS := plotquads rawstartree checkquads

CAIROEXECS := plotquad plotxy plot-constellations

OLDEXECS_OBJS := catalog.o histogram.o verify.o intmap.o tic.o matchfile.o \
	codekd.o idfile.o $(UTIL_OBJS)

PIPELINE_MAIN_OBJ := $(addsuffix .o,$(PIPELINE))
PROSPECTUS_MAIN_OBJ := $(addsuffix .o,$(PROSPECTUS))
FITS_UTILS_MAIN_OBJ := $(addsuffix .o,$(FITS_UTILS))

SIMPLEXY_OBJ := dallpeaks.o dcen3x3.o dfind.o dmedsmooth.o dobjects.o dpeaks.o \
	dselip.o dsigma.o dsmooth.o image2xy.o simplexy.o radix.o qsort_reentrant.o

SIMPLEXY_SHARED_OBJ := simplexy.o dsigma.o dmedsmooth.o dallpeaks.o dpeaks.o \
	dobjects.o dselip.o dsmooth.o dcen3x3.o dfind.o radix.o

FIXAXIS_OBJ := fixaxis.o svn.o

TEST_DFIND_OBJ := dfind.o test_dfind.o

IMAGE2XY := image2xy fixaxis test_dfind

INSTALL_EXECS := $(FITS_UTILS) $(PIPELINE) image2xy fits2fits.py image2pnm.py
INSTALL_CAIRO_EXECS := $(CAIROEXECS)

ALL_OBJ := $(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_OBJ) \
	$(PIPELINE_MAIN_OBJ) $(PROSPECTUS_MAIN_OBJ) $(FITS_UTILS_MAIN_OBJ) \
	$(OTHER_OBJS) $(2MASSOBJ) $(SIMPLEXY_OBJ) $(SIMPLEXY_SHARED_OBJ) $(FIXAXIS_OBJ) \
	$(TEST_DFIND_OBJ)

all: $(QFITS_LIB) $(CFITS_LIB) $(LIBKD_LIB) \
	$(REMAKE_LIBKD) $(REMAKE_ANUTILS) $(REMAKE_ANFILES) \
	$(PIPELINE) $(PROSPECTUS) $(IMAGE2XY) $(FITS_UTILS) fitsverify

cairo: $(CAIROEXECS)

.PHONY: pipeline
pipeline: $(PIPELINE)

.PHONY: prospectus_progs
prospectus_progs: $(PROSPECTUS)

.PHONY: fitsutils
fitsutils: $(FITS_UTILS)

simplexy: image2xy
.PHONY: simplexy

install: $(INSTALL_EXECS)
	@echo Installing in directory $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/bin
	@for x in $(INSTALL_EXECS); do \
		echo cp $$x $(INSTALL_DIR)/bin; \
		cp $$x $(INSTALL_DIR)/bin; \
	done
	mkdir -p $(INSTALL_DIR)/etc
	sed "s+add_path /usr/local/astrometry/+add_path $(INSTALL_DIR)/+" < backend.cfg-dist > $(INSTALL_DIR)/etc/backend.cfg

install-extra: $(INSTALL_CAIRO_EXECS)
	@echo Installing in directory $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/bin
	@for x in $(INSTALL_CAIRO_EXECS); do \
		echo cp $$x $(INSTALL_DIR)/bin; \
		cp $$x $(INSTALL_DIR)/bin; \
	done

stellarium-constellations.c: grab-stellarium-constellations
	-./grab-stellarium-constellations > $@.tmp  &&  mv $@.tmp $@

grab-stellarium-constellations: grab-stellarium-constellations.o \
		boilerplate.o svn.o $(ANUTILS_LIB) $(QFITS_LIB)

test-solver: test-solver.o solver_test.o idfile.o blind_wcs.o verify.o log.o \
		matchfile.o tic.o svd.o \
		$(ANFILES_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

NODEP_OBJS += solver_test.o
ALL_OBJ += test-solver.o

solver_test.o: solver.c
	$(CC) $(CFLAGS) -DTESTING=1 -o $@ -c $<

test_dfind: $(TEST_DFIND_OBJ)

image2xy: $(SIMPLEXY_OBJ) $(CFITS_LIB)

fixaxis: $(FIXAXIS_OBJ) $(CFITS_LIB)

simplexy_shared: _simplexy.so
.PHONY: simplexy_shared

_simplexy.so: $(SIMPLEXY_SHARED_OBJ)
	$(CC) $(SHAREDLIBFLAGS) -o $@ $^

new-wcs: new-wcs.o $(ANUTILS_LIB) $(QFITS_LIB)

rdls2hpls: rdls2hpls.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

rdlstohealpix: rdlstohealpix.o \
		$(ANUTILS_LIB) $(QFITS_LIB)

xylist2fits: xylist2fits.o boilerplate.o svn.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

ngc2000.o: ngc2000.c ngc2000names.c ngc2000entries.c

ngc2000names.c: parse-ngc2000names.awk ngc2000names.dat
	awk -f parse-ngc2000names.awk < ngc2000names.dat  > $@

ngc2000entries.c: parse-ngc2000.awk ngc2000.dat
	awk -f parse-ngc2000.awk < ngc2000.dat  > $@

ngcic-accurate.c: ngcic-accurate.h ngcic-accurate-entries.c

ngcic-accurate-entries.c: parse-ngcic-accurate.awk ngc2000_pos.txt ic2000_pos.txt
	cat ngc2000_pos.txt ic2000_pos.txt | awk -f parse-ngcic-accurate.awk > $@

certifiable: certifiable.o histogram.o matchfile.o solvedfile.o \
		$(UTILS_LIB) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

an-fitstopnm: an-fitstopnm.o \
		$(ANUTILS_LIB) $(QFITS_LIB)

fits-guess-scale: fits-guess-scale.o \
		$(ANUTILS_LIB) $(QFITS_LIB)

get-wcs: get-wcs.o \
		$(ANUTILS_LIB) $(QFITS_LIB)

wcs-annotate: wcs-annotate.o ngc2000.o \
		$(UTIL_OBJS) \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

wcs-xy2rd: wcs-xy2rd.o $(UTIL_OBJS) \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

wcs-rd2xy: wcs-rd2xy.o $(UTIL_OBJS) \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

noisesim: noisesim.o noise.o $(ANUTILS_LIB)

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

cut-an: cut-an.o catalog.o idfile.o usnob.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

startree: startree.o catalog.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

hpquads: hpquads.o codefile.o bt.o pnpoly.o histogram.o tic.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

codetree: codetree.o codekd.o codefile.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

unpermute-stars: unpermute-stars.o idfile.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

unpermute-quads: unpermute-quads.o codekd.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

backend: backend.o scriptutils.o fileutil.o $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

solve-field: solve-field.o scriptutils.o matchfile.o fileutil.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

augment-xylist: augment-xylist.o scriptutils.o fileutil.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

blind: blind-main.o blind.o solver.o codekd.o matchfile.o tic.o \
		solvedclient.o solvedfile.o idfile.o verify.o svd.o \
		intmap.o blind_wcs.o index.o log.c histogram.o tweak.o \
		$(UTIL_OBJS) \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB) $(GSL_LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@.tmp $^
	mv $@.tmp $@

whynot: whynot.o index.o codekd.o matchfile.o tic.o \
		solvedclient.o solvedfile.o \
		idfile.o verify.o svd.o intmap.o blind_wcs.o log.o \
		$(UTIL_OBJS) \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

quadcenters: quadcenters.o fileutil.o boilerplate.o svn.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

startree2rdls: startree2rdls.o fileutil.o boilerplate.o svn.o \
		$(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) $(QFITS_LIB)

agreeable: agreeable.o matchfile.o solvedfile.o solvedclient.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

rdlsinfo: rdlsinfo.o $(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

xylsinfo: xylsinfo.o $(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

wcsinfo: wcsinfo.o $(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) \
		$(QFITS_LIB)

printsolved: printsolved.o solvedfile.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB) 

mergesolved: mergesolved.o solvedfile.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

solvedserver: solvedserver.o solvedfile.o $(UTIL_OBJS) $(ANUTILS_LIB) \
		$(QFITS_LIB)
	$(CC) $(CFLAGS) -o $@ $^ -lm

plotcat: plotcat.o catalog.o tycho2_fits.o tycho2.o usnob_fits.o usnob.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) \
		$(QFITS_LIB)

plotquad.o: plotquad.c
	$(CC) $(CFLAGS) $(CAIRO_INC) -c -o $@ $<

plotquad: plotquad.o cairoutils.o boilerplate.o svn.o $(ANUTILS_LIB) $(QFITS_LIB) 
	$(CC) -o $@ $^ $(LDFLAGS) $(CAIRO_LIB) $(PNG_LIB) $(NETPBM_LIB)

plot-constellations.o: plot-constellations.c
	$(CC) $(CFLAGS) $(CAIRO_INC) $(NETPBM_INC) -c -o $@ $<

plot-constellations: plot-constellations.o constellations.o cairoutils.o \
		ngc2000.o ngcic-accurate.o brightstars.o boilerplate.o svn.o \
		$(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(LDFLAGS) $^ -o $@ $(CAIRO_LIB) $(NETPBM_LIB) $(PNG_LIB)

plotxy.o: plotxy.c
	$(CC) $(CFLAGS) $(CAIRO_INC) -c -o $@ $<

plotxy: plotxy.o cairoutils.o boilerplate.o svn.o $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) -o $@ $^ $(LDFLAGS) $(CAIRO_LIB) $(PNG_LIB) $(NETPBM_LIB)

quadidx: quadidx.o $(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

codeprojections: codeprojections.o codekd.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

quadlocations: quadlocations.o catalog.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) \
		$(QFITS_LIB)

quadscales: quadscales.o histogram.o \
		$(UTIL_OBJS) $(ANFILES_LIB) $(ANUTILS_LIB) $(LIBKD_LIB) \
		$(QFITS_LIB)

quadsperstar: quadsperstar.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANFILES_LIB) $(ANUTILS_LIB) \
		$(QFITS_LIB)

fitsgetext: fitsgetext.o $(ANUTILS_LIB) $(QFITS_LIB)

subtable: subtable.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

tabsort: tabsort.o $(UTIL_OBJS) $(ANUTILS_LIB) fileutil.o $(QFITS_LIB)

column-merge: column-merge.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

tycho2tostellarium: tycho2tostellarium.o tycho2_fits.o $(UTIL_OBJS) $(ANUTILS_LIB) \
		$(QFITS_LIB)

DEP_OBJ := $(ALL_OBJ)
DEP_PREREQS := $(QFITS_LIB)

CFLAGS += $(CFITSIO_CFLAGS)
CFLAGS += $(CAIRO_INC)
include $(COMMON)/makefile.deps

allclean: clean
	rm -f *.o *~ *.dep 

clean:
	rm -f $(EXECS) $(EXTRA_EXECS) $(SOLVER_EXECS) $(MISC_EXECS) \
		$(PIPELINE) $(PROSPECTUS) $(DEPS) deps $(FITS_UTILS) $(ALL_OBJ) $(IMAGE2XY) \
		$(NODEP_OBJS)

