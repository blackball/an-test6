#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "habn:f:r:R:d:D:"
const char HelpString[] =
    "randcat -f catfile -n numstars [-a|-b] [-r/R RAmin/max] [-d/D DECmin/max]\n"
    "  -r -R -d -D set ra and dec limits in radians\n"
    "  -a sets ASCII output, -b (default) sets BINARY output\n";

void output_star(star *thestar, char ASCII, FILE *fid);


extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;

	sidx numstars = 10;
	char ASCII = 1;
	char *fname = NULL;
	double ramin = DEFAULT_RAMIN, ramax = DEFAULT_RAMAX;
	double decmin = DEFAULT_DECMIN, decmax = DEFAULT_DECMAX;
	sidx ii;
	star *thestar;
	FILE *fid = NULL;

	if (argc <= 4) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'a':
			ASCII = 1;
			break;
		case 'b':
			ASCII = 0;
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

#define RANDSEED 999
	am_srand(RANDSEED);

	fprintf(stderr, "randcat: Making %lu random stars", numstars);
#if PLANAR_GEOMETRY==1

	fprintf(stderr, " in the unit square ");
#else

	fprintf(stderr, " on the unit sphere ");
#endif

	fprintf(stderr, "[RANDSEED=%d]\n", RANDSEED);
	if (ramin > DEFAULT_RAMIN || ramax < DEFAULT_RAMAX ||
	        decmin > DEFAULT_DECMIN || decmax < DEFAULT_DECMAX)
#if PLANAR_GEOMETRY==1

		fprintf(stderr, "  using limits %f<=x<=%f ; %f<=y<=%f.\n",
		        ramin, ramax, decmin, decmax);
#else

		fprintf(stderr, "  using limits %f<=RA<=%f ; %f<=DEC<=%f deg.\n",
				rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));
#endif

	fopenout(fname, fid);
	free_fn(fname);

	write_objs_header(fid, ASCII, numstars, DIM_STARS, ramin, ramax, decmin, decmax);

	for (ii = 0;ii < numstars;ii++) {
		thestar = make_rand_star(ramin, ramax, decmin, decmax);
		if (thestar == NULL) {
			fprintf(stderr, "ERROR (randcat) -- out of memory at star %lu\n", ii);
			return (1);
		} else {
			output_star(thestar, ASCII, fid);
			free_star(thestar);
		}
	}
	fclose(fid);

	return (0);

}



void output_star(star *thestar, char ASCII, FILE *fid)
{
	if (ASCII)
		fprintfstar(thestar, fid);
	else if (!ASCII)
		fwritestar(thestar, fid);
}
