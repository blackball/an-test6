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
  double ramin, ramax, decmin, decmax;
  int i;
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

  cat = catalog_open_file(catfname);
  if (!cat) {
	  fprintf(stderr, "Couldn't open catalog.\n");
	  exit(-1);
  }

  fprintf(stderr, "Got %u stars.\n"
		  "Limits RA=[%g, %g] DEC=[%g, %g] degrees.\n",
		  cat->nstars,
		  rad2deg(cat->ramin), rad2deg(cat->ramax),
		  rad2deg(cat->decmin), rad2deg(cat->decmax));

  ramin = 1e100;
  ramax = -1e100;
  decmin = 1e100;
  decmax = -1e100;
  for (i=0; i<cat->nstars; i++) {
	  double* xyz;
	  double ra, dec;
	  xyz = catalog_get_star(cat, i);
	  ra = xy2ra(xyz[0], xyz[1]);
	  dec = z2dec(xyz[2]);
	  if (ra > ramax) ramax = ra;
	  if (ra < ramin) ramin = ra;
	  if (dec > decmax) decmax = dec;
	  if (dec < decmin) decmin = dec;
	  //printf("%10i %20g %20g\n", i, ra, dec);
	  if (i % 100000 == 0) {
		printf(".");
		fflush(stdout);
	  }
	}
	printf("\n");

	printf("RA  range %g, %g\n", rad2deg(ramin),  rad2deg(ramax));
	printf("Dec range %g, %g\n", rad2deg(decmin), rad2deg(decmax));

	printf("\nWriting output...\n");

	cat->ramin = ramin;
	cat->ramax = ramax;
	cat->decmin = decmin;
	cat->decmax = decmax;

	if (inplace) {
		res = catalog_rewrite_header(cat);
	} else {
		res = catalog_write_to_file(cat, outfname);
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
