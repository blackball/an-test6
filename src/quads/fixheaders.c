#include <errno.h>
#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:o:"

extern char *optarg;
extern int optind, opterr, optopt;

char *outfname = NULL, *catfname = NULL;
FILE *outfid = NULL, *catfid = NULL;
char cASCII = (char)READ_FAIL;
char buff[100], oneobjWidth;

int main(int argc, char *argv[])
{
	int argidx, argchar; //  opterr = 0;
	double xx, yy, zz, ra, dec, ramin, ramax, decmin, decmax;
	sidx numstars, whichstar, ii;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'f':
            catfname = mk_catfn(optarg);
            break;
        case 'o':
            outfname = mk_catfn(optarg);
            break;
        default:
            return (OPT_ERR);
        }
    
    dimension DimStars;
    fprintf(stderr, "fixheaders: reading catalogue  %s\n", catfname);
    fprintf(stderr, "            reading results to %s\n", outfname);
    fopenin(catfname, catfid);
    fopenout(outfname, outfid);
    free_fn(outfname);
    free_fn(catfname);

    cASCII = read_objs_header(catfid, &numstars, &DimStars,
                              &ramin, &ramax, &decmin, &decmax);
    if (cASCII == (char)READ_FAIL)
        return (1);
    if (cASCII) {
        sprintf(buff, "%lf,%lf,%lf\n", 0.0, 0.0, 0.0);
        oneobjWidth = strlen(buff);
    }
    fprintf(stderr, "    (%lu stars) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
            numstars, rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));
    //catposmarker = ftello(catfid);
    return 0;
}
