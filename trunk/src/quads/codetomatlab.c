/**
   Reads a .code file and writes it out as Matlab literals.

   input: .code
   output: matlab literals
*/

#include <errno.h>
#include <limits.h>

#include "fileutil.h"
#include "starutil.h"
#include "codefile.h"

#define OPTIONS "hf:"

extern char *optarg;
extern int optind, opterr, optopt;

void print_help(char* progname) {
    fprintf(stderr, "Usage: %s -f <code-file>\n\n",
	    progname);
}

int main(int argc, char *argv[]) {
    int argchar;
    char *codefname = NULL;
    FILE *codefid = NULL;
    double* codes;
    int i, d;
	int numcodes, dimcodes;
	double index_scale;

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
	codes = codefile_read(codefid, &numcodes, &dimcodes, &index_scale);
    fclose(codefid);

    fprintf(stderr, "Number of codes: %i\n", numcodes);
    //fprintf(stderr, "Number of stars in catalogue: %lu\n", numstars);
    fprintf(stderr, "Index scale: %g\n", index_scale);

    printf("codes = zeros([%i,%i]);\n", numcodes, dimcodes);
	for (i=0; i<numcodes; i++) {
		printf("codes(%i,:)=[", i+1);
		for (d=0; d<dimcodes; d++) {
			printf("%g,", codes[i*dimcodes + d]);
		}
		printf("];\n");
    }
    fprintf(stderr, "Done!\n");
	free(codes);
    return 0;
}
