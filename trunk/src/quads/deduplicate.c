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
#include "idfile.h"

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

int npoints = 0;
void progress(void* nil, int ndone);

int main(int argc, char *argv[]) {
    int argidx, argchar;
	kdtree_t *starkd = NULL;
    double duprad = 0.0;
    char *infname = NULL, *outfname = NULL;
    int keep = 0;
	double ramin = -1e300, decmin = -1e300, ramax = 1e300, decmax = 1e300;
	bool radecrange = FALSE;
	double* thestars;
	uint64_t* theids;
	int levels;
	int Nleaf;
	idfile* id;
	catalog* catout;
	idfile* idout;
	char* fn;
	catalog* cat;
	double maxdist;

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

	fn = mk_catfn(infname);
	cat = catalog_open(fn, 1);
    if (!cat) {
		fprintf(stderr, "Couldn't open catalog %s\n", fn);
		exit(-1);
	}
    fprintf(stderr, "Reading catalogue file %s...\n", fn);
    fflush(stderr);
	free_fn(fn);

	fn = mk_idfn(infname);
	id = idfile_open(fn, 1);
	if (!id) {
		fprintf(stderr, "Couldn'tn open idfile %s\n", fn);
		exit(-1);
	}
    fprintf(stderr, "Reading id file %s...\n", fn);
    fflush(stderr);
	free_fn(fn);

	fn = mk_catfn(outfname);
	catout = catalog_open_for_writing(fn);
	if (!catout) {
		fprintf(stderr, "Couldn'tn open file %s for writing.\n", fn);
		exit(-1);
	}
    fprintf(stderr, "Will write to catalogue %s\n", fn);
	free_fn(fn);

	fn = mk_idfn(outfname);
	idout = idfile_open_for_writing(fn);
	if (!idout) {
		fprintf(stderr, "Couldn'tn open file %s for writing.\n", fn);
		exit(-1);
	}
    fprintf(stderr, "                 idfile %s\n", fn);
	free_fn(fn);

	if (idfile_write_header(idout) ||
		catalog_write_header(catout)) {
		fprintf(stderr, "Couldn't write headers.\n");
		exit(-1);
	}

    fprintf(stderr, "Got %u stars.\n"
			"Limits RA=[%g, %g] DEC=[%g, %g] degrees.\n",
            cat->numstars,
			rad2deg(cat->ramin), rad2deg(cat->ramax),
			rad2deg(cat->decmin), rad2deg(cat->decmax));

	if (id->numstars != cat->numstars) {
		fprintf(stderr, "Catalog has %i stars but idfile has %i stars.\n",
				cat->numstars, id->numstars);
		exit(-1);
	}

	thestars = catalog_get_base(cat);
	theids = id->anidarray;

	if (radecrange) {
		int srcind, dstind;

		// ra,dec min,max were specified in degrees...
		ramin  = deg2rad(ramin);
		ramax  = deg2rad(ramax);
		decmin = deg2rad(decmin);
		decmax = deg2rad(decmax);

		dstind = 0;
		for (srcind=0; srcind<cat->numstars; srcind++) {
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

				memcpy(theids + dstind,
					   theids + srcind,
					   sizeof(uint64_t));
			}
		}
		cat->numstars = dstind;
		id->numstars = dstind;
		fprintf(stderr, "There are %i stars in the RA,DEC window you requested.\n", cat->numstars);
	}

	if (keep && keep < cat->numstars) {
		cat->numstars = keep;
		id->numstars = keep;
		fprintf(stderr, "Keeping %i stars.\n", cat->numstars);
	}

	duplicates = il_new(256);

	if (duprad == 0.0) {
		fprintf(stderr, "Not deduplicating - you specified a deduplication radius of zero.\n");
	} else {
		Nleaf = 10;
		levels = (int)((log((double)cat->numstars) - log((double)Nleaf))/log(2.0));
		if (levels < 1) {
			levels = 1;
		}
		fprintf(stderr, "Building KD tree (with %i levels)...", levels);
		fflush(stderr);

		starkd = kdtree_build(catalog_get_base(cat), cat->numstars, DIM_STARS, levels);
		if (!starkd) {
			fprintf(stderr, "Couldn't create star kdtree.\n");
			catalog_close(cat);
			exit(-1);
		}
		fprintf(stderr, "Done.\n");

		// convert from arcseconds to distance on the unit sphere.
		maxdist = sqrt(arcsec2distsq(duprad));
		fprintf(stderr, "Running dual-tree search to find duplicates...\n");
		fflush(stderr);
		// de-duplicate.
		npoints = cat->numstars;
		dualtree_rangesearch(starkd, starkd,
							 RANGESEARCH_NO_LIMIT, maxdist,
							 duplicate_found, NULL,
							 progress, NULL);
		fprintf(stderr, "Done!");
	}

	{
		int dupind = 0;
		int skipind = -1;
		int srcind;
		int N, Ndup;

		N = cat->numstars;
		Ndup = il_size(duplicates);
		fprintf(stderr, "Removing %i duplicate stars (%i stars remain)...\n",
				Ndup, N-Ndup);
		fflush(stderr);

		// remove duplicate entries from the star array.
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
			if (srcind == skipind) {
				skipind = -1;
				continue;
			}

			catalog_write_star(catout, thestars + srcind * DIM_STARS);
			idfile_write_anid(idout, theids[srcind]);
		}
	}
	il_free(duplicates);
	if (starkd)
		kdtree_free(starkd);

	if (catalog_fix_header(catout) ||
		catalog_close(catout) ||
		idfile_fix_header(idout) ||
		idfile_close(idout)) {
		fprintf(stderr, "Error closing output files.\n");
		exit(-1);
	}

	catalog_close(cat);
	idfile_close(id);

	fprintf(stderr, "Done!\n");
	return 0;
}

int last_pct = 0;
void progress(void* nil, int ndone) {
	int pct = (int)(1000.0 * ndone / (double)npoints);
	if (pct != last_pct) {
		if (pct % 10 == 0) {
			printf("(%i %%)", pct/10);
			if (pct % 50 == 0)
				printf("\n");
		} else
			printf(".");
		fflush(stdout);
	}
	last_pct = pct;
}

void duplicate_found(void* nil, int ind1, int ind2, double dist2) {
	if (ind1 <= ind2) return;
	// append the larger of the two.
	il_insert_unique_ascending(duplicates, ind1);
}


