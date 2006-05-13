#include <math.h>

#include "fileutil.h"
#include "starutil.h"
#include "catalog.h"
#include "an_catalog.h"
#include "usnob_fits.h"
#include "tycho2_fits.h"
#include "mathutil.h"

#define OPTIONS "bhgN:"

char* help = "usage: plotcat [-b] [-h] [-g] [-N imsize]"
" <filename> [<filename> ...] > outfile.pgm\n"
"  -h sets Hammer-Aitoff\n"
"  -b sets reverse\n"
"  -g adds grid\n"
"  -N sets edge size of output image\n\n"
"Can read Tycho2.fits, USNOB.fits, AN.fits, and AN.objs.fits files.\n";

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
	int argchar;
	FILE* fid;
	qfits_header* hdr;
	char* valstr;
	void* entries;
	int BLOCK = 100000;
	catalog* cat;
	an_catalog* ancat;
	usnob_fits* usnob;
	tycho2_fits* tycho;

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
		default:
			return (OPT_ERR);
		}

	if (optind == argc) {
		fprintf(stderr, help);
		exit(-1);
	}

	projection=calloc(sizeof(double), N*N);

	entries = malloc(BLOCK * imax(sizeof(usnob_entry),
								  imax(sizeof(tycho2_entry),
									   sizeof(an_entry))));

	for (; optind<argc; optind++) {
		int n, off, i;
		cat = NULL;
		ancat = NULL;
		usnob = NULL;
		tycho = NULL;
		numstars = 0;
		fname = argv[optind];
		fprintf(stderr, "Reading file %s...\n", fname);
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
			char* str = valstr;
			fprintf(stderr, "Astrometry.net file type: \"%s\".\n", valstr);
			if (str[0] == '\'') str++;
			if (strncasecmp(str, CATALOG_AN_FILETYPE, strlen(CATALOG_AN_FILETYPE)) == 0) {
				fprintf(stderr, "Looks like a catalog.\n");
				cat = catalog_open(fname, 0);
				if (!cat) {
					fprintf(stderr, "Couldn't open catalog.\n");
					return 1;
				}
				numstars = cat->numstars;
			} else {
				fprintf(stderr, "Unknown Astrometry.net file type.\n");
				exit(-1);
			}
		} else if (qfits_header_getboolean(hdr, "AN_CATALOG", 0)) {
			fprintf(stderr, "File has AN_CATALOG = T header.\n");
			ancat = an_catalog_open(fname);
			if (!ancat) {
				fprintf(stderr, "Couldn't open catalog.\n");
				exit(-1);
			}
			numstars = ancat->nentries;
		} else if (qfits_header_getboolean(hdr, "USNOB", 0)) {
			fprintf(stderr, "File has USNOB = T header.\n");
			usnob = usnob_fits_open(fname);
			if (!usnob) {
				fprintf(stderr, "Couldn't open catalog.\n");
				exit(-1);
			}
			numstars = usnob->nentries;
		} else if (qfits_header_getboolean(hdr, "TYCHO_2", 0)) {
			fprintf(stderr, "File has TYCHO_2 = T header.\n");
			tycho = tycho2_fits_open(fname);
			if (!tycho) {
				fprintf(stderr, "Couldn't open catalog.\n");
				exit(-1);
			}
			numstars = tycho->nentries;
		} else {
			fprintf(stderr, "I can't figure out what kind of file %s is.\n", fname);
			exit(-1);
		}
		qfits_header_destroy(hdr);

		for (off=0; off<N; off+=n) {
			if (off + BLOCK > numstars)
				n = N - off;
			else
				n = BLOCK;

			if (ancat) {
				an_catalog_read_entries(ancat, off, n, entries);
			} else if (usnob) {
				usnob_fits_read_entries(usnob, off, n, entries);
			} else if (tycho) {
				tycho2_fits_read_entries(tycho, off, n, entries);
			}

			for (i=0; i<n; i++) {
				if(is_power_of_two(off+i+1))
					fprintf(stderr,"  done %u/%u stars\r",off+i+1,numstars);

				if (cat) {
					double* xyz;
					xyz = catalog_get_star(cat, off+i);
					x = xyz[0];
					y = xyz[1];
					z = xyz[2];
				} else if (ancat) {
					an_entry* entry = ((an_entry*)entries) + i;
					x = radec2x(deg2rad(entry->ra), deg2rad(entry->dec));
					y = radec2y(deg2rad(entry->ra), deg2rad(entry->dec));
					z = radec2z(deg2rad(entry->ra), deg2rad(entry->dec));
				} else if (usnob) {
					usnob_entry* entry = ((usnob_entry*)entries) + i;
					x = radec2x(deg2rad(entry->ra), deg2rad(entry->dec));
					y = radec2y(deg2rad(entry->ra), deg2rad(entry->dec));
					z = radec2z(deg2rad(entry->ra), deg2rad(entry->dec));
				} else if (tycho) {
					tycho2_entry* entry = ((tycho2_entry*)entries) + i;
					x = radec2x(deg2rad(entry->RA), deg2rad(entry->DEC));
					y = radec2y(deg2rad(entry->RA), deg2rad(entry->DEC));
					z = radec2z(deg2rad(entry->RA), deg2rad(entry->DEC));
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
		}

		if (cat)
			catalog_close(cat);
		if (ancat)
			an_catalog_close(ancat);
		if (usnob)
			usnob_fits_close(usnob);
		if (tycho)
			tycho2_fits_close(tycho);
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

	free(entries);

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
	free(projection);

	return 0;
}
