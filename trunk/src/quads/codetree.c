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
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"
#include "fileutil.h"

#define OPTIONS "hR:f:F"
const char HelpString[] =
"codetree -f fname [-R KD_RMIN]\n"
"   [-F]   read traditional (non-FITS) input\n"
"  KD_RMIN (default 50) is the max# points per leaf in KD tree\n";

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argidx, argchar;
	int Nleaf = 25;
    FILE* treefid = NULL;
    kdtree_t *codekd = NULL;
    int levels;
    int fitsin = 1;
    char* basename = NULL;
    char* treefname;
    char* codefname;
    codefile* codes;

    if (argc <= 2) {
        fprintf(stderr, HelpString);
        return (OPT_ERR);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'F':
            fitsin = 0;
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

    treefname = mk_ctreefn(basename);
    if (fitsin) {
        codefname = mk_fits_codefn(basename);
    } else {
        codefname = mk_codefn(basename);
    }

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
    fopenout(treefname, treefid);
    free_fn(treefname);

    if (kdtree_write(treefid, codekd)) {
        fprintf(stderr, "Couldn't write star kdtree.\n");
        fclose(treefid);
        codefile_close(codes);
        kdtree_free(codekd);
        exit(-1);
    }

    fprintf(stderr, "done.\n");
    codefile_close(codes);
    fclose(treefid);
    kdtree_free(codekd);

    return (0);
}







