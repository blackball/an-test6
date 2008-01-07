/*
 This file is part of libkd.
 Copyright 2008 Dustin Lang.

 libkd is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, version 2.

 libkd is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with libkd; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "kdtree.h"
#include "kdtree_fits_io.h"

void printHelp(char* progname) {
	printf("\nUsage: %s <input> <output>\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

const char* OPTIONS = "h";

int main(int argc, char** args) {
    int argchar;
	char* progname = args[0];
	kdtree_t* kd;
	char* infn;
	char* outfn;

    while ((argchar = getopt(argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'h':
			printHelp(progname);
			exit(-1);
		}

    if (optind != argc - 2) {
        printHelp(progname);
        exit(-1);
    }

    infn = args[optind];
    outfn = args[optind+1];

    printf("Reading kdtree from file %s ...\n", infn);
    kd = kdtree_fits_read(infn, NULL);

    printf("Treetype: 0x%x\n", kd->treetype);
    printf("Data type:     %s\n", kdtree_kdtype_to_string(kdtree_datatype(kd)));
    printf("Tree type:     %s\n", kdtree_kdtype_to_string(kdtree_treetype(kd)));
    printf("External type: %s\n", kdtree_kdtype_to_string(kdtree_exttype(kd)));

    printf("N data points:  %i\n", kd->ndata);
    printf("Dimensions:     %i\n", kd->ndim);
    printf("Nodes:          %i\n", kd->nnodes);
    printf("Leaf nodes:     %i\n", kd->nbottom);
    printf("Non-leaf nodes: %i\n", kd->ninterior);
    printf("Tree levels:    %i\n", kd->nlevels);

    printf("Legacy nodes: %s\n", (kd->nodes  ? "yes" : "no"));
    printf("LR array:     %s\n", (kd->lr     ? "yes" : "no"));
    printf("Perm array:   %s\n", (kd->perm   ? "yes" : "no"));
    printf("Bounding box: %s\n", (kd->bb.any ? "yes" : "no"));
    printf("Split plane:  %s\n", (kd->split.any ? "yes" : "no"));
    printf("Split dim:    %s\n", (kd->splitdim  ? "yes" : "no"));
    printf("Data:         %s\n", (kd->data.any  ? "yes" : "no"));

    if (kd->minval && kd->maxval) {
        int d;
        printf("Data ranges:\n");
        for (d=0; d<kd->ndim; d++)
            printf("  %i: [%g, %g]\n", d, kd->minval[d], kd->maxval[d]);
    }

    printf("Running kdtree_check...\n");
    if (kdtree_check(kd)) {
        printf("kdtree_check failed.\n");
        exit(-1);
    }

    kdtree_fits_close(kd);

	return 0;
}
