#include <math.h>

#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"
#include "dualtree_rangesearch.h"
#include "blocklist.h"

#define OPTIONS "hR:f:k:d:"
const char HelpString[] =
    "startree -f fname [-R KD_RMIN] [-k keep] [-d radius]\n"
    "  KD_RMIN: (default 50) is the max# points per leaf in KD tree\n"
    "  keep: is the number of stars read from the catalogue\n"
    "  radius: (in arcseconds) is the de-duplication radius: a star found within\n"
    "      this radius of another star will be discarded\n";

char *treefname = NULL;
char *catfname = NULL;

extern char *optarg;
extern int optind, opterr, optopt;


blocklist* duplicates = NULL;
void duplicate_found(void* nil, int ind1, int ind2, double dist2);


int main(int argc, char *argv[])
{
	int argidx, argchar;
	int kd_Rmin = DEFAULT_KDRMIN;
	FILE *catfid = NULL, *treefid = NULL;
	sidx numstars;
	dimension Dim_Stars;
	double ramin, ramax, decmin, decmax;
	stararray *thestars = NULL;
	kdtree *starkd = NULL;
	int nkeep = 0;
	double duprad = -1.0;

	if (argc <= 2) {
		fprintf(stderr, HelpString);
		return (HELP_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'R':
			kd_Rmin = (int)strtoul(optarg, NULL, 0);
			break;
		case 'k':
		  nkeep = atoi(optarg);
		  if (nkeep == 0) {
			printf("Couldn't parse \'keep\': \"%s\"\n", optarg);
			exit(-1);
		  }
		  break;
		case 'd':
		    duprad = atof(optarg);
		    if (duprad < 0.0) {
			printf("Couldn't parse \'radius\': \"%s\"\n", optarg);
			exit(-1);
		    }
		    break;
		case 'f':
			treefname = mk_streefn(optarg);
			catfname = mk_catfn(optarg);
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

	fprintf(stderr, "startree: building KD tree for %s\n", catfname);

	if (nkeep) {
	  fprintf(stderr, "keeping at most %i stars.\n", nkeep);
	}

	fprintf(stderr, "  Reading star catalogue...");
	fflush(stderr);
	fopenin(catfname, catfid);
	free_fn(catfname);
	thestars = readcat(catfid, &numstars, &Dim_Stars,
	                   &ramin, &ramax, &decmin, &decmax, nkeep);
	fclose(catfid);
	if (thestars == NULL)
		return (1);
	fprintf(stderr, "got %lu stars.\n", numstars);
	fprintf(stderr, "    (dim %hu) (limits %f<=ra<=%f;%f<=dec<=%f.)\n",
	        Dim_Stars, rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));

	fprintf(stderr, "  Building star KD tree...");
	fflush(stderr);
	starkd = mk_starkdtree(thestars, kd_Rmin);

	if (starkd == NULL)
		return (2);
	fprintf(stderr, "done (%d nodes, depth %d).\n",
	        starkd->num_nodes, starkd->max_depth);


	while (duprad >= 0.0) {
	    // convert from arcseconds to distance on the unit sphere.
	    double radians;
	    double dist;
	    radians = (duprad / (60.0 * 60.0)) * (M_PI / 180.0);
	    dist = sqrt(2.0*(1.0 - cos(radians)));

	    //printf("dist=%g\n", dist);

	    duplicates = blocklist_int_new(256);

	    // de-duplicate.
	    dualtree_rangesearch(starkd, starkd,
				 RANGESEARCH_NO_LIMIT, dist,
				 duplicate_found, NULL);

	    if (blocklist_count(duplicates) == 0) break;

	    free_kdtree(starkd);
	    starkd = NULL;

	    /*
	      printf("duplicates:\n");
	      blocklist_int_print(duplicates);
	    */

	    // remove duplicate entries from the star array.
	    printf("Removing %i duplicate stars (%i stars remain)...\n",
		   blocklist_count(duplicates), dyv_array_size(thestars) - blocklist_count(duplicates));
	    {
		int dupind = 0;
		int skipind = -1;
		int srcind;
		int destind = 0;
		int N;

		N = dyv_array_size(thestars);
		for (srcind=0; srcind<N; srcind++) {
		    // find the next index to skip
		    if (skipind == -1) {
			if (dupind < blocklist_count(duplicates)) {
			    skipind = blocklist_int_access(duplicates, dupind);
			    dupind++;
			}
		    }
		    if (srcind == skipind) {
			skipind = -1;
			continue;
		    }

		    // optimization: if source and dest are the same...
		    if (srcind == destind) {
			destind++;
			continue;
		    }

		    dyv_array_set(thestars, destind,
				  dyv_array_ref(thestars, srcind));
		    destind++;
		}
		thestars->size -= blocklist_count(duplicates);
	    }

	    blocklist_int_free(duplicates);
	    duplicates = NULL;

	    fprintf(stderr, "Rebuilding star KD tree...");
	    fflush(stderr);
	    starkd = mk_starkdtree(thestars, kd_Rmin);

	    if (starkd == NULL) {
		return 2;
	    }
	    fprintf(stderr, "done (%d nodes, depth %d).\n",
		    starkd->num_nodes, starkd->max_depth);

	    break;
	}

	free_stararray(thestars);

	fprintf(stderr, "  Writing star KD tree to %s...", treefname);
	fflush(stderr);
	fopenout(treefname, treefid);
	free_fn(treefname);
	write_starkd(treefid, starkd, ramin, ramax, decmin, decmax);
	fprintf(stderr, "done.\n");
	fclose(treefid);

	free_kdtree(starkd);

	//basic_am_malloc_report();
	return (0);

}

void duplicate_found(void* nil, int ind1, int ind2, double dist2) {
    if (ind1 <= ind2) return;
    //printf("total %i duplicates found: %i, %i, dist %g\n", ndups, ind1, ind2, sqrt(dist2));
    // append the larger of the two.
    blocklist_int_insert_ascending(duplicates, ind1);
}


