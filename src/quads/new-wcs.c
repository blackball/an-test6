/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>

#include "qfits.h"
#include "qfits_error.h"
#include "an-bool.h"
#include "fitsioutils.h"

static const char* OPTIONS = "hi:w:o:";

static void printHelp(char* progname) {
	printf("%s    -i <input-file>\n"
		   "      -w <WCS-file>\n"
		   "      -o <output-file>\n"
		   "\n",
		   progname);
}

static char* exclude_input[] = {
	// TAN
	"^CTYPE.*",
	"^WCSAXES$",
	"^EQUINOX$",
	"^LONPOLE$",
	"^LATPOLE$",
	"^CRVAL.*",
	"^CRPIX.*",
	"^CUNIT.*",
	"^CD[12]_[12]$",
	// SIP
	"^[AB]P?_ORDER$",
	"^[AB]P?_[[:digit:]]_[[:digit:]]$",
	"^END$",
};
static int NE1 = sizeof(exclude_input) / sizeof(char*);

static char* exclude_wcs[] = {
	"^SIMPLE$",
	"^BITPIX$",
	"^EXTEND$",
	"^NAXIS$",
	"^END$",
};
static int NE2 = sizeof(exclude_wcs) / sizeof(char*);

static bool key_matches(char* key, regex_t* res, char** re_strings, int NE, int* rematched) {
	int e;
	for (e=0; e<NE; e++) {
		regmatch_t match[1];
		int errcode;
		errcode = regexec(res + e, key, sizeof(match)/sizeof(regmatch_t), match, 0);
		if (errcode == REG_NOMATCH)
			continue;
		if (errcode) {
			char err[256];
			regerror(errcode, res + e, err, sizeof(err));
			fprintf(stderr, "Failed to match regular expression \"%s\" with string \"%s\": %s\n", re_strings[e], key, err);
			exit(-1);
		}
		if (rematched)
			*rematched = e;
		return TRUE;
	}
	return FALSE;
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* infn = NULL;
	char* outfn = NULL;
	char* wcsfn = NULL;
	FILE* outfid = NULL;
	char* progname = argv[0];
	int i, N;
	int e;
	regex_t re1[NE1];
	regex_t re2[NE2];
	qfits_header *inhdr, *outhdr, *wcshdr;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'i':
			infn = optarg;
			break;
        case 'o':
			outfn = optarg;
			break;
        case 'w':
			wcsfn = optarg;
			break;
        case '?':
        case 'h':
			printHelp(progname);
            return 0;
        default:
            return -1;
        }

	if (!infn || !outfn || !wcsfn) {
		printHelp(progname);
		exit(-1);
	}

	// turn on QFITS error reporting.
	qfits_err_statset(1);

	outfid = fopen(outfn, "wb");
	if (!outfid) {
		fprintf(stderr, "Failed to open output file %s: %s\n", outfn, strerror(errno));
		exit(-1);
	}

	inhdr = qfits_header_read(infn);
	if (!inhdr) {
		fprintf(stderr, "Failed to read FITS header from input file %s.\n", infn);
		exit(-1);
	}
	wcshdr = qfits_header_read(wcsfn);
	if (!wcshdr) {
		fprintf(stderr, "Failed to read FITS header from WCS file %s.\n", wcsfn);
		exit(-1);
	}

	outhdr = qfits_header_new();
	if (!outhdr) {
		fprintf(stderr, "Failed to allocate new output FITS header.\n");
		exit(-1);
	}

	// Compile regular expressions...
	for (e=0; e<NE1; e++) {
		int errcode;
		errcode = regcomp(re1 + e, exclude_input[e], REG_EXTENDED);
		if (errcode) {
			char err[256];
			regerror(errcode, re1 + e, err, sizeof(err));
			fprintf(stderr, "Failed to compile regular expression \"%s\": %s\n", exclude_input[e], err);
			exit(-1);
		}
	}
	for (e=0; e<NE2; e++) {
		int errcode;
		errcode = regcomp(re2 + e, exclude_wcs[e], REG_EXTENDED);
		if (errcode) {
			char err[256];
			regerror(errcode, re2 + e, err, sizeof(err));
			fprintf(stderr, "Failed to compile regular expression \"%s\": %s\n", exclude_wcs[e], err);
			exit(-1);
		}
	}

	fprintf(stderr, "Reading input file FITS headers...\n");

	N = inhdr->n;
	for (i=0; i<N; i++) {
		char key[FITS_LINESZ + 1];
		char val[FITS_LINESZ + 1];
		char comment[FITS_LINESZ + 1];
		int imatch;

		if (qfits_header_getitem(inhdr, i, key, val, comment, NULL)) {
			fprintf(stderr, "Failed to read FITS header card %i from input file.\n", i);
			exit(-1);
		}

		if (key_matches(key, re1, exclude_input, NE1, &imatch)) {
			printf("Regular expression matched: \"%s\", key \"%s\".\n", exclude_input[imatch], key);
			char newkey[FITS_LINESZ + 1];
			snprintf(newkey, FITS_LINESZ+1, "Original key: \"%s\"", key);
			qfits_header_append(outhdr, "COMMENT", newkey, NULL, NULL);
			snprintf(newkey, FITS_LINESZ+1, "_%.7s", key);
			strcpy(key, newkey);
		}

		qfits_header_append(outhdr, key, val, comment, NULL);
	}

	fprintf(stderr, "Reading WCS file FITS headers...\n");

	qfits_header_append(outhdr, "COMMENT", "", NULL, NULL);
	qfits_header_append(outhdr, "COMMENT", "--Start of Astrometry.net WCS solution--", NULL, NULL);
	qfits_header_append(outhdr, "COMMENT", "", NULL, NULL);

	N = wcshdr->n;
	for (i=0; i<N; i++) {
		char key[FITS_LINESZ + 1];
		char val[FITS_LINESZ + 1];
		char comment[FITS_LINESZ + 1];
		int imatch;

		if (qfits_header_getitem(wcshdr, i, key, val, comment, NULL)) {
			fprintf(stderr, "Failed to read FITS header card %i from WCS file.\n", i);
			exit(-1);
		}

		if (key_matches(key, re2, exclude_wcs, NE2, &imatch)) {
			printf("Regular expression matched: \"%s\", key \"%s\".\n", exclude_wcs[imatch], key);
			char newkey[FITS_LINESZ + 1];
			snprintf(newkey, FITS_LINESZ+1, "Original WCS key: \"%s\"", key);
			qfits_header_append(outhdr, "COMMENT", newkey, NULL, NULL);
			snprintf(newkey, FITS_LINESZ+1, "_%.7s", key);
			strcpy(key, newkey);
		}

		qfits_header_append(outhdr, key, val, comment, NULL);
	}

	qfits_header_append(outhdr, "COMMENT", "", NULL, NULL);
	qfits_header_append(outhdr, "COMMENT", "--End of WCS--", NULL, NULL);
	qfits_header_append(outhdr, "COMMENT", "", NULL, NULL);

	qfits_header_append(outhdr, "END", NULL, NULL, NULL);

	if (qfits_header_dump(outhdr, outfid) ||
		fclose(outfid)) {
		fprintf(stderr, "Failed to write output header.\n");
		exit(-1);
	}

	qfits_header_destroy(inhdr);
	qfits_header_destroy(wcshdr);
	qfits_header_destroy(outhdr);

	// Free regular expressions...
	for (e=0; e<NE1; e++) {
		regfree(re1 + e);
	}
	for (e=0; e<NE2; e++) {
		regfree(re2 + e);
	}

	return 0;
}
