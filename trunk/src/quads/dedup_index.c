/**
   dedup_index: finds identical codes and removes one of them.
   (including permutations)

   input: .code , .quad
   output: .code , .quad

   original author: dstn
*/

#include <math.h>
#include <errno.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "bl.h"
#include "codefile.h"
#include "quadfile.h"

#define OPTIONS "hf:o:FG"
const char HelpString[] =
"dedup_index -f <input-prefix> -o <output-prefix>\n"
"    [-F] : input is in traditional (non-FITS) format\n"
"    [-G] : write output in traditional (non-FITS) format\n";

extern char *optarg;
extern int optind, opterr, optopt;

il* duplicates = NULL;

double* qsort_array;
int qsort_array_stride;
int qsort_permutation;

int sort_permuted_array(const void* v1, const void* v2) {
	int i1 = *(int*)v1;
	int i2 = *(int*)v2;
	double val1, val2;
	val1 = qsort_array[i1 * qsort_array_stride];
	val2 = qsort_array[i2 * qsort_array_stride];
	if (qsort_permutation)
		val1 = 1.0 - val1;
	if (val1 < val2)
		return -1;
	if (val1 > val2)
		return 1;
	return 0;
}

int main(int argc, char *argv[]) {
    int argidx, argchar;
    char *infname = NULL, *outfname = NULL;
    int i;
	char *codeinfname, *codeoutfname, *quadinfname, *quadoutfname;
	int* perm;
	int nequal =0;
	int flip;

    quadfile* quadsin;
    quadfile* quadsout;

    codefile* codesin;
    codefile* codesout;

	int nextskip;
	int skipind;
	int fitsin = 1;
	int fitsout = 1;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'F':
			fitsin = 0;
			break;
		case 'G':
			fitsout = 0;
			break;
		case 'f':
			infname = optarg;
			break;
		case 'o':
			outfname = optarg;
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
		return (HELP_ERR);
    }
    if (!infname || !outfname) {
		fprintf(stderr, HelpString);
		return (HELP_ERR);
    }

    fprintf(stderr, "Reading codes...");
    fflush(stderr);

    if (fitsin) {
        codeinfname = mk_fits_codefn(infname);
    } else {
        codeinfname = mk_codefn(infname);
    }
    codesin = codefile_open(codeinfname, fitsin);
    free_fn(codeinfname);
    if (!codesin) {
        fprintf(stderr, "Couldn't read codefile.\n");
        exit(-1);
    }

    fprintf(stderr, "Reading quads...");
	if (fitsin) {
		quadinfname = mk_fits_quadfn(infname);
	} else {
		quadinfname = mk_quadfn(infname);
	}
    quadsin = quadfile_open(quadinfname, fitsin);
    if (!quadsin) {
        fprintf(stderr, "Couldn't read quads input file %s\n", quadinfname);
        exit(-1);
    }
	if (quadsin->numquads != codesin->numcodes) {
		fprintf(stderr, "Quad file contains %u quads, but code file contains %u codes!\n",
				quadsin->numquads, codesin->numcodes);
		exit(-1);
	}
	free_fn(quadinfname);

	fprintf(stderr, "%i stars are used in %i quads.\n", codesin->numstars, codesin->numcodes);

	if (fitsout) {
		quadoutfname = mk_fits_quadfn(outfname);
        codeoutfname = mk_fits_codefn(outfname);
	} else {
		quadoutfname = mk_quadfn(outfname);
        codeoutfname = mk_codefn(outfname);
	}
    quadsout = quadfile_open_for_writing(quadoutfname, fitsout);
    if (!quadsout) {
        fprintf(stderr, "Couldn't open output quads file %s: %s\n", quadoutfname, strerror(errno));
        exit(-1);
    }
    codesout = codefile_open_for_writing(codeoutfname, fitsout);
    if (!codesout) {
        fprintf(stderr, "Couldn't open output codes file %s: %s\n", codeoutfname, strerror(errno));
        exit(-1);
    }

	qsort_array = codesin->codearray;
	qsort_array_stride = DIM_CODES;

	perm = malloc(DIM_CODES * codesin->numcodes * sizeof(int));
	duplicates = il_new(1024);

	// across-the-diagonal permutations...
	for (flip=0; flip<2; flip++) {
		int cdperm;
		fprintf(stderr, "AB permutation %i...\n", flip);
		for (i=0; i<codesin->numcodes; i++)
			perm[i] = i;

		qsort_permutation = flip;
		qsort(perm, codesin->numcodes, sizeof(int), sort_permuted_array);

		// CD / DC permutation.
		for (i=0; i<(codesin->numcodes-1); i++) {
			for (cdperm=0; cdperm<2; cdperm++) {
				if (!memcmp(codesin->codearray + perm[i  ]*DIM_CODES + (2*cdperm),
							codesin->codearray + perm[i+1]*DIM_CODES,
							2 * sizeof(double)) &&
					!memcmp(codesin->codearray + perm[i  ]*DIM_CODES + (2 + 2*cdperm) % DIM_CODES,
							codesin->codearray + perm[i+1]*DIM_CODES + 2,
							2 * sizeof(double))) {
					nequal++;

					il_insert_unique_ascending(duplicates, perm[i+1]);
					break;
				}
			}
		}
	}
	fprintf(stderr, "%i codes are equal, out of %i codes.\n", nequal, codesin->numcodes);
	free(perm);

	nextskip = -1;
	skipind = 0;
	for (i=0; i<quadsin->numquads; i++) {
		double cx, cy, dx, dy;
		uint iA, iB, iC, iD;
		if (nextskip == -1) {
			if (il_size(duplicates) == skipind) {
				nextskip = -2;
			} else {
				nextskip = il_get(duplicates, skipind);
				skipind++;
			}
		}

		if (nextskip == i) {
			nextskip = -1;
			continue;
		}

        codefile_get_code(codesin, i, &cx, &cy, &dx, &dy);
        codefile_write_code(codesout, cx, cy, dx, dy);

        quadfile_get_starids(quadsin, i, &iA, &iB, &iC, &iD);
        quadfile_write_quad(quadsout, iA, iB, iC, iD);
	}

    // fix .quad file header...
    quadsout->numquads = quadsin->numquads - il_size(duplicates);
    quadsout->numstars = quadsin->numstars;
    quadsout->index_scale = quadsin->index_scale;
    quadsout->index_scale_lower = quadsin->index_scale_lower;

    if (quadfile_fix_header(quadsout) ||
        quadfile_close(quadsout)) {
        printf("Couldn't write quad output file: %s\n", strerror(errno));
        exit(-1);
    }

    codesout->numcodes = codesin->numcodes - il_size(duplicates);
    codesout->numstars = codesin->numstars;
    codesout->index_scale = codesin->index_scale;
    codesout->index_scale_lower = codesin->index_scale_lower;
    if (codefile_fix_header(codesout) ||
        codefile_close(codesout)) {
        printf("Couldn't write code output file: %s\n", strerror(errno));
        exit(-1);
    }

    quadfile_close(quadsin);
    codefile_close(codesin);
	il_free(duplicates);
    fprintf(stderr, "Done!\n");
    return 0;
}


