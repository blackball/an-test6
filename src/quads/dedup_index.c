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
#include "kdutil.h"
#include "fileutil.h"
#include "blocklist.h"
#include "codefile.h"

#define OPTIONS "hf:o:"
const char HelpString[] =
"dedup_index -f <input-prefix> -o <output-prefix>\n";

extern char *optarg;
extern int optind, opterr, optopt;

blocklist* duplicates = NULL;

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
    FILE *quadin = NULL, *quadout = NULL;
    int numstars;
	int numcodes;
	sidx nstars_tmp;
	sidx numquads;
    int Dim_Codes;
	dimension DimQuads;
	double index_scale;
    char *infname = NULL, *outfname = NULL;
    int i;
	char *codeinfname, *codeoutfname, *quadinfname, *quadoutfname;
    double* codearray = NULL;
	int* perm;
	int nequal =0;
	int flip;
	uint* quadarray;
	char readStatus;
	int nextskip;
	int skipind;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
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
 	quadinfname = mk_quadfn(infname);
 	quadoutfname = mk_quadfn(outfname);

    fprintf(stderr, "Reading codes...");
    fflush(stderr);
    fopenin(codeinfname, codein);
    free_fn(codeinfname);
    codearray = codefile_read(codein, &numcodes, &Dim_Codes, &index_scale);
	fprintf(stderr, "\n");
    if (!codearray)
		return (1);
	fclose(codein);

    fprintf(stderr, "Reading quads...");
	fopenin(quadinfname, quadin);
	readStatus = read_quad_header(quadin, &numquads, &nstars_tmp,
								  &DimQuads, &index_scale);
	numstars = nstars_tmp;
	if (numquads != numcodes) {
		fprintf(stderr, "Quad file contains %lu quads, but code file contains %i codes!\n",
				numquads, numcodes);
		exit(-1);
	}
				
	quadarray = malloc(numquads * DimQuads * sizeof(int));
	if (fread(quadarray, sizeof(int), numquads*DimQuads, quadin) != numquads*DimQuads) {
		fprintf(stderr, "Failed to read quads from %s.\n", quadinfname);
		exit(-1);
	}
	free_fn(quadinfname);
	fclose(quadin);

	fprintf(stderr, "%i stars are used in %i quads.\n", numstars, numcodes);

	fopenout(quadoutfname, quadout);
	fopenout(codeoutfname, codeout);

	qsort_array = codearray;
	qsort_array_stride = Dim_Codes;

	perm = malloc(Dim_Codes * numcodes * sizeof(int));

	duplicates = blocklist_int_new(1024);

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

					//fprintf(stderr, "Codes %8i and %8i are equal.  (sorted %8i, %8i)\n", perm[i], perm[i+1], i, i+1);
					nequal++;

					blocklist_int_insert_unique_ascending(duplicates, perm[i+1]);

					break;

					/*
					  for (d=0; d<Dim_Codes; d++) {
					  if (codearray[perm[i  ] * Dim_Codes + ((d + 2*cdperm) % Dim_Codes)] !=
					  codearray[perm[i+1] * Dim_Codes + d])
					  break;
					  }
					  if (d == Dim_Codes) {
					  // match!
					  */
				}
			}
		}
	}
	fprintf(stderr, "%i codes are equal, out of %i codes.\n", nequal, numcodes);
	free(perm);





	// we have to write an updated header after we've processed all the quads.
	write_code_header(codeout, numquads - blocklist_count(duplicates),
					  numstars, DIM_CODES, index_scale);
	write_quad_header(quadout, numquads - blocklist_count(duplicates),
					  numstars, DIM_QUADS, index_scale);

	nextskip = -1;
	skipind = 0;
	for (i=0; i<numquads; i++) {
		double cx, cy, dx, dy;
		sidx iA, iB, iC, iD;
		if (nextskip == -1) {
			if (blocklist_count(duplicates) == skipind) {
				nextskip = -2;
			} else {
				nextskip = blocklist_int_access(duplicates, skipind);
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
		writeonecode(codeout, cx, cy, dx, dy);

		iA = quadarray[i * DimQuads + 0];
		iB = quadarray[i * DimQuads + 1];
		iC = quadarray[i * DimQuads + 2];
		iD = quadarray[i * DimQuads + 3];

		writeonequad(quadout, iA, iB, iC, iD);
	}

	// close .code and .quad files
	if (fclose(codeout)) {
		printf("Couldn't write code output file: %s\n", strerror(errno));
		exit(-1);
	}
	if (fclose(quadout)) {
		printf("Couldn't write quad output file: %s\n", strerror(errno));
		exit(-1);
	}
	
	free(quadarray);
	free(codearray);

    fprintf(stderr, "Done!\n");

    return 0;
}


