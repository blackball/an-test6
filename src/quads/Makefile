.PHONY: all allclean clean

CC = gcc
LFLAGS = -lm

#fast
CFLAGS	= -g -Wall -DAMFAST -D__USE_FIXED_PROTOTYPES__ -DUNIX_TTY_PLATFORM \
	-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 \
	-DUSE_OS_MEMORY_MANAGEMENT -I.

# to turn off asserts:
# -DNDEBUG

#	-Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes \
#	-Wmissing-declarations -Wredundant-decls

MACHINE=$(shell uname -m)
ifneq ($(MACHINE), i686)
	CFLAGS += -DNOT_686
endif

#debug
#CFLAGS	= -g -O0 -Wall -D__USE_FIXED_PROTOTYPES__ -DUNIX_TTY_PLATFORM -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

QFITS_OBJ = qfits/tfits.o qfits/fits_h.o qfits/xmemory.o qfits/simple.o qfits/qerror.o \
	qfits/cache.o qfits/t_iso8601.o qfits/fits_rw.o qfits/byteswap.o \
	qfits/ieeefp-compat.o qfits/fits_md5.o qfits/expkey.o qfits/fits_p.o qfits/md5.o

OBJ = starutil.o fileutil.o xylist.o mathutil.o \
	kdtree/kdtree.o kdtree/kdtree_io.o kdtree/kdtree_fits_io.o \
	healpix.o \
	hitsfile.o ioutils.o matchfile.o lsfile.o catalog.o \
	codefile.o tic.o quadfile.o intmap.o bl.o \
	dualtree_rangesearch.o dualtree.o dualtree_max.o \
	hitlist_healpix.o fitsioutils.o $(QFITS_OBJ)

SOLVER_OBJ = solver.o

EXECS = get_healpix quadidx xylstomatlab rdlstohealpix \
	codeprojections fixmatchfile printmatchfile rdls2hpls \
	certifiable hpquads dedup_index codetree startree \
	printcatalog fixcatalogheader deduplicate \
	randcat genfields codedensity agreeable findquad findstar \

# codetomatlab 

# execs that require solver.o
SOLVER_EXECS = slave

# postmortem requires: findstar(2), findquad, certifiable

MY_OBJ = randcat.o genfields.o findstar.o findquad.o \
	starutil.o fileutil.o mathutil.o fixcatalogheader.o \
	codedensity.o codetree.o printcatalog.o get_healpix.o quadidx.o \
	xylstomatlab.o rdlstohealpix.o slave.o fixmatchfile.o \
	printmatchfile.o rdls2hpls.o certifiable.o agreeable.o \
	hpquads.o codeprojections.o codedensity.o startree.o solver.o \
	deduplicate.o test_healpix.o kdtree/test_kdtree.o

# solvexy2.o 

QFITS_I = -Iqfits
QFITS_L = 

CFLAGS += $(QFITS_I)
LFLAGS += $(QFITS_L)

CFLAGS += -Ikdtree

ALL_OBJ = $(OBJ) $(MY_OBJ) $(SOLVER_OBJ)

all: $(EXECS) $(SOLVER_EXECS)

$(EXECS) :: %: %.o $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

$(SOLVER_EXECS) :: %: %.o $(OBJ) $(SOLVER_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

CUTEST_I = -Ikdtree/CuTest
CUTEST_L = kdtree/CuTest/CuTest.o 

CFLAGS += $(CUTEST_I)

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies:           
DEPS := $(subst .o,.dep,$(ALL_OBJ))

deps: $(DEPS)
	cat *.dep > deps

%.dep : %.c
	gcc -MM $(CFLAGS) -c $< > $@

allclean:
	rm -f KD/*.o *.o $(EXECS) *~ $(DEPS) deps

clean:
	rm -f $(EXECS) $(MY_OBJ) $(OBJ) $(DEPS) deps

$(CUTEST_L) :: %.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(CUTEST_I)

CUTEST_TEST_DRIVERS = kdtree/test_kdtree test_healpix test_bl

tests: $(CUTEST_TEST_DRIVERS)

CUTEST_TEST_OBJS = $(addsuffix .o,$(CUTEST_TEST_DRIVERS))

$(CUTEST_TEST_DRIVERS) :: %: %.o $(CUTEST_L) $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lm

ifneq ($(MAKECMDGOALS),clean)
  ifneq ($(MAKECMDGOALS),allclean)
    include deps
  endif
endif
