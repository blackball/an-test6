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

#define OPTIONS "hR:f:FG"
const char HelpString[] =
"codetree -f fname [-R KD_RMIN]\n"
"   [-F]   read traditional (non-FITS) input\n"
"   [-G]   write traditional (non-FITS) output\n"
"  KD_RMIN (default 50) is the max# points per leaf in KD tree\n";

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argidx, argchar;
	int Nleaf = 25;
    kdtree_t *codekd = NULL;
    int levels;
    bool fitsin = TRUE;
	bool fitsout = TRUE;
    char* basename = NULL;
    char* treefname;
    char* codefname;
    codefile* codes;
	int rtn;

    if (argc <= 2) {
        fprintf(stderr, HelpString);
        return (OPT_ERR);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'F':
            fitsin = FALSE;
            break;
        case 'G':
            fitsout = FALSE;
            break;
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

	if (fitsout)
		treefname = mk_fits_ctreefn(basename);
	else
		treefname = mk_ctreefn(basename);

    if (fitsin)
        codefname = mk_fits_codefn(basename);
	else
        codefname = mk_codefn(basename);

    fprintf(stderr, "codetree: building KD tree for %s\n", codefname);
    fprintf(stderr, "       will write KD tree file %s\n", treefname);

    fprintf(stderr, "  Reading codes...");
    fflush(stderr);

    codes = codefile_open(codefname, fitsin);
    free_fn(codefname);
    if (!codes) {
        exit(-1);
    }
    fprintf(stderr, "got %u codes.\n", codes->numcodes);

    fprintf(stderr, "  Building code KD tree...\n");
    fflush(stderr);
    levels = (int)((log((double)codes->numcodes) - log((double)Nleaf))/log(2.0));
    if (levels < 1) {
        levels = 1;
    }
    fprintf(stderr, "Requesting %i levels.\n", levels);
    codekd = kdtree_build(codes->codearray, codes->numcodes, DIM_CODES, levels);
    if (!codekd)
		exit(-1);
    fprintf(stderr, "done (%d nodes)\n", codekd->nnodes);

	fprintf(stderr, "Optimizing...\n");
	fflush(stderr);
	kdtree_optimize(codekd);
	fprintf(stderr, "Done.\n");

    fprintf(stderr, "  Writing code KD tree to %s...", treefname);
    fflush(stderr);

	if (fitsout)
		rtn = kdtree_fits_write_file(codekd, treefname);
	else
		rtn = kdtree_write_file(codekd, treefname);
    free_fn(treefname);
	if (rtn) {
        fprintf(stderr, "Couldn't write star kdtree.\n");
        exit(-1);
    }

    fprintf(stderr, "done.\n");
    codefile_close(codes);
    kdtree_free(codekd);
	return 0;
}







