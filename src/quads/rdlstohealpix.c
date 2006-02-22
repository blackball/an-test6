#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "healpix.h"
#include "starutil.h"
#include "lsfile.h"

char* OPTIONS = "hH:q";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<rdls-file>]\n"
			"   [-h] print help msg\n"
			"   [-q] quiet mode\n"
			"   [-H healpix]: print the fields with stars in a healpix.\n"
			"\nIf <rdls-file> is not provided, will read from stdin.\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
  FILE* f;
  char* filename;
  int numfields;
  int npoints;
  int i, j;
  int healpixes[12];
  int argchar;
  char* progname = args[0];
  int target = -1;
  bool fromstdin = FALSE;
  bool quiet = FALSE;

  while ((argchar = getopt (argc, args, OPTIONS)) != -1)
	  switch (argchar) {
	  case 'h':
		  printHelp(progname);
		  exit(0);
	  case 'q':
		  quiet = TRUE;
		  break;
	  case 'H':
		  target = atoi(optarg);
		  break;
	  case '?':
		  fprintf(stderr, "Unknown option `-%c'.\n", optopt);
	  default:
		  exit(-1);
	  }

  if (optind < argc) {
	  filename = args[optind];
  } else {
	  fromstdin = TRUE;
  }

  if (fromstdin) {
	  printf("Reading from stdin...\n");
	  f = stdin;
  } else {
	  f = fopen(filename, "r");
	  if (!f) {
		  printf("Couldn't open %s: %s\n", filename, strerror(errno));
		  exit(-1);
	  }
  }

  if (target != -1) {
	  printf("Printing the indices of fields in healpix %i to stderr...\n", target);
  }

  numfields = read_ls_file_header(f);
  if (numfields == -1) {
	  fprintf(stderr, "Couldn't read rdls file.\n");
	  exit(-1);
  }

  printf("NumFields %i.\n", numfields);

  for (j=0; j<numfields; j++) {
	  blocklist* points;

	  points = read_ls_file_field(f, 2);
	  if (!points) {
		  printf("error reading field %i\n", j);
		  break;
	  }

	  for (i=0; i<12; i++) {
		  healpixes[i] = 0;
	  }

	  npoints = blocklist_count(points) / 2;

	  for (i=0; i<npoints; i++) {
		  double ra, dec;
		  int hp;

		  ra = blocklist_double_access(points, i*2);
		  dec = blocklist_double_access(points, i*2 + 1);

		  ra  *= M_PI / 180.0;
		  dec *= M_PI / 180.0;

		  hp = radectohealpix(ra, dec);
		  if ((hp < 0) || (hp >= 12)) {
			  printf("hp=%i\n", hp);
			  continue;
		  }
		  healpixes[hp] = 1;
	  }
	  if (!quiet) {
		  printf("Field %i: healpixes  ", j);
		  for (i=0; i<12; i++) {
			  if (healpixes[i])
				  printf("%i  ", i);
		  }
		  printf("\n");
	  }

	  if ((target != -1) && (healpixes[target])) {
		  fprintf(stderr, "%i ", j);
	  }

	  blocklist_free(points);
  }

  if (!fromstdin)
	  fclose(f);
  return 0;
}
