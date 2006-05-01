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

		infn = args[optind];
	}

	return 0;
}
