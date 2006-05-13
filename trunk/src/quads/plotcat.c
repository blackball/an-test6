#include <math.h>

#include "fileutil.h"
#include "starutil.h"
#include "catalog.h"
#include "an_catalog.h"
#include "usnob_fits.h"
#include "tycho2_fits.h"

#define OPTIONS "bhgf:N:"

char* help = "usage: plotcat [-b] [-h] [-g] [-N imsize]"
" [-f catalog.objs] > outfile.pgm\n"
"  -h sets Hammer-Aitoff, -b sets reverse, -g adds grid\n"
"  -N sets edge size of output image\n";


double *projection;
extern char *optarg;
extern int optind, opterr, optopt;

int N=3000;

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

inline int is_power_of_two(int x) {
		return (x == 0x00000001 ||
		        x == 0x00000002 ||
		        x == 0x00000004 ||
		        x == 0x00000008 ||
		        x == 0x00000010 ||
		        x == 0x00000020 ||
		        x == 0x00000040 ||
		        x == 0x00000080 ||
		        x == 0x00000100 ||
		        x == 0x00000200 ||
		        x == 0x00000400 ||
		        x == 0x00000800 ||
		        x == 0x00001000 ||
		        x == 0x00002000 ||
		        x == 0x00004000 ||
		        x == 0x00008000 ||
		        x == 0x00010000 ||
		        x == 0x00020000 ||
		        x == 0x00040000 ||
		        x == 0x00080000 ||
		        x == 0x00100000 ||
		        x == 0x00200000 ||
		        x == 0x00400000 ||
		        x == 0x00800000 ||
		        x == 0x01000000 ||
		        x == 0x02000000 ||
		        x == 0x04000000 ||
		        x == 0x08000000 ||
		        x == 0x10000000 ||
		        x == 0x20000000 ||
		        x == 0x40000000 ||
		        x == 0x80000000);
}

int main(int argc, char *argv[])
{
	uint ii,jj,numstars=0;
	int reverse=0, hammer=0, grid=0;
	double x=0,y=0,z=0;
	int maxval;
	int X,Y;
	//unsigned long int saturated=0;
	char* fname = NULL;
	int argidx, argchar;
	FILE* fid;
	qfits_header* hdr;
	char* valstr;

	catalog* cat = NULL;
	an_catalog* ancat = NULL;
	usnob_fits* usnob = NULL;
	tycho2_fits* tycho = NULL;

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
		case 'f':
			fname = optarg;
			break;
		default:
			return (OPT_ERR);
		}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		return (OPT_ERR);
	}

	if (!fname) {
		fprintf(stderr, help);
		return 1;
	}
	fid = fopen(fname, "rb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s.  (Specify the complete filename with -f <filename>)\n", fname);
		exit(-1);
	}
	fclose(fid);

	hdr = qfits_header_read(fname);
	if (!hdr) {
		fprintf(stderr, "Couldn't read FITS header from file %s.\n", fname);
		exit(-1);
	}
	// look for AN_FILE (Astrometry.net filetype) in the FITS header.
	valstr = qfits_header_getstr(hdr, "AN_FILE");
	if (valstr) {
		printf("Astrometry.net file type: \'%s\".\n", valstr);
		if (strcasecmp(valstr, CATALOG_AN_FILETYPE) == 0) {
			printf("Looks like a catalog.\n");
			cat = catalog_open(fname, 0);
			if (!cat) {
				fprintf(stderr, "Couldn't open catalog.\n");
				return 1;
			}
			numstars = cat->numstars;
		}
	} else if (qfits_header_getboolean(hdr, "AN_CATALOG", 0)) {
		printf("File has AN_CATALOG = T header.\n");
		ancat = an_catalog_open(fname);
		if (!ancat) {
			fprintf(stderr, "Couldn't open catalog.\n");
			exit(-1);
		}
		numstars = ancat->nentries;
	} else if (qfits_header_getboolean(hdr, "USNOB", 0)) {
		printf("File has USNOB = T header.\n");
		usnob = usnob_fits_open(fname);
		if (!usnob) {
			fprintf(stderr, "Couldn't open catalog.\n");
			exit(-1);
		}
		numstars = usnob->nentries;
	} else if (qfits_header_getboolean(hdr, "TYCHO_2", 0)) {
		printf("File has TYCHO_2 = T header.\n");
		tycho = tycho2_fits_open(fname);
		if (!tycho) {
			fprintf(stderr, "Couldn't open catalog.\n");
			exit(-1);
		}
		numstars = tycho->nentries;
	} else {
		printf("I can't figure out what kind of file %s is.\n", fname);
		exit(-1);
	}
	qfits_header_destroy(hdr);


	projection=calloc(sizeof(double),N*N);

	for (ii = 0; ii < numstars; ii++) {
		double* xyz;
		if(is_power_of_two(ii+1))
			fprintf(stderr,"  done %u/%u stars\r",ii+1,numstars);

		if (cat) {
			xyz = catalog_get_star(cat, ii);
			x = xyz[0];
			y = xyz[1];
			z = xyz[2];
		} else if (ancat) {
			an_entry entry;
			an_catalog_read_entries(ancat, ii, 1, &entry);
			x = radec2x(deg2rad(entry.ra), deg2rad(entry.dec));
			y = radec2y(deg2rad(entry.ra), deg2rad(entry.dec));
			z = radec2z(deg2rad(entry.ra), deg2rad(entry.dec));
		} else if (usnob) {
			usnob_entry entry;
			usnob_fits_read_entries(usnob, ii, 1, &entry);
			x = radec2x(deg2rad(entry.ra), deg2rad(entry.dec));
			y = radec2y(deg2rad(entry.ra), deg2rad(entry.dec));
			z = radec2z(deg2rad(entry.ra), deg2rad(entry.dec));
		} else if (tycho) {
			tycho2_entry entry;
			tycho2_fits_read_entries(tycho, ii, 1, &entry);
			x = radec2x(deg2rad(entry.RA), deg2rad(entry.DEC));
			y = radec2y(deg2rad(entry.RA), deg2rad(entry.DEC));
			z = radec2z(deg2rad(entry.RA), deg2rad(entry.DEC));
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
		projection[X+N*Y]++;
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

	maxval = 0;
	for (ii = 0; ii < (N*N); ii++)
		if (projection[ii] > maxval)
			maxval = projection[ii];

	// Output PGM format
	printf("P5 %d %d %d\n",N,N, 255);
	for (ii = 0; ii < N; ii++) {
		for (jj = 0; jj < N; jj++) {
			putchar((int)(255.0 * projection[ii+N*jj] / (double)maxval));
		}
	}
	/*
	  saturated++;
	  fprintf(stderr," %lu/%lu pixels saturated\n",
	  saturated,((unsigned long int)N*(unsigned long int)N));
	*/
	if (cat)
		catalog_close(cat);
	if (ancat)
		an_catalog_close(ancat);
	if (usnob)
		usnob_fits_close(usnob);
	if (tycho)
		tycho2_fits_close(tycho);

	return 0;
}
