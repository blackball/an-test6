CC=gcc
CFLAGS=-g -Wall -O3 -I../cfitsio
SIMPLEXY= src/dallpeaks.o src/dcen3x3.o src/dfind.o src/dmedsmooth.o src/dobjects.o src/dpeaks.o src/dselip.o src/dsigma.o src/dsmooth.o src/fits2xy.o src/simplexy.o 
SIMPLEXY_SHARED= src/simplexy.o src/dsigma.o src/dmedsmooth.o src/dallpeaks.o src/dpeaks.o src/dobjects.o src/dselip.o src/dsmooth.o src/dcen3x3.o src/dfind.o
FIXAXIS= src/fixaxis.o
all: simplexy fixaxis simplexy_shared
simplexy: $(SIMPLEXY)
	$(CC) -o fits2xy $(SIMPLEXY) -L../cfitsio/ -lcfitsio -I../cfitsio -lm

fixaxis: $(FIXAXIS)
	$(CC) -o fixaxis $(FIXAXIS) -L../cfitsio/ -lcfitsio -I../cfitsio -lm

simplexy_shared:
	$(CC) -shared -o simplexy.so $(SIMPLEXY_SHARED)

$(SIMPLEXY) : src/dimage.h

clean :
	rm ${SIMPLEXY} ${FIXAXIS}
