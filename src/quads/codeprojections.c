/**
   Reads a .code file and writes it out as Matlab literals.

   The format is:

   unsigned short int = MAGIC_VAL
   unsigned long int numCodes = number of codes (quads) in this codefile
   unsigned short int DimCodes = dimension of codes (almost always 4)
   double index_scale = scale of quads (arcmin)
   unsigned long int numstars = number of stars in original catalogue (unclear)
   double c11 code elements for first quad
   double c12
   double c13
   double c14
   double c21 code elements for second quad
   double c22
   double c23
   double c24
   ...
   double cN1 code elements for last (Nth) quad
   double cN2
   double cN3
   double cN4
*/

#include <errno.h>
#include <limits.h>

#include "fileutil.h"
#include "starutil.h"

#define OPTIONS "hf:"

extern char *optarg;
extern int optind, opterr, optopt;

void print_help(char* progname) {
    fprintf(stderr, "Usage: %s -f <code-file>\n\n",
	    progname);
}

struct code_header {
    unsigned short magic;
    unsigned int numcodes;
    unsigned short dim;
    double index_scale;
    unsigned int numstars;
};
typedef struct code_header code_header;

int read_field(void* ptr, int size, FILE* fid) {
    int nread = fread(ptr, 1, size, fid);
    if (nread != size) {
	fprintf(stderr, "Read %i, not %i\n", nread, size);
	return 1;
    }
    return 0;
}

int** hists;
int Nbins = 20;
int Dims;

void add_to_histogram(int dim1, int dim2, double val1, double val2) {
    int xbin, ybin;
    int* hist = hists[dim1 * Dims + dim2];
    xbin = (int)(val1 * Nbins);
    if (xbin >= Nbins) {
	xbin = Nbins;
	printf("truncating value %g\n", val1);
    }
    if (xbin < 0) {
	xbin = 0;
	printf("truncating (up) value %g\n", val1);
    }
    ybin = (int)(val2 * Nbins);
    if (ybin >= Nbins) {
	ybin = Nbins;
	printf("truncating value %g\n", val2);
    }
    if (ybin < 0) {
	ybin = 0;
	printf("truncating (up) value %g\n", val2);
    }

    hist[xbin * Nbins + ybin]++;
}


int main(int argc, char *argv[]) {
    int argchar;
    char *codefname = NULL;
    FILE *codefid = NULL;
    int nread;
    code_header header;
    double* onecode;
    int i, d, e;

    if (argc <= 2) {
	print_help(argv[0]);
	return(OPT_ERR);
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
	switch (argchar) {
	case 'f':
	    codefname = optarg;
	    break;
	case 'h':
	    print_help(argv[0]);
	    return(HELP_ERR);
	default:
	    return (OPT_ERR);
	}

    if (!codefname) {
	print_help(argv[0]);
	return(OPT_ERR);
    }

    fopenin(codefname, codefid);

    if (read_field(&header.magic, sizeof(header.magic), codefid) ||
	read_field(&header.numcodes, sizeof(header.numcodes), codefid) ||
	read_field(&header.dim, sizeof(header.dim), codefid) ||
	read_field(&header.index_scale, sizeof(header.index_scale), codefid) ||
	read_field(&header.numstars, sizeof(header.numstars), codefid)
	) {
	fprintf(stderr, "Error reading header: %s\n", strerror(errno));
	exit(-1);
    }

    if (header.magic != MAGIC_VAL) {
	fprintf(stderr, "Bad magic.  Expect the worst.\n");
    }
    fprintf(stderr, "Number of codes: %i\n", header.numcodes);
    fprintf(stderr, "Dimension of codes: %i\n", header.dim);
    fprintf(stderr, "Index scale: %g\n", header.index_scale);
    fprintf(stderr, "Number of stars in catalogue: %i\n", header.numstars);

    onecode = (double*)malloc(header.dim * sizeof(double));

    Dims = header.dim;
    hists = (int**)malloc(Dims * Dims * sizeof(int*));
    memset(hists, 0, Dims*Dims*sizeof(int*));
    for (d=0; d<Dims; d++) {
	for (e=0; e<d; e++) {
	    hists[d*Dims + e] = (int*)malloc(Nbins * Nbins * sizeof(int));
	    memset(hists[d*Dims + e], 0, Nbins * Nbins * sizeof(int));
	}
    }

    for (i=0; i<header.numcodes; i++) {
	nread = fread(onecode, sizeof(double), header.dim, codefid);
	if (nread != header.dim) {
	    fprintf(stderr, "Read error on code %i: %s\n", i, strerror(errno));
	    break;
	}
	for (d=0; d<Dims; d++) {
	    for (e=0; e<d; e++) {
		add_to_histogram(d, e, onecode[d], onecode[e]);
	    }
	}
    }

    fclose(codefid);

    for (d=0; d<Dims; d++) {
	for (e=0; e<d; e++) {
	    int* hist;
	    printf("hist_%i_%i=zeros([%i,%i]);\n",
		   d, e, Nbins, Nbins);
	    hist = hists[d*Dims + e];
	    for (i=0; i<Nbins; i++) {
		int j;
		printf("hist_%i_%i(%i,:)=[", d, e, i+1);
		for (j=0; j<Nbins; j++) {
		    printf("%i,", hist[i*Nbins + j]);
		}
		printf("];\n");
	    }
	}
    }

    fprintf(stderr, "Done!\n");

    return 0;
}
