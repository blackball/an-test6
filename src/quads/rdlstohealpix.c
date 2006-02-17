#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "healpix.h"

char* OPTIONS = "hH:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] <rdls-file>\n"
			"   [-h] print help msg\n"
			"   [-H healpix]: print the fields with stars in a healpix.\n",
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

  if (argc < 2) {
	  printHelp(progname);
	  exit(-1);
  }

  while ((argchar = getopt (argc, args, OPTIONS)) != -1)
	  switch (argchar) {
	  case 'h':
		  printHelp(progname);
		  exit(0);
	  case 'H':
		  target = atoi(optarg);
		  break;
	  case '?':
		  fprintf(stderr, "Unknown option `-%c'.\n", optopt);
	  default:
		  exit(-1);
	  }

  filename = args[optind];

  f = fopen(filename, "r");
  if (!f) {
      printf("Couldn't open %s: %s\n", filename, strerror(errno));
      exit(-1);
  }

  // first line: numfields
  if (fscanf(f, "NumFields=%i\n", &numfields) != 1) {
      printf("parse error: numfields\n");
      exit(-1);
  }

  if (target != -1) {
	  printf("Printing the indices of fields in healpix %i to stderr...\n", target);
  }

  printf("Numfields = %i\n", numfields);

  for (j=0; j<numfields; j++) {
	  // second line: first field
	  if (fscanf(f, "%i,", &npoints) != 1) {
		  printf("parse error: npoints\n");
		  exit(-1);
	  }
	  //printf("number of points = %i\n", npoints);

	  for (i=0; i<12; i++) {
		  healpixes[i] = 0;
	  }

	  for (i=0; i<npoints; i++) {
		  double ra, dec;
		  int hp;
		  if (fscanf(f, "%lf,%lf", &ra, &dec) != 2) {
			  printf("parse error: points %i\n", i);
			  exit(-1);
		  }
		  if (i != (npoints-1)) {
			  fscanf(f, ",");
		  }

		  ra  *= M_PI / 180.0;
		  dec *= M_PI / 180.0;

		  hp = radectohealpix(ra, dec);
		  if ((hp < 0) || (hp >= 12)) {
			  printf("hp=%i\n", hp);
			  continue;
		  }
		  healpixes[hp] = 1;
	  }
	  printf("Field %i: healpixes  ", j);
	  for (i=0; i<12; i++) {
		  if (healpixes[i])
			  printf("%i  ", i);
	  }
	  printf("\n");

	  if ((target != -1) && (healpixes[target])) {
		  fprintf(stderr, "%i ", j);
	  }
  }

  fclose(f);
  return 0;
}
