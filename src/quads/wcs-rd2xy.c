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

#include "sip_qfits.h"
#include "an-bool.h"
#include "qfits.h"
#include "starutil.h"
#include "bl.h"
#include "xylist.h"
#include "rdlist.h"
#include "boilerplate.h"

const char* OPTIONS = "hi:o:w:f:X:Y:t";

void print_help(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s\n"
		   "   -w <WCS input file>\n"
		   "   -i <rdls input file>\n"
		   "   -o <xyls output file>\n"
		   "  [-f <rdls field index>] (default: all)\n"
		   "  [-X <x-column-name> -Y <y-column-name>]\n"
		   "  [-t]: just use TAN projection, even if SIP extension exists.\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	int c;
	char* rdlsfn = NULL;
	char* wcsfn = NULL;
	char* xylsfn = NULL;
	char* xcol = NULL;
	char* ycol = NULL;
	bool forcetan = FALSE;

	bool hassip = FALSE;
	xylist* xyls = NULL;
	rdlist* rdls = NULL;
	il* fields;
	sip_t sip;
	qfits_header* hdr;
	int i;

	fields = il_new(16);

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
        case 'h':
			print_help(args[0]);
			exit(0);
		case 't':
			forcetan = TRUE;
			break;
		case 'o':
			xylsfn = optarg;
			break;
		case 'i':
			rdlsfn = optarg;
			break;
		case 'w':
			wcsfn = optarg;
			break;
		case 'f':
			il_append(fields, atoi(optarg));
			break;
		case 'X':
			xcol = optarg;
			break;
		case 'Y':
			ycol = optarg;
			break;
		}
	}

	if (optind != argc) {
		print_help(args[0]);
		exit(-1);
	}

	if (!xylsfn || !rdlsfn || !wcsfn) {
		print_help(args[0]);
		exit(-1);
	}

	// read WCS.
	hdr = qfits_header_read(wcsfn);
	if (!hdr) {
		fprintf(stderr, "Failed to read FITS header from file %s.\n", wcsfn);
		exit(-1);
	}

	if (!forcetan) {
		fprintf(stderr, "Trying to parse SIP header from %s...\n", wcsfn);
		if (sip_read_header(hdr, &sip)) {
			fprintf(stderr, "Got SIP header.\n");
			hassip = TRUE;
		} else {
			fprintf(stderr, "Failed to parse SIP header from %s.\n", wcsfn);
		}
	}
	if (!hassip) {
		fprintf(stderr, "Trying to parse TAN header from %s...\n", wcsfn);
		if (tan_read_header(hdr, &(sip.wcstan))) {
			fprintf(stderr, "Got TAN header.\n");
		} else {
			fprintf(stderr, "Failed to parse TAN header from %s.\n", wcsfn);
			exit(-1);
		}
	}

	// read RDLS.
	rdls = rdlist_open(rdlsfn);
	if (!rdls) {
		fprintf(stderr, "Failed to read an rdlist from file %s.\n", rdlsfn);
		exit(-1);
	}
	if (xcol)
		rdls->xname = xcol;
	if (ycol)
		rdls->yname = ycol;

	// write XYLS.
	xyls = xylist_open_for_writing(xylsfn);
	if (!xyls) {
		fprintf(stderr, "Failed to open file %s to write XYLS.\n", xylsfn);
		exit(-1);
	}
	if (xylist_write_header(xyls)) {
		fprintf(stderr, "Failed to write header to XYLS file %s.\n", xylsfn);
		exit(-1);
	}

	if (!il_size(fields)) {
		// add all fields.
		int NF = rdlist_n_fields(rdls);
		for (i=0; i<NF; i++)
			il_append(fields, i);
	}

	fprintf(stderr, "Processing %i extensions...\n", il_size(fields));
	for (i=0; i<il_size(fields); i++) {
		int fieldind = il_get(fields, i);
		double* rdvals;
		int nvals;
		int j;

		nvals = rdlist_n_entries(rdls, fieldind);
		rdvals = malloc(2 * nvals * sizeof(double));
		if (rdlist_read_entries(rdls, fieldind, 0, nvals, rdvals)) {
			fprintf(stderr, "Failed to read rdls data.\n");
			exit(-1);
		}

		if (xylist_write_new_field(xyls)) {
			fprintf(stderr, "Failed to write xyls field header.\n");
			exit(-1);
		}

		for (j=0; j<nvals; j++) {
			double xy[2];
			if (hassip) {
				sip_radec2pixelxy(&sip, rdvals[j*2+0], rdvals[j*2+1], &(xy[0]), &(xy[1]));
			} else {
				tan_radec2pixelxy(&(sip.wcstan), rdvals[j*2+0], rdvals[j*2+1], &(xy[0]), &(xy[1]));
			}

			if (xylist_write_entries(xyls, xy, 1)) {
				fprintf(stderr, "Failed to write xyls entry.\n");
				exit(-1);
			}
		}
		free(rdvals);

		if (xylist_fix_field(xyls)) {
			fprintf(stderr, "Failed to fix xyls field header.\n");
			exit(-1);
		}
	}

	if (xylist_fix_header(xyls) ||
		xylist_close(xyls)) {
		fprintf(stderr, "Failed to fix header of XYLS file.\n");
		exit(-1);
	}

	fprintf(stderr, "Done!\n");

	return 0;
}
