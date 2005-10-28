#include "fileutil.h"
#include "starutil.h"
#include "math.h"
#define OPTIONS "bhgf:N:"

char* help = "usage: plotcat [-b] [-h] [-g] [-N imsize] -f catalog.objs\n"
"> outfile.pgm\n"
"  -h sets Hammer-Aitoff, -b sets reverse, -g adds grid\n";

unsigned int projection[N][N];
extern char *optarg;
extern int optind, opterr, optopt;

unsigned long N=3000;

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
	/* Hammer-Aitoff projection with x-z plane compressed to purely +z side
	 * of xz plane */
	double theta = atan(x/z);
	if (z < 0) {
		if (x < 0) {
			theta = theta - PI;
		} else {
			theta = PI + theta;
		}
	}
	theta /= 2.0;

	double r = sqrt(x*x+z*z);
	double zp, xp;
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

int main(int argc, char *argv[])
{
	char ASCII = READ_FAIL;
	int ii, jj, reverse=0, hammer=0, grid=0;
	sidx numstars;
	dimension Dim_Stars;
	double ramin, ramax, decmin, decmax;
	double x,y,z;
	int X,Y;
	FILE* fid = NULL;
	int argidx, argchar; //  opterr = 0;

	if (argc <= 2) {
		fprintf(stderr, help);
		return 1;
	} 

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
		  N=strtoul(optarg, NULL, 0);
		  break;
		case 'f':
			fid = fopen(optarg,"r");
			break;
		default:
			return (OPT_ERR);
		}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		return (OPT_ERR);
	}

	if (!fid) {
		fprintf(stderr, "ERROR: Failed to open catalog\n");
		return 1;
	}

	ASCII = read_objs_header(fid, &numstars, &Dim_Stars, &ramin, &ramax, &decmin, &decmax);
	if (ASCII == (char)READ_FAIL) {
		fprintf(stderr, "ERROR: Failed to read catalog\n");
		return 1;
	}

	if (Dim_Stars != 3) {
		fprintf(stderr, "ERROR: Catalog has dimension != 3\n");
		return 1;
	}

	for (ii = 0; ii < N; ii++)
		for (jj = 0; jj < N; jj++)
			projection[ii][jj] = 0;

	for (ii = 0; ii < numstars; ii++) {
	  if(is_power_of_two(ii+1))
	    fprintf(stderr,"  done %lu\%lu stars\r",ii+1,numstars);

		if (ASCII) {
			fscanf(fid, "%lf,%lf,%lf\n", &x, &y, &z);
		} else {
			fread(&x, sizeof(double), 1, fid);
			fread(&y, sizeof(double), 1, fid);
			fread(&z, sizeof(double), 1, fid);
		}

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
		projection[X][Y] += 1;
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
				projection[X][Y] = 255;
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
				projection[X][Y] = 255;
			}
		}
	}

	// Output PGM format
	printf("P5 %d %d %d\n", N, N, 255);
	for (ii = 0; ii < N; ii++) {
		for (jj = 0; jj < N; jj++) {
			if (projection[ii][jj] < 255)
				putchar(projection[ii][jj]);
				
			else
				putchar(255);
		}
	}
	return 0;
}
