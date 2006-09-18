#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "rdlist.h"
#include "fieldcheck.h"

char* OPTIONS = "hr:o:A:B:I:J:n:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] <input-match-file> ...\n"
			"   -r rdls-file-template\n"
			"   -o output-file\n"
			"   [-A <first-field>]  (default 0)\n"
			"   [-B <last-field>]   (default the largest field encountered)\n"
			"   [-I first-field-filenum]\n"
			"   [-J last-field-filenum]\n"
			"   [-n <number-of-RDLS-stars-to-check>]\n\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char* rdlsfname = NULL;
	rdlist* rdls = NULL;
	int nfields;
	int firstfield=0, lastfield=INT_MAX-1;
	int firstfieldfile=1, lastfieldfile=INT_MAX-1;
	int f;
	int fieldfile;
	double* radecs = NULL;
	double* xyz = NULL;
	fieldcheck_file* fcf;
	char* fcfname = NULL;
	int Ncheck;
	int lastfield_orig;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		case 'A':
			firstfield = atoi(optarg);
			break;
		case 'B':
			lastfield = atoi(optarg);
			break;
		case 'I':
			firstfieldfile = atoi(optarg);
			break;
		case 'J':
			lastfieldfile = atoi(optarg);
			break;
		case 'n':
			Ncheck = atoi(optarg);
			break;
		case 'r':
			rdlsfname = optarg;
			break;
		case 'o':
			fcfname = optarg;
			break;
		default:
			return (OPT_ERR);
		}
	}

	if (!rdlsfname || !fcfname) {
		fprintf(stderr, "You must specify an RDLS file and fieldcheck file!\n");
		printHelp(progname);
		exit(-1);
	}
	if (lastfield < firstfield) {
		fprintf(stderr, "Last field (-B) must be at least as big as first field (-A)\n");
		exit(-1);
	}
	lastfield_orig = lastfield;

	fcf = fieldcheck_file_open_for_writing(fcfname);
	if (!fcf) {
		fprintf(stderr, "Failed to open file %s for writing fieldcheck file.\n", fcfname);
		exit(-1);
	}
	if (fieldcheck_file_write_header(fcf)) {
		fprintf(stderr, "Failed to write fieldcheck file header.\n");
		exit(-1);
	}

	for (fieldfile=firstfieldfile; fieldfile<=lastfieldfile; fieldfile++) {
		fieldcheck fc;
		char fn[256];

		sprintf(fn, rdlsfname, fieldfile);
		printf("Reading rdls file %s ...\n", fn);
		fflush(stdout);
		rdls = rdlist_open(fn);
		if (!rdls) {
			fprintf(stderr, "Couldn't read rdls file.\n");
			exit(-1);
		}
		rdls->xname = "RA";
		rdls->yname = "DEC";

		nfields = rdls->nfields;
		printf("Read %i fields from rdls file.\n", nfields);
		lastfield = lastfield_orig;
		if (nfields < lastfield) {
			lastfield = nfields - 1;
		}

		for (f=firstfield; f<=lastfield; f++) {
			int fieldnum = f;
			int j;
			int M;
			double xavg, yavg, zavg;
			double fieldrad2;
			double dist2;
			double x, y, z;
			double ra, dec;

			if (f % 100 == 0) {
				printf(".");
				fflush(stdout);
			}

			M = rdlist_n_entries(rdls, fieldnum);
			if (Ncheck && Ncheck < M)
				M = Ncheck;

			radecs = realloc(radecs, 2*M*sizeof(double));
			rdlist_read_entries(rdls, fieldnum, 0, M, radecs);

			xyz = realloc(xyz, M * 3 * sizeof(double));
			xavg = yavg = zavg = 0.0;
			for (j=0; j<M; j++) {
				ra  = deg2rad(radecs[2*j]);
				dec = deg2rad(radecs[2*j + 1]);
				x = radec2x(ra, dec);
				y = radec2y(ra, dec);
				z = radec2z(ra, dec);
				xavg += x;
				yavg += y;
				zavg += z;
				xyz[j*3 + 0] = x;
				xyz[j*3 + 1] = y;
				xyz[j*3 + 2] = z;
			}
			xavg /= (double)M;
			yavg /= (double)M;
			zavg /= (double)M;

			ra = xy2ra(xavg, yavg);
			dec = z2dec(zavg);

			// make another sweep through, finding the field star furthest from the mean.
			// this gives an estimate of the field radius.
			fieldrad2 = 0.0;
			for (j=0; j<M; j++) {
				x = xyz[j*3 + 0];
				y = xyz[j*3 + 1];
				z = xyz[j*3 + 2];
				dist2 = square(x - xavg) + square(y - yavg) + square(z - zavg);
				if (dist2 > fieldrad2)
					fieldrad2 = dist2;
			}

			fc.filenum = fieldfile;
			fc.fieldnum = fieldnum;
			fc.ra  = rad2deg(ra);
			fc.dec = rad2deg(dec);
			fc.radius = distsq2arcsec(fieldrad2);

			fieldcheck_file_write_entry(fcf, &fc);
		}
		printf("\n");

		rdlist_close(rdls);
	}
	free(radecs);
	free(xyz);

	if (fieldcheck_file_fix_header(fcf) ||
		fieldcheck_file_close(fcf)) {
		fprintf(stderr, "Failed to write fieldcheck file header.\n");
		exit(-1);
	}

	return 0;
}
