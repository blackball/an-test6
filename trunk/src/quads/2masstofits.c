#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <zlib.h>

#include "2mass.h"

#define OPTIONS "ho:N:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-N <healpix-nside>]  (default = 8.)\n"
		   "  <input-file> [<input-file> ...]\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int c;
	char* outfn = NULL;
	int startoptind;
	int Nside = 8;
	int HP;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'N':
			Nside = atoi(optarg);
			break;
		case 'o':
			outfn = optarg;
			break;
		}
    }

	if (!outfn || (optind == argc)) {
		print_help(args[0]);
		exit(-1);
	}

	if (Nside < 1) {
		fprintf(stderr, "Nside must be >= 1.\n");
		print_help(args[0]);
		exit(-1);
	}

	HP = 12 * Nside * Nside;

	printf("Nside = %i, using %i healpixes.\n", Nside, HP);

	printf("Reading 2MASS files... ");
	fflush(stdout);

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		gzFile* fiz = NULL;
		//int i;
		char line[1024];

		infn = args[optind];
		printf("\nReading file %i of %i: %s\n", optind - startoptind,
			   argc - startoptind, infn);
		infn = args[optind];
		fiz = gzopen(infn, "rb");
		if (!fiz) {
			fprintf(stderr, "Failed to open file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}

		for (;;) {
			twomass_entry e;

			if (!gzgets(fiz, line, 1024)) {
				fprintf(stderr, "Failed to read a line from file %s: %s\n", infn, strerror(errno));
				exit(-1);
			}
			if (!strlen(line))
				break;

			if (twomass_parse_entry(&e, line)) {
				fprintf(stderr, "Failed to parse 2MASS entry from file %s.\n", infn);
				exit(-1);
			}
		}

		gzclose(fiz);
	}

	return 0;
}

