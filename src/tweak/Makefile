CC=gcc
CFLAGS=-g -Wall -O0 -I../cfitsio -I../wcstools-3.6.4 -I../quads \
       -I../quads/libkd -I../qfits-6.1.0 -I../qfits-6.1.0/src -I../levmar-2.1.2
LIBS= -L../cfitsio/ -lcfitsio -L../wcstools-3.6.4/libwcs -lwcs -lm \
      -L../levmar-2.1.2 -llevmar -L../quads/libkd -lkd \
      -L../quads/qfits-6.1.0/src/.libs/ -lqfits \
      -L../quads/an-common/ -lanutils
OBJS= tweak.o ../quads/starutil.o ../quads/mathutil.o \
../quads/healpix.o ezfits.o sip.o sip_util.o ../quads/bl.o \
../quads/dualtree_rangesearch.o ../quads/dualtree.o tweak_internal.o

#../quads/kdtree/kdtree.o   ../quads/kdtree/kdtree_fits_io.o \
#../quads/qfits/byteswap.o  ../quads/qfits/fits_p.o    ../quads/qfits/simple.o \
#../quads/qfits/cache.o     ../quads/qfits/fits_rw.o    ../quads/qfits/tfits.o \
#../quads/qfits/expkey.o    ../quads/qfits/ieeefp-compat.o \
#../quads/qfits/t_iso8601.o ../quads/qfits/fits_h.o    ../quads/qfits/md5.o \
#../quads/qfits/xmemory.o   ../quads/qfits/fits_md5.o  ../quads/qfits/qerror.o \
#../quads/fitsioutils.o ../quads/kdtree/kdtree_access.o \
#../quads/kdtree/kdtree_dim.o ../quads/ioutils.o \

all: tweak test_sip

tweak: $(OBJS)
	$(CC) -o tweak $(OBJS) $(LIBS)

test_sip: ../quads/starutil.o sip.o ../quads/mathutil.o test_sip.o sip_util.o
	$(CC) -o test_sip test_sip.o ../quads/starutil.o sip.o ../quads/mathutil.o sip_util.o -L../wcstools-3.6.4/libwcs -lwcs -lm -L../cfitsio -lcfitsio 

clean :
	rm ${OBJS}
