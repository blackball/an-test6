#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "an_catalog.h"
#include "boilerplate.h"
#include "fitsioutils.h"

#define OPTIONS "ho:"

static void print_help(char* progname) {
	boilerplate_help_header(stdout);
    printf("\nUsage: %s\n"
		   "   -o <output-filename>     (eg, an-voss.fits)\n"
		   "  <input-file> [<input-file> ...]\n"
		   "\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	char* outfn = NULL;
	int c;
	int startoptind;
	an_catalog* cat;
	char val[64];

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'o':
			outfn = optarg;
			break;
		}
    }

	if (!outfn || (optind == argc)) {
		print_help(args[0]);
		exit(-1);
	}

	cat = an_catalog_open_for_writing(outfn);
	if (!cat) {
		fprintf(stderr, "Failed to initialized FITS output file %s.\n", outfn);
		exit(-1);
	}
	boilerplate_add_fits_headers(cat->header);
	qfits_header_add(cat->header, "HISTORY", "Created by the program \"vossetal2006toan\"", NULL, NULL);
	qfits_header_add(cat->header, "HISTORY", "command line:", NULL, NULL);
	fits_add_args(cat->header, args, argc);
	qfits_header_add(cat->header, "HISTORY", "(end of command line)", NULL, NULL);

	if (an_catalog_write_headers(cat)) {
		fprintf(stderr, "Failed to write header for FITS file %s.\n", outfn);
		exit(-1);
	}

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		an_entry an;
		//char* map;
		//size_t mapsize;
		FILE* fid;
		//struct stat st;
		//int SIZE = 105;
		int i;

		infn = args[optind];
		printf("Opening catalog file %s...\n", infn);
		fid = fopen(infn, "rb");
		if (!fid) {
			fprintf(stderr, "Failed to open input file %s.\n", infn);
			return -1;
		}

		/*
		  if (fstat(fileno(fid), &st)) {
		  fprintf(stderr, "Failed to fstat.\n");
		  return -1;
		  }
		  mapsize = st.st_size;
		*/

		//for (i=0; i<(mapsize/SIZE); i++) {
		for (i=0;; i++) {
			char line[256];
			char* pline;
			int rahr, ramin, decdeg, decmin;
			double rasec, decsec;
			char decsign;
			int ob;

			//line = map + i*SIZE + 32;

			if (!fgets(line, sizeof(line), fid)) {
				if (feof(fid))
					break;
				fprintf(stderr, "Error reading: %s\n", strerror(errno));
				exit(-1);
			}

			pline = line + 32;
			printf("line: %.30s...\n", pline);

			if (sscanf(pline, "%d %d %lf %c %d %d %lf",
					   &rahr, &ramin, &rasec,
					   &decsign, &decdeg, &decmin, &decsec) != 7) {
				fprintf(stderr, "Failed to parse line %i.\n", i);
				exit(-1);
			}

			memset(&an, 0, sizeof(an));
			an.ra = (rahr + ramin/60.0 + rasec/3600.0) * 15.0;
			an.dec = (decdeg + decmin/60.0) * (decsign == '-' ? -1.0 : 1.0);

			ob = 0;
			//an.obs[ob].catalog = AN_SOURCE_UNKNOWN;
			//an.obs[ob].band = 'X';

			// trick cut-an.
			an.obs[ob].catalog = AN_SOURCE_USNOB;
			an.obs[ob].band = 'E';
			
			an.obs[ob].id = i;
			an.obs[ob].mag = 1.0;
			ob++;
			an.nobs = ob;

			an_catalog_write_entry(cat, &an);
		}
		fclose(fid);

		printf("Wrote %i entries.\n", i);
	}

	sprintf(val, "%u", cat->nentries);
	qfits_header_mod(cat->header, "NOBJS", val, "Number of objects in this catalog.");
	if (an_catalog_fix_headers(cat) ||
		an_catalog_close(cat)) {
		fprintf(stderr, "Error fixing the header or closing AN catalog.\n");
		exit(-1);
	}
	return 0;
}
