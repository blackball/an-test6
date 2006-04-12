#include <math.h>
#include "fileutil.h"
#include "starutil.h"
#include "catalog.h"
#define OPTIONS "bhgf:N:I:"

char* help = "usage: plotcat [-b] [-h] [-g] [-N imsize] [-I intensity]"
" [-f catalog.objs] > outfile.pgm\n"
"  -h sets Hammer-Aitoff, -b sets reverse, -g adds grid\n"
"  -N sets edge size of output image, -I sets intensity scale for pixels\n";


double *projection;
extern char *optarg;
extern int optind, opterr, optopt;

int N=3000;
double Increment=1.0;

#define PI M_PI

inline void project_equal_area(double x, double y, double z, int *X, int *Y)
{
	double Xp = x*sqrt(1./(1. + z));
	double Yp = y*sqrt(1./(1. + z));
	*X = (unsigned int) floor(N*0.5*(1+0.9*Xp));
	*Y = (unsigned int) floor(N*0.5*(1+0.9*Yp));
	if (*X > N || *Y > N) {
		fprintf(stderr, "BAIL: xind=%d, yind=%d\n", *X, *Y);
		exit(1);
	}
}

inline void project_hammer_aitoff_x(double x, double y, double z, int *X, int *Y)
{

	double theta = atan(x/z);
	double r = sqrt(x*x+z*z);
	double zp, xp;

	/* Hammer-Aitoff projection with x-z plane compressed to purely +z side
	 * of xz plane */

	if (z < 0) {
		if (x < 0) {
			theta = theta - PI;
		} else {
			theta = PI + theta;
		}
	}
	theta /= 2.0;

	zp = r*cos(theta);
	xp = r*sin(theta);
	if (zp < -0.01) {
		fprintf(stderr,
			"BAIL: Hammer projection, got negative z: "
			"z=%lf; x=%lf; zp=%lf; xp=%lf\n", z,x,zp, xp);
		exit(1);
	}
	project_equal_area(xp,y,zp, X, Y);
}

// borrowed from andrew moore...
inline int is_power_of_two(int x) {
    int result = (x > 0);
	while (result && (x > 1)) {
		result = ((x & 1) == 0);
		x = x >> 1;
	}
	return result;
}

int main(int argc, char *argv[])
{
	uint ii,jj,numstars;
	int reverse=0, hammer=0, grid=0;
	double x,y,z;
	int X,Y;
	unsigned long int saturated=0;
	catalog* cat;
	char* basename = NULL;
	int argidx, argchar;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'b':
			reverse = 1;
			break;
		case 'h':
			hammer = 1;
			break;
		case 'g':
			grid = 1;
			break;
		case 'N':
		  N=(int)strtoul(optarg, NULL, 0);
		  break;
		case 'I':
		  Increment=strtod(optarg, NULL);
		  break;
		case 'f':
			basename = optarg;
			break;
		default:
			return (OPT_ERR);
		}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		return (OPT_ERR);
	}

	if (!basename) {
		fprintf(stderr, help);
		return 1;
	}
	cat = catalog_open(basename, 0);
	if (!cat) {
		fprintf(stderr, "Couldn't open catalog.\n");
		return 1;
	}
	numstars = cat->nstars;

	projection=calloc(sizeof(double),N*N);

	for (ii = 0; ii < numstars; ii++) {
		double* xyz;
	  if(is_power_of_two(ii+1))
	    fprintf(stderr,"  done %u/%u stars\r",ii+1,numstars);

		xyz = catalog_get_star(cat, ii);
		x = xyz[0];
		y = xyz[1];
		z = xyz[2];

		if (!hammer) {
			if ((z <= 0 && !reverse) || (z >= 0 && reverse)) 
				continue;
			if (reverse)
				z = -z;
			project_equal_area(x, y, z, &X, &Y);
		} else {
			/* Hammer-Aitoff projection */
			project_hammer_aitoff_x(x, y, z, &X, &Y);
		}
		projection[X+N*Y] += Increment;
	}

	{
		int r, d, R=100, D=100;
		for (r=0; r<=R; r++)
			for (d=0; d<=D; d++) {
				double ra = 224.4 + ((double)(r-R/2) / (double)R) * 0.25;
				double dec = -1.05 + ((double)(d-D/2) / (double)D) * 0.25;;
				ra = deg2rad(ra);
				dec = deg2rad(dec);
				x = radec2x(ra, dec);
				y = radec2y(ra, dec);
				z = radec2z(ra, dec);
				if (!hammer) {
					if ((z <= 0 && !reverse) || (z >= 0 && reverse)) 
						fprintf(stderr, "\nack!\n");
					if (reverse)
						z = -z;
					project_equal_area(x, y, z, &X, &Y);
				} else {
					/* Hammer-Aitoff projection */
					project_hammer_aitoff_x(x, y, z, &X, &Y);
				}
				fprintf(stderr, "\nX,Y = %i,%i\n", X, Y);
				projection[X+N*Y] = 255;
			}
	}


	if (grid) {
		/* Draw a line for ra=-160...+160 in 10 degree sections */
		for (ii=-160; ii <= 160; ii+= 10) {
			int RES = 10000;
			for (jj=-RES; jj<RES; jj++) {
				/* Draw a bunch of points for dec -90...90*/

				double ra = deg2rad(ii);
				double dec = jj/(double)RES * PI/2.0;
				x = radec2x(ra,dec);
				y = radec2y(ra,dec);
				z = radec2z(ra,dec);

				if (!hammer) {
					if ((z <= 0 && !reverse) || (z >= 0 && reverse)) 
						continue;
					if (reverse)
						z = -z;
					project_equal_area(x, y, z, &X, &Y);
				} else {
					/* Hammer-Aitoff projection */
					project_hammer_aitoff_x(x, y, z, &X, &Y);
				}
				projection[X+N*Y] = 255;
			}
		}
		/* Draw a line for dec=-80...+80 in 10 degree sections */
		for (ii=-80; ii <= 80; ii+= 10) {
			int RES = 10000;
			for (jj=-RES; jj<RES; jj++) {
				/* Draw a bunch of points for dec -90...90*/

				double ra = jj/(double)RES * PI;
				double dec = deg2rad(ii);
				x = radec2x(ra,dec);
				y = radec2y(ra,dec);
				z = radec2z(ra,dec);

				if (!hammer) {
					if ((z <= 0 && !reverse) || (z >= 0 && reverse)) 
						continue;
					if (reverse)
						z = -z;
					project_equal_area(x, y, z, &X, &Y);
				} else {
					/* Hammer-Aitoff projection */
					project_hammer_aitoff_x(x, y, z, &X, &Y);
				}
				projection[X+N*Y] = 255;
			}
		}
	}

	// Output PGM format
	printf("P5 %d %d %d\n",N,N, 255);
	for (ii = 0; ii < N; ii++) {
		for (jj = 0; jj < N; jj++) {
			if (projection[ii+N*jj] <= 255)
				putchar(ceil(projection[ii+N*jj]));
			else {
				putchar(255);
				saturated++;
			}
		}
	}

	fprintf(stderr," %lu/%lu pixels saturated\n",
		saturated,((unsigned long int)N*(unsigned long int)N));

	catalog_close(cat);

	return 0;
}
