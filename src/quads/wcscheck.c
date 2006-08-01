#include <stdio.h>
#include <stdlib.h>

#include "wcs.h"

int main(int argc, char** args) {
	char* fn;
	struct wcsprm wcs;
	int Ncoords = 4;
	int Nelems = 2;
	
	if (argc != 2) {
		fprintf(stderr, "usage: %s <FITS-file>\n", args[0]);
		exit(-1);
	}
	fn = args[1];

	wcs.flag = -1;
	if (wcsini(1, 2, &wcs)) {
		fprintf(stderr, "wcsini() failed.\n");
		exit(-1);
	}

	// print it out...
	wcsprt(&wcs);

	{
		double pixels[Ncoords][Nelems];
		double imgcrd[Ncoords][Nelems];
		wcsp2s(&wcs, Ncoords, Nelems, pixels, imgcrd, ...);
	}
	wcsfree(&wcs);

}
