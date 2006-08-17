#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <zlib.h>

#include "2mass.h"
#include "2mass_catalog.h"
#include "healpix.h"
#include "starutil.h"

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
	int i;

	twomass_catalog** cats;

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
	cats = calloc(HP, sizeof(twomass_catalog*));

	printf("Nside = %i, using %i healpixes.\n", Nside, HP);

	printf("Reading 2MASS files... ");
	fflush(stdout);

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		gzFile* fiz = NULL;
		char line[1024];
		int nentries;

		infn = args[optind];
		printf("\nReading file %i of %i: %s\n", 1 + optind - startoptind,
			   argc - startoptind, infn);
		infn = args[optind];
		fiz = gzopen(infn, "rb");
		if (!fiz) {
			fprintf(stderr, "Failed to open file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}
		nentries = 0;
		for (;;) {
			twomass_entry e;
			int hp;

			if (gzeof(fiz))
				break;

			if (gzgets(fiz, line, 1024) == Z_NULL) {
				if (gzeof(fiz))
					break;
				fprintf(stderr, "Failed to read a line from file %s: %s\n", infn, strerror(errno));
				exit(-1);
			}
			/*
			  if (!strlen(line))
			  break;
			*/

			if (twomass_parse_entry(&e, line)) {
				fprintf(stderr, "Failed to parse 2MASS entry from file %s.\n", infn);
				exit(-1);
			}

			hp = radectohealpix_nside(deg2rad(e.ra), deg2rad(e.dec), Nside);
			if (!cats[hp]) {
				char fn[256];
				sprintf(fn, outfn, hp);
				cats[hp] = twomass_catalog_open_for_writing(fn);
				if (!cats[hp]) {
					fprintf(stderr, "Failed to open 2MASS catalog for writing to file %s (hp %i).\n", fn, hp);
					exit(-1);
				}
				if (twomass_catalog_write_headers(cats[hp])) {
					fprintf(stderr, "Failed to write 2MASS catalog headers: %s\n", fn);
					exit(-1);
				}
			}
			if (twomass_catalog_write_entry(cats[hp], &e)) {
				fprintf(stderr, "Failed to write 2MASS catalog entry.\n");
				exit(-1);
			}

			nentries++;
			if (!(nentries % 100000)) {
				printf(".");
				fflush(stdout);
			}
		}
		gzclose(fiz);
		printf("\n");

		printf("Read %i entries.\n", nentries);
	}

	for (i=0; i<HP; i++) {
		if (!cats[i])
			continue;
		if (twomass_catalog_fix_headers(cats[i]) ||
			twomass_catalog_close(cats[i])) {
			fprintf(stderr, "Failed to close 2MASS catalog.\n");
			exit(-1);
		}
	}
	free(cats);

	return 0;
}

