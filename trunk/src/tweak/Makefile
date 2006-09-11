CC=gcc
CFLAGS=-g -Wall -O0 -I../cfitsio -I../wcstools-3.6.4 -I../quads
OBJS= src/tweak.o ../quads/starutil.o ../quads/mathutil.o
tweak: $(OBJS)
	$(CC) -o tweak $(OBJS) -L../cfitsio/ -lcfitsio -L../wcstools-3.6.4/libwcs -lwcs -lm

clean :
	rm ${OBJS}
