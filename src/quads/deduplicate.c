/**
   deduplicate: finds pairs of stars within a given distance of each other and removes
   one of the stars (the one that appears second).

   input: .objs catalogue
   output: .objs catalogue

   original author: dstn
*/

#include <math.h>

#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"
#include "dualtree_rangesearch.h"
#include "blocklist.h"
#include "KD/amma.h"

#define OPTIONS "hi:o:d:k:"
const char HelpString[] =
"deduplicate -d dist -i <input-file> -o <output-file> [-k keep]\n"
"  radius: (in arcseconds) is the de-duplication radius: a star found within\n"
"      this radius of another star will be discarded\n"
"  keep: read this number of stars from the catalogue and deduplicate them;\n"
"      ignore the rest of the catalogue.\n";

extern char *optarg;
extern int optind, opterr, optopt;

blocklist* duplicates = NULL;
void duplicate_found(void* nil, int ind1, int ind2, double dist2);

int main(int argc, char *argv[]) {
    int argidx, argchar;
    FILE *fin = NULL, *fout = NULL;
    sidx numstars;
    dimension Dim_Stars;
    double ramin, ramax, decmin, decmax;
    stararray *thestars = NULL;
    kdtree *starkd = NULL;
    double duprad = -1.0;
    char *infname = NULL, *outfname = NULL;
    double radians;
    double dist;
    int i;
    int keep = 0;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
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

    fprintf(stderr, "Reading catalogue %s...", infname);
    fflush(stderr);

    fopenin(infname, fin);

    fprintf(stderr, "Writing to catalogue %s...\n", outfname);
    fopenout(outfname, fout);

    thestars = readcat(fin, &numstars, &Dim_Stars,
					   &ramin, &ramax, &decmin, &decmax, keep);
    fclose(fin);
    if (thestars == NULL)
		return (1);
    fprintf(stderr, "got %lu stars.\n", numstars);
    fprintf(stderr, "    (dim %hu) (limits %f<=ra<=%f;%f<=dec<=%f.)\n",
			Dim_Stars, rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));

    fprintf(stderr, "Building KD tree...");
    fflush(stderr);

    starkd = mk_starkdtree(thestars, 10);
    if (starkd == NULL)
		return (2);
    fprintf(stderr, "done (%d nodes, depth %d).\n",
			starkd->num_nodes, starkd->max_depth);

    // convert from arcseconds to distance on the unit sphere.
    radians = (duprad / (60.0 * 60.0)) * (M_PI / 180.0);
    dist = sqrt(2.0*(1.0 - cos(radians)));

    duplicates = blocklist_int_new(256);

    // de-duplicate.
    dualtree_rangesearch(starkd, starkd,
						 RANGESEARCH_NO_LIMIT, dist,
						 duplicate_found, NULL);

    if (blocklist_count(duplicates) == 0) {
		fprintf(stderr, "No duplicate stars found.\n");
    } else {
		int dupind = 0;
		int skipind = -1;
		int srcind;
		int destind = 0;
		int N, Ndup;

		// remove repeated entries from "duplicates"
		// (yes, it can happen)
		for (i=0; i<blocklist_count(duplicates)-1; i++) {
			int a, b;
			a = blocklist_int_access(duplicates, i);
			b = blocklist_int_access(duplicates, i+1);
			if (a == b) {
				//fprintf(stderr, "Removed duplicate %i\n", a);
				blocklist_remove_index(duplicates, i);
				i--;
			}
		}

		N = dyv_array_size(thestars);
		Ndup = blocklist_count(duplicates);

		// remove duplicate entries from the star array.
		fprintf(stderr, "Removing %i duplicate stars (%i stars remain)...\n",
				Ndup, N-Ndup);
		for (srcind=0; srcind<N; srcind++) {
			// find the next index to skip
			if (skipind == -1) {
				if (dupind < Ndup) {
					skipind = blocklist_int_access(duplicates, dupind);
					//fprintf(stderr, "skipind=%i, srcind=%i\n", skipind, srcind);
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
				dyv* v;
				v = dyv_array_ref(thestars, srcind);
				thestars->array[srcind] = NULL;
				free_dyv(v);
				skipind = -1;
				continue;
			}

			// optimization: if source and dest are the same...
			if (srcind == destind) {
				destind++;
				continue;
			}

			thestars->array[destind] = dyv_array_ref(thestars, srcind);
			/*
			  dyv_array_set(thestars, destind,
			  dyv_array_ref(thestars, srcind));
			*/
			destind++;
		}
		thestars->size -= blocklist_count(duplicates);

    }
	blocklist_int_free(duplicates);
	duplicates = NULL;

	free_kdtree(starkd);
	starkd = NULL;

    fprintf(stderr, "Finding new {ra,dec}{min,max}...\n");
    fflush(stderr);
    {
		int N;
		double x,y,z;
		double ra,dec;

		N = thestars->size;
		decmin = ramin =  1e300;
		decmax = ramax = -1e300;
		for (i=0; i<N; i++) {
			dyv* v = dyv_array_ref(thestars, i);
			x = dyv_ref(v, 0);
			y = dyv_ref(v, 1);
			z = dyv_ref(v, 2);
			ra = xy2ra(x,y);
			dec = z2dec(z);
			if (ra > ramax) ramax = ra;
			if (ra < ramin) ramin = ra;
			if (dec > decmax) decmax = dec;
			if (dec < decmin) decmin = dec;
		}
		fprintf(stderr, "  ra=[%g, %g], dec=[%g, %g]\n",
				rad2deg(ramin),  rad2deg(ramax),
				rad2deg(decmin), rad2deg(decmax));
    }

    fprintf(stderr, "Writing catalogue...\n");
    fflush(stderr);

    numstars = thestars->size;

    write_objs_header(fout, numstars, DIM_STARS,
					  ramin, ramax, decmin, decmax);

    for (i=0; i<numstars; i++) {
		dyv* thestar = dyv_array_ref(thestars, i);
		fwritestar(thestar, fout);
    }
    fclose(fout);

    free_stararray(thestars);

    fprintf(stderr, "Done!\n");

#if !defined(USE_OS_MEMORY_MANAGEMENT)
	// free Andrew Moore's memory pool (so Valgrind works)
	am_free_to_os();
#endif
    return 0;
}

void duplicate_found(void* nil, int ind1, int ind2, double dist2) {
    if (ind1 <= ind2) return;
    // append the larger of the two.
    blocklist_int_insert_ascending(duplicates, ind1);
}


