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

#define OPTIONS "hR:f:"
const char HelpString[] =
"codetree -f fname [-R KD_RMIN]\n"
"  KD_RMIN (default 50) is the max# points per leaf in KD tree\n";

extern char *optarg;
extern int optind, opterr, optopt;

char *treefname = NULL;
char *codefname = NULL;

int main(int argc, char *argv[]) {
    int argidx, argchar;
	int Nleaf = 25;
    FILE *codefid = NULL, *treefid = NULL;
    int numcodes, Dim_Codes;
    double index_scale;
    double* codearray = NULL;
    kdtree_t *codekd = NULL;
    int levels;

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
	    treefname = mk_ctreefn(optarg);
	    codefname = mk_codefn(optarg);
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

    fprintf(stderr, "codetree: building KD tree for %s\n", codefname);
    fprintf(stderr, "       will write KD tree file %s\n", treefname);

    fprintf(stderr, "  Reading codes...");
    fflush(stderr);
    fopenin(codefname, codefid);
    free_fn(codefname);
    codearray = codefile_read(codefid, &numcodes, &Dim_Codes, &index_scale);
    if (!codearray)
	return (1);
    fprintf(stderr, "got %d codes (dim %hu).\n", numcodes, Dim_Codes);

    fprintf(stderr, "  Building code KD tree...\n");
    fflush(stderr);

    levels = (int)((log((double)numcodes) - log((double)Nleaf))/log(2.0));
    if (levels < 1) {
	levels = 1;
    }
    fprintf(stderr, "Requesting %i levels.\n", levels);

    codekd = kdtree_build(codearray, numcodes, Dim_Codes, levels);

    if (!codekd)
		exit(-1);

    fprintf(stderr, "done (%d nodes)\n", codekd->nnodes);
    fclose(codefid);

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
	free(codearray);
	kdtree_free(codekd);
	exit(-1);
    }

    fprintf(stderr, "done.\n");
    free(codearray);
    fclose(treefid);
    kdtree_free(codekd);

    return (0);
}







