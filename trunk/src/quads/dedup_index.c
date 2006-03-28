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
    FILE *codein = NULL, *codeout = NULL;
    int numstars;
	int numcodes;
	uint numquads;
    int Dim_Codes;
	double index_scale;
    char *infname = NULL, *outfname = NULL;
    int i;
	char *codeinfname, *codeoutfname, *quadinfname, *quadoutfname;
    double* codearray = NULL;
	int* perm;
	int nequal =0;
	int flip;
    quadfile* quadsin;
    quadfile* quadsout = NULL;
	int nextskip;
	int skipind;
	int fitsin = 1;
	int fitsout = 1;
    int rtn;

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

 	codeinfname = mk_codefn(infname);
 	codeoutfname = mk_codefn(outfname);

    fprintf(stderr, "Reading codes...");
    fflush(stderr);
    fopenin(codeinfname, codein);
    free_fn(codeinfname);
    codearray = codefile_read(codein, &numcodes, &Dim_Codes, &index_scale);
    if (!codearray) {
        fprintf(stderr, "Couldn't read codefile.\n");
        exit(-1);
    }
	fclose(codein);

    fprintf(stderr, "Reading quads...");
	if (fitsin) {
		quadinfname = mk_fits_quadfn(infname);
		quadsin = quadfile_fits_open(quadinfname);
	} else {
		quadinfname = mk_quadfn(infname);
		quadsin = quadfile_open(quadinfname);
	}
    if (!quadsin) {
        fprintf(stderr, "Couldn't read quads input file %s\n", quadinfname);
        exit(-1);
    }
    numstars = quadsin->numstars;
    numquads = quadsin->numquads;
	if (numquads != numcodes) {
		fprintf(stderr, "Quad file contains %u quads, but code file contains %u codes!\n",
				numquads, numcodes);
		exit(-1);
	}
	free_fn(quadinfname);

	fprintf(stderr, "%i stars are used in %i quads.\n", numstars, numcodes);

	if (fitsout) {
		quadoutfname = mk_fits_quadfn(outfname);
		quadsout = quadfile_fits_open_for_writing(quadoutfname);
	} else {
		quadoutfname = mk_quadfn(outfname);
        quadsout = quadfile_open_for_writing(quadoutfname);
	}
    if (!quadsout) {
        fprintf(stderr, "Couldn't open output quads file %s: %s\n", quadoutfname, strerror(errno));
        exit(-1);
    }

	fopenout(codeoutfname, codeout);

	qsort_array = codearray;
	qsort_array_stride = Dim_Codes;

	perm = malloc(Dim_Codes * numcodes * sizeof(int));
	duplicates = il_new(1024);

	// across-the-diagonal permutations...
	for (flip=0; flip<2; flip++) {
		int cdperm;
		fprintf(stderr, "AB permutation %i...\n", flip);
		for (i=0; i<numcodes; i++)
			perm[i] = i;

		qsort_permutation = flip;
		qsort(perm, numcodes, sizeof(int), sort_permuted_array);

		// CD / DC permutation.
		for (i=0; i<(numcodes-1); i++) {
			for (cdperm=0; cdperm<2; cdperm++) {
				if (!memcmp(codearray + perm[i  ]*Dim_Codes + (2*cdperm),
							codearray + perm[i+1]*Dim_Codes,
							2 * sizeof(double)) &&
					!memcmp(codearray + perm[i  ]*Dim_Codes + (2 + 2*cdperm) % Dim_Codes,
							codearray + perm[i+1]*Dim_Codes + 2,
							2 * sizeof(double))) {
					nequal++;

					il_insert_unique_ascending(duplicates, perm[i+1]);
					break;
				}
			}
		}
	}
	fprintf(stderr, "%i codes are equal, out of %i codes.\n", nequal, numcodes);
	free(perm);

    write_code_header(codeout, numquads-il_size(duplicates),
					  numstars, DIM_CODES, index_scale);
	
	nextskip = -1;
	skipind = 0;
	for (i=0; i<numquads; i++) {
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

		cx = codearray[i * Dim_Codes + 0];
		cy = codearray[i * Dim_Codes + 1];
		dx = codearray[i * Dim_Codes + 2];
		dy = codearray[i * Dim_Codes + 3];
		write_one_code(codeout, cx, cy, dx, dy);

        quadfile_get_starids(quadsin, i, &iA, &iB, &iC, &iD);
		if (fitsout) {
			quadfile_fits_write_quad(quadsout, iA, iB, iC, iD);
		} else {
			quadfile_write_quad(quadsout, iA, iB, iC, iD);
		}
	}

    // fix .quad file header...
    quadsout->numquads = numquads - il_size(duplicates);
    quadsout->numstars = numstars;
    quadsout->index_scale = index_scale;
    quadsout->index_scale_lower = quadsin->index_scale_lower;

    rtn = 0;
	if (fitsout) {
		rtn = quadfile_fits_fix_header(quadsout);
	} else {
        rtn = quadfile_fix_header(quadsout);
	}
    if (rtn || quadfile_close(quadsout)) {
        printf("Couldn't write quad output file: %s\n", strerror(errno));
        exit(-1);
    }

	if (fclose(codeout)) {
		printf("Couldn't write code output file: %s\n", strerror(errno));
		exit(-1);
	}
    quadfile_close(quadsin);
	free(codearray);
	il_free(duplicates);
    fprintf(stderr, "Done!\n");
    return 0;
}


