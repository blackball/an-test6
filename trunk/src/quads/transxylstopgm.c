#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// X = L * R
// all three are three-by-three.
void matrix_matrix_mult(double* X, double* L, double* R) {
  int i, j, k;
  for (i=0; i<3; i++) {
	for (j=0; j<3; j++) {
	  double sum = 0.0;
	  for (k=0; k<3; k++) {
		sum += L[3*i + k] * R[3*k + j];
	  }
	  X[3*i + j] = sum;
	}
  }
}

// X = M * x
// M is three-by-three, {x,X} are three-by-one
void matrix_vector_mult(double* X, double* M, double* x) {
  int i, j;
  for (i=0; i<3; i++) {
	double sum = 0.0;
	for (j=0; j<3; j++) {
	  sum += M[3*i + j] * x[j];
	}
	X[i] = sum;
  }
}

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
  double Rmin = 4.0;
  double Rmax = 5.0;
  int R;

  double M[9] =
  {
	-0.00042460475764,  -0.00000017906308,   0.81569691747735,
	-0.00000037007722,   0.00041676669626, -11.61146167502250,
	0.0, 0.0, 1.0
  };

  double radec[3];
  double xy[3];



  double image_size = 1000.0;
  double seconds_per_pix = 5.0;
  double midpoint_ra  = 0.031;
  double midpoint_dec = -10.791;



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

	  // transform them.
	  xy[0] = points[i*2];
	  xy[1] = points[i*2+1];
	  xy[2] = 1.0;
	  matrix_vector_mult(radec, M, xy);

	  printf("      -> ra %f, dec %f\n", radec[0], radec[1]);

	  // scale them to image space
	  {
		double ix, iy;
		ix = ((radec[0] - midpoint_ra)  * 3600.0 / seconds_per_pix)
		  + (image_size / 2.0);
		iy = ((radec[1] - midpoint_dec) * 3600.0 / seconds_per_pix)
		  + (image_size / 2.0);

		printf("      -> image %i, %i\n", (int)(ix+0.5), (int)(iy+0.5));

		points[i*2] = ix;
		points[i*2 + 1] = iy;
	  }
  }

  // find the range of X and Y values...
  {
	double minx = +1e100;
	double miny = +1e100;
	double maxx = -1e100;
	double maxy = -1e100;

	for (i=0; i<npoints; i++) {
      if (points[i*2] > maxx) {
		maxx = points[i*2];
	  }
      if (points[i*2] < minx) {
		minx = points[i*2];
	  }
      if (points[i*2+1] > maxy) {
		maxy = points[i*2+1];
	  }
      if (points[i*2+1] < miny) {
		miny = points[i*2+1];
	  }
	}
	printf("image range: %f to %f by %f to %f\n", minx, maxx, miny, maxy);
  }

  W = H = (int)image_size;

  image = (char*)malloc(W * H);
  for (i=0; i<W*H; i++) {
	image[i] = 0;
  }

  R = (int)(1.0 + Rmax);
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
		double rad;
		if (y + k < 0)
		  continue;
		if (y + k >= H)
		  continue;
		rad = sqrt(j*j + k*k);
		if ((rad < Rmin) || (rad > Rmax))
		  continue;
		image[(y+k)*W + (x+j)] = 255;
	  }
	}
  }

  // pgm format is great :)
  fprintf(outf, "P5 %i %i 255\n", W, H);
  fwrite(image, 1, W*H, outf);

  fclose(outf);
  free(image);
  free(points);
  fclose(f);
}
