/**
   Writes a Keir-style kdtree of code values.

   Input: .code
   Output: .ckdt
*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "codefile.h"
#include "kdtree.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"
#include "fileutil.h"
#include "fitsioutils.h"

#define OPTIONS "hR:f:"
const char HelpString[] =
"codetree -f fname [-R KD_RMIN]\n"
"  KD_RMIN (default 50) is the max# points per leaf in KD tree\n";

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argidx, argchar;
	int Nleaf = 25;
    kdtree_t *codekd = NULL;
    int levels;
    char* basename = NULL;
    char* treefname;
    char* codefname;
    codefile* codes;
	int rtn;
	qfits_header* hdr;
	char val[32];

    if (argc <= 2) {
        fprintf(stderr, HelpString);
        return (OPT_ERR);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'R':
            Nleaf = (int)strtoul(optarg, NULL, 0);
            break;
        case 'f':
            basename = optarg;
            break;
        case '?':
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
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
    if (!basename) {
        fprintf(stderr, HelpString);
        return (OPT_ERR);
    }

	treefname = mk_ctreefn(basename);
	codefname = mk_codefn(basename);

    fprintf(stderr, "codetree: building KD tree for %s\n", codefname);
    fprintf(stderr, "       will write KD tree file %s\n", treefname);

    fprintf(stderr, "  Reading codes...");
    fflush(stderr);

    codes = codefile_open(codefname, 1);
    free_fn(codefname);
    if (!codes) {
        exit(-1);
    }
    fprintf(stderr, "got %u codes.\n", codes->numcodes);

    fprintf(stderr, "  Building code KD tree...\n");
    fflush(stderr);
    levels = kdtree_compute_levels(codes->numcodes, Nleaf);
    fprintf(stderr, "Requesting %i levels.\n", levels);
    codekd = kdtree_build(codes->codearray, codes->numcodes, DIM_CODES, levels);
    if (!codekd)
		exit(-1);
    fprintf(stderr, "done (%d nodes)\n", codekd->nnodes);

    fprintf(stderr, "  Writing code KD tree to %s...", treefname);
    fflush(stderr);

	hdr = qfits_header_new();
	qfits_header_add(hdr, "AN_FILE", "CKDT", "This file is a code kdtree.", NULL);
	sprintf(val, "%u", Nleaf);
	qfits_header_add(hdr, "NLEAF", val, "Target number of points in leaves.", NULL);
	sprintf(val, "%u", levels);
	qfits_header_add(hdr, "LEVELS", val, "Number of kdtree levels.", NULL);

	fits_copy_header(codes->header, hdr, "INDEXID");
	fits_copy_header(codes->header, hdr, "HEALPIX");
	qfits_header_add(hdr, "COMMENT", "codetree command line:", NULL, NULL);
	fits_add_args(hdr, argv, argc);

	rtn = kdtree_fits_write_file(codekd, treefname, hdr);
	qfits_header_destroy(hdr);
    free_fn(treefname);
	if (rtn) {
        fprintf(stderr, "Couldn't write code kdtree.\n");
        exit(-1);
    }

    fprintf(stderr, "done.\n");
    codefile_close(codes);
    kdtree_free(codekd);
	return 0;
}







