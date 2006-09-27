.PHONY: all allclean clean

CC := gcc
LFLAGS := -lm

CFLAGS :=

#CFLAGS	+= -g
#CFLAGS += -O0
#CFLAGS += -O2
#CFLAGS += -pg
CFLAGS += -Wall
CFLAGS += -fomit-frame-pointer
CFLAGS += -O3 
# This wrecks our ability to do "fitsmd5" and similar fun byte-level tests
#CFLAGS += -ffast-math 
CFLAGS += -fno-math-errno -fno-trapping-math

GCC_VERSTR := $(shell gcc --version | head -n 1)
$(warning gcc version string: "$(GCC_VERSTR)")

AWK := awk
#AWK := gawk --lint --lint-old

GCC_MAJOR := $(shell echo "$(GCC_VERSTR)" | $(AWK) 'BEGIN{RS=" ";got=0} /[0-9]\.[0-9]*/{i=index($$0, "."); if (i && !got) {print substr($$0, 1, i-1); got=1}}')
GCC_MINOR := $(shell echo "$(GCC_VERSTR)" | $(AWK) 'BEGIN{RS=" ";got=0} /[0-9]\.[0-9]*/{i=index($$0, "."); if (i && !got) {i++; j=i; while (index("0123456789", substr($$0, j, 1))) { j++; } print substr($$0, i, j-i); got=1}}')
$(warning gcc version $(GCC_MAJOR) dot $(GCC_MINOR))
$(warning $(shell echo shell is $$SHELL))
TXT := (($(GCC_MAJOR) > 3) || (($(GCC_MAJOR) == 3) && ($(GCC_MINOR) >= 3))) ? 1 : 0
$(warning expression is $(TXT))
GCC_HAS_FFINITE := $(shell echo "$$(( $(TXT) ))")
$(warning result is $(GCC_HAS_FFINITE))
#GCC_HAS_FFINITE := $(shell echo "$$(( (($(GCC_MAJOR) > 3) || (($(GCC_MAJOR) == 3) && ($(GCC_MINOR) >= 3))) ? 1 : 0 ))" )
#$(warning gcc_has_ffinite: $(GCC_HAS_FFINITE))
# 3.2 doesn't have it
# 3.3 does
# 4.0 does
ifeq ($(GCC_HAS_FFINITE), 1)
	CFLAGS += -ffinite-math-only -fno-signaling-nans
endif

# to turn off asserts:
CFLAGS += -DNDEBUG

#CFLAGS += -Wextra
#CFLAGS += -Wpointer-arith

#CFLAGS += -fmudflap
#LFLAGS += -lmudflap

# What functions were and weren't inlined?
#CFLAGS += -Winline
#	-Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
#	-Wmissing-declarations -Wredundant-decls

# check out:
#   __attribute__ ((nothrow))
#   __attribute__ ((nonnull))
#   -fforce-addr
#   -fprefetch-loop-arrays

# -fmudflap -fmudflapth -fmudflapir
#    instrument all risky pointer/array dereferencing operations,	
#    some standard library string/heap functions, and some other
#    associated constructs with range/validity tests.
#    The instrumentation relies on a separate runtime library (libmudflap),
#    which will be linked into a program if -fmudflap is given at link time
#    Run-time behavior of the instrumented program is controlled by the
#    MUDFLAP_OPTIONS environment variable. See env MUDFLAP_OPTIONS=-help
#    a.out for its options.
#    Use -fmudflapth instead of -fmudflap to compile and to link if your program
#    is multi-threaded. Use -fmudflapir, in addition to -fmudflap or -fmudflapth,
#    if instrumentation should ignore pointer reads. This produces less
#    instrumentation (and therefore faster execution) and still provides some protection against outright memory corrupting writes, but allows erroneously read data to propagate within a program.

CFLAGS += -Wall -D__USE_FIXED_PROTOTYPES__ \
	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
	-I. -I../cfitsio

MACHINE:=$(shell uname -m)
ifeq ($(MACHINE), i686)
	GCCVER:=$(shell gcc --version | head -1 | awk '{print $$3}' | awk 'BEGIN{FS="."} {print $$1 "." $$2}')
	ifeq ($(GCCVER), 3.2)
		CFLAGS += -mcpu=pentium4 
	endif
	ifeq ($(GCCVER), 4.0)
		CFLAGS += -mtune=pentium4
	endif
	CFLAGS += -march=pentium4
	#CFLAGS += -mfpmath=sse -mmmx -msse -msse2
	# This seems to kill qfits...
	#CFLAGS += -malign-double
else
	CFLAGS += -DNOT_686
endif

QFITS_OBJ := qfits/tfits.o qfits/fits_h.o qfits/xmemory.o qfits/simple.o qfits/qerror.o \
	qfits/cache.o qfits/t_iso8601.o qfits/fits_rw.o qfits/byteswap.o \
	qfits/ieeefp-compat.o qfits/fits_md5.o qfits/expkey.o qfits/fits_p.o qfits/md5.o
QFITS_LIB := libqfits.a

OBJ := starutil.o fileutil.o xylist.o rdlist.o mathutil.o \
	kdtree/kdtree.o kdtree/kdtree_access.o kdtree/kdtree_dim.o \
	kdtree/kdtree_io.o kdtree/kdtree_fits_io.o \
	healpix.o \
	idfile.o qidxfile.o \
	hitsfile.o ioutils.o matchfile.o lsfile.o catalog.o \
	codefile.o tic.o quadfile.o intmap.o bl.o \
	dualtree_rangesearch.o dualtree.o dualtree_max.o \
	hitlist_healpix.o fitsioutils.o verify.o donuts.o histogram.o \
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

#usnobtile usnobcount 

QFITS_UTILS := fitsgetext subtable tabsort

CFITS_UTILS := tablist modhead fitscopy tabmerge fitstomatlab liststruc listhead

FITS_UTILS := $(QFITS_UTILS) $(CFITS_UTILS)

MISC_EXECS := ppmnormrgb whitebalance 2masstofits 

#randcat.o genfields.o findstar.o findquad.o \
#printcatalog.o get_healpix.o 
#printmatchfile.o rdls2hpls.o certifiable.o
#kdtree/test_kdtree.o \
#mergehits.o checkquads.o


QFITS_I := -Iqfits
QFITS_L := 

CFLAGS += $(QFITS_I)
LFLAGS += $(QFITS_L)

NETPBM_I :=
NETPBM_L := -L. -lnetpbm

#CFLAGS += $(NETPBM_I)
#LFLAGS += $(NETPBM_L)

CFLAGS += -Ikdtree

KDTREE_OBJS := kdtree/kdtree.o kdtree/kdtree_fits_io.o kdtree/kdtree_io.o kdtree/kdtree_access.o kdtree/kdtree_dim.o

UTIL_OBJS := fitsioutils.o ioutils.o fileutil.o starutil.o mathutil.o \
	boilerplate.o svn.o

OTHER_OBJS := usnob.o usnob_fits.o tycho2.o tycho2_fits.o healpix.o bl.o \
	bt.o an_catalog.o catalog.o idfile.o starkd.o quadfile.o codefile.o \
	rdlist.o xylist.o histogram.o tic.o permutedsort.o codekd.o \
	donuts.o verify.o dualtree_rangesearch.o dualtree.o dualtree_max.o \
	solver.o matchfile.o solvedclient.o solvedfile.o hitlist_healpix.o \
	intmap.o hitsfile.o qidxfile.o kdtree/kdtree_dim_4.o

2MASSOBJ := 2masstofits.o 2mass_catalog.o 2mass.o

PIPELINE := usnobtofits tycho2tofits build_an_catalog hpowned cut_an startree \
	hpquads codetree unpermute-quads unpermute-stars homer slave agreeable \
	solvedserver printsolved

PROSPECTUS := plotcat quadidx codedensity codeprojections quadlocations \
	quadscales quadsperstar

PIPELINE_MAIN_OBJ := $(addsuffix .o,$(PIPELINE))
PROSPECTUS_MAIN_OBJ := $(addsuffix .o,$(PROSPECTUS))
FITS_UTILS_MAIN_OBJ := $(addsuffix .o,$(FITS_UTILS))

ALL_OBJ := $(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_OBJ) \
	$(PIPELINE_MAIN_OBJ) $(PROSPECTUS_MAIN_OBJ) $(FITS_UTILS_MAIN_OBJ) \
	$(OTHER_OBJS)
ALL_OBJ += $(2MASSOBJ)

.PHONY: all
all: $(PIPELINE) $(PROSPECTUS) $(FITS_UTILS)

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

## DEBUG
kdtst: kdtst.o $(KDTREE_OBJS) $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^ -lm

.PHONY: tags
tags:
	etags `find . -name "*.h" -o -name "*.c"`

$(QFITS_LIB): $(QFITS_OBJ)
	ar cr $@ $^
	ranlib $@

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

kdtree/kdtree_dim_4.o: kdtree/kdtree_dim.c
	$(CC) $(CFLAGS) -o $@ -c $^ -DKD_DIM=4

#hptst: hptst.o healpix.o
#	$(CC) $(CFLAGS) -o $@ $^ -lm

#CFITSIO_CFLAGS := -Icfitsio/include
CFITSIO_CFLAGS := -Icfitsio -I../cfitsio
#CFITSIO_CFLAGS += -Wall -O2
CFITSIO_CFLAGS += -Wall -O0 -g
CFITSIO_LIB := cfitsio/libcfitsio.a
#CFITSIO_LFLAGS := $(CFITSIO_LIB) -lm
CFITSIO_LFLAGS := -lm
CFITSIO_OBJS := $(addsuffix .o,$(CFITS_UTILS))
CFITSIO_SRCS := $(CFITSIO_OBJS:.o=.c)

$(CFITSIO_LIB): $(wildcard cfitsio/*.c) $(wildcard cfitsio/*.h) cfitsio/Makefile
	$(MAKE) -C cfitsio

cfitsio/Makefile: cfitsio/Makefile.in
	cd cfitsio && ./configure

$(CFITS_UTILS) :: %: %.o $(CFITSIO_LIB)
	$(CC) $(CFITSIO_CFLAGS) $^ -o $@ $(CFITSIO_LFLAGS)

# Gah, this is lame, but I can't figure out how to avoid it; I need
# CFITSIO_CFLAGS to be set for just these object files.
tablist.o: tablist.c
	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@
modhead.o: modhead.c
	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@
fitscopy.o: fitscopy.c
	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@
tabmerge.o: tabmerge.c
	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@
fitstomatlab.o: fitstomatlab.c
	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@
liststruc.o: liststruc.c
	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@
listhead.o: listhead.c
	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@

#$(CFITSIO_OBJS) :: %.o: %.c
#	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@

#$(CFITSIO_OBJS): $(CFITSIO_SRCS)
#	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@

$(EXECS) :: %: %.o $(OBJ) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

#$(SOLVER_EXECS) :: %: %.o $(OBJ) $(QFITS_LIB) $(SOLVER_OBJ) $(SOLVER_OBJ_SPECIAL)
#	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) -pthread

#$(EXTRAS) :: %: %.o $(OBJ) $(QFITS_LIB)
#	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

#test_bt: test_bt.o bt.o bl.o
#	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

#an_deduplicate: an_deduplicate.o an_catalog.o dualtree_rangesearch.o dualtree.o \
#	kdtree/kdtree.o bl.o fitsioutils.o ioutils.o mathutil.o starutil.o $(QFITS_LIB)
#	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

2masstofits: $(2MASSOBJ) healpix.o $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) -lz

usnobtofits: usnobtofits.o usnob_fits.o usnob.o healpix.o bl.o \
		$(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

tycho2tofits: tycho2tofits.o tycho2_fits.o tycho2.o healpix.o bl.o \
		$(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

#usnobprint: usnob_fits_print.o usnob_fits.o usnob.o healpix.o bl.o \
#		$(UTIL_OBJS) $(QFITS_LIB)
#	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

hpowned: hpowned.o healpix.o mathutil.o boilerplate.o svn.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

build_an_catalog: build_an_catalog.o an_catalog.o usnob_fits.o tycho2_fits.o \
		tycho2.o usnob.o healpix.o bl.o $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

cut_an: cut_an.o an_catalog.o catalog.o idfile.o healpix.o bl.o \
		$(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

startree: startree.o starkd.o catalog.o $(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

hpquads: hpquads.o starkd.o quadfile.o codefile.o bt.o rdlist.o xylist.o \
		healpix.o histogram.o bl.o tic.o permutedsort.o $(UTIL_OBJS) \
		$(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

codetree: codetree.o codekd.o codefile.o $(KDTREE_OBJS) $(UTIL_OBJS) \
		$(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

unpermute-stars: unpermute-stars.o starkd.o quadfile.o idfile.o \
		$(KDTREE_OBJS) $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

unpermute-quads: unpermute-quads.o quadfile.o codekd.o \
		$(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

homer: homer.o donuts.o xylist.o bl.o dualtree_rangesearch.o dualtree.o \
		$(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

slave: slave.o solver.o starkd.o codekd.o quadfile.o matchfile.o bl.o tic.o \
		solvedclient.o solvedfile.o healpix.o idfile.o verify.o \
		xylist.o hitlist_healpix.o intmap.o kdtree/kdtree_dim_4.o \
		$(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) -pthread

agreeable: agreeable.o matchfile.o bl.o hitsfile.o hitlist_healpix.o \
		solvedfile.o solvedclient.o intmap.o healpix.o $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

printsolved: printsolved.o mathutil.o boilerplate.o svn.o \
		solvedfile.o bl.o $(QFITS_LIB)
	$(CC) $(CFLAGS) -o $@ $^ -lm

solvedserver: solvedserver.o solvedfile.o ioutils.o bl.o boilerplate.o svn.o $(QFITS_LIB)
	$(CC) $(CFLAGS) -o $@ $^

plotcat: plotcat.o an_catalog.o catalog.o tycho2_fits.o tycho2.o rdlist.o \
		xylist.o usnob_fits.o usnob.o healpix.o bl.o $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

quadidx: quadidx.o quadfile.o qidxfile.o bl.o $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

codedensity: codedensity.o codekd.o dualtree_max.o bl.o \
		$(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

codeprojections: codeprojections.o codefile.o \
		$(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

quadlocations: quadlocations.o quadfile.o catalog.o starkd.o bl.o $(UTIL_OBJS) \
		$(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

quadscales: quadscales.o histogram.o starkd.o quadfile.o $(UTIL_OBJS) \
		$(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

quadsperstar: quadsperstar.o qidxfile.o \
		$(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

fitsgetext: fitsgetext.o bl.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

subtable: subtable.o bl.o fitsioutils.o ioutils.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

tabsort: tabsort.o fitsioutils.o ioutils.o permutedsort.o fileutil.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

usnobcount: usnobcount.o 
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)
#merctree.o bl.o healpix.o $(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)

usnobtile: usnobtile.o merctree.o bl.o healpix.o \
		$(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) $(NETPBM_L)

sdsstile: sdsstile.o rdlist.o xylist.o bl.o \
		$(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) $(NETPBM_L)

sdssfieldtile: sdssfieldtile.o xylist.o bl.o \
		$(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) $(NETPBM_L)

indextile: indextile.o starkd.o healpix.o bl.o \
		$(KDTREE_OBJS) $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) $(NETPBM_L)

indexquad: indexquad.o starkd.o bl.o permutedsort.o \
		$(KDTREE_OBJS) $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) $(NETPBM_L)

sdssquad: sdssquad.o rdlist.o xylist.o bl.o permutedsort.o \
		$(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

usnobtile-test: usnobtile.o merctree.o bl.o healpix.o \
		$(UTIL_OBJS) $(KDTREE_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) $(NETPBM_L)

zoomout: zoomout.o pnmutils.o
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) $(NETPBM_L)

tycho2tostellarium: tycho2tostellarium.o tycho2_fits.o $(UTIL_OBJS) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

#CUTEST_I := -Ikdtree/CuTest
#CUTEST_L := kdtree/CuTest/CuTest.o 
#CFLAGS += $(CUTEST_I)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


# Dependencies:           
DEPS := $(subst .o,.dep,$(ALL_OBJ))

deps: $(DEPS)
	cat *.dep > deps

%.dep : %.c
	$(CC) -MM $(CFLAGS) $(CFITSIO_CFLAGS) $(NETPBM_I) -c $< > $@

kdtree/kdtree_dim_4.dep: kdtree/kdtree_dim.c
	$(CC) -MM $(CFLAGS) $(CFITSIO_CFLAGS) $(NETPBM_I) -c $< -DKD_DIM=4 > $@

allclean:
	rm -f KD/*.o *.o $(EXECS) $(EXTRA_EXECS) $(SOLVER_EXECS) $(MISC_EXECS) $(OBJ) *~ $(DEPS) deps $(QFITS_OBJ) $(QFITS_LIB)

clean:
	rm -f $(EXECS) $(EXTRA_EXECS) $(SOLVER_EXECS) $(MISC_EXECS) \
		$(PIPELINE) $(PROSPECTUS) $(QFITS_LIB) $(DEPS) deps \
		$(FITS_UTILS) $(ALL_OBJ)

#$(CUTEST_L) :: %.o : %.c
#	$(CC) $(CFLAGS) -c $< -o $@ $(CUTEST_I)

#CUTEST_TEST_DRIVERS := kdtree/test_kdtree test_healpix test_bl

#tests: $(CUTEST_TEST_DRIVERS)

#CUTEST_TEST_OBJS := $(addsuffix .o,$(CUTEST_TEST_DRIVERS))

#$(CUTEST_TEST_DRIVERS) :: %: %.o $(CUTEST_L) $(OBJ)
#	$(CC) $(CFLAGS) -o $@ $^ -lm

ifneq ($(MAKECMDGOALS),clean)
  ifneq ($(MAKECMDGOALS),allclean)
    #echo "Don't worry if a warning is printed immediately below this line."
    include deps
  endif
endif
