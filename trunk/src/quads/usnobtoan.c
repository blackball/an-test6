#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "usnob_fits.h"
#include "qfits.h"
#include "starutil.h"
#include "fitsioutils.h"

#define OPTIONS "ho:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename>\n"
		   "  <input-file> [<input-file> ...]\n"
		   "\nInput files must be FITS format USNO-B files.\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	char* outfn = NULL;
	int startoptind;
    int c;
	an_catalog* cat;

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

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		usnob_fits* usnob;
		int BLOCK = 1000;
		usnob_entry entries[BLOCK];
		int offset, n, i;

		infn = args[optind];
		usnob = usnob_fits_open(infn);
		if (!usnob) {
			fprintf(stderr, "Couldn't open input file %s.\n", infn);
			exit(-1);
		}

		offset = 0;
		while (offset < usnob_fits_count_entries(usnob)) {
			n = imin(offset + BLOCK, usnob_fits_count_entries(usnob));
			if (usnob_fits_read_entries(usnob, offset, n, entries) == -1) {
				fprintf(stderr, "Error reading entries %i to %i in file %s.\n",
						offset, offset+n-1, infn);
				exit(-1);
			}

			for (i=0; i<n; i++) {
			}

			offset += n;
		}

	}

	return 0;
}
