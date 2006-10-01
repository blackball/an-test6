CC=gcc
CFLAGS=-g -Wall -O0 -I../cfitsio -I../wcstools-3.6.4 -I../quads \
       -I../quads/kdtree -I../quads/qfits
LIBS= -L../cfitsio/ -lcfitsio -L../wcstools-3.6.4/libwcs -lwcs -lm
OBJS= tweak.o ../quads/starutil.o ../quads/mathutil.o \
../quads/kdtree/kdtree.o   ../quads/kdtree/kdtree_fits_io.o \
../quads/qfits/byteswap.o  ../quads/qfits/fits_p.o    ../quads/qfits/simple.o \
../quads/qfits/cache.o     ../quads/qfits/fits_rw.o    ../quads/qfits/tfits.o \
../quads/qfits/expkey.o    ../quads/qfits/ieeefp-compat.o \
../quads/qfits/t_iso8601.o ../quads/qfits/fits_h.o    ../quads/qfits/md5.o \
../quads/qfits/xmemory.o   ../quads/qfits/fits_md5.o  ../quads/qfits/qerror.o \
../quads/fitsioutils.o ../quads/kdtree/kdtree_access.o \
../quads/kdtree/kdtree_dim.o ../quads/ioutils.o \
../quads/healpix.o ezfits.o sip.o 

all: tweak test_sip

tweak: $(OBJS)
	$(CC) -o tweak $(OBJS) $(LIBS)

test_sip: ../quads/starutil.o sip.o ../quads/mathutil.o test_sip.o
	$(CC) -o test_sip test_sip.o ../quads/starutil.o sip.o ../quads/mathutil.o -lm

clean :
	rm ${OBJS}
