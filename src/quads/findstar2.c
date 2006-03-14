#include <errno.h>
#include <math.h>
#include <string.h>
#include "starutil.h"
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"
#include "fileutil.h"
#include "catalog.h"

#define OPTIONS "hf:ixrRt:"
const char HelpString[] =
    "findstar -f fname -t dist {-i | -x | -r | -R} then read stdin\n";

extern char *optarg;
extern int optind, opterr, optopt;

char *treefname = NULL, *catfname = NULL, *basefname = NULL;
FILE *treefid = NULL;
catalog* cat = NULL;

int main(int argc, char *argv[])
{
	int argchar; //  opterr = 0;
	char whichset = 0, xyzset = 0, radecset = 0, dtolset = 0, read_dtol = 0;
	double dtol = 0.0;
	sidx numstars, whichstar;
	double xx, yy, zz, ra, dec;
	kdtree_qres_t *kq = NULL;
	kdtree_t *starkd = NULL;
	double qp[3];
	double* qpp = NULL;
	char scanrez = 1;
	int ii;

	if (argc <= 3) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 't':
			dtol = strtod(optarg, NULL);
			if (dtol < 0.0)
				dtol = 0.0;
			dtolset = 1;
			break;
		case 'i':
			whichset = 1;
			xyzset = 0;
			radecset = 0;
			break;
		case 'x':
			xyzset = 1;
			whichset = 0;
			radecset = 0;
			break;
		case 'r':
			radecset = 1;
			whichset = 0;
			xyzset = 0;
			break;
		case 'R':
			radecset = 1;
			whichset = 0;
			xyzset = 0;
			read_dtol = 1;
			break;
		case 'f':
			treefname = mk_streefn(optarg);
			catfname = mk_catfn(optarg);
			basefname = strdup(optarg);
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			fprintf(stderr, HelpString);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

	if (!xyzset && !radecset && !whichset) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	if (whichset) {
		cat = catalog_open(basefname, 0);
		if (!cat) {
			fprintf(stderr, "ERROR: couldn't open catalog");
		}
		free(cat);
	}

	fprintf(stderr, "findstar: getting stars from %s\n", treefname);
	fflush(stderr);
	fopenin(treefname, treefid);
	free_fn(treefname);
	starkd = kdtree_read(treefid);
	fclose(treefid);
	if (starkd == NULL)
		return 2;
	numstars = starkd->ndata;
	fprintf(stderr, "%lu stars, %d nodes\n", numstars, starkd->nnodes);

	while (!feof(stdin) && scanrez) {
		if (whichset) {
			scanrez = fscanf(stdin, "%lu", &whichstar);
			if (scanrez == 1) {
				if (whichstar < 0 || whichstar >= numstars) {
					fprintf(stdin, "ERROR: No such star %lu\n", whichstar);
					continue;
				}
				qpp = catalog_get_star(cat, whichstar);
			}
		} else if (radecset) {
			if (read_dtol) 
				scanrez = fscanf(stdin, "%lf %lf %lf", &ra, &dec, &dtol);
			else
				scanrez = fscanf(stdin, "%lf %lf", &ra, &dec);
			if ((scanrez == 2 && !read_dtol) || (scanrez ==
						3 && read_dtol)) {
				qp[0] = radec2x(deg2rad(ra), deg2rad(dec));
				qp[1] = radec2y(deg2rad(ra), deg2rad(dec));
				qp[2] = radec2z(deg2rad(ra), deg2rad(dec));
				qpp = qp;
			}
		} else { //xyzset
			scanrez = fscanf(stdin, "%lf %lf %lf", &xx, &yy, &zz);
			if (scanrez == 3) {
				qp[0] = xx;
				qp[1] = yy;
				qp[2] = zz;
				qpp = qp;
			} else {
				fprintf(stderr, "ERROR: need 3 numbers (x,y,z)\n");
				continue;
			}
		}

		kq = kdtree_rangesearch(starkd, qpp, dtol*dtol); 
		if (!kq) continue;

		fprintf(stdout, "{\n");
		for (ii = 0; ii < kq->nres; ii++) {
			fprintf(stdout, "%u: ((%lf, %lf, %lf), (%lf, %lf)),\n",
				kq->inds[ii], kq->results[ii], kq->results[ii+1], kq->results[ii+2],
				rad2deg(xy2ra(kq->results[ii], kq->results[ii+1])),
				rad2deg(z2dec(kq->results[ii+2])));
		}
		fprintf(stdout, "}\n");
		fflush(stdin);

		kdtree_free_query(kq);
	}

	if (cat)
		catalog_close(cat);

	return 0;
}
