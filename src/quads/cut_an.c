#include <math.h>

#include "an_catalog.h"
#include "catalog.h"
#include "idfile.h"
#include "healpix.h"
#include "starutil.h"
#include "mathutil.h"
#include "fitsioutils.h"

#define OPTIONS "ho:i:N:n:m:M:H:d:RGe:A"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template> -i <output-idfile-template>\n"
		   "  [-H <big healpix>]  or  [-A] (all-sky)\n"
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
	bool* owned;
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
	bool allsky = FALSE;
	stardata* sweeplist;
	int nowned = 0;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'A':
			allsky = TRUE;
			break;
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

	if (!outfn || !idfn || (optind == argc)) {
		printf("Specify catalog and idfile names.\n");
		print_help(args[0]);
		exit(-1);
	}
	if ((!sdss && !galex) || (sdss && galex)) {
		printf("Must choose one of SDSS or Galex.\n");
		print_help(args[0]);
		exit(-1);
	}
	if ((bighp == -1) && !allsky) {
		printf("Either specify a healpix (-H) or all-sky (-A).\n");
		print_help(args[0]);
		exit(-1);
	}

	HP = 12 * Nside * Nside;

	printf("Nside=%i, HP=%i, maxperhp=%i, max number of stars = HP*maxperhp = %i.\n", Nside, HP, maxperhp, HP*maxperhp);
	printf("Healpix side length: %g arcmin.\n", healpix_side_length_arcmin(Nside));
	if (bighp != -1)
		printf("Writing big healpix %i.\n", bighp);
	else
		printf("Writing an all-sky catalog.\n");

	if (deduprad > 0.0) {
		printf("Deduplication radius %f arcsec.\n", deduprad);
		deduprad = arcsec2distsq(deduprad);
	}

	starlists = calloc(HP, sizeof(bl*));
	owned = calloc(HP, sizeof(bool));

	// find the set of small healpixes that this big healpix owns
	// (including a bit of overlap)
	if (bighp != -1) {
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
	} else
		memset(owned, 1, HP * sizeof(bool));

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
	if (allsky)
		qfits_header_add(cat->header, "ALLSKY", "T", "All-sky catalog.", NULL);


	sprintf(fn, idfn, bighp);
	id = idfile_open_for_writing(fn);
	if (!id) {
		fprintf(stderr, "Couldn't open file %s for writing IDs.\n", fn);
		exit(-1);
	}
	id->healpix = bighp;
	if (allsky)
		qfits_header_add(id->header, "ALLSKY", "T", "All-sky catalog.", NULL);

	qfits_header_add(id->header, "HISTORY", "cut_an command line:", NULL, NULL);
	fits_add_args(id->header, args, argc);
	qfits_header_add(id->header, "HISTORY", "(end of cut_an command line)", NULL, NULL);

	qfits_header_add(cat->header, "HISTORY", "cut_an command line:", NULL, NULL);
	fits_add_args(cat->header, args, argc);
	qfits_header_add(cat->header, "HISTORY", "(end of cut_an command line)", NULL, NULL);

	if (catalog_write_header(cat) ||
		idfile_write_header(id)) {
		fprintf(stderr, "Failed to write catalog or idfile header.\n");
		exit(-1);
	}

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		int i, N;
		an_catalog* ancat;
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

		for (i=0; i<N; i++) {
			stardata sd;
			int j;
			double summag;
			int hp;
			int nmag;
			double sumredmag;
			int nredmag;
			an_entry* an;

			an = an_catalog_read_entry(ancat);
			if (!an) {
				fprintf(stderr, "Failed to read Astrometry.net catalog entry.\n");
				exit(-1);
			}

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
			sumredmag = 0.0;
			nredmag = 0;
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
					if (an[i].obs[j].catalog == AN_SOURCE_USNOB) {
						if (an[i].obs[j].band == 'O' || an[i].obs[j].band == 'J') {
							summag += an[i].obs[j].mag;
							nmag++;
						} else if (an[i].obs[j].band == 'E' || an[i].obs[j].band == 'F') {
							sumredmag += an[i].obs[j].mag;
							nredmag++;
						}
					} else if (an[i].obs[j].catalog == AN_SOURCE_TYCHO2) {
						if (an[i].obs[j].band == 'B' || an[i].obs[j].band == 'H') {
							summag += an[i].obs[j].mag;
							nmag++;
						} else if (an[i].obs[j].band == 'V') {
							sumredmag += an[i].obs[j].mag;
							nredmag++;
						}
					}
				}
			}
			if (!nmag)
				continue;

			summag /= (double)nmag;
			sd.mag = summag;

			if (galex && epsilon>0) {
				if (nredmag) {
					sumredmag /= (double)nredmag;
					sd.mag += epsilon * (summag - sumredmag);
				}
			}

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

		if (bighp != -1)
			printf("Discarded %i stars not in this big healpix.\n", ndiscarded);
		printf("Discarded %i duplicate stars.\n", nduplicates);

		if (maxperhp)
			for (i=0; i<HP; i++) {
				int size;
				stardata* d;
				if (!starlists[i]) continue;
				size = bl_size(starlists[i]);
				if (size)
					d = bl_access(starlists[i], size-1);
				if (size < maxperhp) continue;
				bl_remove_index_range(starlists[i], maxperhp, size-maxperhp);
			}

		an_catalog_close(ancat);
	}

	free(entries);

	for (i=0; i<HP; i++)
		if (!starlists[i])
			owned[i] = 0;

	// sweep through the healpixes...
	for (i=0; i<HP; i++)
		if (owned[i])
			nowned++;
	sweeplist = malloc(nowned * sizeof(stardata));

	for (k=0;; k++) {
		nowned = 0;
		// gather up the stars that will be used in this sweep...
		for (i=0; i<HP; i++) {
			stardata* sd;
			int N;
			if (!owned[i]) continue;
			N = bl_size(starlists[i]);
			if (k >= N) {
				owned[i] = 0;
				continue;
			}
			sd = bl_access(starlists[i], k);
			memcpy(sweeplist + nowned, sd, sizeof(stardata));
			nowned++;
		}

		// sort them by magnitude...
		qsort(sweeplist, nowned, sizeof(stardata), sort_stardata_mag);

		// write them out...
		for (i=0; i<nowned; i++) {
			double xyz[3];
			stardata* sd = sweeplist + i;
			radec2xyzarr(deg2rad(sd->ra), deg2rad(sd->dec), xyz);

			catalog_write_star(cat, xyz);
			idfile_write_anid(id, sd->id);
				
			nwritten++;
			if (nwritten == maxperbighp)
				break;
		}
		printf("sweep %i: got %i stars (%i total)\n", k, nowned, nwritten);
		fflush(stdout);
		// if we broke out of the loop...
		if (nwritten == maxperbighp)
			break;
		if (!nowned)
			break;
	}
	printf("Made %i sweeps through the healpixes.\n", k);

	free(sweeplist);

	catalog_fix_header(cat);
	catalog_close(cat);
	idfile_fix_header(id);
	idfile_close(id);

	free(owned);
	free(starlists);

	return 0;
}




