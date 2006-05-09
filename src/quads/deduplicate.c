/**
   deduplicate: finds pairs of stars within a given distance of each other and removes
   one of the stars (the one that appears second).

   input: .objs catalogue
   output: .objs catalogue

   original author: dstn
*/

#include <math.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "dualtree_rangesearch.h"
#include "bl.h"
#include "catalog.h"

#define OPTIONS "hi:o:d:k:A:B:C:D:"
const char HelpString[] =
"deduplicate -d dist -i <input-file> -o <output-file> [-k keep]\n"
"            [-A ra-min -B ra-max -C dec-min -D dec-max]\n"
"  radius: (in arcseconds) is the de-duplication radius: a star found within\n"
"      this radius of another star will be discarded\n"
"  keep: read this number of stars from the catalogue and deduplicate them;\n"
"      ignore the rest of the catalogue.\n"
"  (ra,dec)-(min,max): (in degrees) ignore any stars outside this range of RA, DEC.\n";


extern char *optarg;
extern int optind, opterr, optopt;

il* duplicates = NULL;
void duplicate_found(void* nil, int ind1, int ind2, double dist2);

int main(int argc, char *argv[]) {
    int argidx, argchar;
	catalog* cat;
    kdtree_t *starkd = NULL;
    double duprad = 0.0;
    char *infname = NULL, *outfname = NULL;
    double dist;
    int i;
    int keep = 0;
	double ramin = -1e300, decmin = -1e300, ramax = 1e300, decmax = 1e300;
	bool radecrange = FALSE;
	double* thestars;
	int levels;
	int Nleaf;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'A':
			ramin = atof(optarg);
			radecrange = TRUE;
			break;
		case 'B':
			ramax = atof(optarg);
			radecrange = TRUE;
			break;
		case 'C':
			decmin = atof(optarg);
			radecrange = TRUE;
			break;
		case 'D':
			decmax = atof(optarg);
			radecrange = TRUE;
			break;
		case 'd':
			duprad = atof(optarg);
			if (duprad < 0.0) {
				printf("Couldn't parse \'radius\': \"%s\"\n", optarg);
				exit(-1);
			}
			break;
		case 'k':
			keep = atoi(optarg);
			if (keep < 0) {
				printf("Couldn't parse \'keep\': \"%s\"\n", optarg);
				exit(-1);
			}
			break;
		case 'i':
			infname = optarg;
			break;
		case 'o':
			outfname = optarg;
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			fprintf(stderr, HelpString);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

    if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		fprintf(stderr, HelpString);
		return (HELP_ERR);
    }
    if (!infname || !outfname || duprad < 0.0) {
		fprintf(stderr, HelpString);
		return (HELP_ERR);
    }

	if (radecrange) {
		fprintf(stderr, "Will keep stars in range RA=[%g, %g], DEC=[%g, %g] degrees.\n",
				ramin, ramax, decmin, decmax);
	}

    fprintf(stderr, "Reading catalogue %s...\n", infname);
    fflush(stderr);

    fprintf(stderr, "Will write to catalogue %s...\n", outfname);

	cat = catalog_open(infname, 0, 1);

    if (!cat) {
		fprintf(stderr, "Couldn't open catalog %s.objs\n", infname);
		exit(-1);
	}
    fprintf(stderr, "Got %u stars.\n"
			"Limits RA=[%g, %g] DEC=[%g, %g] degrees.\n",
            cat->nstars,
			rad2deg(cat->ramin), rad2deg(cat->ramax),
			rad2deg(cat->decmin), rad2deg(cat->decmax));

	thestars = catalog_get_base(cat);

	if (radecrange) {
		int srcind, dstind;

		// ra,dec min,max were specified in degrees...
		ramin  = deg2rad(ramin);
		ramax  = deg2rad(ramax);
		decmin = deg2rad(decmin);
		decmax = deg2rad(decmax);

		dstind = 0;
		for (srcind=0; srcind<cat->nstars; srcind++) {
			double* s;
			double x, y, z, ra, dec;
			s = catalog_get_star(cat, srcind);
			x = s[0];
			y = s[1];
			z = s[2];
			ra = xy2ra(x, y);
			dec = z2dec(z);
			if (inrange(ra,  ramin,  ramax) &&
				inrange(dec, decmin, decmax)) {

				if (srcind == dstind)
					continue;

				memcpy(thestars + dstind*DIM_STARS,
					   thestars + srcind*DIM_STARS,
					   DIM_STARS * sizeof(double));
			}
		}
		cat->nstars = dstind;
		fprintf(stderr, "There are %i stars in the RA,DEC window you requested.\n", cat->nstars);
	}

	if (keep && keep < cat->nstars) {
		cat->nstars = keep;
		fprintf(stderr, "Keeping %i stars.\n", cat->nstars);
	}

	if (duprad == 0.0) {
		fprintf(stderr, "Not deduplicating - you specified a deduplication radius of zero.\n");
	} else {

		Nleaf = 10;
		levels = (int)((log((double)cat->nstars) - log((double)Nleaf))/log(2.0));
		if (levels < 1) {
			levels = 1;
		}

		fprintf(stderr, "Building KD tree (with %i levels)...", levels);
		fflush(stderr);

		starkd = kdtree_build(catalog_get_base(cat), cat->nstars, DIM_STARS, levels);
		if (!starkd) {
			fprintf(stderr, "Couldn't create star kdtree.\n");
			catalog_close(cat);
			exit(-1);
		}
		fprintf(stderr, "Done.\n");

		// convert from arcseconds to distance on the unit sphere.
		dist = sqrt(arcsec2distsq(duprad));

		duplicates = il_new(256);

		fprintf(stderr, "Running dual-tree search to find duplicates...\n");
		fflush(stderr);

		// de-duplicate.
		dualtree_rangesearch(starkd, starkd,
							 RANGESEARCH_NO_LIMIT, dist,
							 duplicate_found, NULL);

		fprintf(stderr, "Done!");

		if (il_size(duplicates) == 0) {
			fprintf(stderr, "No duplicate stars found.\n");
		} else {
			int dupind = 0;
			int skipind = -1;
			int srcind;
			int destind = 0;
			int N, Ndup;

			// remove repeated entries from "duplicates"
			// (yes, it can happen)
			for (i=0; i<il_size(duplicates)-1; i++) {
				int a, b;
				a = il_get(duplicates, i);
				b = il_get(duplicates, i+1);
				if (a == b) {
					il_remove(duplicates, i);
					i--;
				}
			}

			N = cat->nstars;
			Ndup = il_size(duplicates);

			// remove duplicate entries from the star array.
			fprintf(stderr, "Removing %i duplicate stars (%i stars remain)...\n",
					Ndup, N-Ndup);
			for (srcind=0; srcind<N; srcind++) {
				// find the next index to skip
				if (skipind == -1) {
					if (dupind < Ndup) {
						skipind = il_get(duplicates, dupind);
						dupind++;
					}
					// shouldn't happen, I think...
					if ((skipind > 0) && (skipind < srcind)) {
						fprintf(stderr, "Warning: duplicate index %i skipped\n", skipind);
						fprintf(stderr, "skipind=%i, srcind=%i\n", skipind, srcind);
						skipind = -1;
					}
				}
				if (srcind == skipind)
					continue;

				// optimization: if source and dest are the same...
				if (srcind == destind) {
					destind++;
					continue;
				}

				memcpy(thestars + destind*DIM_STARS, thestars + srcind*DIM_STARS,
					   DIM_STARS * sizeof(double));

				destind++;
			}
			cat->nstars -= il_size(duplicates);
		}
		il_free(duplicates);
		duplicates = NULL;

		kdtree_free(starkd);
		starkd = NULL;

	}

    fprintf(stderr, "Finding new {ra,dec}{min,max}...\n");
    fflush(stderr);

	catalog_compute_radecminmax(cat);

	fprintf(stderr, "  ra=[%g, %g], dec=[%g, %g]\n",
			rad2deg(cat->ramin),  rad2deg(cat->ramax),
			rad2deg(cat->decmin), rad2deg(cat->decmax));

    fprintf(stderr, "Writing catalogue...\n");
    fflush(stderr);

	if (catalog_write_to_file(cat, outfname, 0)) {
		fprintf(stderr, "Couldn't write catalog to file %s.\n", outfname);
		catalog_close(cat);
		exit(-1);
	}

	catalog_close(cat);

    fprintf(stderr, "Done!\n");
    return 0;
}

void duplicate_found(void* nil, int ind1, int ind2, double dist2) {
    if (ind1 <= ind2) return;
    // append the larger of the two.
    il_insert_ascending(duplicates, ind1);
}


