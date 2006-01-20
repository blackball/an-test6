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

int main(int argc, char *argv[]) {
    int argchar;
    char *codefname = NULL;
    FILE *codefid = NULL;
    int nread;
    code_header header;
    double* onecode;
    int i, d;

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

    /*
      ;
      This doesn't work because of struct alignment.

      nread = fread(&header, 1, sizeof(code_header), codefid);
      if (nread != sizeof(code_header)) {
      printf("Read error: %s: %s\n", codefname, strerror(errno));
      exit(-1);
      }
    */

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

    printf("codes = zeros([%i,%i]);\n", header.numcodes, header.dim);
    for (i=0; i<header.numcodes; i++) {
	nread = fread(onecode, sizeof(double), header.dim, codefid);
	if (nread != header.dim) {
	    fprintf(stderr, "Read error on code %i: %s\n", i, strerror(errno));
	    break;
	}
	printf("codes(%i,:)=[", i+1);
	for (d=0; d<header.dim; d++) {
	    printf("%g,", onecode[d]);
	}
	printf("];\n");
    }

    fclose(codefid);

    fprintf(stderr, "Done!\n");

    return 0;
}
