#include <math.h>

#include "an_catalog.h"
#include "catalog.h"
#include "idfile.h"
#include "healpix.h"
#include "starutil.h"
#include "mathutil.h"
#include "fitsioutils.h"

#define OPTIONS "ho:N:n:d:RGe:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s\n"
		   "  -o <output-filename>\n"
		   "  [-n <max-stars-per-fine-healpix>]\n"
		   "  [-N <nside>]: fine-scale healpixelization.\n"
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
	float mag;
	an_entry entry;
};
typedef struct stardata stardata;

int compare_stardata_mag(const void* v1, const void* v2) {
	const stardata* d1 = v1;
	const stardata* d2 = v2;
	float diff = d1->mag - d2->mag;
	if (diff < 0.0) return -1;
	if (diff == 0.0) return 0;
	return 1;
}

int main(int argc, char** args) {
	char* outfn = NULL;
	int c;
	int startoptind;
	int i, k, HP;
	int Nside = 100;
	bl** starlists;
	int maxperhp = 0;
	char fn[256];
	an_catalog* cat;
	int nwritten;
	int BLOCK = 100000;
	double deduprad = 0.0;
	double epsilon = 0.0;
	bool sdss = FALSE;
	bool galex = FALSE;
	stardata* sweeplist;
	int nowned;
	int ninfiles;

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
		case 'N':
			Nside = atoi(optarg);
			break;
		case 'o':
			outfn = optarg;
			break;
		case 'n':
			maxperhp = atoi(optarg);
			break;
		}
    }

	if (!outfn || (optind == argc)) {
		printf("Specify output and input filesnames.\n");
		print_help(args[0]);
		exit(-1);
	}
	if ((!sdss && !galex) || (sdss && galex)) {
		printf("Must choose one of SDSS or Galex.\n");
		print_help(args[0]);
		exit(-1);
	}

	HP = 12 * Nside * Nside;

	printf("Nside=%i, HP=%i, maxperhp=%i, max number of stars = HP*maxperhp = %i.\n", Nside, HP, maxperhp, HP*maxperhp);
	printf("Healpix side length: %g arcmin.\n", healpix_side_length_arcmin(Nside));

	if (deduprad > 0.0) {
		printf("Deduplication radius %f arcsec.\n", deduprad);
		deduprad = arcsec2distsq(deduprad);
	}

	starlists = calloc(HP, sizeof(bl*));

	cat = an_catalog_open_for_writing(outfn);
	if (!cat) {
		fprintf(stderr, "Couldn't open file %s for writing catalog.\n", fn);
		exit(-1);
	}

	qfits_header_add(cat->header, "HISTORY", "filter_an command line:", NULL, NULL);
	fits_add_args(cat->header, args, argc);
	qfits_header_add(cat->header, "HISTORY", "(end of filter_an command line)", NULL, NULL);

	ninfiles = argc - optind;
	if (ninfiles == 1) {
		qfits_header* hdr = qfits_header_read(args[optind]);
		if (!hdr) {
			fprintf(stderr, "Couldn't read FITS header from %s.\n", args[optind]);
			exit(-1);
		}
		fits_copy_header(hdr, cat->header, "HEALPIX");
		fits_copy_header(hdr, cat->header, "NSIDE");
		qfits_header_add(cat->header, "HISTORY", "** filter_an: history from input file:", NULL, NULL);
		fits_copy_all_headers(hdr, cat->header, "HISTORY");
		qfits_header_add(cat->header, "HISTORY", "** filter_an: end of history from input file.", NULL, NULL);
		qfits_header_add(cat->header, "COMMENT", "** filter_an: comments from input file:", NULL, NULL);
		fits_copy_all_headers(hdr, cat->header, "HISTORY");
		qfits_header_add(cat->header, "COMMENT", "** filter_an: end of comments from input file.", NULL, NULL);
	}

	// add placeholders...
	for (k=0; k< (maxperhp ? maxperhp : 100); k++) {
		char key[64];
		sprintf(key, "SWEEP%i", (k+1));
		qfits_header_add(cat->header, key, "-1", "placeholder", NULL);
	}

	if (an_catalog_write_headers(cat)) {
		fprintf(stderr, "Failed to write output catalog header.\n");
		exit(-1);
	}

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		int i, N;
		an_catalog* ancat;
		int nduplicates;
		int lastgrass = 0;

		infn = args[optind];
		ancat = an_catalog_open(infn);
		if (!ancat) {
			fprintf(stderr, "Couldn't open Astrometry.net catalog %s.\n", infn);
			exit(-1);
		}
		ancat->br.blocksize = BLOCK;
		N = an_catalog_count_entries(ancat);
		printf("Reading   %i entries from %s...\n", N, infn);
		fflush(stdout);
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
			int grass;

			grass = i*80 / N;
			if (grass != lastgrass) {
				printf(".");
				fflush(stdout);
				lastgrass = grass;
			}

			an = an_catalog_read_entry(ancat);
			if (!an) {
				fprintf(stderr, "Failed to read Astrometry.net catalog entry.\n");
				exit(-1);
			}

			hp = radectohealpix_nside(deg2rad(an->ra), deg2rad(an->dec), Nside);

			// dumbass magnitude averaging!
			summag = 0.0;
			nmag = 0;
			sumredmag = 0.0;
			nredmag = 0;
			if (sdss) {
				for (j=0; j<an->nobs; j++) {
					if (an->obs[j].catalog == AN_SOURCE_USNOB) {
						if (an->obs[j].band == 'E' || an->obs[j].band == 'F') {
							summag += an->obs[j].mag;
							nmag++;
						}
					} else if (an->obs[j].catalog == AN_SOURCE_TYCHO2) {
						if (an->obs[j].band == 'V' || an->obs[j].band == 'H') {
							summag += an->obs[j].mag;
							nmag++;
						}
					}
				}
			} else if (galex) {
				for (j=0; j<an->nobs; j++) {
					if (an->obs[j].catalog == AN_SOURCE_USNOB) {
						if (an->obs[j].band == 'O' || an->obs[j].band == 'J') {
							summag += an->obs[j].mag;
							nmag++;
						} else if (an->obs[j].band == 'E' || an->obs[j].band == 'F') {
							sumredmag += an->obs[j].mag;
							nredmag++;
						}
					} else if (an->obs[j].catalog == AN_SOURCE_TYCHO2) {
						if (an->obs[j].band == 'B' || an->obs[j].band == 'H') {
							summag += an->obs[j].mag;
							nmag++;
						} else if (an->obs[j].band == 'V') {
							sumredmag += an->obs[j].mag;
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
				radec2xyzarr(deg2rad(an->ra), deg2rad(an->dec), xyz);
				for (j=0; j<bl_size(starlists[hp]); j++) {
					double xyz2[3];
					stardata* sd2 = bl_access(starlists[hp], j);
					radec2xyzarr(deg2rad(sd2->entry.ra), deg2rad(sd2->entry.dec), xyz2);
					if (!distsq_exceeds(xyz, xyz2, 3, deduprad)) {
						nduplicates++;
						dup = TRUE;
						break;
					}
				}
				if (dup)
					continue;
			}

			memcpy(&sd.entry, an, sizeof(an_entry));
			bl_insert_sorted(starlists[hp], &sd, compare_stardata_mag);
		}
		printf("\n");

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

	// sweep through the healpixes...
	nowned = 0;
	for (i=0; i<HP; i++)
		if (starlists[i])
			nowned++;
	sweeplist = malloc(nowned * sizeof(stardata));

	nwritten = 0;
	for (k=0; !maxperhp || (k < maxperhp); k++) {
		char key[64];
		char val[64];
		nowned = 0;
		// gather up the stars that will be used in this sweep...
		for (i=0; i<HP; i++) {
			stardata* sd;
			int N;
			if (!starlists[i]) continue;
			N = bl_size(starlists[i]);
			if (k >= N)
				continue;
			sd = bl_access(starlists[i], k);
			memcpy(sweeplist + nowned, sd, sizeof(stardata));
			nowned++;
		}

		// sort them by magnitude...
		qsort(sweeplist, nowned, sizeof(stardata), compare_stardata_mag);

		// write them out...
		for (i=0; i<nowned; i++) {
			stardata* sd = sweeplist + i;
			an_catalog_write_entry(cat, &sd->entry);
			nwritten++;
		}

		// add to FITS header...
		if (maxperhp || (k<100)) {
			sprintf(key, "SWEEP%i", (k+1));
			sprintf(val, "%i", nowned);
			qfits_header_mod(cat->header, key, val, " ");
		}

		printf("sweep %i: got %i stars (%i total)\n", k, nowned, nwritten);
		fflush(stdout);
		if (!nowned)
			break;
	}
	printf("Made %i sweeps through the healpixes.\n", k);

	free(sweeplist);

	if (an_catalog_fix_headers(cat) ||
		an_catalog_close(cat)) {
		fprintf(stderr, "Failed to close output catalog.\n");
		exit(-1);
	}

	for (i=0; i<HP; i++)
		if (starlists[i])
			bl_free(starlists[i]);
	free(starlists);

	return 0;
}




