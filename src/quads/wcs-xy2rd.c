/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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
		   "   -i <xyls input file>\n"
		   "   -o <rdls output file>\n"
		   "  [-f <xyls field index>] (default: all)\n"
		   "  [-X <x-column-name> -Y <y-column-name>]\n"
		   "  [-t]: just use TAN projection, even if SIP extension exists.\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	int c;
	char* xylsfn = NULL;
	char* wcsfn = NULL;
	char* rdlsfn = NULL;
	char* xcol = NULL;
	char* ycol = NULL;
	bool forcetan = FALSE;

	bool hassip = FALSE;
	rdlist* rdls = NULL;
	xylist* xyls = NULL;
	il* fields;
	sip_t sip;
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
			rdlsfn = optarg;
			break;
		case 'i':
			xylsfn = optarg;
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

	if (!rdlsfn || !xylsfn || !wcsfn) {
		print_help(args[0]);
		exit(-1);
	}

	// read WCS.
	if (forcetan) {
		memset(&sip, 0, sizeof(sip_t));
		if (!tan_read_header_file(wcsfn, &(sip.wcstan))) {
			fprintf(stderr, "Failed to parse TAN header from %s.\n", wcsfn);
			exit(-1);
		}
	} else {
		if (!sip_read_header_file(wcsfn, &sip)) {
			printf("Failed to parse SIP header from %s.\n", wcsfn);
			exit(-1);
		}
	}

	// read XYLS.
	xyls = xylist_open(xylsfn);
	if (!xyls) {
		fprintf(stderr, "Failed to read an xylist from file %s.\n", xylsfn);
		exit(-1);
	}
	if (xcol)
		xylist_set_xname(xyls, xcol);
	if (ycol)
		xylist_set_yname(xyls, ycol);

	// write RDLS.
	rdls = rdlist_open_for_writing(rdlsfn);
	if (!rdls) {
		fprintf(stderr, "Failed to open file %s to write RDLS.\n", rdlsfn);
		exit(-1);
	}
	if (rdlist_write_header(rdls)) {
		fprintf(stderr, "Failed to write header to RDLS file %s.\n", rdlsfn);
		exit(-1);
	}

	if (!il_size(fields)) {
		// add all fields.
		int NF = xylist_n_fields(xyls);
		for (i=1; i<=NF; i++)
			il_append(fields, i);
	}

	fprintf(stderr, "Processing %i extensions...\n", il_size(fields));
	for (i=0; i<il_size(fields); i++) {
		int fieldind = il_get(fields, i);
		double* xyvals;
		int nvals;
		int j;

		nvals = xylist_n_entries(xyls, fieldind);
		xyvals = malloc(2 * nvals * sizeof(double));
		if (xylist_read_entries(xyls, fieldind, 0, nvals, xyvals)) {
			fprintf(stderr, "Failed to read xyls data.\n");
			exit(-1);
		}

		if (rdlist_write_new_field(rdls)) {
			fprintf(stderr, "Failed to write rdls field header.\n");
			exit(-1);
		}

		for (j=0; j<nvals; j++) {
			double radec[2];
			if (hassip) {
				sip_pixelxy2radec(&sip, xyvals[j*2+0], xyvals[j*2+1], &(radec[0]), &(radec[1]));
			} else {
				tan_pixelxy2radec(&(sip.wcstan), xyvals[j*2+0], xyvals[j*2+1], &(radec[0]), &(radec[1]));
			}

			if (rdlist_write_entries(rdls, radec, 1)) {
				fprintf(stderr, "Failed to write rdls entry.\n");
				exit(-1);
			}
		}
		free(xyvals);

		if (rdlist_fix_field(rdls)) {
			fprintf(stderr, "Failed to fix rdls field header.\n");
			exit(-1);
		}
	}

	if (rdlist_fix_header(rdls) ||
		rdlist_close(rdls)) {
		fprintf(stderr, "Failed to fix header of RDLS file.\n");
		exit(-1);
	}

	if (xylist_close(xyls)) {
		fprintf(stderr, "Failed to close XYLS file.\n");
	}

	fprintf(stderr, "Done!\n");

	return 0;
}
