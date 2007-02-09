BASEDIR := .
COMMON := an-common

NO_QFITS := 1
export NO_QFITS

.PHONY: all
all:

include $(COMMON)/makefile.common
include $(COMMON)/makefile.cfitsio

LDFLAGS := $(LDFLAGS_DEF)
LDFLAGS += -lm

CFLAGS := $(CFLAGS_DEF)
CFLAGS += $(CFITS_INC)
CFLAGS += -Iver

CPPFLAGS := $(CFLAGS)

SIMPLEXY= src/dallpeaks.o src/dcen3x3.o src/dfind.o \
	src/dmedsmooth.o src/dobjects.o src/dpeaks.o src/dselip.o \
	src/dsigma.o src/dsmooth.o src/fits2xy.o src/simplexy.o \
	src/radix.o

SIMPLEXY_SHARED= src/simplexy.o src/dsigma.o src/dmedsmooth.o \
	src/dallpeaks.o src/dpeaks.o src/dobjects.o src/dselip.o \
	src/dsmooth.o src/dcen3x3.o src/dfind.o src/radix.o

FIXAXIS= src/fixaxis.o

all: $(REMAKE_CFITSIO) simplexy fixaxis simplexy_shared

simplexy: fits2xy

fits2xy: $(SIMPLEXY) $(CFITS_LIB)
	$(CC) -o fits2xy $^ $(LDFLAGS)

fixaxis: $(FIXAXIS) $(CFITS_LIB)
	$(CC) -o fixaxis $^ $(LDFLAGS)

simplexy_shared: _simplexy.so

_simplexy.so: $(SIMPLEXY_SHARED)
	$(CC) -shared -o $@ $^

$(SIMPLEXY) : src/dimage.h

clean :
	-rm ${SIMPLEXY} ${FIXAXIS} fits2xy fixaxis _simplexy.so
