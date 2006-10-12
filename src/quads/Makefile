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

OBJ := starutil.o fileutil.o xylist.o rdlist.o mathutil.o \
	healpix.o idfile.o qidxfile.o \
	hitsfile.o ioutils.o matchfile.o lsfile.o catalog.o \
	codefile.o tic.o quadfile.o intmap.o bl.o \
	dualtree_rangesearch.o dualtree.o dualtree_max.o \
	hitlist_healpix.o verify.o donuts.o histogram.o \
	solvedclient.o fieldcheck.o an_catalog.o permutedsort.o \
	bt.o solvedfile.o starkd.o codekd.o merctree.o

EXECS := whynot certifiable \
	findquad findstar \
	matchstats \
	plotmatch \
	plotquads matchtordls \
	tunematch \
	overlapstats \
	render_mercator \
	make-merctree \
	rawstartree

QFITS_UTILS := fitsgetext subtable tabsort

CFITS_UTILS := tablist modhead fitscopy tabmerge fitstomatlab liststruc listhead

FITS_UTILS := $(QFITS_UTILS) $(CFITS_UTILS)

MISC_EXECS := ppmnormrgb whitebalance 2masstofits 

NETPBM_I :=
NETPBM_L := -L. -lnetpbm

#CFLAGS += $(NETPBM_I)
#LFLAGS += $(NETPBM_L)

UTIL_OBJS := ioutils.o fileutil.o starutil.o mathutil.o \
	boilerplate.o svn.o

OTHER_OBJS := usnob.o usnob_fits.o tycho2.o tycho2_fits.o healpix.o bl.o \
	bt.o an_catalog.o catalog.o idfile.o starkd.o quadfile.o codefile.o \
	rdlist.o xylist.o histogram.o tic.o permutedsort.o codekd.o \
	donuts.o verify.o dualtree_rangesearch.o dualtree.o dualtree_max.o \
	solver.o matchfile.o solvedclient.o solvedfile.o hitlist_healpix.o \
	intmap.o hitsfile.o qidxfile.o #kdtree/kdtree_dim_4.o

2MASSOBJ := 2masstofits.o 2mass_catalog.o 2mass.o

PIPELINE := usnobtofits tycho2tofits build-an-catalog hpowned cut-an startree \
	hpquads codetree unpermute-quads unpermute-stars homer slave agreeable \
	solvedserver printsolved

PROSPECTUS := plotcat quadidx codeprojections quadlocations \
	quadscales quadsperstar
# HCAK - codedensity is broken at the moment...
# codedensity 

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

#	$(REMAKE_QFITS)

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

rdlstohealpix: rdlstohealpix.o rdlist.o xylist.o fitsioutils.o ioutils.o \
		healpix.o bl.o $(QFITS_LIB)

.PHONY: tags
tags:
	etags `find . -name "*.h" -o -name "*.c"`

#WCSLIB_LIB := /usr/local/wcslib-4.2/lib/libwcs.a
#WCSLIB_INC := /usr/local/wcslib-4.2/include/wcslib

#homer2: homer2.o label.o xylist.o fitsioutils.o ioutils.o bl.o $(QFITS_LIB)
#	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^ -lm

#badfields: badfields.o solvedfile.o bl.o 
#	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^

#badfields.o: badfields.c badfields.inc
#	$(CC) $(CFLAGS) -c $< -o $@

#badfields.inc: opBadfields.par badfields.awk
#	awk -f badfields.awk $< > $@

#wcscheck: wcscheck.o starutil.o mathutil.o libqfits.a
#	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^ $(WCSLIB_LIB)

#wcscheck.o: wcscheck.c
#	$(CC) $(CFLAGS) -c $< -o $@ -I$(WCSLIB_INC)

#kdspeed: kdspeed.o kdtree/kdtree.o kdtree/kdtree_dim.o kdtree/kdtree_access.o \
#	kdtree/kdtree_dim_4.o tic.o
#	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^

#kdtree/kdtree_dim_4.o: kdtree/kdtree_dim.c
#	$(CC) $(CFLAGS) -o $@ -c $^ -DKD_DIM=4

#hptst: hptst.o healpix.o
#	$(CC) $(CFLAGS) -o $@ $^ -lm

#CFITSIO_CFLAGS := -Icfitsio/include
#CFITSIO_CFLAGS += -Wall -O2
#CFITSIO_LFLAGS := $(CFITSIO_LIB) -lm

#CFITSIO_CFLAGS := -Icfitsio -I../cfitsio
#CFITSIO_CFLAGS += -Wall -O0 -g
#CFITSIO_LIB := cfitsio/libcfitsio.a
#CFITSIO_LFLAGS := -lm
#CFITSIO_OBJS := $(addsuffix .o,$(CFITS_UTILS))
#CFITSIO_SRCS := $(CFITSIO_OBJS:.o=.c)

$(CFITS_UTILS) :: %: %.o $(CFITS_LIB)

$(EXECS) :: %: %.o $(OBJ) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

2masstofits: $(2MASSOBJ) healpix.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lz

usnobtofits: usnobtofits.o usnob_fits.o usnob.o healpix.o bl.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

tycho2tofits: tycho2tofits.o tycho2_fits.o tycho2.o healpix.o bl.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

hpowned: hpowned.o healpix.o mathutil.o boilerplate.o svn.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

build-an-catalog: build-an-catalog.o an_catalog.o usnob_fits.o tycho2_fits.o \
		tycho2.o usnob.o healpix.o bl.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

cut-an: cut-an.o an_catalog.o catalog.o idfile.o healpix.o bl.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

startree: startree.o starkd.o catalog.o $(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

hpquads: hpquads.o starkd.o quadfile.o codefile.o bt.o rdlist.o xylist.o \
		healpix.o histogram.o bl.o tic.o permutedsort.o $(UTIL_OBJS) \
		$(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

codetree: codetree.o codekd.o codefile.o $(UTIL_OBJS) \
		$(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

unpermute-stars: unpermute-stars.o starkd.o quadfile.o idfile.o $(UTIL_OBJS) \
		$(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

unpermute-quads: unpermute-quads.o quadfile.o codekd.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

homer: homer.o donuts.o xylist.o bl.o dualtree_rangesearch.o dualtree.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

slave: slave.o solver.o starkd.o codekd.o quadfile.o matchfile.o bl.o tic.o \
		solvedclient.o solvedfile.o healpix.o idfile.o verify.o \
		xylist.o hitlist_healpix.o intmap.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -pthread

agreeable: agreeable.o matchfile.o bl.o hitsfile.o hitlist_healpix.o \
		solvedfile.o solvedclient.o intmap.o healpix.o $(UTIL_OBJS) \
		$(ANUTILS_LIB) $(QFITS_LIB)

printsolved: printsolved.o solvedfile.o bl.o $(UTIL_OBJS) \
		$(ANUTILS_LIB) $(QFITS_LIB) 
	$(CC) $(CFLAGS) -o $@ $^ -lm

solvedserver: solvedserver.o solvedfile.o bl.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) -o $@ $^ -lm

plotcat: plotcat.o an_catalog.o catalog.o tycho2_fits.o tycho2.o rdlist.o \
		xylist.o usnob_fits.o usnob.o healpix.o starkd.o bl.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

quadidx: quadidx.o quadfile.o qidxfile.o bl.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

codedensity: codedensity.o codekd.o dualtree_max.o bl.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

codeprojections: codeprojections.o codefile.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

quadlocations: quadlocations.o quadfile.o catalog.o starkd.o bl.o $(UTIL_OBJS) $(ANUTILS_LIB) \
		$(LIBKD_LIB) $(QFITS_LIB)

quadscales: quadscales.o histogram.o starkd.o quadfile.o $(UTIL_OBJS) $(ANUTILS_LIB) \
		$(LIBKD_LIB) $(QFITS_LIB)

quadsperstar: quadsperstar.o qidxfile.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)

fitsgetext: fitsgetext.o bl.o $(QFITS_LIB)

subtable: subtable.o bl.o $(UTIL_OBJS) $(ANUTILS_LIB) ioutils.o $(QFITS_LIB)

tabsort: tabsort.o $(UTIL_OBJS) $(ANUTILS_LIB) ioutils.o permutedsort.o fileutil.o $(QFITS_LIB)

usnobcount: usnobcount.o 
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

usnobtile: usnobtile.o merctree.o bl.o healpix.o \
		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

sdsstile: sdsstile.o rdlist.o xylist.o bl.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

sdssfieldtile: sdssfieldtile.o xylist.o bl.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

indextile: indextile.o starkd.o healpix.o bl.o \
		$(LIBKD_LIB) $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

indexquad: indexquad.o starkd.o bl.o permutedsort.o \
		$(LIBKD_LIB) $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

sdssquad: sdssquad.o rdlist.o xylist.o bl.o permutedsort.o \
		$(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

#usnobtile-test: usnobtile.o merctree.o bl.o healpix.o \
#		$(UTIL_OBJS) $(LIBKD_LIB) $(ANUTILS_LIB) $(QFITS_LIB)
#	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) $(NETPBM_L)

zoomout: zoomout.o pnmutils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(NETPBM_L)

tycho2tostellarium: tycho2tostellarium.o tycho2_fits.o $(UTIL_OBJS) $(ANUTILS_LIB) $(QFITS_LIB)

DEP_OBJ := $(ALL_OBJ)
DEP_PREREQS := $(QFITS_LIB)

CFLAGS += $(CFITSIO_CFLAGS) $(NETPBM_I)
include $(COMMON)/makefile.deps

allclean: clean
	rm -f *.o *~ *.dep 
#$(KD_LIB)/*.o 
#$(QFITS_LIB)

clean:
	rm -f $(EXECS) $(EXTRA_EXECS) $(SOLVER_EXECS) $(MISC_EXECS) \
		$(PIPELINE) $(PROSPECTUS) $(DEPS) deps $(FITS_UTILS) $(ALL_OBJ)

