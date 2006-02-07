#include <math.h>

#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"
#include "KD/amma.h"

#define OPTIONS "hl:f:k:"

const char HelpString[] =
"startree -f fname [-l kdtree-leaf-size] [-k keep]\n"
"  -l kdtree-leaf-size: (default 50) is the max# points per leaf in KD tree\n"
"  -k keep: is the number of stars to read from the catalogue\n";

char *treefname = NULL;
char *catfname = NULL;

extern char *optarg;
extern int optind, opterr, optopt;

/*
  I used to have de-duplication incorporated into the startree process,
  but we can't just remove the duplicates from the array of stars, because
  the position of a star in the array is significant: it's assumed to be the
  same as the position of the star in the .objs catalogue.
  Therefore, we instead have to do de-duplication of the objs file, creating
  a fresh objs file with consistent indices.
  The "deduplicate" program does this.
*/

int main(int argc, char *argv[]) {
	int argidx, argchar;
	int kd_Rmin = DEFAULT_KDRMIN;
	FILE *catfid = NULL, *treefid = NULL;
	sidx numstars;
	dimension Dim_Stars;
	double ramin, ramax, decmin, decmax;
	stararray *thestars = NULL;
	kdtree *starkd = NULL;
	int nkeep = 0;

	if (argc <= 2) {
		fprintf(stderr, HelpString);
		return (HELP_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'l':
			kd_Rmin = (int)strtoul(optarg, NULL, 0);
			break;
		case 'k':
		  nkeep = atoi(optarg);
		  if (nkeep == 0) {
			printf("Couldn't parse \'keep\': \"%s\"\n", optarg);
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

	free_stararray(thestars);

	fprintf(stderr, "  Writing star KD tree to %s...", treefname);
	fflush(stderr);
	fopenout(treefname, treefid);
	free_fn(treefname);
	write_starkd(treefid, starkd, ramin, ramax, decmin, decmax);
	fprintf(stderr, "done.\n");
	fclose(treefid);

	free_kdtree(starkd);

#if !defined(USE_OS_MEMORY_MANAGEMENT)
	// free Andrew Moore's memory pool (so Valgrind works)
	am_free_to_os();
#endif
	return 0;
}

