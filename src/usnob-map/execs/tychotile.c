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
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>

#include "starutil.h"
#include "kdtree.h"
#include "healpix.h"
#include "mathutil.h"
#include "merctree.h"
#include "keywords.h"

#define max(a, b)  ((a)>(b)?(a):(b))
#define min(a, b)  ((a)<(b)?(a):(b))

#define OPTIONS "x:y:X:Y:w:h:g:asc:"

extern char *optarg;
extern int optind, opterr, optopt;

static void node_contained(kdtree_t* kd, int node, void* extra);
static void node_overlaps(kdtree_t* kd, int node, void* extra);
static void leaf_node(kdtree_t* kd, int node);
static void expand_node(kdtree_t* kd, int node);
static void addstar(double xp, double yp, merc_flux* flux);

/*
char* merc_file     = "/home/gmaps/usnob-images/tycho.mkdt.fits";
*/
char* map_template  = "/home/gmaps/usnob-images/tycho-maps/tycho-zoom%i_%02i_%02i.png";
  char* merc_file     = "/h/260/dstn/local/tycho-maps/tycho.mkdt.fits";

float* fluximg; // RGB
float xscale, yscale;
int Noob;
int Nib;
int w=0, h=0;

// The lower-left corner of the image.
double xorigin, yorigin;
// The merctree.
merctree* merc;

bool arith = FALSE;
bool arc = FALSE;
double colorcor = 1.44;

static int mercx2pix(double xp) {
	return (int)floor(xscale * (xp - xorigin));
}
static int mercy2pix(double yp) {
	return (h-1) - (int)floor(yscale * (yp - yorigin));
}

int main(int argc, char *argv[]) {
    int argchar;
	bool gotx, goty, gotX, gotY, gotw, goth;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int i;
	double px0, py0, px1, py1;
	double pixperx, pixpery;
	double xzoom, yzoom;
	int zoomlevel;
	int xpix0, xpix1, ypix0, ypix1;
	double gain = 0.0;

	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'c':
			colorcor = atof(optarg);
			break;
		case 's':
			arc = TRUE;
			break;
		case 'a':
			arith = TRUE;
			break;
		case 'x':
			x0 = atof(optarg);
			gotx = TRUE;
			break;
		case 'X':
			x1 = atof(optarg);
			goty = TRUE;
			break;
		case 'y':
			y0 = atof(optarg);
			gotX = TRUE;
			break;
		case 'Y':
			y1 = atof(optarg);
			gotY = TRUE;
			break;
		case 'w':
			w = atoi(optarg);
			gotw = TRUE;
			break;
		case 'h':
			h = atoi(optarg);
			goth = TRUE;
			break;
		case 'g':
			gain = atof(optarg);
			break;
		}

	if (!(gotx && goty && gotX && gotY && gotw && goth)) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	/*
	  fprintf(stderr, "X range: [%g, %g] degrees\n", x0, x1);
	  fprintf(stderr, "Y range: [%g, %g] degrees\n", y0, y1);
	*/

	x0 = deg2rad(x0);
	x1 = deg2rad(x1);
	y0 = deg2rad(y0);
	y1 = deg2rad(y1);

	// Mercator projected position
	px0 = x0 / (2.0 * M_PI);
	px1 = x1 / (2.0 * M_PI);
	py0 = (M_PI + asinh(tan(y0))) / (2.0 * M_PI);
	py1 = (M_PI + asinh(tan(y1))) / (2.0 * M_PI);
	if (px1 < px0) {
		fprintf(stderr, "Error, px1 < px0 (%g < %g)\n", px1, px0);
		exit(-1);
	}
	if (py1 < py0) {
		fprintf(stderr, "Error, py1 < py0 (%g < %g)\n", py1, py0);
		exit(-1);
	}
	if (py0 < 0.0) {
		//fprintf(stderr, "Tweaking py0 %.10g -> %.10g.\n", py0, 0.0);
		py0 = 0.0;
	}
	if (py1 > 1.0) {
		//fprintf(stderr, "Tweaking py1 %.10g -> %.10g.\n", py1, 1.0);
		py1 = 1.0;
	}

	pixperx = (double)w / (px1 - px0);
	pixpery = (double)h / (py1 - py0);

	/*
	  fprintf(stderr, "Projected X range: [%g, %g]\n", px0, px1);
	  fprintf(stderr, "Projected Y range: [%g, %g]\n", py0, py1);
	  fprintf(stderr, "X,Y pixel scale: %g, %g.\n", pixperx, pixpery);
	*/
	xzoom = pixperx / 256.0;
	yzoom = pixpery / 256.0;
	//fprintf(stderr, "X,Y zoom %g, %g\n", xzoom, yzoom);
	{
		double fxzoom;
		double fyzoom;
		fxzoom = log(xzoom) / log(2.0);
		fyzoom = log(yzoom) / log(2.0);
		//fprintf(stderr, "fzoom %g, %g\n", fxzoom, fyzoom);
	}
	zoomlevel = (int)rint(log(xzoom) / log(2.0));
	fprintf(stderr, "Zoom level %i.\n", zoomlevel);

	if (px0 < 0.0) {
		px0 += 1.0;
		px1 += 1.0;
		//fprintf(stderr, "Wrapping X around to projected range [%g, %g]\n", px0, px1);
	}

	xpix0 = px0 * pixperx;
	ypix0 = py0 * pixpery;
	xpix1 = xpix0 + w;
	ypix1 = ypix0 + h;

	//fprintf(stderr, "Pixel positions: (%i,%i), (%i,%i) vs (%i,%i)\n", xpix0, ypix0, xpix0+w, ypix0+h, xpix1, ypix1);

	/*
	  if (!forcetree && (zoomlevel <= 0)) {
	  char fn[256];
	  char buff[1024];
	  FILE *fp;

	  sprintf(fn, map_template, zoomlevel, (ypix0+1)/256, (xpix0+1)/256);
	  fprintf(stderr, "Loading image from file %s.\n", fn);
	  fp = fopen(fn, "r");
	  // write file to stdout
	  for (;;) {
	  int nr = fread(buff, 1, sizeof(buff), fp);
	  if (!nr) {
	  if (feof(fp))
	  break;
	  if (ferror(fp)) {
	  fprintf(stderr, "Error reading from file %s: %s\n", fn, strerror(errno));
	  break;
	  }
	  }
	  if (fwrite(buff, 1, nr, stdout) != nr) {
	  fprintf(stderr, "Error writing: %s\n", strerror(errno));
	  break;
	  }
	  }
	  return 0;
	  }
	*/
	{
		double querylow[2], queryhigh[2];
		double wraplow[2], wraphigh[2];
		bool wrapra;
		double xmargin, ymargin;

		// Magic number 2: number of pixels around a star.
		xmargin = 2.0 / pixperx;
		ymargin = 2.0 / pixpery;
		fprintf(stderr, "margins: %g, %g\n", xmargin, ymargin);

		wrapra = (px1 > 1.0);
		if (wrapra) {
			querylow [0] = px0 - xmargin;
			queryhigh[0] = 1.0;
			wraplow  [0] = 0.0;
			wraphigh [0] = px1 - 1.0 + xmargin;
			if (wraplow [0] < 0.0) wraplow [0] = 0.0;
			if (wraphigh[0] > 1.0) wraphigh[0] = 1.0;
		} else {
			querylow [0] = px0 - xmargin;
			queryhigh[0] = px1 + xmargin;
		}
		querylow [1] = wraplow [1] = py0 - ymargin;
		queryhigh[1] = wraphigh[1] = py1 + ymargin;
		if (querylow [0] < 0.0) querylow [0] = 0.0;
		if (querylow [1] < 0.0) querylow [1] = 0.0;
		if (queryhigh[0] > 1.0) queryhigh[0] = 1.0;
		if (queryhigh[1] > 1.0) queryhigh[1] = 1.0;
		if (wraplow [1] < 0.0) wraplow [1] = 0.0;
		if (wraphigh[1] > 1.0) wraphigh[1] = 1.0;

		fluximg = calloc(w*h*3, sizeof(float));
		if (!fluximg) {
			fprintf(stderr, "Failed to allocate flux image.\n");
			exit(-1);
		}

		xscale = pixperx;
		yscale = pixpery;
		Noob = 0;
		Nib = 0;

		/*
		  fprintf(stderr, "Query: x:[%g,%g] y:[%g,%g]\n",
		  querylow[0], queryhigh[0], querylow[1], queryhigh[1]);
		  if (wrapra)
		  fprintf(stderr, "Query': x:[%g,%g] y:[%g,%g]\n",
		  wraplow[0], wraphigh[0], wraplow[1], wraphigh[1]);
		*/

		merc = merctree_open(merc_file);
		if (!merc) {
			fprintf(stderr, "Failed to open merctree %s\n", merc_file);
			exit(-1);
		}

		xorigin = px0;
		yorigin = py0;
		kdtree_nodes_contained(merc->tree, querylow, queryhigh,
							   node_contained, node_overlaps, NULL);
	
		if (wrapra) {
			xorigin = px0 - 1.0;
			kdtree_nodes_contained(merc->tree, wraplow, wraphigh,
								   node_contained, node_overlaps, NULL);
		}
		merctree_close(merc);


		fprintf(stderr, "%i points outside image bounds.\n", Noob);
		fprintf(stderr, "%i points inside image bounds.\n", Nib);

		{
			float amp = 0.0;

			amp = pow(4.0, min(5, zoomlevel)) * 32.0 * exp(gain * log(4.0));

			/*
			  for (i=0; i<(3*w*h); i++)
			  fluximg[i] = pow(fluximg[i] * amp, 0.25) * 255.0;
			*/

			printf("P6 %d %d %d\n", w, h, 255);
			for (i=0; i<(w*h); i++) {
				unsigned char pix[3];
				double r, g, b, I, f, R, G, B, maxRGB;
				/*
				  pix[0] = min(255, fluximg[3*i + 0]);
				  pix[1] = min(255, fluximg[3*i + 1]);
				  pix[2] = min(255, fluximg[3*i + 2]);
				*/
				r = fluximg[3*i+0];
				g = fluximg[3*i+1] * sqrt(colorcor); //1.2;
				b = fluximg[3*i+2] * colorcor; //1.44;
				I = (r + g + b) / 3;
				if (I == 0.0) {
					R = G = B = 0.0;
				} else {
					if (arc) {
						f = asinh(I * amp);
					} else {
						f = pow(I * amp, 0.25);
					}
					R = f*r/I;
					G = f*g/I;
					B = f*b/I;
					maxRGB = max(R, max(G, B));
					if (maxRGB > 1.0) {
						R /= maxRGB;
						G /= maxRGB;
						B /= maxRGB;
					}
				}

				pix[0] = min(255, 255.0*R);
				pix[1] = min(255, 255.0*G);
				pix[2] = min(255, 255.0*B);

				fwrite(pix, 1, 3, stdout);
			}
		}

		free(fluximg);
		return 0;
	}
}

// Non-leaf nodes that are fully contained within the query rectangle.
static void node_contained(kdtree_t* kd, int node, void* extra) {
	expand_node(kd, node);
}

// Leaf nodes that overlap the query rectangle.
static void node_overlaps(kdtree_t* kd, int node, void* extra) {
	leaf_node(kd, node);
}

static void expand_node(kdtree_t* kd, int node) {
	int xp0, xp1, yp0, yp1;
	int D = 2;
	double bblo[D], bbhi[D];

	if (KD_IS_LEAF(kd, node)) {
		leaf_node(kd, node);
		return;
	}

	// check if this whole box fits inside a pixel.
	if (!kdtree_get_bboxes(kd, node, bblo, bbhi)) {
		fprintf(stderr, "Error, node %i does not have bounding boxes.\n", node);
		exit(-1);
	}
	xp0 = mercx2pix(bblo[0]);
	xp1 = mercx2pix(bbhi[0]);
	if (xp1 == xp0) {
		yp0 = mercy2pix(bblo[1]);
		yp1 = mercy2pix(bbhi[1]);
		if (yp1 == yp0) {
			// This node fits inside a single pixel of the output image.
			addstar(bblo[0], bblo[1], &(merc->stats[node].flux));
			return;
		}
	}

	expand_node(kd,  KD_CHILD_LEFT(node));
	expand_node(kd, KD_CHILD_RIGHT(node));
}

static void leaf_node(kdtree_t* kd, int node) {
	int k;
	int L, R;

	L = kdtree_left(kd, node);
	R = kdtree_right(kd, node);

	for (k=L; k<=R; k++) {
		double pt[2];
		merc_flux* flux;

		kdtree_copy_data_double(kd, k, 1, pt);
		flux = merc->flux + k;
		addstar(pt[0], pt[1], flux);
	}
}

static void addstar(double xp, double yp,
					merc_flux* flux) {
	/*
	  int dx[] = { -1,  0,  1,  0,  0 };
	  int dy[] = {  0,  0,  0, -1,  1 };
	  float scale[] = { 1, 1, 1, 1, 1 };
	  int mindx = -1;
	  int maxdx = 1;
	  int mindy = -1;
	  int maxdy = 1;
	*/
	/*
	  int dx[] = { 0 };
	  int dy[] = { 0 };
	  float scale[] = { 1 };
	  int mindx = 0;
	  int maxdx = 0;
	  int mindy = 0;
	  int maxdy = 0;
	*/

	/*
	  x=[-2:2];
	  y=[-2:2];
	  xx=repmat(x, [5,1]);
	  yy=repmat(y', [1,5]);
	  E=exp(-(xx.^2 + yy.^2)/(2 * 0.5));
	  E./sum(sum(E))
	*/
	int dx[] = { -2, -1,  0,  1,  2, 
				 -2, -1,  0,  1,  2, 
				 -2, -1,  0,  1,  2, 
				 -2, -1,  0,  1,  2, 
				 -2, -1,  0,  1,  2 };
	int dy[] = {  -2, -2, -2, -2, -2,
				  -1, -1, -1, -1, -1,
				  0,  0,  0,  0,  0,
				  1,  1,  1,  1,  1,
				  2,  2,  2,  2,  2 };
	float scale[] = {
		0.00010678874539,  0.00214490928858,   0.00583046794284,  0.00214490928858,  0.00010678874539,
		0.00214490928858,  0.04308165471265,  0.11710807914534,  0.04308165471265,  0.00214490928858,
		0.00583046794284,  0.11710807914534,  0.31833276350651,  0.11710807914534,  0.00583046794284,
		0.00214490928858,  0.04308165471265,  0.11710807914534,  0.04308165471265,  0.00214490928858,
		0.00010678874539,  0.00214490928858,  0.00583046794284,  0.00214490928858,  0.00010678874539 };
	int mindx = -2;
	int maxdx = 2;
	int mindy = -2;
	int maxdy = 2;
	int i;
	int x, y;

	x = mercx2pix(xp);
	if (x+maxdx < 0 || x+mindx >= w) {
		Noob++;
		return;
	}
	y = mercy2pix(yp);
	if (y+maxdy < 0 || y+mindy >= h) {
		Noob++;
		return;
	}
	Nib++;

	for (i=0; i<sizeof(dx)/sizeof(int); i++) {
		int ix, iy;
		float r, g, b;
		ix = x + dx[i];
		if ((ix < 0) || (ix >= w)) continue;
		iy = y + dy[i];
		if ((iy < 0) || (iy >= h)) continue;
		r = flux->rflux;
		b = flux->bflux;
		if (arith)
			g = (r + b) / 2.0;
		else
			g = sqrt(r * b);
		fluximg[3*(iy*w + ix) + 0] += r * scale[i];
		fluximg[3*(iy*w + ix) + 1] += g * scale[i];
		fluximg[3*(iy*w + ix) + 2] += b * scale[i];
	}
}

