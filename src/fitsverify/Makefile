fitsverify: fitsverify.c ftverify.c fvrf_data.c fvrf_file.c fvrf_head.c fvrf_key.c fvrf_misc.c
	gcc -o fitsverify ftverify.c fvrf_data.c fvrf_file.c fvrf_head.c fvrf_key.c fvrf_misc.c -DSTANDALONE -L../cfitsio -lcfitsio -lm -I../cfitsio
clean:
	rm fitsverify
