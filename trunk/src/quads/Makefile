.PHONY: all allclean clean

CC := gcc
LFLAGS := -lm

CFLAGS :=

#fast
CFLAGS	+= -g
#CFLAGS += -O0
#CFLAGS += -O2
#CFLAGS += -pg
#CFLAGS += -fomit-frame-pointer
CFLAGS += -O3 -ffast-math

# What functions were and weren't inlined?
#CFLAGS += -Winline

# check out __attribute__ ((pure))
# check out __attribute__ ((const))
# check out __attribute__ ((nothrow))
# check out __attribute__ ((nonnull))

# to turn off asserts:
CFLAGS += -DNDEBUG

#CFLAGS += -Werror

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


#	-Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
#	-Wmissing-declarations -Wredundant-decls

QFITS_OBJ := qfits/tfits.o qfits/fits_h.o qfits/xmemory.o qfits/simple.o qfits/qerror.o \
	qfits/cache.o qfits/t_iso8601.o qfits/fits_rw.o qfits/byteswap.o \
	qfits/ieeefp-compat.o qfits/fits_md5.o qfits/expkey.o qfits/fits_p.o qfits/md5.o

OBJ := starutil.o fileutil.o xylist.o rdlist.o mathutil.o \
	kdtree/kdtree.o kdtree/kdtree_io.o kdtree/kdtree_fits_io.o \
	healpix.o \
	idfile.o qidxfile.o \
	hitsfile.o ioutils.o matchfile.o lsfile.o catalog.o \
	codefile.o tic.o quadfile.o intmap.o bl.o \
	dualtree_rangesearch.o dualtree.o dualtree_max.o \
	hitlist_healpix.o fitsioutils.o verify.o donuts.o histogram.o \
	solvedclient.o fieldcheck.o \
	$(QFITS_OBJ)

SOLVER_OBJ := solver.o

EXECS := whynot checkquads mergehits agreeable certifiable quadidx \
	hpquads dedup_index codetree startree \
	deduplicate \
	randcat genfields codedensity findquad findstar \
	codeprojections quadlocations hpowned fitsgetext quadsperstar \
	subtable matchstats unpermute-stars unpermute-quads \
	cert2 rdlstofieldcheck tabsort

EXTRAS := rdls2hpls get_healpix printcatalog xylstomatlab rdlstohealpix 
# codeprojections printmatchfile 
# codetomatlab 


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

ALL_OBJ := $(OBJ) $(MY_OBJ) $(SOLVER_OBJ)

EXTRA_EXECS = xylist_texttofits hplimits plotcat 
#usnobprint usnobtofits tycho2tofits ancatalog cut_an split_an \
#an_deduplicate

all: $(EXTRA_EXECS) $(EXECS) $(SOLVER_EXECS) solvedserver printsolved

printsolved: printsolved.o
	$(CC) $(CFLAGS) -o $@ $^

solvedserver: solvedserver.o solvedfile.o bl.o
	$(CC) $(CFLAGS) -o $@ $^

FITS_UTILS := tablist modhead fitscopy tabmerge

fitsutils: $(FITS_UTILS)

CFITSIO_CFLAGS := -Icfitsio/include
CFITSIO_CFLAGS += -Wall -O2
CFITSIO_LIB := libcfitsio.a
CFITSIO_LFLAGS := $(CFITSIO_LIB) -lm

CFITSIO_OBJS := $(addsuffix .o,$(FITS_UTILS))

$(FITS_UTILS) :: %: %.o $(CFITSIO_LIB)
	$(CC) $(CFITSIO_CFLAGS) $^ -o $@ $(CFITSIO_LFLAGS)

$(CFITSIO_OBJS) :: %.o: %.c
	$(CC) $(CFITSIO_CFLAGS) -c $< -o $@

$(EXECS) :: %: %.o $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

$(SOLVER_EXECS) :: %: %.o $(OBJ) $(SOLVER_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) -pthread

an_deduplicate: an_deduplicate.o an_catalog.o dualtree_rangesearch.o dualtree.o \
	kdtree/kdtree.o bl.o fitsioutils.o ioutils.o mathutil.o starutil.o $(QFITS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

plotcat: plotcat.o starutil.o an_catalog.o catalog.o tycho2_fits.o tycho2.o \
	rdlist.o xylist.o \
	usnob_fits.o usnob.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

xylist_texttofits: xylist_texttofits.o xylist.o fitsioutils.o ioutils.o bl.o $(QFITS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

usnobtofits: usnobtofits.o usnob_fits.o usnob.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

tycho2tofits: tycho2tofits.o tycho2_fits.o tycho2.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

usnobprint: usnob_fits_print.o usnob_fits.o usnob.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

ancatalog: build_an_catalog.o an_catalog.o usnob_fits.o tycho2_fits.o tycho2.o usnob.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

cut_an: cut_an.o an_catalog.o catalog.o idfile.o fitsioutils.o \
	ioutils.o healpix.o mathutil.o bl.o starutil.o $(QFITS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

split_an: split_an.o an_catalog.o fitsioutils.o ioutils.o healpix.o mathutil.o bl.o $(QFITS_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

hplimits: hplimits.o healpix.o mathutil.o bl.o
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
	rm -f KD/*.o *.o $(EXECS) $(EXTRA_EXECS) $(SOLVER_EXECS) *~ $(DEPS) deps $(QFITS_OBJ) $(OBJ)

clean:
	rm -f $(EXECS) $(SOLVER_EXECS) $(EXTRA_EXECS) $(MY_OBJ) $(OBJ) $(DEPS) deps

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
