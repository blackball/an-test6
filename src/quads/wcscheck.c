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
#include "qfits.h"
#include "starutil.h"
#include "bl.h"
#include "xylist.h"
#include "rdlist.h"

const char* OPTIONS = "hx:y:l:f:X:Y:r:I";

void print_help(char* progname) {
	//boilerplate_help_header(stdout);
	printf("\nUsage: %s\n"
		   "  [-x <x-pixel-coord> -y <y-pixel>] (can be repeated)\n"
		   "  [-l <xy-list file> -f <xy-list field index>]\n"
		   "     [-X <x-column-name> -Y <y-column-name>]\n"
		   "  [-r <rdls output file>]\n"
		   "  [-I]: invert (rdls -> xyls)\n"
		   "  <WCS-input-file>\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	int c;
	char* fn;
	struct WorldCoor* wcs;
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
	int invert = 0;

	xpix = dl_new(16);
	ypix = dl_new(16);

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'I':
			invert = 1;
			break;
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

	if (invert && !(xylsfn && rdlsfn)) {
		printf("To invert, you must supply RDLS and XYLS filenames.\n");
		print_help(args[0]);
		exit(-1);
	}

	if (xylsfn && !invert) {
		double* xyvals;
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

	if (rdlsfn && !invert) {
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

	if (invert) {
		double* rdvals;
		int i;
		int nvals;
		xyls = xylist_open_for_writing(xylsfn);
		if (!xyls) {
			fprintf(stderr, "Failed to write xylist to file %s.\n", xylsfn);
			exit(-1);
		}
		if (xcol)
			xyls->xname = xcol;
		if (ycol)
			xyls->yname = ycol;
		if (xylist_write_header(xyls) ||
			xylist_write_new_field(xyls)) {
			fprintf(stderr, "Failed to write header to XYLS file %s.\n", xylsfn);
			exit(-1);
		}
		rdls = rdlist_open(rdlsfn);
		if (!rdls) {
			fprintf(stderr, "Failed to read an RDLS from file %s.\n", rdlsfn);
			exit(-1);
		}
		nvals = rdlist_n_entries(rdls, fieldind);
		rdvals = malloc(2 * nvals * sizeof(double));
		if (rdlist_read_entries(rdls, fieldind, 0, nvals, rdvals)) {
			fprintf(stderr, "Failed to read RDLS data from file %s.\n", rdlsfn);
			exit(-1);
		}
		for (i=0; i<nvals; i++) {
			dl_append(xpix, rdvals[2*i + 0]);
			dl_append(ypix, rdvals[2*i + 1]);
		}
		free(rdvals);
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

	wcs = wcsninit(hdrstring, hdrstringlen);
	if (!wcs) {
		fprintf(stderr, "Failed to parse WCS from FITS header.\n");
		exit(-1);
	}

	if (!iswcs(wcs)) {
		fprintf(stderr, "No WCS structure found.\n");
		exit(-1);
	}

	{
		double pixels[Ncoords][Nelems];
		double world[Ncoords][Nelems];
		double u, v, ra, dec;
		double xyz[3];
		int i;

		for (i=0; i<Ncoords; i++) {
			if (invert) {
				int offscale;
				ra  = dl_get(xpix, i);
				dec = dl_get(ypix, i);
				wcs2pix(wcs, ra, dec, &u, &v, &offscale);
			} else {
				u = dl_get(xpix, i);
				v = dl_get(ypix, i);
				pix2wcs(wcs, u, v, &ra, &dec);
			}
			pixels[i][0] = u;
			pixels[i][1] = v;
			world [i][0] = ra;
			world [i][1] = dec;
		}

		for (i=0; i<Ncoords; i++) {
			printf("Pixel     (%g, %g)\n", pixels[i][0], pixels[i][1]);
			printf("World     (%g, %g)\n", world[i][0], world[i][1]);
			ra  = deg2rad(world[i][0]);
			dec = deg2rad(world[i][1]);
			radec2xyzarr(ra, dec, xyz);
			printf("      xyz (%g, %g, %g)\n", xyz[0], xyz[1], xyz[2]);
			printf("\n");

			if (invert) {
				if (xylist_write_entries(xyls, pixels[i], 1)) {
					fprintf(stderr, "Failed to write entry to XYLS file.\n");
					exit(-1);
				}
			} else {
				if (rdls &&
					rdlist_write_entries(rdls, world[i], 1)) {
					fprintf(stderr, "Failed to write entries to RDLS file.\n");
					exit(-1);
				}
			}

		}
	}
	wcsfree(&wcs);

	if (invert) {
		if (xylist_fix_field(rdls) ||
			xylist_fix_header(rdls) ||
			xylist_close(rdls)) {
			fprintf(stderr, "Failed to close XYLS file.\n");
			exit(-1);
		}
	}

	if (!invert && rdls) {
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
