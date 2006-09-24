CC=gcc
CFLAGS=-g -Wall -O3 -I../cfitsio
SIMPLEXY= src/dallpeaks.o src/dcen3x3.o src/dfind.o src/dmedsmooth.o src/dobjects.o src/dpeaks.o src/dselip.o src/dsigma.o src/dsmooth.o src/fits2xy.o src/simplexy.o 
FIXAXIS= src/fixaxis.o
all: simplexy fixaxis
simplexy: $(SIMPLEXY)
	$(CC) -o fits2xy $(SIMPLEXY) -L../cfitsio/ -lcfitsio -I../cfitsio -lm

fixaxis: $(FIXAXIS)
	$(CC) -o fixaxis $(FIXAXIS) -L../cfitsio/ -lcfitsio -I../cfitsio -lm

$(SIMPLEXY) : src/dimage.h

clean :
	rm ${SIMPLEXY} ${FIXAXIS}
