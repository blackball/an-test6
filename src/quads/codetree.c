/**
   Reads a list of codes and writes a code kdtree.

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
#include "fileutil.h"
#include "fitsioutils.h"
#include "codekd.h"
#include "boilerplate.h"

#define OPTIONS "hR:f:o:"

static void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s\n"
		   "    -f <input-basename>\n"
		   "    -o <output-basename>\n"
		   "    [-R <target-leaf-node-size>]   (default 25)\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argidx, argchar;
	char* progname = argv[0];

	int Nleaf = 25;
    codetree *codekd = NULL;
    int levels;
    char* basenamein = NULL;
    char* basenameout = NULL;
    char* treefname;
    char* codefname;
    codefile* codes;
	int rtn;
	qfits_header* hdr;
	char val[32];

    if (argc <= 2) {
        printHelp(progname);
		exit(-1);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'R':
            Nleaf = (int)strtoul(optarg, NULL, 0);
            break;
        case 'f':
            basenamein = optarg;
            break;
        case 'o':
            basenameout = optarg;
            break;
        case '?':
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        case 'h':
			printHelp(progname);
			exit(-1);
        default:
            return (OPT_ERR);
        }

    if (optind < argc) {
        for (argidx = optind; argidx < argc; argidx++)
            fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		printHelp(progname);
		exit(-1);
    }
    if (!basenamein || !basenameout) {
		printHelp(progname);
		exit(-1);
    }

	codefname = mk_codefn(basenamein);
	treefname = mk_ctreefn(basenameout);

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
	codekd = codetree_new();
	if (!codekd) {
		fprintf(stderr, "Failed to allocate a codetree structure.\n");
		exit(-1);
	}
    codekd->tree = kdtree_build(codes->codearray, codes->numcodes, DIM_CODES, levels);
    if (!codekd) {
		fprintf(stderr, "Failed to build code kdtree.\n");
		exit(-1);
	}
    fprintf(stderr, "done (%d nodes)\n", codetree_N(codekd));

    fprintf(stderr, "  Writing code KD tree to %s...", treefname);
    fflush(stderr);

	hdr = codetree_header(codekd);
	qfits_header_add(hdr, "AN_FILE", "CKDT", "This file is a code kdtree.", NULL);
	sprintf(val, "%u", Nleaf);
	qfits_header_add(hdr, "NLEAF", val, "Target number of points in leaves.", NULL);
	sprintf(val, "%u", levels);
	qfits_header_add(hdr, "LEVELS", val, "Number of kdtree levels.", NULL);
	fits_copy_header(codes->header, hdr, "INDEXID");
	fits_copy_header(codes->header, hdr, "HEALPIX");
	fits_copy_header(codes->header, hdr, "CXDX");
	fits_copy_header(codes->header, hdr, "CIRCLE");

	boilerplate_add_fits_headers(hdr);
	qfits_header_add(hdr, "HISTORY", "This file was created by the program \"codetree\".", NULL, NULL);
	qfits_header_add(hdr, "HISTORY", "codetree command line:", NULL, NULL);
	fits_add_args(hdr, argv, argc);
	qfits_header_add(hdr, "HISTORY", "(end of codetree command line)", NULL, NULL);
	qfits_header_add(hdr, "HISTORY", "** codetree: history from input file:", NULL, NULL);
	fits_copy_all_headers(codes->header, hdr, "HISTORY");
	qfits_header_add(hdr, "HISTORY", "** codetree: end of history from input file.", NULL, NULL);

	rtn = codetree_write_to_file(codekd, treefname);
    free_fn(treefname);
	if (rtn) {
        fprintf(stderr, "Couldn't write code kdtree.\n");
        exit(-1);
    }

    fprintf(stderr, "done.\n");
    codefile_close(codes);
    codetree_close(codekd);
	return 0;
}







