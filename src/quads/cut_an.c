

#include "an_catalog.h"
#include "catalog.h"
#include "healpix.h"
#include "starutil.h"

#define OPTIONS "ho:N:n:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-n <max-stars-per-healpix>]\n"
		   "  [-N <nside>]: healpixelization at the fine-scale; default=9.\n"
		   "  <input-file> [<input-file> ...]\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

struct stardata {
	double ra;
	double dec;
	float mag;
	int64_t id;
	//int small_hp;
	//unsigned char big_hp;
};
typedef struct stardata stardata;

int sort_stardata_mag(const void* v1, const void* v2) {
	stardata* d1 = (stardata*)v1;
	stardata* d2 = (stardata*)v2;
	float diff = d1->mag - d2->mag;
	if (diff < 0.0) return -1;
	if (diff == 0.0) return 0;
	return 1;
}

int main(int argc, char** args) {
	char* outfn = NULL;
	int c;
	int startoptind;
	int i, j, HP;
	int Nside = 9;
	//catalog* cats;
	bl** starlists;

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

	starlists = malloc(HP * sizeof(bl*));
	for (i=0; i<HP; i++)
		starlists[i] = bl_new(32, sizeof(stardata));

	/*
	  cats = malloc(HP * sizeof(catalog*));
	  memset(cats, 0, HP * sizeof(catalog*));
	*/

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		int hp;
		int i, N;
		an_catalog* ancat;

		infn = args[optind];
		ancat = an_catalog_open(infn);
		if (!ancat) {
			fprintf(stderr, "Couldn't open Astrometry.net catalog %s.\n", infn);
			exit(-1);
		}
		N = an_catalog_count_entries(ancat);
		for (i=0; i<N; i++) {
			an_entry an;
			stardata sd;
			int j;
			double summag;
			int nmag;

			if (an_catalog_read_entries(ancat, i, 1, &an)) {
				fprintf(stderr, "Error reading AN catalog entry.\n");
				exit(-1);
			}

			hp = radectohealpix_nside(deg2rad(an.ra), deg2rad(an.dec), Nside);

			sd.ra = an.ra;
			sd.dec = an.dec;
			sd.id = an.id;

			// dumbass magnitude averaging!
			summag = 0.0;
			nmag = 0;
			for (j=0; j<an.nobs; j++) {
				if (an.obs[j].catalog == AN_SOURCE_USNOB) {
					if (an.obs[j].band == 'E' || an.obs[j].band == 'F') {
						summag += an.obs[j].mag;
						nmag++;
					}
				} else if (an.obs[j].catalog == AN_SOURCE_TYCHO2) {
					if (an.obs[j].band == 'V' || an.obs[j].band == 'H') {
						summag += an.obs[j].mag;
						nmag++;
					}
				}
			}

			summag /= (double)nmag;
			sd.mag = summag;

			bl_append(starlists[hp], &sd);
		}

		an_catalog_close(ancat);
	}

	// sort the stars within each fine healpix.
	for (i=0; i<HP; i++) {
		if (bl_size(starlists[i]) == 0)
			continue;
		bl_sort(starlists[i], sort_stardata_mag);
		{
			stardata* d1, *d2;
			d1 = (stardata*)bl_access(starlists[i], 0);
			d2 = (stardata*)bl_access(starlists[i], 1);
			printf("first two mags: %g, %g\n", d1->mag, d2->mag);
		}
	}

	// go through the big healpixes...
	for (j=0; j<12; j++) {
		// for each big healpix, find the set of small healpixes it owns
		// (including a bit of overlap)
	}

	free(starlists);

	return 0;
}




