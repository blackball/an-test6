/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <math.h>
#include <errno.h>
#include <string.h>

#include "an_catalog.h"
#include "catalog.h"
#include "idfile.h"
#include "healpix.h"
#include "starutil.h"
#include "mathutil.h"
#include "fitsioutils.h"
#include "boilerplate.h"

#define OPTIONS "ho:i:N:n:m:M:H:d:e:ARGZ"

void print_help(char* progname) {
	boilerplate_help_header(stdout);
    printf("\nUsage: %s\n"
		   "   -o <output-objs-template>    (eg, an-sdss-%%02i.objs.fits)\n"
		   "  [-i <output-id-template>]      (eg, an-sdss-%%02i.id.fits)\n"
		   "  [-H <big healpix>]  or  [-A] (all-sky)\n"
		   "  [-m <minimum-magnitude-to-use>]\n"
		   "  [-M <maximum-magnitude-to-use>]\n"
		   "  [-n <max-stars-per-fine-healpix-grid>]    (ie, number of sweeps)\n"
		   "  [-S <max-stars-per-(big)-healpix]         (ie, max number of stars in the cut)\n"
		   "  [-N <nside>]:   fine healpixelization grid; default 100.\n"
		   "  [-d <dedup-radius>]: deduplication radius (arcseconds)\n"
		   "  ( [-R] or [-G] or [-Z] )\n"
		   "        R: SDSS r-band\n"
		   "        G: Galex\n"
		   "        Z: SDSS z-band\n"
		   "  [-e <Galex-epsilon>]\n"
		   "  <input-file> [<input-file> ...]\n"
		   "\n"
		   "Input files must be Astrometry.net catalogs.\n"
		   "If no ID filename template is given, ID files will not be written.\n"
		   "\n", progname);
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
	const stardata* d1 = v1;
	const stardata* d2 = v2;
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
	idfile* id = NULL;
	int nwritten;
	int BLOCK = 100000;
	double deduprad = 0.0;
	double epsilon = 0.0;
	bool sdss = FALSE;
	bool galex = FALSE;
	bool zband = FALSE;
	bool allsky = FALSE;
	stardata* sweeplist;
	int npix;
	stardata** stararrays;
	int* stararrayN;

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
		case 'Z':
			zband = TRUE;
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

	if (!outfn || (optind == argc)) {
		printf("Specify catalog and idfile names and input files.\n");
		print_help(args[0]);
		exit(-1);
	}
	i=0;
	if (sdss) i++;
	if (galex) i++;
	if (zband) i++;
	if (i != 1) {
		printf("Must choose exactly one of SDSS r-band (-R), SDSS z-band (-Z), Galex (-G).\n");
		print_help(args[0]);
		exit(-1);
	}
	if ((bighp == -1) && !allsky) {
		printf("Either specify a healpix (-H) or all-sky (-A).\n");
		print_help(args[0]);
		exit(-1);
	}

	if (!idfn) {
		printf("Warning: ID files will not be written.\n");
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

	// find the set of small healpixes that this big healpix owns
	// (including a bit of overlap)
	if (bighp != -1) {
		owned = calloc(HP, sizeof(bool));
		for (i=0; i<HP; i++) {
			uint big, x, y;
			uint nn, neigh[8], k;
			healpix_decompose_xy(i, &big, &x, &y, Nside);
			if (big != bighp)
				continue;
			owned[i] = 1;
			if (x == 0 || y == 0 || (x == Nside-1) || (y == Nside-1)) {
				// add its neighbours.
				nn = healpix_get_neighbours(i, neigh, Nside);
				for (k=0; k<nn; k++)
					owned[neigh[k]] = 1;
			}
		}
	} else
		owned = NULL;

	nwritten = 0;

	sprintf(fn, outfn, bighp);
	cat = catalog_open_for_writing(fn);
	if (!cat) {
		fflush(stdout);
		fprintf(stderr, "Couldn't open file %s for writing catalog.\n", fn);
		exit(-1);
	}
	cat->healpix = bighp;
	if (allsky)
		qfits_header_add(cat->header, "ALLSKY", "T", "All-sky catalog.", NULL);


	if (idfn) {
		sprintf(fn, idfn, bighp);
		id = idfile_open_for_writing(fn);
		if (!id) {
			fflush(stdout);
			fprintf(stderr, "Couldn't open file %s for writing IDs.\n", fn);
			exit(-1);
		}
		id->healpix = bighp;
		if (allsky)
			qfits_header_add(id->header, "ALLSKY", "T", "All-sky catalog.", NULL);

		boilerplate_add_fits_headers(id->header);
		qfits_header_add(id->header, "HISTORY", "This file was generated by the program \"cut-an\".", NULL, NULL);
		qfits_header_add(id->header, "HISTORY", "cut-an command line:", NULL, NULL);
		fits_add_args(id->header, args, argc);
		qfits_header_add(id->header, "HISTORY", "(end of cut-an command line)", NULL, NULL);
	}

	boilerplate_add_fits_headers(cat->header);
	qfits_header_add(cat->header, "HISTORY", "This file was generated by the program \"cut-an\".", NULL, NULL);
	qfits_header_add(cat->header, "HISTORY", "cut-an command line:", NULL, NULL);
	fits_add_args(cat->header, args, argc);
	qfits_header_add(cat->header, "HISTORY", "(end of cut-an command line)", NULL, NULL);

	// add placeholders...
	for (k=0; k< (maxperhp ? maxperhp : 100); k++) {
		char key[64];
		sprintf(key, "SWEEP%i", (k+1));
		if (id)
			qfits_header_add(id->header, key, "-1", "placeholder", NULL);
		qfits_header_add(cat->header, key, "-1", "placeholder", NULL);
	}

	if (catalog_write_header(cat) ||
		(id && idfile_write_header(id))) {
		fflush(stdout);
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
			fflush(stdout);
			fprintf(stderr, "Couldn't open Astrometry.net catalog %s.\n", infn);
			exit(-1);
		}
		ancat->br.blocksize = BLOCK;
		N = an_catalog_count_entries(ancat);
		printf("Reading   %i entries from %s...\n", N, infn);
		fflush(stdout);
		ndiscarded = 0;
		nduplicates = 0;

		for (i=0; i<N; i++) {
			stardata sd;
			int j;
			double redmag;
			int hp;
			int nred;
			double bluemag;
			int nblue;
			an_entry* an;

			an = an_catalog_read_entry(ancat);
			if (!an) {
				fflush(stdout);
				fprintf(stderr, "Failed to read Astrometry.net catalog entry.\n");
				exit(-1);
			}

			hp = radectohealpix(deg2rad(an->ra), deg2rad(an->dec), Nside);

			if (owned && !owned[hp]) {
				ndiscarded++;
				continue;
			}

			sd.ra = an->ra;
			sd.dec = an->dec;
			sd.id = an->id;

			// dumbass magnitude averaging!
			if (sdss || galex) {
				redmag = 0.0;
				nred = 0;
				bluemag = 0.0;
				nblue = 0;
				if (sdss) {
					for (j=0; j<an->nobs; j++) {
						if (an->obs[j].catalog == AN_SOURCE_USNOB) {
							if (an->obs[j].band == 'E' || an->obs[j].band == 'F') {
								redmag += an->obs[j].mag;
								nred++;
							}
						} else if (an->obs[j].catalog == AN_SOURCE_TYCHO2) {
							if (an->obs[j].band == 'V' || an->obs[j].band == 'H') {
								redmag += an->obs[j].mag;
								nred++;
							}
						}
					}
				} else if (galex) {
					for (j=0; j<an->nobs; j++) {
						if (an->obs[j].catalog == AN_SOURCE_USNOB) {
							if (an->obs[j].band == 'O' || an->obs[j].band == 'J') {
								bluemag += an->obs[j].mag;
								nblue++;
							} else if (an->obs[j].band == 'E' || an->obs[j].band == 'F') {
								redmag += an->obs[j].mag;
								nred++;
							}
						} else if (an->obs[j].catalog == AN_SOURCE_TYCHO2) {
							if (an->obs[j].band == 'B' || an->obs[j].band == 'H') {
								bluemag += an->obs[j].mag;
								nblue++;
							} else if (an->obs[j].band == 'V') {
								redmag += an->obs[j].mag;
								nred++;
							}
						}
					}
				}
				if (!nred)
					continue;

				redmag /= (double)nred;
				sd.mag = redmag;

				if (galex && epsilon>0) {
					if (nblue) {
						bluemag /= (double)nblue;
						sd.mag += epsilon * (bluemag - redmag);
					}
				}
			} else if (zband) {
				bool gotit = FALSE;
				for (j=0; j<an->nobs; j++) {
					if ((an->obs[j].catalog == AN_SOURCE_2MASS) &&
						(an->obs[j].band == 'J')) {
						gotit = TRUE;
						sd.mag = an->obs[j].mag;
						break;
					}
				}
				if (!gotit)
					continue;
			}

			if (sd.mag < minmag)
				continue;
			if (sd.mag > maxmag)
				continue;

			if (!starlists[hp])
				starlists[hp] = bl_new(maxperhp ? maxperhp : 10, sizeof(stardata));

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
	free(owned);

	// compact the stars into arrays.
	npix = 0;
	for (i=0; i<HP; i++)
		if (starlists[i]) {
			starlists[npix] = starlists[i];
			npix++;
		}
	starlists = realloc(starlists, npix * sizeof(bl*));
	// (reuse the bl* storage as stardata* storage)
	stararrays = (stardata**)starlists;
	stararrayN = malloc(npix * sizeof(int));

	for (i=0; i<npix; i++) {
		bl* list;
		int n;
		list = starlists[i];
		n = bl_size(list);
		stararrays[i] = malloc(n * sizeof(stardata));
		bl_copy(list, 0, n, stararrays[i]);
		bl_free(list);
		stararrayN[i] = n;
	}

	sweeplist = malloc(npix * sizeof(stardata));

	// sweep through the healpixes...
	for (k=0;; k++) {
		char key[64];
		char val[64];
		int nsweep = 0;
		// gather up the stars that will be used in this sweep...
		for (i=0; i<npix; i++) {
			stardata* sd;
			int N;
			if (!stararrays[i])
				continue;
			N = stararrayN[i];
			if (k >= N) {
				free(stararrays[i]);
				stararrays[i] = NULL;
				continue;
			}
			sd = stararrays[i] + k;
			memcpy(sweeplist + nsweep, sd, sizeof(stardata));
			nsweep++;
		}
		if (!nsweep)
			break;

		// sort them by magnitude...
		qsort(sweeplist, nsweep, sizeof(stardata), sort_stardata_mag);

		// write them out...
		for (i=0; i<nsweep; i++) {
			double xyz[3];
			stardata* sd = sweeplist + i;
			radec2xyzarr(deg2rad(sd->ra), deg2rad(sd->dec), xyz);

			if (catalog_write_star(cat, xyz) ||
				(id && idfile_write_anid(id, sd->id))) {
				fflush(stdout);
				fprintf(stderr, "Failed to write star to catalog.  Possible cause: %s\n", strerror(errno));
				exit(-1);
			}

			nwritten++;
			if (nwritten == maxperbighp)
				break;
		}

		// add to FITS header...
		if (maxperhp || (k<100)) {
			sprintf(key, "SWEEP%i", (k+1));
			sprintf(val, "%i", nsweep);
			if (id)
				qfits_header_mod(id->header , key, val, "");
			qfits_header_mod(cat->header, key, val, "");
		}

		printf("sweep %i: got %i stars (%i total)\n", k, nsweep, nwritten);
		fflush(stdout);
		// if we broke out of the loop...
		if (nwritten == maxperbighp)
			break;
	}
	printf("Made %i sweeps through the healpixes.\n", k);

	free(sweeplist);
	free(stararrayN);
	for (i=0; i<npix; i++)
		free(stararrays[i]);
	free(stararrays);

	if (catalog_fix_header(cat) ||
		catalog_close(cat) ||
		(id && (idfile_fix_header(id) ||
				idfile_close(id)))) {
		fflush(stdout);
		fprintf(stderr, "Failed to close output files.\n");
		exit(-1);
	}

	return 0;
}




