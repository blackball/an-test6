#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char** args) {
  FILE* f;
  FILE* outf;
  char* filename;
  char* outfile;
  int BUFSIZE = 65536;
  char buffer[BUFSIZE];
  int numfields;
  int npoints;
  double* points;
  int i, j, k;
  char* image;
  int W, H;
  int R = 10;

  if (argc != 3) {
	printf("%s <xyls-file> <pgm-file>\n", args[0]);
	exit(-1);
  }

  filename = args[1];

  f = fopen(filename, "r");
  if (!f) {
      printf("Couldn't open %s: %s\n", filename, strerror(errno));
      exit(-1);
  }

  outfile = args[2];
  outf = fopen(outfile, "w");
  if (!outf) {
      printf("Couldn't open %s: %s\n", outfile, strerror(errno));
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

      printf("    %i:  %f, %f\n", i, points[i*2], points[i*2+1]);

      if (i != (npoints-1)) {
          fscanf(f, ",");
      }
  }

  // find the largest X and Y values...
  W = H = 0;
  for (i=0; i<npoints; i++) {
      if (points[i*2] > W) {
          W = 1 + (int)points[i*2];
      }
      if (points[i*2 + 1] > H) {
          H = 1 + (int)points[i*2 + 1];
      }
  }

  printf("image size will be: %i by %i\n", W, H);

  image = (char*)malloc(W * H);
  for (i=0; i<W*H; i++) {
      image[i] = 0;
  }

  for (i=0; i<npoints; i++) {
      int x,y;
      x = (int)(0.5 + points[i*2]);
      y = (int)(0.5 + points[i*2 + 1]);
      for (j=-R; j<=R; j++) {
          if (x + j < 0)
              continue;
          if (x + j >= W)
              continue;
          for (k=-R; k<=R; k++) {
              if (y + k < 0)
                  continue;
              if (y + k >= H)
                  continue;
              if (sqrt(j*j + k*k) > R)
                  continue;
              image[(y+k)*W + (x+j)] = 255;
          }
      }
  }

  fprintf(outf, "P5 %i %i 255\n", W, H);
  fwrite(image, 1, W*H, outf);

  /*
    ;
    // first line: numfields
    if (!fgets(buffer, BUFSIZE, f)) {
    printf("Read error: %s\n", strerror(errno));
    exit(-1);
    }
  */

  fclose(outf);
  free(image);
  free(points);
  fclose(f);
}
