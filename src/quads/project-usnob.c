/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <stdio.h>

#include "usnob_fits.h"
#include "usnob.h"
#include "starutil.h"
#include "healpix.h"
#include "boilerplate.h"

#define OPTIONS "h" //H:N:"

void print_help(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage:\n"
		   "  %s <usnob-file.fits> <output-dir>\n"
		   , progname);
	//-H <healpix> -N <nside>
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int c;
	char* infn;
	char* outfilename;
	usnob_fits* usnob;
	int i, j, N;
	int hp, Nside;
	double center[3];

	while ((c = getopt(argc, args, OPTIONS)) != -1) {
		switch (c) {
		case '?':
		case 'h':
			print_help(args[0]);
			exit(0);
		}
	}

	// make sure there is one non-option argument: the usnob fits filename.
	if (argc <3) {
		print_help(args[0]);
		exit(-1);
	}

	// try to open the file.
	infn = args[1];
	fprintf(stderr, "Reading USNOB catalog file %s\n", infn);
	usnob = usnob_fits_open(infn);
	if (!usnob) {
		fprintf(stderr, "Failed to read USNOB catalog from file %s.\n", infn);
		exit(-1);
	}

	// find out which healpix the file covers.
	hp = qfits_header_getint(usnob->header, "HEALPIX", -1);
	Nside = qfits_header_getint(usnob->header, "NSIDE", -1);
	if ((hp == -1) || (Nside == -1)) {
		fprintf(stderr, "Failed to find \"HEALPIX\" and \"NSIDE\" headers in file %s.\n", infn);
		exit(-1);
	}

	// pull out the center of the healpix.
	healpix_to_xyzarr(hp, Nside, 0.5, 0.5, center);

	// for each star...
	N = usnob_fits_count_entries(usnob);
	fprintf(stderr, "File contains %i stars.\n", N);

	/*
	printf("obs=[\n");
	for (i=0; i<N; i++) {
	  usnob_entry* star;
	  star = usnob_fits_read_entry(usnob);
	  for (j=0; j<5; j++){
	    printf("%g", star->obs[j].mag);
	    if(j<4){
	      printf(", ");
	    }
	  }
	  printf(";\n");
	}
	printf("];\n");
	*/

	outfilename = args[2];
	fprintf(stderr, "Writing to %s\n", outfilename);

	FILE *output_file;

	output_file = fopen(outfilename, "wb");

	for (i=1; i<N; i++) {
		double xyz[3];
		double px, py;
		int numMags = 0;
		double sumMags = 0;
		double buffer[8];

		usnob_entry* star;
		// grab the star...
		star = usnob_fits_read_entry(usnob);

		// find its xyz position
		radec2xyzarr(deg2rad(star->ra), deg2rad(star->dec), xyz);
		// project it around the center
		star_coords(xyz, center, &px, &py);
//		fprintf(stderr, "%g, %g,", px, py);

		buffer[0] = px;
		buffer[1] = py;

		for (j=0; j<5; j++){
			if(star->obs[j].mag > 0){
				numMags++;
				sumMags+= star->obs[j].mag;
			}
		}

		if (numMags > 0){
			buffer[2] = sumMags / (double)numMags;
		}
		else{
			buffer[2] = 0;
		}
		//fprintf(stderr, " %g, ", meanMag);

		for (j=0; j<5; j++){
			buffer[j+3] = star->obs[j].field;
			//fprintf(stderr, "%d, ", intBuffer[j]);
		}

		fwrite(buffer, sizeof(double), 8, output_file);

		//fprintf(stderr, "\n");

	}
	fclose(output_file);

	usnob_fits_close(usnob);

	return 0;
}


