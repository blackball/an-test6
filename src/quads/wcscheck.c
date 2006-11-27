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
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "wcs.h"
#include "wcshdr.h"
#include "qfits.h"
#include "starutil.h"
#include "bl.h"
#include "xylist.h"
#include "rdlist.h"

const char* OPTIONS = "hx:y:l:f:X:Y:r:";

void print_help(char* progname) {
	//boilerplate_help_header(stdout);
	printf("\nUsage: %s\n"
		   "  [-x <x-pixel-coord> -y <y-pixel>] (can be repeated)\n"
		   "  [-l <xy-list file> -f <xy-list field index>]\n"
		   "     [-X <x-column-name> -Y <y-column-name>]\n"
		   "  [-r <rdls output file>]\n"
		   "  <WCS-input-file>\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	int c;
	char* fn;
	struct wcsprm wcs;
	qfits_header* hdr;
	int Ncoords;
	int Nelems = 2;
	int i;
	int ncards;
	char* hdrstring;
	int hdrstringlen;
	FILE* f;
	dl* xpix;
	dl* ypix;
	char* xylsfn = NULL;
	xylist* xyls = NULL;
	int fieldind = 0;
	char* xcol = NULL;
	char* ycol = NULL;
	char* rdlsfn = NULL;
	rdlist* rdls = NULL;

	xpix = dl_new(16);
	ypix = dl_new(16);

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'r':
			rdlsfn = optarg;
			break;
		case 'l':
			xylsfn = optarg;
			break;
		case 'f':
			fieldind = atoi(optarg);
			break;
		case 'X':
			xcol = optarg;
			break;
		case 'Y':
			ycol = optarg;
			break;
		case 'x':
			dl_append(xpix, atof(optarg));
			break;
		case 'y':
			dl_append(ypix, atof(optarg));
			break;
		}
	}

	if (optind != argc - 1) {
		print_help(args[0]);
		exit(-1);
	}

	if (dl_size(xpix) != dl_size(ypix)) {
		printf("Number of x and y pixels must be equal!\n");
		print_help(args[0]);
		exit(-1);
	}

	if (xylsfn) {
		double* xyvals;
		int i;
		int nvals;
		xyls = xylist_open(xylsfn);
		if (!xyls) {
			fprintf(stderr, "Failed to read an xylist from file %s.\n", xylsfn);
			exit(-1);
		}
		if (xcol)
			xyls->xname = xcol;
		if (ycol)
			xyls->yname = ycol;
		nvals = xylist_n_entries(xyls, fieldind);
		xyvals = malloc(2 * nvals * sizeof(double));
		if (xylist_read_entries(xyls, fieldind, 0, nvals, xyvals)) {
			fprintf(stderr, "Failed to read xylist data from file %s.\n", xylsfn);
			exit(-1);
		}
		for (i=0; i<nvals; i++) {
			dl_append(xpix, xyvals[2*i + 0]);
			dl_append(ypix, xyvals[2*i + 1]);
		}
		free(xyvals);
	}

	if (rdlsfn) {
		rdls = rdlist_open_for_writing(rdlsfn);
		if (!rdls) {
			fprintf(stderr, "Failed to open file %s to write RDLS.\n", rdlsfn);
			exit(-1);
		}
		if (rdlist_write_header(rdls) ||
			rdlist_write_new_field(rdls)) {
			fprintf(stderr, "Failed to write header to RDLS file %s.\n", rdlsfn);
			exit(-1);
		}
	}

	Ncoords = dl_size(xpix);

	fn = args[optind];

	hdr = qfits_header_read(fn);
	if (!hdr) {
		fprintf(stderr, "Failed to read FITS header from file %s.\n", fn);
		exit(-1);
	}
	ncards = hdr->n;
	qfits_header_destroy(hdr);

	hdrstringlen = FITS_LINESZ * ncards;
	hdrstring = malloc(hdrstringlen);
	f = fopen(fn, "rb");
	if (fread(hdrstring, 1, hdrstringlen, f) != hdrstringlen) {
		fprintf(stderr, "Failed to read FITS header: %s\n", strerror(errno));
		exit(-1);
	}

	wcs.flag = -1;
	if (wcsini(1, 2, &wcs)) {
		fprintf(stderr, "wcsini() failed.\n");
		exit(-1);
	}

	{
		int nreject;
		int nwcs;
		int rtn;
		struct wcsprm* wcsii;
		rtn = wcspih(hdrstring, ncards, 0, 3, &nreject, &nwcs, &wcsii);
		if (rtn) {
			fprintf(stderr, "wcspih() failed: return val %i.\n", rtn);
		}
		printf("Got %i WCS representations, %i cards rejected.\n",
			   nwcs, nreject);
		// print it out...
		for (i=0; i<nwcs; i++) {
			printf("WCS %i:\n", i);
			wcsset(wcsii + i);
			wcsprt(wcsii + i);
			printf("\n");
		}

		if (nwcs == 1)
			wcs = wcsii[0];
	}

	{
		double pixels[Ncoords][Nelems];
		double imgcrd[Ncoords][Nelems];
		double phi[Ncoords];
		double theta[Ncoords];
		double world[Ncoords][Nelems];
		int status[Ncoords];
		int rtn;
		double ra, dec;
		double xyz[3];
		int i;

		for (i=0; i<Ncoords; i++) {
			pixels[i][0] = dl_get(xpix, i);
			pixels[i][1] = dl_get(ypix, i);
		}

		/*
		  pixels[0][0] = 0.0;
		  pixels[0][1] = 0.0;
		  pixels[1][0] = 0.0;
		  pixels[1][1] = 4096.0;
		  pixels[2][0] = 4096.0;
		  pixels[2][1] = 0.0;
		  pixels[3][0] = 4096.0;
		  pixels[3][1] = 4096.0;
		  pixels[0][0] = 25.9374;
		  pixels[0][1] = 75.5292;
		  pixels[1][0] = 2003.38;
		  pixels[1][1] = 1423.66;
		  pixels[2][0] = 25.9374;
		  pixels[2][1] = 1423.66;
		  pixels[3][0] = 75.5292;
		  pixels[3][1] = 2003.38;
		*/

		rtn = wcsp2s(&wcs, Ncoords, Nelems, (const double*)pixels,
					 (double*)imgcrd, 
					 phi, theta, (double*)world, status);
		if (rtn) {
			fprintf(stderr, "wcsp2s failed: return value %i\n", rtn);
			exit(-1);
		}

		for (i=0; i<Ncoords; i++) {
			printf("Status     %i\n", status[i]);
			printf("wcs.lng = %i, wcs.lat = %i.\n", wcs.lng, wcs.lat);
			printf("Pixel     (%g, %g)\n", pixels[i][0], pixels[i][1]);
			printf("Image     (%g, %g)\n", imgcrd[i][0], imgcrd[i][1]);
			printf("World     (%g, %g)\n", world[i][0], world[i][1]);
			ra  = deg2rad(world[i][0]);
			dec = deg2rad(world[i][1]);
			radec2xyzarr(ra, dec, xyz);
			printf("      xyz (%g, %g, %g)\n", xyz[0], xyz[1], xyz[2]);
			printf("Phi,Theta (%g, %g)\n", phi[i], theta[i]);
			printf("\n");

			if (rdls &&
				rdlist_write_entries(rdls, world[i], 1)) {
				fprintf(stderr, "Failed to write entries to RDLS file.\n");
				exit(-1);
			}
		}

		printf("worldra=[");
		for (i=0; i<Ncoords; i++)
			printf("%g,", world[i][0]);
		printf("];\n");
		printf("worlddec=[");
		for (i=0; i<Ncoords; i++)
			printf("%g,", world[i][1]);
		printf("];\n");

	}
	wcsfree(&wcs);

	if (rdls) {
		if (rdlist_fix_field(rdls) ||
			rdlist_fix_header(rdls) ||
			rdlist_close(rdls)) {
			fprintf(stderr, "Failed to close RDLS file.\n");
			exit(-1);
		}
	}

	free(hdrstring);
	return 0;
}
