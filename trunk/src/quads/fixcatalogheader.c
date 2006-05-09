/**
   \file A program to fix the headers of catalogue files.

   It seems the NYU people just set the RA,DEC limits to be some constant value
   that isn't a tight bound on the actual stars in the catalogue - eg,
   [0, 360], [-90, 90].  This program reads a catalogue, finds the range of actual
   RA,DEC values, and writes the contents to a new file.  The body of the file is
   unchanged.

   If you use the same filename for input and output, it will fix the header in-place,
   which is much faster than copying the file, but of course has the potential to mess
   up your file.
*/

#include <byteswap.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "catalog.h"

#define OPTIONS "hf:o:"

extern char *optarg;
extern int optind, opterr, optopt;


void print_help(char* progname) {
    printf("\nUsage: %s -f <input-file> -o <output-file>\n\n"
		   "  <input-file> and <output-file> should be "
		   "catalogues, and should be specified without "
		   "their .objs suffixes.\n\n"
		   "  <input-file> and be the same as <output-file>;"
		   "in this case, the file will be "
		   "modified in-place, which is much faster than "
		   "creating a copy.\n\n", progname);
}

int main(int argc, char *argv[]) {
	int argchar;
	char *outfname = NULL, *catfname = NULL;
	bool inplace;
	catalog* cat;
	int res;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'f':
			catfname = mk_catfn(optarg);
			break;
		case 'o':
			outfname = mk_catfn(optarg);
			break;
		case 'h':
			print_help(argv[0]);
			exit(0);
		default:
			return (OPT_ERR);
		}
    
	if ((catfname == NULL) || (outfname == NULL)) {
		print_help(argv[0]);
		exit(-1);
	}

	fprintf(stderr, "fixheaders: reading catalogue  %s\n", catfname);
	fprintf(stderr, "            writing results to %s\n", outfname);
	inplace = (strcmp(catfname, outfname) == 0);

	cat = catalog_open(catfname, 0, 0);
	if (!cat) {
		fprintf(stderr, "Couldn't open catalog.\n");
		exit(-1);
	}

	fprintf(stderr, "Got %u stars.\n"
			"Limits RA=[%g, %g] DEC=[%g, %g] degrees.\n",
			cat->nstars,
			rad2deg(cat->ramin), rad2deg(cat->ramax),
			rad2deg(cat->decmin), rad2deg(cat->decmax));

	fprintf(stderr, "Computing RA/DEC range...\n");
	fflush(stderr);
	catalog_compute_radecminmax(cat);

	fprintf(stderr, "Actual limits RA=[%g, %g] DEC=[%g, %g] degrees.\n",
			rad2deg(cat->ramin), rad2deg(cat->ramax),
			rad2deg(cat->decmin), rad2deg(cat->decmax));

	printf("\nWriting output...\n");

	if (inplace) {
		res = catalog_rewrite_header(cat);
	} else {
		res = catalog_write_to_file(cat, outfname, 0);
	}
	if (res) {
		fprintf(stderr, "Couldn't write catalog to file %s.\n", outfname);
		catalog_close(cat);
		exit(-1);
	}

	catalog_close(cat);
	free_fn(catfname);
	free_fn(outfname);

    return 0;
}
