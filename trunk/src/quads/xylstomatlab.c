#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

int main(int argc, char** args) {
  FILE* f;
  char* filename;
  char* varname;
  int numfields;
  int npoints;
  double* points;
  int i;

  if (argc != 3) {
	printf("%s <xyls-file> <variable-name>\n", args[0]);
	exit(-1);
  }

  filename = args[1];
  varname = args[2];

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

  // second line: first field
  if (fscanf(f, "%i,", &npoints) != 1) {
      printf("parse error: npoints\n");
      exit(-1);
  }

  printf("number of points = %i\n", npoints);

  points = (double*)malloc(sizeof(double) * npoints * 2);
  for (i=0; i<npoints; i++) {
      points[i*2] = points[i*2+1] = 0.0;
      if (fscanf(f, "%lf,%lf", points + i*2, points + i*2 + 1) != 2) {
          printf("parse error: points %i\n", i);
          exit(-1);
      }
      //printf("    %i:  %f, %f\n", i, points[i*2], points[i*2+1]);
      if (i != (npoints-1)) {
          fscanf(f, ",");
      }
  }

  fprintf(stderr, "%s=[", varname);
  for (i=0; i<npoints; i++) {
	  fprintf(stderr, "%g,%g;", points[i*2], points[i*2+1]);
  }
  fprintf(stderr, "];\n");

  fclose(f);
  return 0;
}
