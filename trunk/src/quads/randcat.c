#include "starutil.h"
#include "fileutil.h"
#include "catalog.h"

#define OPTIONS "hn:f:r:R:d:D:S:"
const char HelpString[] =
  "randcat -f fname -n numstars [-r/R RAmin/max] [-d/D DECmin/max]\n"
    "  -r -R -d -D set ra and dec limits in radians\n";

extern char *optarg;
extern int optind, opterr, optopt;

int RANDSEED = 999;

int main(int argc, char *argv[])
{
	int argidx, argchar;
	int numstars = 10;
	char *fname = NULL;
	double ramin = DEFAULT_RAMIN, ramax = DEFAULT_RAMAX;
	double decmin = DEFAULT_DECMIN, decmax = DEFAULT_DECMAX;
	int i;
	catalog cat;

	if (argc <= 4) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'S':
			RANDSEED = atoi(optarg);
			break;
		case 'n':
			numstars = strtoul(optarg, NULL, 0);
			break;
		case 'r':
			ramin = strtod(optarg, NULL);
			break;
		case 'R':
			ramax = strtod(optarg, NULL);
			break;
		case 'd':
			decmin = strtod(optarg, NULL);
			break;
		case 'D':
			decmax = strtod(optarg, NULL);
			break;
		case 'f':
			fname = mk_catfn(optarg);
			break;
		case '?':
			fprintf (stderr, "Unknown option `-%c'.\n", optopt);
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
		return (OPT_ERR);
	}

	srand(RANDSEED);

	fprintf(stderr, "randcat: Making %u random stars\n", numstars);
	fprintf(stderr, "[RANDSEED=%d]\n", RANDSEED);
	if (ramin > DEFAULT_RAMIN || ramax < DEFAULT_RAMAX ||
		decmin > DEFAULT_DECMIN || decmax < DEFAULT_DECMAX)
		fprintf(stderr, "  using limits %f<=RA<=%f ; %f<=DEC<=%f deg.\n",
				rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));

	cat.nstars = numstars;
	cat.stars = malloc(numstars * DIM_STARS * sizeof(double));

	for (i=0; i <numstars; i++) {
		make_rand_star(cat.stars + i*DIM_STARS, ramin, ramax, decmin, decmax);
	}

	catalog_compute_radecminmax(&cat);

	if (catalog_write_to_file(&cat, fname, 0)) {
		fprintf(stderr, "Failed to write catalog.\n");
		free(cat.stars);
		exit(-1);
	}

	free(cat.stars);

	return (0);

}



