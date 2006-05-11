#include <math.h>

#include "an_catalog.h"
#include "healpix.h"
#include "starutil.h"

#define OPTIONS "ho:N:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-N <nside>]: healpixelization at the fine-scale; default=100.\n"
		   "  <input-file> [<input-file> ...]\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	char* outfn = NULL;
	int c;
	int startoptind;
	int i, j, HP;
	int Nside = 100;
	unsigned char* owned;
	an_catalog* cats[12];

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

	HP = 12 * Nside * Nside;

	printf("Nside=%i, HP=%i.\n", Nside, HP);

	for (i=0; i<12; i++) {
		char fn[256];
		sprintf(fn, outfn, i);
		cats[i] = an_catalog_open_for_writing(fn);
		if (!cats[i]) {
			fprintf(stderr, "Failed to open output catalog file %s.\n", fn);
			exit(-1);
		}
		if (an_catalog_write_headers(cats[i])) {
			fprintf(stderr, "Failed to write catalog headers for %s.\n", fn);
			exit(-1);
		}
	}

	owned = malloc(12 * HP);
	memset(owned, 0, 12 * HP);
	// for each big healpix, find the set of small healpixes it owns
	// (including a bit of overlap)
	// (only set owned[i] if healpix 'i' actually contains stars.)
	for (j=0; j<12; j++) {
		for (i=0; i<HP; i++) {
			uint big, x, y;
			uint nn, neigh[8], k;
			healpix_decompose(i, &big, &x, &y, Nside);
			if (big != j)
				continue;
			owned[j*HP + i] = 1;
			if (x == 0 || y == 0 || (x == Nside-1) || (y == Nside-1)) {
				// add its neighbours.
				nn = healpix_get_neighbours_nside(i, neigh, Nside);
				for (k=0; k<nn; k++)
					owned[j*HP + neigh[k]] = 1;
			}
		}
	}

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		int hp;
		int N;
		int BLOCK = 1000;
		int off, n;
		an_catalog* ancat;

		infn = args[optind];
		ancat = an_catalog_open(infn);
		if (!ancat) {
			fprintf(stderr, "Couldn't open Astrometry.net catalog %s.\n", infn);
			exit(-1);
		}
		N = an_catalog_count_entries(ancat);
		printf("Reading %i entries from %s...\n", N, infn);
		fflush(stdout);
		for (off=0; off<N; off+=n) {
			an_entry an[BLOCK];
			int j;

			if (off + BLOCK > N)
				n = N - off;
			else
				n = BLOCK;

			if (an_catalog_read_entries(ancat, off, n, an)) {
				fprintf(stderr, "Error reading %i AN catalog entries.\n", n);
				exit(-1);
			}

			for (i=0; i<n; i++) {
				hp = radectohealpix_nside(deg2rad(an[i].ra), deg2rad(an[i].dec), Nside);
				for (j=0; j<12; j++) {
					if (!owned[j*HP + hp])
						continue;
					an_catalog_write_entry(cats[j], an + i);
				}
			}
		}
		an_catalog_close(ancat);
	}

	for (i=0; i<12; i++) {
		if (an_catalog_fix_headers(cats[i]) ||
			an_catalog_close(cats[i])) {
			fprintf(stderr, "Failed to close catalog %i.\n", i);
			exit(-1);
		}
	}

	free(owned);
	return 0;
}




