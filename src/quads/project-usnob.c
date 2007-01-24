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
		   "  %s <usnob-file.fits>\n"
		   , progname);
	//-H <healpix> -N <nside> 
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int c;
	char* infn;
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
	if (optind != (argc-1)) {
		print_help(args[0]);
		exit(-1);
	}

	// try to open the file.
	infn = args[optind];
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
	printf("allstars=[\n");
	for (i=0; i<N; i++) {
		double xyz[3];
		double px, py;
		usnob_entry* star;
		// grab the star...
		star = usnob_fits_read_entry(usnob);

		// only output the stars flagged as diffraction spikes
		// NOTE: to output all stars, set diffraction_only to false 
		bool diffraction_only = 1;
		if (diffraction_only && star->diffraction_spike){
			// find its xyz position
			radec2xyzarr(deg2rad(star->ra), deg2rad(star->dec), xyz);
			// project it around the center
			star_coords(xyz, center, &px, &py);
			printf("%g, %g,", px, py);


			for (j=0; j<5; j++){
				printf("%g", star->obs[j].mag);
		  		fprintf(stderr, "%c ", usnob_get_survey_band(star->obs[j].survey));
		  		if(j<4){
		    		printf(", ");
		  		}
			}

			printf(";\n");
			fprintf(stderr, "\n");
		}

	}
	printf("];\n");

	usnob_fits_close(usnob);

	return 0;
}

