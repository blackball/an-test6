/**
   \file Applies a star kdtree permutation array to the corresponding
   .id file, and also rewrites the .quad file to produce new .id, .quad
   and .skdt files that are consistent and don't require permutation.

   In:  .quad, .id, .skdt
   Out: .quad, .id, .skdt

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
#include "quadfile.h"
#include "idfile.h"
#include "fitsioutils.h"
#include "qfits.h"

#define OPTIONS "hf:o:q:"

void printHelp(char* progname) {
	printf("%s -f <input-basename> [-q <input-quadfile-basename>] -o <output-basename>\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char **args) {
    int argchar;
    quadfile* qfin;
	quadfile* qfout;
	idfile* idin;
	idfile* idout;
	kdtree_t* treein;
	kdtree_t* treeout;
	char* progname = args[0];
	char* basein = NULL;
	char* baseout = NULL;
	char* basequadin = NULL;
	char* fn;
	int i;
	qfits_header* starhdr;
	qfits_header* hdr;
	int healpix;
	int starhp;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'q':
			basequadin = optarg;
			break;
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

	if (basequadin)
		fn = mk_quadfn(basequadin);
	else
		fn = mk_quadfn(basein);
	printf("Reading quadfile from %s ...\n", fn);
	qfin = quadfile_open(fn, 0);
	if (!qfin) {
		fprintf(stderr, "Failed to read quadfile from %s.\n", fn);
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
	if (qfin->healpix != healpix) {
		fprintf(stderr, "Quadfile header says it's healpix %i, but idfile say %i.\n",
				qfin->healpix, idin->healpix);
	}
	starhp = qfits_header_getint(starhdr, "HEALPIX", -1);
	if (starhp == -1)
		fprintf(stderr, "Warning, input star kdtree didn't have a HEALPIX header.\n");
	else if (starhp != healpix) {
		fprintf(stderr, "Idfile says it's healpix %i, but star kdtree says %i.\n",
				healpix, starhp);
		exit(-1);
	}

	fn = mk_quadfn(baseout);
	printf("Writing quadfile to %s ...\n", fn);
	qfout = quadfile_open_for_writing(fn);
	if (!qfout) {
		fprintf(stderr, "Failed to write quadfile to %s.\n", fn);
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

	qfout->healpix = healpix;
	idout->healpix = healpix;

	qfits_header_add(qfout->header, "HISTORY", "unpermute-stars2 command line:", NULL, NULL);
	fits_add_args(qfout->header, args, argc);
	qfits_header_add(qfout->header, "HISTORY", "(end of unpermute-stars2 command line)", NULL, NULL);
	fits_copy_all_headers(qfin->header, qfout->header, "HISTORY");
	fits_copy_all_headers(qfin->header, qfout->header, "COMMENT");

	qfits_header_add(idout->header, "HISTORY", "unpermute-stars2 command line:", NULL, NULL);
	fits_add_args(idout->header, args, argc);
	qfits_header_add(idout->header, "HISTORY", "(end of unpermute-stars2 command line)", NULL, NULL);
	fits_copy_all_headers(idin->header, idout->header, "HISTORY");
	fits_copy_all_headers(idin->header, idout->header, "COMMENT");

	if (quadfile_write_header(qfout) ||
		idfile_write_header(idout)) {
		fprintf(stderr, "Failed to write quadfile or idfile header.\n");
		exit(-1);
	}

	for (i=0; i<treein->ndata; i++) {
		uint64_t id;
		int ind = treein->perm[i];
		id = idfile_get_anid(idin, ind);
		if (idfile_write_anid(idout, id)) {
			fprintf(stderr, "Failed to write idfile entry.\n");
			exit(-1);
		}
	}
	for (i=0; i<qfin->numquads; i++) {
		int j;
		uint stars[4];
		quadfile_get_starids(qfin, i, stars, stars+1, stars+2, stars+3);
		for (j=0; j<4; j++)
			stars[j] = treein->perm[stars[j]];
		if (quadfile_write_quad(qfout, stars[0], stars[1], stars[2], stars[3])) {
			fprintf(stderr, "Failed to write quadfile entry.\n");
			exit(-1);
		}
	}

	if (quadfile_fix_header(qfout) ||
		quadfile_close(qfout) ||
		idfile_fix_header(idout) ||
		idfile_close(idout)) {
		fprintf(stderr, "Failed to close output quadfile or idfile.\n");
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
	fits_copy_header(qfin->header, hdr, "HEALPIX");
	qfits_header_add(hdr, "HISTORY", "unpermute-stars2 command line:", NULL, NULL);
	fits_add_args(hdr, args, argc);
	qfits_header_add(hdr, "HISTORY", "(end of unpermute-stars2 command line)", NULL, NULL);
	fits_copy_all_headers(starhdr, hdr, "HISTORY");
	fits_copy_all_headers(starhdr, hdr, "COMMENT");

	quadfile_close(qfin);
	idfile_close(idin);

	fn = mk_streefn(baseout);
	printf("Writing star kdtree to %s ...\n", fn);
	if (kdtree_fits_write_file(treeout, fn, hdr)) {
		fprintf(stderr, "Failed to write star kdtree.\n");
		exit(-1);
	}
	free_fn(fn);
	free(treeout);

	kdtree_close(treein);
	qfits_header_destroy(starhdr);

	return 0;
}
