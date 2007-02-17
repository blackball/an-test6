/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <math.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "keywords.h"
#include "fileutil.h"
#include "starutil.h"
#include "catalog.h"
#include "an_catalog.h"
#include "usnob_fits.h"
#include "tycho2_fits.h"
#include "mathutil.h"
#include "rdlist.h"
#include "boilerplate.h"
#include "starkd.h"

#define OPTIONS "bhgN:f:ts"

static void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s [-b] [-h] [-g] [-N imsize]"
		   " <filename> [<filename> ...] > outfile.pgm\n"
		   "  -h sets Hammer-Aitoff (default is an equal-area, positive-Z projection)\n"
		   "  -b sets reverse (negative-Z projection)\n"
		   "  -g adds RA,DEC grid\n"
		   "  -N sets edge size of output image\n"
		   "  [-s]: write two-byte-per-pixel PGM (default is one-byte-per-pixel)\n"
		   "  [-f <field-num>]: for RA,Dec lists (rdls), which field to use (default: all)\n\n"
		   "  [-L <field-range-low>]\n"
		   "  [-H <field-range-high>]\n"
		   "\n"
		   "  [-t]: for USNOB inputs, include Tycho-2 stars, even though their format isn't quite right.\n"
		   "\n"
		   "Can read Tycho2.fits, USNOB.fits, AN.fits, AN.objs.fits, AN.skdt.fits, and rdls.fits files.\n"
		   "\n", progname);
}
		   
double *projection;
extern char *optarg;
extern int optind, opterr, optopt;

#define PI M_PI

Inline void getxy(double px, double py, int N,
				  int* X, int* Y) {
	px = 0.5 + (px - 0.5) * 0.99;
	py = 0.5 + (py - 0.5) * 0.99;
	*X = (int)nearbyint(px * N);
	*Y = (int)nearbyint(py * N);
}

int main(int argc, char *argv[])
{
	char* progname = argv[0];
	uint ii,jj,numstars=0;
	int reverse=0, hammer=0, grid=0;
	double x=0,y=0,z=0;
	int maxval;
	int X,Y;
	char* fname = NULL;
	int argchar;
	FILE* fid;
	qfits_header* hdr;
	char* valstr;
	int BLOCK = 100000;
	catalog* cat;
	startree* skdt;
	an_catalog* ancat;
	usnob_fits* usnob;
	tycho2_fits* tycho;
	dl* rdls;
	il* fields;
	int backside = 0;
	double px, py;
	int N=3000;
	unsigned char* img;

	int fieldslow = -1;
	int fieldshigh = -1;
	int notycho = 1;
	int imgmax = 255;

	fields = il_new(32);

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 's':
			imgmax = 65535;
			break;
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
			il_append(fields, atoi(optarg));
			break;
		case 'L':
			fieldslow = atoi(optarg);
			break;
		case 'H':
			fieldshigh = atoi(optarg);
			break;
		case 't':
			notycho = 0;
			break;
		default:
			return (OPT_ERR);
		}

	if (optind == argc) {
		printHelp(progname);
		exit(-1);
	}

	if (((fieldslow == -1) && (fieldshigh != -1)) ||
		((fieldslow != -1) && (fieldshigh == -1)) ||
		(fieldslow > fieldshigh)) {
		printHelp(progname);
		fprintf(stderr, "If you specify -L you must also specify -H.\n");
		exit(-1);
	}
	if (fieldslow != -1) {
		int f;
		for (f=fieldslow; f<=fieldshigh; f++)
			il_append(fields, f);
	}

	projection=calloc(sizeof(double), N*N);

	for (; optind<argc; optind++) {
		int i;
		char* key;
		cat = NULL;
		skdt = NULL;
		ancat = NULL;
		usnob = NULL;
		tycho = NULL;
		rdls = NULL;
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
		valstr = qfits_pretty_string(qfits_header_getstr(hdr, "AN_FILE"));
		if (valstr) {
			fprintf(stderr, "Astrometry.net file type: \"%s\".\n", valstr);
			if (strncasecmp(valstr, AN_FILETYPE_CATALOG, strlen(AN_FILETYPE_CATALOG)) == 0) {
				fprintf(stderr, "Looks like a catalog.\n");
				cat = catalog_open(fname, 0);
				if (!cat) {
					fprintf(stderr, "Couldn't open catalog.\n");
					return 1;
				}
				numstars = cat->numstars;
			} else if (strncasecmp(valstr, AN_FILETYPE_STARTREE, strlen(AN_FILETYPE_STARTREE)) == 0) {
				fprintf(stderr, "Looks like a star kdtree.\n");
				skdt = startree_open(fname);
				if (!skdt) {
					fprintf(stderr, "Couldn't open star kdtree.\n");
					return 1;
				}
				numstars = startree_N(skdt);
			} else if (strncasecmp(valstr, AN_FILETYPE_RDLS, strlen(AN_FILETYPE_RDLS)) == 0) {
				rdlist* rdlsfile;
				int nfields, f;
				fprintf(stderr, "Looks like an rdls (RA,DEC list)\n");
				rdlsfile = rdlist_open(fname);
				if (!rdlsfile) {
					fprintf(stderr, "Couldn't open RDLS file.\n");
					return 1;
				}
				rdls = dl_new(256);
				nfields = il_size(fields);
				if (!nfields) {
					nfields = rdlsfile->nfields;
					fprintf(stderr, "Plotting all %i fields.\n", nfields);
				}
				for (f=0; f<nfields; f++) {
					int fld;
					dl* rd;
					int j, M;
					if (il_size(fields))
						fld = il_get(fields, f);
					else
						fld = f;
					rd = rdlist_get_field(rdlsfile, fld);
					if (!rd) {
						fprintf(stderr, "RDLS file does not contain field %i.\n", fld);
						return 1;
					}
					fprintf(stderr, "Field %i has %i entries.\n", fld, dl_size(rd)/2);
					M = dl_size(rd);
					for (j=0; j<M; j++)
						dl_append(rdls, dl_get(rd, j));
					dl_free(rd);
				}
				rdlist_close(rdlsfile);
				numstars = dl_size(rdls)/2;
			} else {
				fprintf(stderr, "Unknown Astrometry.net file type: \"%s\".\n", valstr);
				exit(-1);
			}
		}
		// "AN_CATALOG" gets truncated...
		key = qfits_header_findmatch(hdr, "AN_CAT");
		if (key) {
			if (qfits_header_getboolean(hdr, key, 0)) {
				fprintf(stderr, "File has AN_CATALOG = T header.\n");
				ancat = an_catalog_open(fname);
				if (!ancat) {
					fprintf(stderr, "Couldn't open catalog.\n");
					exit(-1);
				}
				numstars = ancat->nentries;
				ancat->br.blocksize = BLOCK;
			}
		}
		if (qfits_header_getboolean(hdr, "USNOB", 0)) {
			fprintf(stderr, "File has USNOB = T header.\n");
			usnob = usnob_fits_open(fname);
			if (!usnob) {
				fprintf(stderr, "Couldn't open catalog.\n");
				exit(-1);
			}
			numstars = usnob->nentries;
			usnob->br.blocksize = BLOCK;
		} else if (qfits_header_getboolean(hdr, "TYCHO_2", 0)) {
			fprintf(stderr, "File has TYCHO_2 = T header.\n");
			tycho = tycho2_fits_open(fname);
			if (!tycho) {
				fprintf(stderr, "Couldn't open catalog.\n");
				exit(-1);
			}
			numstars = tycho->nentries;
			tycho->br.blocksize = BLOCK;
		}
		qfits_header_destroy(hdr);
		if (!(cat || skdt || ancat || usnob || tycho || rdls)) {
			fprintf(stderr, "I can't figure out what kind of file %s is.\n", fname);
			exit(-1);
		}

		fprintf(stderr, "Reading %i stars...\n", numstars);

		for (i=0; i<numstars; i++) {
			if (is_power_of_two(i+1)) {
				if (backside) {
					fprintf(stderr, "%i stars project onto the opposite hemisphere.\n", backside);
				}
				fprintf(stderr,"  done %u/%u stars\r",i+1,numstars);
			}

			if (cat) {
				double* xyz;
				xyz = catalog_get_star(cat, i);
				x = xyz[0];
				y = xyz[1];
				z = xyz[2];
			} else if (skdt) {
				double xyz[3];
				if (startree_get(skdt, i, xyz)) {
					fprintf(stderr, "Failed to read star %i from star kdtree.\n", i);
					exit(-1);
				}
				x = xyz[0];
				y = xyz[1];
				z = xyz[2];
			} else if (rdls) {
				double ra, dec;
				ra  = dl_get(rdls, 2*i);
				dec = dl_get(rdls, 2*i+1);
				x = radec2x(deg2rad(ra), deg2rad(dec));
				y = radec2y(deg2rad(ra), deg2rad(dec));
				z = radec2z(deg2rad(ra), deg2rad(dec));
			} else if (ancat) {
				an_entry* entry = an_catalog_read_entry(ancat);
				x = radec2x(deg2rad(entry->ra), deg2rad(entry->dec));
				y = radec2y(deg2rad(entry->ra), deg2rad(entry->dec));
				z = radec2z(deg2rad(entry->ra), deg2rad(entry->dec));
			} else if (usnob) {
				usnob_entry* entry = usnob_fits_read_entry(usnob);
				if (notycho && (entry->ndetections == 0))
					continue;
				x = radec2x(deg2rad(entry->ra), deg2rad(entry->dec));
				y = radec2y(deg2rad(entry->ra), deg2rad(entry->dec));
				z = radec2z(deg2rad(entry->ra), deg2rad(entry->dec));
			} else if (tycho) {
				tycho2_entry* entry = tycho2_fits_read_entry(tycho);
				x = radec2x(deg2rad(entry->RA), deg2rad(entry->DEC));
				y = radec2y(deg2rad(entry->RA), deg2rad(entry->DEC));
				z = radec2z(deg2rad(entry->RA), deg2rad(entry->DEC));
			}

			if (!hammer) {
				if ((z <= 0 && !reverse) || (z >= 0 && reverse)) {
					backside++;
					continue;
				}
				if (reverse)
					z = -z;
				project_equal_area(x, y, z, &px, &py);
			} else {
				/* Hammer-Aitoff projection */
				project_hammer_aitoff_x(x, y, z, &px, &py);
			}
			getxy(px, py, N, &X, &Y);
			projection[X+N*Y]++;
		}

		if (cat)
			catalog_close(cat);
		if (skdt)
			startree_close(skdt);
		if (rdls)
			dl_free(rdls);
		if (ancat)
			an_catalog_close(ancat);
		if (usnob)
			usnob_fits_close(usnob);
		if (tycho)
			tycho2_fits_close(tycho);
	}

	maxval = 0;
	for (ii = 0; ii < (N*N); ii++)
		if (projection[ii] > maxval)
			maxval = projection[ii];

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
					project_equal_area(x, y, z, &px, &py);
				} else {
					/* Hammer-Aitoff projection */
					project_hammer_aitoff_x(x, y, z, &px, &py);
				}
				getxy(px, py, N, &X, &Y);
				projection[X+N*Y] = maxval;
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
					project_equal_area(x, y, z, &px, &py);
				} else {
					/* Hammer-Aitoff projection */
					project_hammer_aitoff_x(x, y, z, &px, &py);
				}
				getxy(px, py, N, &X, &Y);
				projection[X+N*Y] = maxval;
			}
		}
	}

	// Output PGM format
	printf("P5 %d %d %d\n",N, N, imgmax);
	// hack - we reuse the "projection" storage to store the image data:
	if (imgmax == 255) {
		img = (unsigned char*)projection;
		for (ii=0; ii<(N*N); ii++)
			img[ii] = (int)((double)imgmax * projection[ii] / (double)maxval);
		if (fwrite(img, 1, N*N, stdout) != (N*N)) {
			fprintf(stderr, "Failed to write image: %s\n", strerror(errno));
			exit(-1);
		}
	} else {
		uint16_t* img = (uint16_t*)projection;
		for (ii=0; ii<(N*N); ii++) {
			img[ii] = (int)((double)imgmax * projection[ii] / (double)maxval);
			img[ii] = htons(img[ii]);
		}
		if (fwrite(img, 2, N*N, stdout) != (N*N)) {
			fprintf(stderr, "Failed to write image: %s\n", strerror(errno));
			exit(-1);
		}
	}
	free(projection);

	il_free(fields);

	return 0;
}
