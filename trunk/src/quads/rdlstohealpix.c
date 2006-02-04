#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "healpix.h"

int main(int argc, char** args) {
  FILE* f;
  char* filename;
  int numfields;
  int npoints;
  int i, j;
  int healpixes[12];

  if (argc != 2) {
	printf("%s <rdls-file>\n", args[0]);
	exit(-1);
  }

  filename = args[1];

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
  }

  fclose(f);
  return 0;
}
