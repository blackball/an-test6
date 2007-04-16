/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <errno.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "starutil.h"
#include "kdtree.h"
#include "starkd.h"
#include "fileutil.h"
#include "catalog.h"

#define OPTIONS "hf:ixrRt:"
const char HelpString[] =
    "findstar -f fname -t dist {-i | -x | -r | -R} then read stdin\n";

extern char *optarg;
extern int optind, opterr, optopt;

char *treefname = NULL, *catfname = NULL, *basefname = NULL;
catalog* cat = NULL;

int main(int argc, char *argv[])
{
	int argchar;
	char whichset = 0, xyzset = 0, radecset = 0, dtolset = 0, read_dtol = 0;
	double dtol = 0.0;
	uint numstars, whichstar;
	double xx, yy, zz, ra, dec;
	kdtree_qres_t *kq = NULL;
	startree* starkd = NULL;
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
		char* catfname = mk_catfn(basefname);
		cat = catalog_open(catfname, 0);
		if (!cat) {
			fprintf(stderr, "ERROR: couldn't open catalog.\n");
		}
		free_fn(catfname);
	}

	fprintf(stderr, "findstar: getting stars from %s\n", treefname);
	fflush(stderr);
	starkd = startree_open(treefname);
	free_fn(treefname);
	if (!starkd)
		return 2;
	numstars = startree_N(starkd);
	fprintf(stderr, "%u stars\n", numstars);

	while (!feof(stdin) && scanrez) {
		if (whichset) {
			scanrez = fscanf(stdin, "%u", &whichstar);
			if (scanrez == 1) {
				if (whichstar >= numstars) {
					fprintf(stdin, "ERROR: No such star %u\n", whichstar);
					continue;
				}
				if (cat)
					qpp = catalog_get_star(cat, whichstar);
				else {
					startree_get(starkd, whichstar, qp);
					qpp = qp;
				}
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

		fprintf(stderr, "findstar:: Got info; searching\n");
		kq = kdtree_rangesearch(starkd->tree, qpp, dtol*dtol); 
		if (!kq) continue;
		fprintf(stderr, "findstar:: got %d results\n", kq->nres);

		fprintf(stdout, "{\n");
		for (ii = 0; ii < kq->nres; ii++) {
			double rx, ry, rz;
			rx = kq->results.d[3*ii];
			ry = kq->results.d[3*ii + 1];
			rz = kq->results.d[3*ii + 2];
			fprintf(stdout, "%u: ((%lf, %lf, %lf), (%lf, %lf)),\n",
					kq->inds[ii], rx, ry, rz,
					rad2deg(xy2ra(rx, ry)), rad2deg(z2dec(rz)));
		}
		fprintf(stdout, "}\n");
		fflush(stdout);
		fflush(stdin);

		kdtree_free_query(kq);
	}

	if (cat)
		catalog_close(cat);
	else
		startree_close(starkd);

	return 0;
}
