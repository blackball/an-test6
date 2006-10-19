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

#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "xylist.h"
#include "fitsioutils.h"
#include "qfits.h"
#include "label.h"

char* OPTIONS = "hi:X:Y:o:d:t:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options]\n"
			"   -i <input-xylist-filename>\n"
			"     [-X <x-column-name>]\n"
			"     [-Y <y-column-name>]\n"
			"   -o <output-xylist-filename>\n"
			"   -d <donut-distance-pixels>\n"
			"   -t <donut-threshold>\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	char* infname = NULL;
	char* outfname = NULL;
	char* xcol = NULL;
	char* ycol = NULL;
	xylist* xyls = NULL;
	xylist* xylsout = NULL;
	int i, N;
	double* fieldxy = NULL;
	double donut_dist = 5.0;
	double donut_thresh = 0.25;
	char val[64];

	//int* hough = NULL;
	unsigned char* hough = NULL;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'd':
			donut_dist = atof(optarg);
			break;
		case 't':
			donut_thresh = atof(optarg);
			break;
		case 'X':
			xcol = optarg;
			break;
		case 'Y':
			ycol = optarg;
			break;
		case 'i':
			infname = optarg;
			break;
		case 'o':
			outfname = optarg;
			break;
		default:
		case 'h':
			printHelp(progname);
			exit(0);
		}
	}
	if (!infname || !outfname) {
		printHelp(progname);
		exit(-1);
	}

	xyls = xylist_open(infname);
	if (!xyls) {
		fprintf(stderr, "Failed to read xylist from file %s.\n", infname);
		exit(-1);
	}
	if (xcol)
		xyls->xname = xcol;
	if (ycol)
		xyls->yname = ycol;

	xylsout = xylist_open_for_writing(outfname);
	if (!xylsout) {
		fprintf(stderr, "Failed to open xylist file %s for writing.\n", outfname);
		exit(-1);
	}
	if (xcol)
		xylsout->xname = xcol;
	if (ycol)
		xylsout->yname = ycol;

	qfits_header_add(xylsout->header, "HOMERED", "T", "This file has been homered (donuts removed)", NULL);
	sprintf(val, "%f", donut_dist);
	qfits_header_add(xylsout->header, "HMR_DST", val, "Donut radius (pixels)", NULL);
	sprintf(val, "%f", donut_thresh);
	qfits_header_add(xylsout->header, "HMR_THR", val, "Donut threshold (fraction)", NULL);
	qfits_header_add(xylsout->header, "HISTORY", "homer command line:", NULL, NULL);
	fits_add_args(xylsout->header, argv, argc);
	qfits_header_add(xylsout->header, "HISTORY", "(end of homer command line)", NULL, NULL);
	qfits_header_add(xylsout->header, "HISTORY", "** homer: history from input xyls:", NULL, NULL);
	fits_copy_all_headers(xyls->header, xylsout->header, "HISTORY");
	qfits_header_add(xylsout->header, "HISTORY", "** homer end of history from input xyls.", NULL, NULL);
	qfits_header_add(xylsout->header, "COMMENT", "** homer: comments from input xyls:", NULL, NULL);
	fits_copy_all_headers(xyls->header, xylsout->header, "COMMENT");
	qfits_header_add(xylsout->header, "COMMENT", "** homer: end of comments from input xyls.", NULL, NULL);

	if (xylist_write_header(xylsout)) {
		fprintf(stderr, "Failed to write xyls header.\n");
		exit(-1);
	}

	N = xyls->nfields;
	for (i=0; i<N; i++) {
		int nf;
		int NF;
		int j;
		int maxx, maxy;
		label_info* labels;
		int Nlabels;
		int* labelimg;

		//printf("Field %i.\n", i);
		NF = xylist_n_entries(xyls, i);
		fieldxy = realloc(fieldxy, NF * 2 * sizeof(double));
		xylist_read_entries(xyls, i, 0, NF, fieldxy);
		nf = NF;

		maxx = 0;
		maxy = 0;
		for (j=0; j<NF; j++) {
			assert(fieldxy[j*2] >= 0);
			assert(fieldxy[j*2+1] >= 0);
			if (fieldxy[j*2] > maxx)
				maxx = fieldxy[j*2];
			if (fieldxy[j*2+1] > maxy)
				maxy = fieldxy[j*2+1];
		}

		//hough = realloc(hough, maxx * maxy * sizeof(int));
		//memset(hough, 0, maxx * maxy * sizeof(int));

		hough = realloc(hough, maxx * maxy);
		memset(hough, 0, maxx * maxy);

		for (j=0; j<NF; j++) {
			int x, y;
			int xc, yc;
			int xlo, xhi, ylo, yhi;
			xc = fieldxy[j*2];
			yc = fieldxy[j*2+1];
			xlo = xc - donut_dist;
			xhi = xc + donut_dist;
			ylo = yc - donut_dist;
			yhi = yc + donut_dist;
			if (xlo < 0) xlo = 0;
			if (ylo < 0) xlo = 0;
			if (xhi >= maxx) xhi = maxx - 1;
			if (yhi >= maxy) yhi = maxy - 1;
			for (y=ylo; y<=yhi; y++) {
				for (x=xlo; x<=xhi; x++) {
					if (((x-xc)*(x-xc) + (y-yc)*(y-yc)) > donut_dist*donut_dist)
						continue;
					hough[y*maxx + x]++;
					if (hough[y*maxx + x] == 0)
						hough[y*maxx + x] = 255;
				}
			}
		}

		{
			int houghmax;
			FILE* fid;
			houghmax = 0;
			for (j=0; j<(maxx*maxy); j++)
				if (hough[j] > houghmax) houghmax = hough[j];
			fid = fopen("hough.pgm", "wb");
			fprintf(fid, "P5 %d %d %d\n", maxx, maxy, 255);
			for (j=0; j<(maxx*maxy); j++)
				fprintf(fid, "%c", (unsigned char)(hough[j] * 255 / houghmax));
			fclose(fid);
		}

		labelimg = malloc(maxx * maxy * sizeof(int));
		labels = label_image_super_2(hough, maxx, maxy, labelimg, &Nlabels);
		printf("%i labels.\n", Nlabels);

		{
			FILE* fid;
			fid = fopen("label.pgm", "wb");
			fprintf(fid, "P5 %d %d %d\n", maxx, maxy, 255);
			for (j=0; j<(maxx*maxy); j++)
				fprintf(fid, "%c", (unsigned char)(labelimg[j] % 256));
			fclose(fid);
		}

		//printf("Labels: ");
		for (j=0; j<Nlabels; j++)
			//printf("%i ", labels[j].label);
			assert(labels[j].label == j+1);
		//printf("\n");

		for (j=0; j<(maxx*maxy); j++)
			if (labelimg[j])
				assert(labels[labelimg[j]-1].value == hough[j]);

		for (;;) {
			int maxval;
			label_info* lab;
			int ilab;
			unsigned char* dilate;
			int* nearbylabel;
			int x, y;
			int ndilated;
			int nnearby;

			/*
			  printf("Label values: ");
			  for (j=0; j<Nlabels; j++)
			  printf("%i ", labels[j].value);
			  printf("\n");
			*/

			maxval = 0;
			lab = NULL;
			ilab = -1;
			for (j=0; j<Nlabels; j++) {
				if (labels[j].value > maxval) {
					maxval = labels[j].value;
					lab = labels + j;
					ilab = j;
				}
			}
			if (maxval == 0)
				break;

			printf("Greatest val: %i\n", maxval);
			printf("Center %g,%g\n", lab->xcenter, lab->ycenter);

			{
				static int step=1;
				char fn[256];
				FILE* fid;
				sprintf(fn, "step%03i.pgm", step++);
				fid = fopen(fn, "wb");
				fprintf(fid, "P5 %d %d %d\n", maxx, maxy, 255);
				for (j=0; j<(maxx*maxy); j++)
					fprintf(fid, "%c", (unsigned char)
							(labelimg[j] ? (labels[labelimg[j]-1].value * 255 / maxval) : 0));
				fclose(fid);
			}

			dilate = calloc(maxx * maxy, 1);
			for (j=0; j<(maxx*maxy); j++)
				if (labelimg[j] == lab->label)
					dilate[j] = 1;

			for (;;) {
				/*
				  static int step=1;
				  char fn[256];
				  FILE* fid;
				  sprintf(fn, "step%03i.pgm", step++);
				  fid = fopen(fn, "wb");
				  fprintf(fid, "P5 %d %d %d\n", maxx, maxy, 255);
				  for (j=0; j<(maxx*maxy); j++)
				  fprintf(fid, "%c", (unsigned char)(dilate[j] ? 255:0));
				  fclose(fid);
				*/

				for (y=0; y<maxy; y++)
					for (x=0; x<maxx; x++) {
						int ind = y*maxx + x;
						if (dilate[ind] != 1)
							continue;
						if ((x > 0) &&
							!dilate[ind-1] &&
							hough[ind-1] &&
							(hough[ind] >= hough[ind-1])) dilate[ind-1] = 2;
						if ((x < maxx-1) &&
							!dilate[ind+1] &&
							hough[ind+1] &&
							(hough[ind] >= hough[ind+1])) dilate[ind+1] = 2;
						if ((y > 0) &&
							!dilate[ind-maxx] &&
							hough[ind-maxx] &&
							(hough[ind] >= hough[ind-maxx])) dilate[ind-maxx] = 2;
						if ((y < maxy-1) &&
							!dilate[ind+maxx] &&
							hough[ind+maxx] &&
							(hough[ind] >= hough[ind+maxx])) dilate[ind+maxx] = 2;
					}

				ndilated = 0;
				for (j=0; j<(maxx*maxy); j++)
					if (dilate[j] == 2)
						ndilated++;

				printf("N dilated: %i\n", ndilated);

				if (!ndilated)
					break;

				for (j=0; j<(maxx*maxy); j++)
					if (dilate[j] == 2)
						dilate[j] = 1;
			}

			/*
			  {
			  static int step=1;
			  char fn[256];
			  FILE* fid;
			  sprintf(fn, "step%03i.pgm", step++);
			  fid = fopen(fn, "wb");
			  fprintf(fid, "P5 %d %d %d\n", maxx, maxy, 255);
			  for (j=0; j<(maxx*maxy); j++)
			  fprintf(fid, "%c", (unsigned char)(dilate[j] ? 255:0));
			  fclose(fid);
			  }
			*/

			nearbylabel = calloc(Nlabels, sizeof(int));
			for (j=0; j<(maxx*maxy); j++)
				if (dilate[j])
					nearbylabel[labelimg[j]] = 1;
			nnearby = 0;
			for (j=0; j<Nlabels; j++)
				if (nearbylabel[j])
					nnearby++;
			printf("N nearby: %i\n", nnearby);

			printf("Neighbours: ");
			for (j=0; j<Nlabels; j++)
				if (nearbylabel[j])
					printf("%i ", j);
			printf("\n");

			free(dilate);

			for (j=0; j<Nlabels; j++)
				if (nearbylabel[j])
					labels[j].value = 0;

			assert(nearbylabel[ilab]);

			free(nearbylabel);
		}


		free(labelimg);

		if (xylist_write_new_field(xylsout) ||
			xylist_write_entries(xylsout, fieldxy, nf) ||
			xylist_fix_field(xylsout)) {
			fprintf(stderr, "Failed to write xyls field.\n");
			exit(-1);
		}
		fflush(NULL);
	}

		free(hough);

	free(fieldxy);

	if (xylist_fix_header(xylsout) ||
		xylist_close(xylsout)) {
		fprintf(stderr, "Failed to close xyls.\n");
		exit(-1);
	}

	xylist_close(xyls);

	return 0;
}

