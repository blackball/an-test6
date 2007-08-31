/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "kdtree.h"
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "matchobj.h"
#include "catalog.h"
#include "tic.h"
#include "quadfile.h"
#include "idfile.h"
#include "intmap.h"
#include "xylist.h"
#include "rdlist.h"
#include "qidxfile.h"
#include "verify.h"
#include "qfits.h"
#include "ioutils.h"
#include "starkd.h"
#include "codekd.h"
#include "index.h"
#include "qidxfile.h"
#include "boilerplate.h"
#include "sip.h"
#include "sip_qfits.h"

const char* OPTIONS = "hx:w:i:"; // r:

void print_help(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s\n"
		   "   -w <WCS input file>\n"
		   "   -x <xyls input file>\n"
		   //"   -r <rdls output file>\n"
		   "   -i <index-name>\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	int c;
	char* xylsfn = NULL;
	char* wcsfn = NULL;
	//char* rdlsfn = NULL;

	sl* indexnames;
	pl* indexes;
	pl* qidxes;

	//rdlist* rdls = NULL;
	xylist* xyls = NULL;
	sip_t sip;
	qfits_header* hdr;
	int i;
	int W, H;
	double xyzcenter[3];
	double fieldrad2;

	double pixr2 = 1.0;

	indexnames = sl_new(8);

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'i':
			sl_append(indexnames, optarg);
			break;
			/*
		case 'r':
		rdlsfn = optarg;
		break;
			*/
		case 'x':
			xylsfn = optarg;
			break;
		case 'w':
			wcsfn = optarg;
			break;
		}
	}
	if (optind != argc) {
		print_help(args[0]);
		exit(-1);
	}
	if (/*!rdlsfn ||*/ !xylsfn || !wcsfn) {
		print_help(args[0]);
		exit(-1);
	}

	// read WCS.
	hdr = qfits_header_read(wcsfn);
	if (!hdr) {
		fprintf(stderr, "Failed to read FITS header from file %s.\n", wcsfn);
		exit(-1);
	}
	fprintf(stderr, "Trying to parse SIP header from %s...\n", wcsfn);
	if (!sip_read_header(hdr, &sip)) {
		fprintf(stderr, "Failed to parse SIP header from %s.\n", wcsfn);
	}
	// image W, H
	W = qfits_header_getint(hdr, "IMAGEW", 0);
	H = qfits_header_getint(hdr, "IMAGEH", 0);
	if (!(W && H)) {
		fprintf(stderr, "WCS file %s didn't contain IMAGEW and IMAGEH headers.\n", wcsfn);
		// FIXME - use bounds of xylist?
		exit(-1);
	}

	// read XYLS.
	xyls = xylist_open(xylsfn);
	if (!xyls) {
		fprintf(stderr, "Failed to read an xylist from file %s.\n", xylsfn);
		exit(-1);
	}

	// read RDLS.
	/*
	  rdls = rdlist_open(rdlsfn);
	  if (!rdls) {
	  fprintf(stderr, "Failed to open rdlist from file %s.\n", rdlsfn);
	  exit(-1);
	  }
	*/

	// read indices.
	indexes = pl_new(8);
	qidxes = pl_new(8);
	for (i=0; i<sl_size(indexnames); i++) {
		char* name = sl_get(indexnames, i);
		index_t* indx;
		char* qidxfn;
		qidxfile* qidx;
		fprintf(stderr, "Loading index from %s...\n", name);
		indx = index_load(name, 0);
		if (!indx) {
			fprintf(stderr, "Failed to read index \"%s\".\n", name);
			exit(-1);
		}
		pl_append(indexes, indx);

		qidxfn = mk_qidxfn(name);
		qidx = qidxfile_open(qidxfn, 0);
		if (!qidx) {
			fprintf(stderr, "Failed to open qidxfile \"%s\".\n", qidxfn);
			exit(-1);
		}
		free_fn(qidxfn);
		pl_append(qidxes, qidx);
	}
	sl_free2(indexnames);

	// Find field center and radius.
	sip_pixelxy2xyzarr(&sip, W/2, H/2, xyzcenter);
	fieldrad2 = arcsec2distsq(sip_pixel_scale(&sip) * hypot(W/2, H/2));

	// Find all stars in the field.
	for (i=0; i<pl_size(indexes); i++) {
		kdtree_qres_t* res;
		index_t* indx;
		uint nquads;
		uint* quads;
		int j;
		qidxfile* qidx;
		il* uniqquadlist;
		il* quadlist;
		il* fullquadlist;
		il* starlist;
		il* starsinquadslist;
		il* starsinquadsfull;
		dl* starxylist;
		il* corrstars;
		il* corrquads;
		il* corruniqquads;
		il* corrfullquads;
		double* fieldxy;
		int Nfield;
		kdtree_t* ftree;
		int Nleaf = 5;
        int dimquads;

		indx = pl_get(indexes, i);
		qidx = pl_get(qidxes, i);

		// Find index stars.
		res = kdtree_rangesearch_options(indx->starkd->tree, xyzcenter, fieldrad2*1.05,
										 KD_OPTIONS_SMALL_RADIUS | KD_OPTIONS_RETURN_POINTS);
		if (!res || !res->nres) {
			fprintf(stderr, "No index stars found.\n");
			exit(-1);
		}
		fprintf(stderr, "Found %i index stars in range.\n", res->nres);

		starlist = il_new(16);
		starxylist = dl_new(16);

		// Find which ones in range are inside the image rectangle.
		for (j=0; j<res->nres; j++) {
			int starnum = res->inds[j];
			double x, y;
			if (!sip_xyzarr2pixelxy(&sip, res->results.d + j*3, &x, &y))
				continue;
			if ((x < 0) || (y < 0) || (x >= W) || (y >= H))
				continue;
			il_append(starlist, starnum);
			dl_append(starxylist, x);
			dl_append(starxylist, y);
		}
		fprintf(stderr, "Found %i index stars inside the field.\n", il_size(starlist));

		uniqquadlist = il_new(16);
		quadlist = il_new(16);

		// For each index star, find the quads of which it is a part.
		for (j=0; j<il_size(starlist); j++) {
			int k;
			int starnum = il_get(starlist, j);
			if (qidxfile_get_quads(qidx, starnum, &quads, &nquads)) {
				fprintf(stderr, "Failed to get quads for star %i.\n", starnum);
				exit(-1);
			}
			//fprintf(stderr, "star %i is involved in %i quads.\n", starnum, nquads);
			for (k=0; k<nquads; k++) {
				il_insert_ascending(quadlist, quads[k]);
				il_insert_unique_ascending(uniqquadlist, quads[k]);
			}
		}
		fprintf(stderr, "Found %i quads partially contained in the field.\n", il_size(uniqquadlist));

		// Find quads that are fully contained in the image.
		fullquadlist = il_new(16);
		for (j=0; j<il_size(uniqquadlist); j++) {
			int quad = il_get(uniqquadlist, j);
			int ind = il_index_of(quadlist, quad);
			if (ind + 3 >= il_size(quadlist))
				continue;
			if (il_get(quadlist, ind+3) != quad)
				continue;
			il_append(fullquadlist, quad);
		}
		fprintf(stderr, "Found %i quads fully contained in the field.\n", il_size(fullquadlist));

        dimquads = quadfile_dimquads(indx->quads);

		// Find the stars that are in quads.
		starsinquadslist = il_new(16);
		for (j=0; j<il_size(uniqquadlist); j++) {
            int k;
			uint stars[dimquads];
			int quad = il_get(uniqquadlist, j);
			quadfile_get_stars(indx->quads, quad, stars);
            for (k=0; k<dimquads; k++)
                il_insert_unique_ascending(starsinquadslist, stars[k]);
		}
		fprintf(stderr, "Found %i stars involved in quads (partially contained).\n", il_size(starsinquadslist));

		// Find the stars that are in quads that are completely contained.
		starsinquadsfull = il_new(16);
		for (j=0; j<il_size(fullquadlist); j++) {
            int k;
			uint stars[dimquads];
			int quad = il_get(fullquadlist, j);
			quadfile_get_stars(indx->quads, quad, stars);
            for (k=0; k<dimquads; k++)
                il_insert_unique_ascending(starsinquadsfull, stars[k]);
		}
		fprintf(stderr, "Found %i stars involved in quads (fully contained).\n", il_size(starsinquadsfull));

		// Now find correspondences between index objects and field objects.
		Nfield = xylist_n_entries(xyls, 1);
		fieldxy = malloc(Nfield * 2 * sizeof(double));
		if (xylist_read_entries(xyls, 1, 0, Nfield, fieldxy)) {
			fprintf(stderr, "Failed to read xyls entries.\n");
			exit(-1);
		}
		// Build a tree out of the field objects (in pixel space)
		ftree = kdtree_build(NULL, fieldxy, Nfield, 2, Nleaf, KDTT_DOUBLE, KD_BUILD_SPLIT);
		if (!ftree) {
			fprintf(stderr, "Failed to build kdtree.\n");
			exit(-1);
		}
		// For each index object involved in quads, search for a correspondence.
		corrstars = il_new(16);
		for (j=0; j<il_size(starsinquadslist); j++) {
			int star;
			double sxyz[3];
			double sxy[2];
			kdtree_qres_t* fres;
			star = il_get(starsinquadslist, j);
			if (startree_get(indx->starkd, star, sxyz)) {
				fprintf(stderr, "Failed to get position for star %i.\n", star);
				exit(-1);
			}
			if (!sip_xyzarr2pixelxy(&sip, sxyz, sxy, sxy+1)) {
				fprintf(stderr, "SIP backward for star %i.\n", star);
				exit(-1);
			}
			fres = kdtree_rangesearch_options(ftree, sxy, pixr2,
											  KD_OPTIONS_SMALL_RADIUS);
			if (!fres || !fres->nres)
				continue;
			if (fres->nres > 1) {
				fprintf(stderr, "%i matches for star %i.\n", fres->nres, star);
			}
			il_append(corrstars, star);
		}
		fprintf(stderr, "Found %i correspondences for stars involved in quads (partially contained).\n",
				il_size(corrstars));

		// Find quads built only from stars with correspondences.
		corrquads = il_new(16);
		corruniqquads = il_new(16);
		for (j=0; j<il_size(corrstars); j++) {
			int k;
			int starnum = il_get(corrstars, j);
			if (qidxfile_get_quads(qidx, starnum, &quads, &nquads)) {
				fprintf(stderr, "Failed to get quads for star %i.\n", starnum);
				exit(-1);
			}
			//fprintf(stderr, "star %i is involved in %i quads.\n", starnum, nquads);
			for (k=0; k<nquads; k++) {
				/* Don't need this - since we'll only get "full" quads if all four are in the "corr" set.
				  uint sA, sB, sC, sD;
				  int quad = quads[k];
				  quadfile_get_starids(indx->quads, quad, &sA, &sB, &sC, &sD);
				  if (!(il_contains(corrstars, sA) &&
				  il_contains(corrstars, sB) &&
				  il_contains(corrstars, sC) &&
				  il_contains(corrstars, sD)))
				  continue;
				*/
				il_insert_ascending(corrquads, quads[k]);
				il_insert_unique_ascending(corruniqquads, quads[k]);
			}
		}
		// This number doesn't mean anything :)
		//fprintf(stderr, "Found %i quads built from stars with correspondences.\n", il_size(corruniqquads));

		// Find quads that are fully contained in the image.
		corrfullquads = il_new(16);
		for (j=0; j<il_size(corruniqquads); j++) {
			int quad = il_get(corruniqquads, j);
			int ind = il_index_of(corrquads, quad);
			if (ind + 3 >= il_size(corrquads))
				continue;
			if (il_get(corrquads, ind+3) != quad)
				continue;
			il_append(corrfullquads, quad);
		}
		fprintf(stderr, "Found %i quads built from stars with correspondencs, fully contained in the field.\n", il_size(corrfullquads));

		for (j=0; j<il_size(corrfullquads); j++) {
			uint stars[dimquads];
			int k;
			int ind;
			double px,py;
			int quad = il_get(corrfullquads, j);
			quadfile_get_stars(indx->quads, quad, stars);
			// Gah! map...
			for (k=0; k<dimquads; k++) {
				ind = il_index_of(starlist, stars[k]);
				px = dl_get(starxylist, ind*2+0);
				py = dl_get(starxylist, ind*2+1);
				printf("%g %g ", px, py);
			}
			printf("\n");
		}

		il_free(fullquadlist);
		il_free(uniqquadlist);
		il_free(quadlist);
		il_free(starlist);
	}

	if (xylist_close(xyls)) {
		fprintf(stderr, "Failed to close XYLS file.\n");
	}
	/*
	  if (rdlist_close(rdls)) {
	  fprintf(stderr, "Failed to close RDLS file.\n");
	  }
	*/
	return 0;
}
