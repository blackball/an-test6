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

#include "boilerplate.h"
#include "bl.h"
#include "gd.h"

#define OPTIONS "hW:H:w:"

static void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s [options] <quads>  > output.png\n"
		   "  -W <width>        Width of output image.\n"
		   "  -H <height>       Height of output image.\n"
		   "  [-w <width>]      Width of lines to draw (default: 5).\n"
		   "  <x1> <y1> <x2> <y2> <x3> <y3> <x4> <y4> [...]\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *args[]) {
	int argchar;
	char* progname = args[0];
	int W = 0, H = 0;
	int lw = 5;
	int nquads;
	int i;
	dl* coords;

	gdImagePtr img;
	int imblack;
	int imwhite;

	//gdImagePtr brush;
	//int brblack, brwhite;

	gdPointPtr pts;

	coords = dl_new(16);

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
		case 'W':
			W = atoi(optarg);
			break;
		case 'H':
			H = atoi(optarg);
			break;
		case 'w':
			lw = atoi(optarg);
			break;
		case 'h':
		case '?':
		default:
			printHelp(progname);
			exit(-1);
		}

	if ((argc - optind) % 8) {
		printHelp(progname);
		exit(-1);
	}

	if (!(W && H)) {
		printHelp(progname);
		exit(-1);
	}

	for (i=optind; i<argc; i++) {
		double pos = atof(args[i]);
		dl_append(coords, pos);
	}
	nquads = dl_size(coords) / 8;

	/*
	  img = gdImageCreate(W, H);	
	  imblack = gdImageColorAllocate(img, 0,0,0);
	  imwhite = gdImageColorAllocate(img, 255,255,255); 
	*/

	img = gdImageCreateTrueColor(W, H);
	imblack = gdImageColorAllocateAlpha(img, 0,0,0,127);
	//imblack = gdImageColorAllocateAlpha(img, 0,0,0,0);
	imwhite = gdImageColorAllocateAlpha(img, 255,255,255,0); 

	//img = gdImageCreate(W, H);
	//imblack = gdImageColorAllocateAlpha(img, 0,0,0,0);
	//imwhite = gdImageColorAllocate(img, 255, 255, 255); 
	//imwhite = gdImageColorAllocate(img, 255, 255, 255); 


	/* Allocate the color black (red, green and blue all minimum).
	   Since this is the first color in a new image, it will
	   be the background color. */
	//imblack = gdImageColorAllocate(img, 0, 0, 0);
	//imblack = gdImageColorAllocateAlpha(img, 255,255,255,0);

	/*
	//brush = gdImageCreate(lw*2+1, lw*2+1);
	brush = gdImageCreateTrueColor(lw*2+1, lw*2+1);
	//brblack = gdImageColorAllocate(brush, 0,0,0);
	//brwhite = gdImageColorAllocate(brush, 255,255,255);
	{
		int c = lw;
		int x, y;
		double twosig2 = 2.0*(lw/2.0)*(lw/2.0);
		// FIXME lots of symmetry to exploit here!
		//for (y=0; y<=lw; y++) {
		//for (x=y; x<=lw; x++) {
		for (y=0; y<=(lw*2); y++) {
			for (x=0; x<=(lw*2); x++) {
				double d2 = ((x-c)*(x-c) + (y-c)*(y-c));
				//int val = (int)(255.0 * exp(-d2 / twosig2));
				//int col = gdImageColorAllocate(brush, val,val,val);
				int val = (int)(127.0 * (1.0 - exp(-d2 / twosig2)));
				int col = gdImageColorAllocateAlpha(brush, 255,255,255, val);
				gdImageSetPixel(brush, x, y, col);
			}
		}
	}
	gdImageSetBrush(img, brush);
	*/

	gdImageSetAntiAliased(img, imwhite);
	gdImageSetThickness(img, lw);

	pts = malloc(8 * sizeof(gdPoint));
	for (i=0; i<nquads; i++) {
		int j;
		for (j=0; j<4; j++) {
			pts[j].x = dl_get(coords, i*8 + j*2);
			pts[j].y = dl_get(coords, i*8 + j*2 + 1);
		}
		gdImagePolygon(img, pts, 4, imwhite);
		//gdImagePolygon(img, pts, 4, gdAntiAliased);
		//gdImagePolygon(img, pts, 4, gdBrushed);
	}

	gdImagePng(img, stdout);

	//gdImageDestroy(brush);

	gdImageDestroy(img);

	return 0;
}
