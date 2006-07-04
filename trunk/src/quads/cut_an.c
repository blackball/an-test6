#include <math.h>

#include "an_catalog.h"
#include "catalog.h"
#include "idfile.h"
#include "healpix.h"
#include "starutil.h"
#include "mathutil.h"
#include "fitsioutils.h"

#define OPTIONS "ho:i:N:n:m:M:H:d:RGe:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template> -i <output-idfile-template>\n"
		   "  [-H <big healpix>]\n"
		   "  [-m <minimum-magnitude-to-use>]\n"
		   "  [-M <maximum-magnitude-to-use>]\n"
		   "  [-n <max-stars-per-(small)-healpix>]\n"
		   "  [-S <max-stars-per-(big)-healpix]\n"
		   "  [-N <nside>]: healpixelization at the fine-scale; default=100.\n"
		   "  [-d <dedup-radius>]: deduplication radius (arcseconds)\n"
		   "  ( [-R] or [-G] )\n"
		   "        R: SDSS R-band\n"
		   "        G: Galex\n"
		   "  [-e <Galex-epsilon>]\n"
		   "  <input-file> [<input-file> ...]\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

struct stardata {
	double ra;
	double dec;
	uint64_t id;
	float mag;
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
	char* idfn = NULL;
	int c;
	int startoptind;
	int i, k, HP;
	int Nside = 100;
	bl** starlists;
	int maxperhp = 0;
	double minmag = -1.0;
	double maxmag = 30.0;
	int* owned;
	int maxperbighp = 0;
	int bighp = -1;
	char fn[256];
	catalog* cat;
	idfile* id;
	int nwritten;
	an_entry* entries;
	int BLOCK = 100000;
	double deduprad = 0.0;
	double epsilon = 0.0;
	bool sdss = FALSE;
	bool galex = FALSE;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'R':
			sdss = TRUE;
			break;
		case 'G':
			galex = TRUE;
			break;
		case 'e':
			epsilon = atof(optarg);
			break;
		case 'd':
			deduprad = atof(optarg);
			break;
		case 'H':
			bighp = atoi(optarg);
			break;
		case 'S':
			maxperbighp = atoi(optarg);
			break;
		case 'N':
			Nside = atoi(optarg);
			break;
		case 'o':
			outfn = optarg;
			break;
		case 'i':
			idfn = optarg;
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

	if (!outfn || !idfn || (optind == argc) || (bighp == -1)) {
		print_help(args[0]);
		exit(-1);
	}
	if ((!sdss && !galex) || (sdss && galex)) {
		printf("Must choose one of SDSS or Galex.\n");
		print_help(args[0]);
		exit(-1);
	}

	HP = 12 * Nside * Nside;

	printf("Nside=%i, HP=%i, maxperhp=%i, HP*maxperhp=%i.\n", Nside, HP, maxperhp, HP*maxperhp);
	printf("Writing big healpix %i.\n", bighp);

	if (deduprad > 0.0) {
		printf("Deduplication radius %f arcsec.\n", deduprad);
		// convert from arcseconds to distance^2 on the unit sphere.
		deduprad = arcsec2distsq(deduprad);
	}

	starlists = calloc(sizeof(bl*), HP);
	owned = calloc(sizeof(int), HP);

	// for each big healpix, find the set of small healpixes it owns
	// (including a bit of overlap)
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

	/*{
	  int pixesowned;
	  pixesowned = 0;
	  for (i=0; i<HP; i++)
	  if (owned[i])
	  pixesowned++;
	  printf("This big healpix owns %i small healpix.\n", pixesowned);
	  if (maxperhp)
	  printf("Max stars in this catalog will be %i\n", pixesowned * maxperhp);
	  }
	*/

	nwritten = 0;
	entries = malloc(BLOCK * sizeof(an_entry));

	// go through the healpixes, writing the star locations to the
	// catalog file, and the star ids to the idfile.
	sprintf(fn, outfn, bighp);
	cat = catalog_open_for_writing(fn);
	if (!cat) {
		fprintf(stderr, "Couldn't open file %s for writing catalog.\n", fn);
		exit(-1);
	}
	cat->healpix = bighp;

	qfits_header_add(cat->header, "COMMENT", "cut_an command line:", NULL, NULL);
	fits_add_args(cat->header, args, argc);

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
		int off, n;
		int ndiscarded;
		int nduplicates;

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
		nduplicates = 0;
		for (off=0; off<N; off+=n) {
			stardata sd;
			int j;
			double summag;
			int nmag;
			an_entry* an = entries;

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
				if (sdss) {
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
				} else if (galex) {
					for (j=0; j<an[i].nobs; j++) {
						if ((an[i].obs[j].catalog == AN_SOURCE_USNOB) &&
							//(usnob_get_survey_epoch(an[i].obs[j].survey, j) == 2) &&
							(an[i].obs[j].band == 'O' || an[i].obs[j].band == 'J')) {
							summag += an[i].obs[j].mag;
							nmag++;
						} else if ((an[i].obs[j].catalog == AN_SOURCE_TYCHO2) &&
								   (an[i].obs[j].band == 'B' || an[i].obs[j].band == 'H')) {
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
					stardata* last = bl_access(starlists[hp], maxperhp-1);
					if (sd.mag > last->mag)
						// this new star is dimmer than the "maxperhp"th one in the list...
						continue;
				}
				if (deduprad > 0.0) {
					double xyz[3];
					bool dup = FALSE;
					radec2xyzarr(deg2rad(sd.ra), deg2rad(sd.dec), xyz);
					for (j=0; j<bl_size(starlists[hp]); j++) {
						double xyz2[3];
						stardata* sd2 = bl_access(starlists[hp], j);
						radec2xyzarr(deg2rad(sd2->ra), deg2rad(sd2->dec), xyz2);
						if (!distsq_exceeds(xyz, xyz2, 3, deduprad)) {
							nduplicates++;
							dup = TRUE;
							break;
						}
					}
					if (dup)
						continue;
				}
				bl_insert_sorted(starlists[hp], &sd, sort_stardata_mag);
			}
		}

		printf("Discarded %i stars not in this big healpix.\n", ndiscarded);
		printf("Discarded %i duplicate stars.\n", nduplicates);

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

	free(entries);

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




