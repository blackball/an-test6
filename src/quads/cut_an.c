#include <math.h>

#include "an_catalog.h"
#include "catalog.h"
#include "idfile.h"
#include "healpix.h"
#include "starutil.h"

#define OPTIONS "ho:N:n:m:M:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-m <minimum-magnitude-to-use>]\n"
		   "  [-M <maximum-magnitude-to-use>]\n"
		   "  [-n <max-stars-per-(small)-healpix>]\n"
		   "  [-S <max-stars-per-(big)-healpix]\n"
		   "  [-N <nside>]: healpixelization at the fine-scale; default=100.\n"
		   "  <input-file> [<input-file> ...]\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

struct stardata {
	double ra;
	double dec;
	float mag;
	uint64_t id;
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
	int Nside = 100;
	//catalog* cats;
	bl** starlists;
	int maxperhp = 0;
	double minmag = -1.0;
	double maxmag = 30.0;
	int* owned;
	int maxperbighp = 0;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'S':
			maxperbighp = atoi(optarg);
			break;
		case 'N':
			Nside = atoi(optarg);
			break;
		case 'o':
			outfn = optarg;
			break;
		case 'n':
			maxperhp = atoi(optarg);
			break;
		case 'm':
			minmag = atof(optarg);
			break;
		case 'M':
			maxmag = atof(optarg);
			break;
		}
    }

	if (!outfn || (optind == argc)) {
		print_help(args[0]);
		exit(-1);
	}

	HP = 12 * Nside * Nside;

	starlists = malloc(HP * sizeof(bl*));
	memset(starlists, 0, HP * sizeof(bl*));
	/*
	  for (i=0; i<HP; i++)
	  starlists[i] = bl_new(32, sizeof(stardata));
	*/
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
		int BLOCK = 1000;
		int off, n;

		infn = args[optind];
		ancat = an_catalog_open(infn);
		if (!ancat) {
			fprintf(stderr, "Couldn't open Astrometry.net catalog %s.\n", infn);
			exit(-1);
		}
		N = an_catalog_count_entries(ancat);
		printf("Reading %i entries...\n", N);
		off = 0;
		while (off < N) {
			an_entry an[BLOCK];
			stardata sd;
			int j;
			double summag;
			int nmag;

			if (off + BLOCK > N)
				n = N - off;
			else
				n = BLOCK;

			if (an_catalog_read_entries(ancat, off, n, an)) {
				fprintf(stderr, "Error reading %i AN catalog entries.\n", n);
				exit(-1);
			}
			off += n;

			for (i=0; i<n; i++) {
				hp = radectohealpix_nside(deg2rad(an[i].ra), deg2rad(an[i].dec), Nside);

				sd.ra = an[i].ra;
				sd.dec = an[i].dec;
				sd.id = an[i].id;

				// dumbass magnitude averaging!
				summag = 0.0;
				nmag = 0;
				for (j=0; j<an[i].nobs; j++) {
					if (an[i].obs[j].catalog == AN_SOURCE_USNOB) {
						if (an[i].obs[j].band == 'E' || an[i].obs[j].band == 'F') {
							summag += an[i].obs[j].mag;
							nmag++;
						}
					} else if (an[i].obs[j].catalog == AN_SOURCE_TYCHO2) {
						if (an[i].obs[j].band == 'V' || an[i].obs[j].band == 'H') {
							summag += an[i].obs[j].mag;
							nmag++;
						}
					}
				}

				summag /= (double)nmag;
				sd.mag = summag;

				if (sd.mag < minmag)
					continue;
				if (sd.mag > maxmag)
					continue;

				if (!starlists[hp])
					starlists[hp] = bl_new(32, sizeof(stardata));

				if (maxperhp && (bl_size(starlists[hp]) >= maxperhp)) {
					// is this list full?
					stardata* last = bl_access(starlists[hp], bl_size(starlists[hp])-1);
					if (sd.mag > last->mag)
						// this new star is dimmer than the last one in the list...
						continue;
				}
				//bl_append(starlists[hp], &sd);
				bl_insert_sorted(starlists[hp], &sd, sort_stardata_mag);
			}
		}

		an_catalog_close(ancat);
	}

	// sort the stars within each fine healpix.
	for (i=0; i<HP; i++) {
		int j;
		if (!starlists[i]) continue;
		printf("small healpix %i has %i stars.\n", i, bl_size(starlists[i]));
		/*
		  if (bl_size(starlists[i]) == 0)
		  continue;
		  bl_sort(starlists[i], sort_stardata_mag);
		*/
		/*
		  {
		  stardata* d1, *d2;
		  d1 = (stardata*)bl_access(starlists[i], 0);
		  d2 = (stardata*)bl_access(starlists[i], 1);
		  printf("first two mags: %g, %g\n", d1->mag, d2->mag);
		  }
		*/
		printf("mags: ");
		for (j=0; j<bl_size(starlists[i]); j++) {
			stardata* d1 = (stardata*)bl_access(starlists[i], j);
			printf("%g ", d1->mag);
		}
		printf("\n");
	}

	owned = malloc(HP * sizeof(int));

	// go through the big healpixes...
	for (j=0; j<12; j++) {
		char fn[256];
		uint nn, neigh[8], k;
		catalog* cat;
		idfile* id;
		int nwritten;

		// for each big healpix, find the set of small healpixes it owns
		// (including a bit of overlap)
		// (only set owned[i] if healpix 'i' actually contains stars.)
		memset(owned, 0, HP * sizeof(int));
		for (i=0; i<HP; i++) {
			uint bighp, x, y;
			if (!starlists[i]) continue;
			healpix_decompose(i, &bighp, &x, &y, Nside);
			if (bighp != j)
				continue;
			owned[i] = 1;
			if (x == 0 || y == 0 || (x == Nside-1) || (y == Nside-1)) {
				// add its neighbours.
				nn = healpix_get_neighbours_nside(i, neigh, Nside);
				for (k=0; k<nn; k++) {
					if (starlists[neigh[k]])
						owned[neigh[k]] = 1;
				}
			}
		}

		// go through the healpixes, writing the star locations to the
		// catalog file, and the star ids to the idfile.
		sprintf(fn, outfn, j);
		cat = catalog_open_for_writing(fn);
		if (!cat) {
			fprintf(stderr, "Couldn't open file %s for writing catalog.\n", fn);
			exit(-1);
		}
		catalog_write_header(cat);

		id = idfile_open_for_writing(fn);
		if (!id) {
			fprintf(stderr, "Couldn't open file %s for writing IDs.\n", fn);
			exit(-1);
		}
		idfile_write_header(id);

		nwritten = 0;
		for (k=0;; k++) {
			int nowned = 0;
			for (i=0; i<HP; i++) {
				stardata* sd;
				double xyz[3];
				int N;
				if (!owned[i]) continue;
				N = bl_size(starlists[i]);
				if (k >= N) {
					owned[i] = 0;
					continue;
				}
				nowned++;
				sd = (stardata*)bl_access(starlists[i], k);

				xyz[0] = radec2x(deg2rad(sd->ra), deg2rad(sd->dec));
				xyz[1] = radec2y(deg2rad(sd->ra), deg2rad(sd->dec));
				xyz[2] = radec2z(deg2rad(sd->ra), deg2rad(sd->dec));
				catalog_write_star(cat, xyz);

				idfile_write_anid(id, sd->id);

				nwritten++;
				if (nwritten == maxperbighp)
					break;
			}
			printf("sweep %i: got %i stars (%i total)\n", k, nowned, nwritten);
			if (nwritten == maxperbighp)
				break;
			if (!nowned)
				break;
		}
		printf("Made %i sweeps through the healpixes.\n", k);

		catalog_fix_header(cat);
		catalog_close(cat);
		idfile_fix_header(id);
		idfile_close(id);
	}

	free(starlists);

	return 0;
}




