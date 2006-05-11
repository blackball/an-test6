#include <math.h>

#include "an_catalog.h"
#include "catalog.h"
#include "idfile.h"
#include "healpix.h"
#include "starutil.h"

#define OPTIONS "ho:N:m:M:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-m <minimum-magnitude-to-use>]\n"
		   "  [-M <maximum-magnitude-to-use>]\n"
		   "  [-N <nside>]: healpixelization at the fine-scale; default=100.\n"
		   "  <input-file> [<input-file> ...]\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	char* outfn = NULL;
	char* idfn = NULL;
	int c;
	int startoptind;
	int i, k, HP;
	int Nside = 100;
	int maxperhp = 0;
	double minmag = -1.0;
	double maxmag = 30.0;
	unsigned char* owned;
	char fn[256];
	an_catalog** cats;

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

	printf("Nside=%i, HP=%i.\n", Nside, HP);

	printf("Writing big healpix %i.\n", bighp);

	starlists = malloc(HP * sizeof(bl*));
	memset(starlists, 0, HP * sizeof(bl*));

	owned = malloc(HP * sizeof(int));
	memset(owned, 0, HP * sizeof(int));
	// for each big healpix, find the set of small healpixes it owns
	// (including a bit of overlap)
	// (only set owned[i] if healpix 'i' actually contains stars.)
	for (i=0; i<HP; i++) {
		uint big, x, y;
		uint nn, neigh[8], k;
		healpix_decompose(i, &big, &x, &y, Nside);
		if (big != bighp)
			continue;
		owned[i] = 1;
		if (x == 0 || y == 0 || (x == Nside-1) || (y == Nside-1)) {
			// add its neighbours.
			nn = healpix_get_neighbours_nside(i, neigh, Nside);
			for (k=0; k<nn; k++)
				owned[neigh[k]] = 1;
		}
	}

	//printf("Big healpix %i owns:\n", bighp);
	pixesowned = 0;
	for (i=0; i<HP; i++) {
		if (owned[i]) {
			//printf("%i ", i);
			pixesowned++;
		}
	}
	//printf("\n");
	printf("This big healpix owns %i small healpix.\n", pixesowned);

	printf("Max stars in this catalog will be %i\n", pixesowned * maxperhp);

	nwritten = 0;

	// go through the healpixes, writing the star locations to the
	// catalog file, and the star ids to the idfile.
	sprintf(fn, outfn, bighp);
	cat = catalog_open_for_writing(fn);
	if (!cat) {
		fprintf(stderr, "Couldn't open file %s for writing catalog.\n", fn);
		exit(-1);
	}
	catalog_write_header(cat);

	sprintf(fn, idfn, bighp);
	id = idfile_open_for_writing(fn);
	if (!id) {
		fprintf(stderr, "Couldn't open file %s for writing IDs.\n", fn);
		exit(-1);
	}
	idfile_write_header(id);

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		int hp;
		int i, N;
		an_catalog* ancat;
		int BLOCK = 1000;
		int off, n;
		int ndiscarded;

		infn = args[optind];
		ancat = an_catalog_open(infn);
		if (!ancat) {
			fprintf(stderr, "Couldn't open Astrometry.net catalog %s.\n", infn);
			exit(-1);
		}
		N = an_catalog_count_entries(ancat);
		printf("Reading   %i entries from %s...\n", N, infn);
		fflush(stdout);
		ndiscarded = 0;
		for (off=0; off<N; off+=n) {
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

			for (i=0; i<n; i++) {
				hp = radectohealpix_nside(deg2rad(an[i].ra), deg2rad(an[i].dec), Nside);

				if (!owned[hp]) {
					ndiscarded++;
					continue;
				}

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
				if (!nmag)
					continue;

				summag /= (double)nmag;
				sd.mag = summag;

				if (sd.mag < minmag)
					continue;
				if (sd.mag > maxmag)
					continue;

				if (!starlists[hp])
					starlists[hp] = bl_new(10, sizeof(stardata));

				if (maxperhp && (bl_size(starlists[hp]) >= maxperhp)) {
					// is this list full?
					stardata* last = bl_access(starlists[hp], bl_size(starlists[hp])-1);
					if (sd.mag > last->mag)
						// this new star is dimmer than the last one in the list...
						continue;
				}
				bl_insert_sorted(starlists[hp], &sd, sort_stardata_mag);
			}
		}

		printf("Discarded %i stars not in this big healpix.\n", ndiscarded);

		if (maxperhp) {
			//printf("largest magnitudes: ");
			for (i=0; i<HP; i++) {
				int size;
				stardata* d;
				if (!starlists[i]) continue;
				size = bl_size(starlists[i]);
				if (size) {
					d = bl_access(starlists[i], size-1);
					//printf("%g ", d->mag);
				}
				if (size < maxperhp) continue;
				bl_remove_index_range(starlists[i], maxperhp, size-maxperhp);
			}
			//printf("\n");
		}

		/*
		  {
		  int maxsize = 0;
		  for (i=0; i<HP; i++) {
		  if (!owned[i]) continue;
		  if (!starlists[i]) continue;
		  if (bl_size(starlists[i]) > maxsize)
		  maxsize = bl_size(starlists[i]);
		  }
		  printf("Longest list has %i stars.\n", maxsize);
		  {
		  int hist[maxsize+1];
		  memset(hist, 0, (maxsize+1)*sizeof(int));
		  for (i=0; i<HP; i++) {
		  if (!owned[i]) continue;
		  if (!starlists[i])
		  hist[0]++;
		  else
		  hist[bl_size(starlists[i])]++;
		  }
		  for (i=0; i<=maxsize; i++) {
		  if (hist[i])
		  printf("  %i stars: %i lists\n", i, hist[i]);
		  }
		  }
		  }
		*/

		an_catalog_close(ancat);
	}

	for (i=0; i<HP; i++)
		if (!starlists[i])
			owned[i] = 0;

	// sweep through the healpixes...
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
		fflush(stdout);
		if (nwritten == maxperbighp)
			break;
		if (!nowned)
			break;
	}
	printf("Made %i sweeps through the healpixes.\n", k);
	fflush(stdout);

	catalog_fix_header(cat);
	catalog_close(cat);
	idfile_fix_header(id);
	idfile_close(id);

	free(owned);
	free(starlists);

	return 0;
}




