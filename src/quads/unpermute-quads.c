/**
   \file Applies a code kdtree permutation array to the corresponding
   .quad file to produce new .quad and .ckdt files that are
   consistent and don't require permutation.

   In:  .quad, .ckdt
   Out: .quad, .ckdt

   Original author: dstn
*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "kdtree.h"
#include "starutil.h"
#include "fileutil.h"
#include "quadfile.h"
#include "fitsioutils.h"
#include "codekd.h"
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
    quadfile* quadin;
	quadfile* quadout;
	codetree* treein;
	codetree* treeout;
	char* progname = args[0];
	char* basein = NULL;
	char* baseout = NULL;
	char* fn;
	int i;
	qfits_header* codehdr;
	qfits_header* hdr;
	int healpix;
	int codehp;

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

	fn = mk_ctreefn(basein);
	printf("Reading code tree from %s ...\n", fn);
	treein = codetree_open(fn);
	if (!treein) {
		fprintf(stderr, "Failed to read code kdtree from %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);
	codehdr = codetree_header(treein);

	fn = mk_quadfn(basein);
	printf("Reading quads from %s ...\n", fn);
	quadin = quadfile_open(fn, 0);
	if (!quadin) {
		fprintf(stderr, "Failed to read quads from %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	healpix = quadin->healpix;
	codehp = qfits_header_getint(codehdr, "HEALPIX", -1);
	if (codehp == -1)
		fprintf(stderr, "Warning, input code kdtree didn't have a HEALPIX header.\n");
	else if (codehp != healpix) {
		fprintf(stderr, "Quadfile says it's healpix %i, but code kdtree says %i.\n",
				healpix, codehp);
		exit(-1);
	}

	fn = mk_quadfn(baseout);
	printf("Writing quads to %s ...\n", fn);
	quadout = quadfile_open_for_writing(fn);
	if (!quadout) {
		fprintf(stderr, "Failed to write quads to %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	quadout->healpix = healpix;
	quadout->indexid = quadin->indexid;
	quadout->numstars = quadin->numstars;
	quadout->index_scale = quadin->index_scale;
	quadout->index_scale_lower = quadin->index_scale_lower;

	qfits_header_add(quadout->header, "HISTORY", "unpermute-quads command line:", NULL, NULL);
	fits_add_args(quadout->header, args, argc);
	qfits_header_add(quadout->header, "HISTORY", "(end of unpermute-quads command line)", NULL, NULL);
	qfits_header_add(quadout->header, "HISTORY", "** unpermute-quads: history from input:", NULL, NULL);
	fits_copy_all_headers(quadin->header, quadout->header, "HISTORY");
	qfits_header_add(quadout->header, "HISTORY", "** unpermute-quads end of history from input.", NULL, NULL);
	qfits_header_add(quadout->header, "COMMENT", "** unpermute-quads: comments from input:", NULL, NULL);
	fits_copy_all_headers(quadin->header, quadout->header, "COMMENT");
	qfits_header_add(quadout->header, "COMMENT", "** unpermute-quads: end of comments from input.", NULL, NULL);
	fits_copy_header(quadin->header, quadout->header, "CXDX");
	fits_copy_header(quadin->header, quadout->header, "CIRCLE");

	if (quadfile_write_header(quadout)) {
		fprintf(stderr, "Failed to write quadfile header.\n");
		exit(-1);
	}

	for (i=0; i<codetree_N(treein); i++) {
		uint sA, sB, sC, sD;
		int ind = codetree_get_permuted(treein, i);
		quadfile_get_starids(quadin, ind, &sA, &sB, &sC, &sD);
		if (quadfile_write_quad(quadout, sA, sB, sC, sD)) {
			fprintf(stderr, "Failed to write quad entry.\n");
			exit(-1);
		}
	}

	if (quadfile_fix_header(quadout) ||
		quadfile_close(quadout)) {
		fprintf(stderr, "Failed to close output quadfile.\n");
		exit(-1);
	}

	treeout = codetree_new();
	treeout->tree = calloc(1, sizeof(kdtree_t));
	treeout->tree->tree   = treein->tree->tree;
	treeout->tree->data   = treein->tree->data;
	treeout->tree->ndata  = treein->tree->ndata;
	treeout->tree->ndim   = treein->tree->ndim;
	treeout->tree->nnodes = treein->tree->nnodes;

	hdr = codetree_header(treeout);
	qfits_header_add(hdr, "AN_FILE", "CKDT", "This is a code kdtree.", NULL);
	fits_copy_header(quadin->header, hdr, "HEALPIX");
	qfits_header_add(hdr, "HISTORY", "unpermute-quads command line:", NULL, NULL);
	fits_add_args(hdr, args, argc);
	qfits_header_add(hdr, "HISTORY", "(end of unpermute-quads command line)", NULL, NULL);
	qfits_header_add(hdr, "HISTORY", "** unpermute-quads: history from input ckdt:", NULL, NULL);
	fits_copy_all_headers(codehdr, hdr, "HISTORY");
	qfits_header_add(hdr, "HISTORY", "** unpermute-quads end of history from input ckdt.", NULL, NULL);
	qfits_header_add(hdr, "COMMENT", "** unpermute-quads: comments from input ckdt:", NULL, NULL);
	fits_copy_all_headers(codehdr, hdr, "COMMENT");
	qfits_header_add(hdr, "COMMENT", "** unpermute-quads: end of comments from input ckdt.", NULL, NULL);
	fits_copy_header(codehdr, hdr, "CXDX");
	fits_copy_header(codehdr, hdr, "CIRCLE");

	quadfile_close(quadin);

	fn = mk_ctreefn(baseout);
	printf("Writing code kdtree to %s ...\n", fn);
	if (codetree_write_to_file(treeout, fn) ||
		codetree_close(treeout)) {
		fprintf(stderr, "Failed to write code kdtree.\n");
		exit(-1);
	}
	free_fn(fn);

	codetree_close(treein);

	return 0;
}
