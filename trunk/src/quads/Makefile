.PHONY: all allclean clean

CC := gcc
LFLAGS := -lm

CFLAGS :=

#CFLAGS	+= -g
#CFLAGS += -O0
#CFLAGS += -O2
#CFLAGS += -pg
CFLAGS += -fomit-frame-pointer
CFLAGS += -O3 -ffast-math

# to turn off asserts:
CFLAGS += -DNDEBUG

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
	-I.

MACHINE:=$(shell uname -m)
ifeq ($(MACHINE), i686)
	GCCVER:=$(shell gcc --version | head -1 | gawk '{print $$3}' | gawk 'BEGIN{FS="."} {print $$1 "." $$2}')
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
	bt.o solvedfile.o

SOLVER_OBJ := solver.o
SOLVER_OBJ_SPECIAL := kdtree/kdtree_dim_4.o

EXECS := whynot agreeable certifiable quadidx \
	hpquads codetree startree \
	codedensity findquad findstar \
	codeprojections quadlocations hpowned fitsgetext \
	subtable matchstats unpermute-quads \
	tabsort plotmatch \
	plotquads matchtordls homer \
	unpermute-stars2 tunematch quadscales

# things we don't use often:
#	randcat genfields
#   quadsperstar
#   genmatch makematch 
#	xylstomatlab \
#	galex_stats 
#	rdlstofieldcheck 
#   checkquads 
#   mergehits 
#   rdlstohealpix \

# obsolescent:
#	deduplicate \
#   unpermute-stars 
#	cert2 
#   dedup_index 

EXTRAS := rdls2hpls get_healpix printcatalog xylist_texttofits hplimits \
	filter_an
#kdspeed

# execs that require solver.o
SOLVER_EXECS := slave

# postmortem requires: findstar(2), findquad, certifiable

MY_OBJ := randcat.o genfields.o findstar.o findquad.o \
	starutil.o fileutil.o mathutil.o \
	codedensity.o codetree.o printcatalog.o get_healpix.o quadidx.o \
	xylstomatlab.o rdlstohealpix.o slave.o \
	printmatchfile.o rdls2hpls.o certifiable.o agreeable.o \
	hpquads.o dedup_index.o codeprojections.o codedensity.o startree.o \
	solver.o deduplicate.o test_healpix.o kdtree/test_kdtree.o \
	usnobtofits.o usnob.o usnob_fits.o tycho2.o \
	tycho2tofits.o tycho2_fits.o build_an_catalog.o cut_an.o \
	mergehits.o checkquads.o

QFITS_I := -Iqfits
QFITS_L := 

CFLAGS += $(QFITS_I)
LFLAGS += $(QFITS_L)

CFLAGS += -Ikdtree

ALL_OBJ := $(OBJ) $(MY_OBJ) $(SOLVER_OBJ) $(QFITS_OBJ)

all: $(EXECS) $(SOLVER_EXECS) solvedserver printsolved plotcat

$(QFITS_LIB): $(QFITS_OBJ)
	ar cr $@ $^
	ranlib $@

WCSLIB_LIB := /usr/local/wcslib-4.2/lib/libwcs.a
WCSLIB_INC := /usr/local/wcslib-4.2/include/wcslib

wcscheck: wcscheck.o starutil.o mathutil.o libqfits.a
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^ $(WCSLIB_LIB)

wcscheck.o: wcscheck.c
	$(CC) $(CFLAGS) -c $< -o $@ -I$(WCSLIB_INC)

kdspeed: kdspeed.o kdtree/kdtree.o kdtree/kdtree_dim.o kdtree/kdtree_access.o \
	kdtree/kdtree_dim_4.o tic.o
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $^

kdtree/kdtree_dim_4.o: kdtree/kdtree_dim.c
	$(CC) $(CFLAGS) -o $@ -c $^ -DKD_DIM=4

hptst: hptst.o healpix.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

printsolved: printsolved.o mathutil.o
	$(CC) $(CFLAGS) -o $@ $^ -lm

solvedserver: solvedserver.o solvedfile.o bl.o
	$(CC) $(CFLAGS) -o $@ $^

FITS_UTILS := tablist modhead fitscopy tabmerge fitstomatlab liststruc listhead

fitsutils: $(FITS_UTILS)

CFITSIO_CFLAGS := -Icfitsio/include
#CFITSIO_CFLAGS += -Wall -O2
CFITSIO_CFLAGS += -Wall -O0 -g
CFITSIO_LIB := libcfitsio.a
CFITSIO_LFLAGS := $(CFITSIO_LIB) -lm

CFITSIO_OBJS := $(addsuffix .o,$(FITS_UTILS))

$(FITS_UTILS) :: %: %.o $(CFITSIO_LIB)
	$(CC) $(CFITSIO_CFLAGS) $^ -o $@ $(CFITSIO_LFLAGS)

$(CFITSIO_OBJS) :: %.o: %.c
	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@

$(EXECS) :: %: %.o $(OBJ) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

$(SOLVER_EXECS) :: %: %.o $(OBJ) $(QFITS_LIB) $(SOLVER_OBJ) $(SOLVER_OBJ_SPECIAL)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) -pthread

$(EXTRAS) :: %: %.o $(OBJ) $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

test_bt: test_bt.o bt.o bl.o
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

an_deduplicate: an_deduplicate.o an_catalog.o dualtree_rangesearch.o dualtree.o \
	kdtree/kdtree.o bl.o fitsioutils.o ioutils.o mathutil.o starutil.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

plotcat: plotcat.o starutil.o an_catalog.o catalog.o tycho2_fits.o tycho2.o \
	rdlist.o xylist.o \
	usnob_fits.o usnob.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

usnobtofits: usnobtofits.o usnob_fits.o usnob.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

tycho2tofits: tycho2tofits.o tycho2_fits.o tycho2.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

usnobprint: usnob_fits_print.o usnob_fits.o usnob.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

ancatalog: build_an_catalog.o an_catalog.o usnob_fits.o tycho2_fits.o tycho2.o usnob.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

cut_an: cut_an.o an_catalog.o catalog.o idfile.o fitsioutils.o \
	ioutils.o healpix.o mathutil.o bl.o starutil.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

split_an: split_an.o an_catalog.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_LIB)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

CUTEST_I := -Ikdtree/CuTest
CUTEST_L := kdtree/CuTest/CuTest.o 

CFLAGS += $(CUTEST_I)

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies:           
DEPS := $(subst .o,.dep,$(ALL_OBJ))

deps: $(DEPS)
	cat *.dep > deps

%.dep : %.c
	$(CC) -MM $(CFLAGS) -c $< > $@

allclean:
	rm -f KD/*.o *.o $(EXECS) $(EXTRA_EXECS) $(SOLVER_EXECS) *~ $(DEPS) deps $(QFITS_OBJ) $(OBJ) $(QFITS_LIB)

clean:
	rm -f $(EXECS) $(SOLVER_EXECS) $(EXTRA_EXECS) $(MY_OBJ) $(OBJ) $(QFITS_LIB) $(DEPS) deps

$(CUTEST_L) :: %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(CUTEST_I)

CUTEST_TEST_DRIVERS := kdtree/test_kdtree test_healpix test_bl

tests: $(CUTEST_TEST_DRIVERS)

CUTEST_TEST_OBJS := $(addsuffix .o,$(CUTEST_TEST_DRIVERS))

$(CUTEST_TEST_DRIVERS) :: %: %.o $(CUTEST_L) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm

ifneq ($(MAKECMDGOALS),clean)
  ifneq ($(MAKECMDGOALS),allclean)
    include deps
  endif
endif
