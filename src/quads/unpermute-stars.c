/**
   \file Applies a star kdtree permutation array to the corresponding
   .cat and .id files to produce new .cat, .id, and .skdt files that are
   consistent and don't require permutation.

   In:  .cat, .id, .skdt
   Out: .cat, .id, .skdt

   Original author: dstn
*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "kdtree.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"
#include "starutil.h"
#include "fileutil.h"
#include "catalog.h"
#include "idfile.h"
#include "fitsioutils.h"
#include "qfits.h"

#define OPTIONS "hf:o:"

void printHelp(char* progname) {
	printf("%s -f <input-basename> -o <output-basename>\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char **args) {
    int argchar;
    catalog* catin;
	catalog* catout;
	idfile* idin;
	idfile* idout;
	kdtree_t* treein;
	kdtree_t* treeout;
	char* progname = args[0];
	char* basein = NULL;
	char* baseout = NULL;
	char* fn;
	int i;
	qfits_header* starhdr;
	qfits_header* hdr;
	int healpix;
	int starhp;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
        case 'f':
			basein = optarg;
			break;
        case 'o':
			baseout = optarg;
			break;
        case '?':
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        case 'h':
			printHelp(progname);
            return 0;
        default:
            return -1;
        }

	if (!basein || !baseout) {
		printHelp(progname);
		exit(-1);
	}

	fn = mk_streefn(basein);
	printf("Reading star tree from %s ...\n", fn);
	treein = kdtree_fits_read_file(fn);
	if (!treein) {
		fprintf(stderr, "Failed to read star kdtree from %s.\n", fn);
		exit(-1);
	}
	starhdr = qfits_header_read(fn);
	if (!starhdr) {
		fprintf(stderr, "Failed to read star kdtree header from %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	fn = mk_catfn(basein);
	printf("Reading catalog from %s ...\n", fn);
	catin = catalog_open(fn, 0);
	if (!catin) {
		fprintf(stderr, "Failed to read catalog from %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	fn = mk_idfn(basein);
	printf("Reading id file from %s ...\n", fn);
	idin = idfile_open(fn, 0);
	if (!idin) {
		fprintf(stderr, "Failed to read id file from %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	healpix = idin->healpix;
	if (catin->healpix != healpix) {
		fprintf(stderr, "Catalog header says it's healpix %i, but idfile say %i.\n",
				catin->healpix, idin->healpix);
	}
	starhp = qfits_header_getint(starhdr, "HEALPIX", -1);
	if (starhp == -1)
		fprintf(stderr, "Warning, input star kdtree didn't have a HEALPIX header.\n");
	else if (starhp != healpix) {
		fprintf(stderr, "Idfile says it's healpix %i, but star kdtree says %i.\n",
				healpix, starhp);
		exit(-1);
	}

	fn = mk_catfn(baseout);
	printf("Writing catalog to %s ...\n", fn);
	catout = catalog_open_for_writing(fn);
	if (!catout) {
		fprintf(stderr, "Failed to write catalog to %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	fn = mk_idfn(baseout);
	printf("Writing id to %s ...\n", fn);
	idout = idfile_open_for_writing(fn);
	if (!idout) {
		fprintf(stderr, "Failed to write id file to %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	catout->healpix = healpix;
	idout ->healpix = healpix;

	qfits_header_add(catout->header, "HISTORY", "unpermute-stars command line:", NULL, NULL);
	fits_add_args(catout->header, args, argc);
	qfits_header_add(catout->header, "HISTORY", "(end of unpermute-stars command line)", NULL, NULL);
	fits_copy_all_headers(catin->header, catout->header, "HISTORY");
	fits_copy_all_headers(catin->header, catout->header, "COMMENT");

	qfits_header_add(idout->header, "HISTORY", "unpermute-stars command line:", NULL, NULL);
	fits_add_args(idout->header, args, argc);
	qfits_header_add(idout->header, "HISTORY", "(end of unpermute-stars command line)", NULL, NULL);
	fits_copy_all_headers(idin->header, idout->header, "HISTORY");
	fits_copy_all_headers(idin->header, idout->header, "COMMENT");

	if (catalog_write_header(catout) ||
		idfile_write_header(idout)) {
		fprintf(stderr, "Failed to write catalog or idfile header.\n");
		exit(-1);
	}

	for (i=0; i<treein->ndata; i++) {
		double* star;
		uint64_t id;
		int ind = treein->perm[i];
		star = catalog_get_star(catin, ind);
		id = idfile_get_anid(idin, ind);
		if (catalog_write_star(catout, star) ||
			idfile_write_anid(idout, id)) {
			fprintf(stderr, "Failed to write catalog or idfile entry.\n");
			exit(-1);
		}
	}

	if (catalog_fix_header(catout) ||
		catalog_close(catout) ||
		idfile_fix_header(idout) ||
		idfile_close(idout)) {
		fprintf(stderr, "Failed to close output catalog or idfile.\n");
		exit(-1);
	}

	treeout = calloc(1, sizeof(kdtree_t));
	treeout->tree   = treein->tree;
	treeout->data   = treein->data;
	treeout->ndata  = treein->ndata;
	treeout->ndim   = treein->ndim;
	treeout->nnodes = treein->nnodes;

	hdr = qfits_header_default();
	qfits_header_add(hdr, "AN_FILE", "SKDT", "This is a star kdtree.", NULL);
	fits_copy_header(catin->header, hdr, "HEALPIX");
	qfits_header_add(hdr, "HISTORY", "unpermute-stars command line:", NULL, NULL);
	fits_add_args(hdr, args, argc);
	qfits_header_add(hdr, "HISTORY", "(end of unpermute-stars command line)", NULL, NULL);
	fits_copy_all_headers(starhdr, hdr, "HISTORY");
	fits_copy_all_headers(starhdr, hdr, "COMMENT");

	catalog_close(catin);
	idfile_close(idin);

	fn = mk_streefn(baseout);
	printf("Writing star kdtree to %s ...\n", fn);
	if (kdtree_fits_write_file(treeout, fn, NULL)) {
		fprintf(stderr, "Failed to write star kdtree.\n");
		exit(-1);
	}
	free_fn(fn);
	free(treeout);

	kdtree_close(treein);
	qfits_header_destroy(starhdr);

	return 0;
}
