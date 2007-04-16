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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "an_catalog.h"
#include "starutil.h"
#include "mathutil.h"
#include "rdlist.h"
#include "kdtree.h"
#include "bl.h"
#include "dualtree_rangesearch.h"

kdtree_t* antree;

static void rangesearch_callback(void* extra, int gind, int aind, double dist2) {
	il* anlist = extra;
	il_append(anlist, antree->perm[aind]);
}

int main(int argc, char** args) {

	char* rdfname = "/h/42/dstn/stars/GALEX_FIELDS/galex-fuv.rdls.fits";
	char* catfname = "/h/42/dstn/local/AN/an_hp354.fits";
	char* catoutfname = "an-galex-cut.fits";
	double nearbydist = 5.0; // arcsec

	rdlist* rdls;
	an_catalog* cat;

	double* tmpdata;
	double* gdata;
	double* adata;
	int Ndata;
	int levels;

	kdtree_t* galextree;

	an_entry* ancat;

	an_catalog* catout;

	il* anlist;

	int i;

	double maxdist = sqrt(arcsec2distsq(nearbydist));

	cat = an_catalog_open(catfname);
	rdls = rdlist_open(rdfname);

 	if (!cat) {
		fprintf(stderr, "Failed to open AN catalog %s.\n", catfname);
		exit(-1);
	}
 	if (!rdls) {
		fprintf(stderr, "Failed to open Galex FUV rdls file %s\n", rdfname);
		exit(-1);
	}

	catout = an_catalog_open_for_writing(catoutfname);
	if (!catout) {
		fprintf(stderr, "Failed to open AN catalog %s for writing.\n", catoutfname);
		exit(-1);
	}
	if (an_catalog_write_headers(catout)) {
		fprintf(stderr, "Failed to write to AN catalog %s.\n", catoutfname);
		exit(-1);
	}


	Ndata = rdlist_n_entries(rdls, 0);
	tmpdata = malloc(Ndata * 2 * sizeof(double));
	rdlist_read_entries(rdls, 0, 0, Ndata, tmpdata);
	rdlist_close(rdls);
	gdata = malloc(Ndata * 3 * sizeof(double));
	for (i=0; i<Ndata; i++)
		radec2xyzarr(deg2rad(tmpdata[i*2+0]), deg2rad(tmpdata[i*2+1]), gdata + i*3);
	free(tmpdata);
	levels = kdtree_compute_levels(Ndata, 5);
	galextree = kdtree_build(gdata, Ndata, 3, levels);

	Ndata = cat->nentries;
	ancat = malloc(Ndata * sizeof(an_entry));
	an_catalog_read_entries(cat, 0, Ndata, ancat);
	adata = malloc(Ndata * 3 * sizeof(double));
	for (i=0; i<Ndata; i++) {
		/*
		  an_entry* en = an_catalog_read_entry(cat);
		  assert(en);
		*/
		an_entry* en = ancat + i;
		radec2xyzarr(deg2rad(en->ra), deg2rad(en->dec), adata + i*3);
	}
	an_catalog_close(cat);

	levels = kdtree_compute_levels(Ndata, 5);
	antree = kdtree_build(adata, Ndata, 3, levels);

	anlist = il_new(1024);
	dualtree_rangesearch(galextree, antree, RANGESEARCH_NO_LIMIT, maxdist,
						 rangesearch_callback, anlist, NULL, NULL);

	kdtree_free(antree);
	free(adata);
	kdtree_free(galextree);
	free(gdata);

	for (i=0; i<il_size(anlist); i++) {
		an_catalog_write_entry(catout, ancat + il_get(anlist, i));
	}
	il_free(anlist);

	if (an_catalog_fix_headers(catout) ||
		an_catalog_close(catout)) {
		fprintf(stderr, "Failed to close output catalog.\n");
		exit(-1);
	}

	return 0;
}

