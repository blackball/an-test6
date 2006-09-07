#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "an_catalog.h"
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_macros.h"
#include "kdtree/kdtree_access.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"
#include "merctree.h"
#include "bl.h"
#include "ppm.h"
#include "pnm.h"

#define OPTIONS "x:y:X:Y:w:h:l:"

extern char *optarg;
extern int optind, opterr, optopt;

//#define max(a,b) (((a)>(b))?(a):(b))

static void get_nodes_in(kdtree_t* kd, kdtree_node_t* node,
						 real* qlo, real* qhi, pl* nodelist) {
	real *bblo, *bbhi;
	bblo = kdtree_get_bb_low (kd, node);
	bbhi = kdtree_get_bb_high(kd, node);
	if (!kdtree_do_boxes_overlap(bblo, bbhi, qlo, qhi, kd->ndim))
		return;
	if (kdtree_is_box_contained(bblo, bbhi, qlo, qhi, kd->ndim)) {
		pl_append(nodelist, node);
		return;
	}
	if (kdtree_node_is_leaf(kd, node)) {
		pl_append(nodelist, node);
	} else {
		get_nodes_in(kd, kdtree_get_child1(kd, node), qlo, qhi, nodelist);
		get_nodes_in(kd, kdtree_get_child2(kd, node), qlo, qhi, nodelist);
	}
}

static void get_nodes_contained_in(kdtree_t* kd, real* querylow,
								   real* queryhigh, pl* nodelist) {
	get_nodes_in(kd, kdtree_get_root(kd), querylow, queryhigh, nodelist);
}

int main(int argc, char *argv[]) {
    int argchar;
	bool gotx, goty, gotX, gotY, gotw, goth;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;
	char* map_template = "/h/42/dstn/local/maps/usnob-zoom%i.ppm";
	char* merc_template = "/h/42/dstn/local/maps/merc/merc_hp%03i.fits";
	char fn[256];
	int i;
	double lines = 0.0;

	double px0, py0, px1, py1;
	double xperpix, yperpix;
	double xzoom, yzoom;
	int zoomlevel;
	int xpix0, xpix1, ypix0, ypix1;

	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'l':
			lines = atof(optarg);
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
		}

	if (!(gotx && goty && gotX && gotY && gotw && goth)) {
		fprintf(stderr, "Invalid inputs.\n");
		exit(-1);
	}

	fprintf(stderr, "X range: [%g, %g] degrees\n", x0, x1);
	fprintf(stderr, "Y range: [%g, %g] degrees\n", y0, y1);

	x0 = deg2rad(x0);
	x1 = deg2rad(x1);
	y0 = deg2rad(y0);
	y1 = deg2rad(y1);

	// Mercator projected position
	px0 = x0 / (2.0 * M_PI);
	px1 = x1 / (2.0 * M_PI);
	py0 = asinh(tan(y0));
	py1 = asinh(tan(y1));
	xperpix = fabs(px1 - px0) / (double)w;
	yperpix = fabs(py1 - py0) / (double)h;
	fprintf(stderr, "Projected X range: [%g, %g]\n", px0, px1);
	fprintf(stderr, "Projected Y range: [%g, %g]\n", py0, py1);
	fprintf(stderr, "X,Y pixel scale: %g, %g.\n", xperpix, yperpix);
	xzoom = 1.0 / (xperpix * 256.0);
	yzoom = 1.0 / (yperpix * 256.0 / (2.0 * M_PI));
	fprintf(stderr, "X,Y zoom %g, %g\n", xzoom, yzoom);
	{
		double fxzoom;
		double fyzoom;
		fxzoom = log(xzoom) / log(2.0);
		fyzoom = log(yzoom) / log(2.0);
		fprintf(stderr, "fzoom %g, %g\n", fxzoom, fyzoom);
	}
	/*
	  if ((fabs(xzoom - rint(xzoom)) > 0.05) ||
	  (fabs(xzoom - yzoom) > 0.05)) {
	  fprintf(stderr, "Invalid zoom level.\n");
	  exit(-1);
	  }
	*/
	zoomlevel = (int)rint(log(xzoom) / log(2.0));
	fprintf(stderr, "Zoom level %i.\n", zoomlevel);

	if (px0 < 0.0) {
		px0 += 1.0;
		px1 += 1.0;
		fprintf(stderr, "Wrapping X around to projected range [%g, %g]\n", px0, px1);
	}

	xpix0 = px0 / xperpix;
	ypix0 = (-py1 + M_PI) / yperpix;
	xpix1 = px1 / xperpix;
	ypix1 = (-py0 + M_PI) / yperpix;

	fprintf(stderr, "Pixel positions: (%i,%i), (%i,%i) vs (%i,%i)\n", xpix0, ypix0, xpix0+w, ypix0+h, xpix1, ypix1);

	xpix1 = xpix0 + w;
	ypix1 = ypix0 + h;

	if (zoomlevel <= 5) {
		char fn[256];
		FILE* fin;
		int rows, cols, format;
		pixval maxval;
		off_t imgstart;
		unsigned char* map;
		size_t mapsize;
		unsigned char* img;
		int y;
		unsigned char* outimg;

		sprintf(fn, map_template, zoomlevel);
		fprintf(stderr, "Loading image from file %s.\n", fn);
		fin = pm_openr_seekable(fn);
		ppm_readppminit(fin, &cols, &rows, &maxval, &format);
		if (PNM_FORMAT_TYPE(format) != PPM_TYPE) {
			fprintf(stderr, "Map file must be PPM.\n");
			exit(-1);
		}
		if (maxval != 255) {
			fprintf(stderr, "Error: PPM maxval %i (not 255).\n", maxval);
			exit(-1);
		}
		imgstart = ftello(fin);
		mapsize = cols * rows * 3;
		map = mmap(NULL, mapsize, PROT_READ, MAP_PRIVATE, fileno(fin), 0);
		if (map == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap image file.\n");
			exit(-1);
		}
		img = map + imgstart;

		if (xpix0 < 0 || ypix0 < 0 || xpix1 > cols || ypix1 > rows) {
			fprintf(stderr, "Requested pixels (%i,%i) to (%i,%i) aren't within image bounds (0,0),(%i,%i)\n",
					xpix0, ypix0, xpix1, ypix1, cols, rows);
			exit(-1);
		}

		/*
		  for (y=ypix0; y<ypix1; y++) {
		  unsigned char* start = img + 3*((y * cols) + xpix0);
		  fwrite(start, 1, w*3, stdout);
		  }
		*/
		outimg = malloc(3 * w * h);
		for (y=ypix0; y<ypix1; y++) {
			unsigned char* start = img + 3*((y * cols) + xpix0);
			memcpy(outimg + 3*w*(y-ypix0), start, 3*w);
		}

		munmap(map, mapsize);

		if (lines != 0.0) {
			double rstart, rend, dstart, dend;
			double r, d;
			unsigned char linecolor[] = { 255, 0, 0 };
			double linealpha = 0.25;
			int c;
			int x;
			double x0wrap = (x0 < 0.0 ? 2.0*M_PI : 0.0);

			rstart = floor(rad2deg(x0 + x0wrap) / lines) * lines;
			rend   =  ceil(rad2deg(x1 + x0wrap) / lines) * lines;
			dstart = floor(rad2deg(y0) / lines) * lines;
			dend   =  ceil(rad2deg(y1) / lines) * lines;
			for (r=rstart; r<=rend; r+=lines) {
				int px;
				px = (int)rint(((deg2rad(r) - (x0 + x0wrap)) / (2.0*M_PI)) / xperpix);
				//fprintf(stderr, "RA %g: pix %i.\n", r, px);
				if (px < 0 || px >= w)
					continue;
				for (y=0; y<h; y++) {
					for (c=0; c<3; c++) {
						int ind = 3*((y*w) + px) + c;
						outimg[ind] = (unsigned char)(outimg[ind] * (1.0 - linealpha) + linecolor[c] * linealpha);
					}
				}
			}
			for (d=dstart; d<=dend; d+=lines) {
				int py;
				py = (int)rint((M_PI - asinh(tan(deg2rad(d)))) / yperpix) - ypix0;
				//fprintf(stderr, "DEC %g: pix %i.\n", d, py);
				if (py < 0 || py >= h)
					continue;
				for (x=0; x<w; x++) {
					for (c=0; c<3; c++) {
						int ind = 3*((py*w) + x) + c;
						outimg[ind] = (unsigned char)(outimg[ind] * (1.0 - linealpha) + linecolor[c] * linealpha);
					}
				}
			}
		}

		// output PPM.
		printf("P6 %d %d %d\n", w, h, 255);
		fwrite(outimg, 1, w*h*3, stdout);

		free(outimg);

		return 0;
	} else {
		int Nside = 9;
		int HP;
		int hp;
		/*
		  int x, y;
		  int xstart, xend, ystart, yend;
		  int* hpimg;
		  int hpimgsize = 256;
		  bool* hps;
		*/
		il* hplist;
		merctree* merc;
		real querylow[2], queryhigh[2];
		pl* nodelist;
		float* fluximg;
		float xscale, yscale;
		int Noob;
		int Nib;

		HP = 12 * Nside * Nside;

		/*
		  hpimg = malloc(hpimgsize * hpimgsize * sizeof(int));
		  for (y=0; y<hpimgsize; y++) {
		  double xval, yval;
		  double ra, dec;
		  yval = (((double)y / (double)hpimgsize) * 2.0 * M_PI) - M_PI;
		  dec = atan(sinh(yval));
		  for (x=0; x<hpimgsize; x++) {
		  xval = ((double)x / (double)hpimgsize);
		  ra = xval * 2.0 * M_PI;
		  hp = radectohealpix_nside(ra, dec, Nside);
		  hpimg[y*hpimgsize + x] = hp;
		  }
		  }

		  xstart = (int)floor(px0 * hpimgsize);
		  xend   = (int)ceil (px1 * hpimgsize);
		  ystart = (int)floor((py0 + M_PI) / (2.0*M_PI) * hpimgsize);
		  yend   = (int)ceil ((py1 + M_PI) / (2.0*M_PI) * hpimgsize);

		  //fprintf(stderr, "x [%i, %i], y [%i, %i]\n", xstart, xend, ystart, yend);

		  hps = calloc(HP, sizeof(bool));

		  for (y=ystart; y<=yend; y++) {
		  if (y < 0 || y >= hpimgsize)
		  continue;
		  for (x=xstart; x<=xend; x++) {
		  if (x < 0 || x >= hpimgsize)
		  continue;
		  hps[hpimg[y*hpimgsize + x]] = TRUE;
		  }
		  }
		*/
		/*
		  for (y=ystart-1; y<=yend+1; y++) {
		  if (y < 0 || y >= hpimgsize)
		  continue;
		  for (x=xstart-1; x<=xend+1; x++) {
		  if (x < 0 || x >= hpimgsize)
		  continue;
		  hps[hpimg[y*hpimgsize + x]] = TRUE;
		  }
		  }
		*/

		/*
		  free(hpimg);
		*/

		hplist = il_new(32);

		/*
		  fprintf(stderr, "Healpixes: ");
		  for (i=0; i<HP; i++) {
		  if (hps[i]) {
		  il_append(hplist, i);
		  fprintf(stderr, "%i ", i);
		  }
		  }
		  fprintf(stderr, "\n");

		  free(hps);
		*/

		querylow[0] = px0;
		querylow[1] = (py0 + M_PI) / (2.0 * M_PI);
		queryhigh[0] = px1;
		queryhigh[1] = (py1 + M_PI) / (2.0 * M_PI);

		nodelist = pl_new(256);

		fluximg = calloc(w*h*3, sizeof(float));
		if (!fluximg) {
			fprintf(stderr, "Failed to allocate flux image.\n");
			exit(-1);
		}

		xscale = (float)w / (queryhigh[0] - querylow[0]);
		yscale = (float)h / (queryhigh[1] - querylow[1]);

		Noob = 0;
		Nib = 0;

		fprintf(stderr, "Query: x:[%g,%g] y:[%g,%g]\n",
				querylow[0], queryhigh[0], querylow[1], queryhigh[1]);

		for (i=0; i<HP; i++) {
			double x,y,z;
			double ra,dec;
			real bblo[2], bbhi[2];
			double minx, miny, maxx, maxy, minxnz;
			double dx[] = { 0.0, 0.0, 1.0, 1.0 };
			double dy[] = { 0.0, 1.0, 1.0, 0.0 };
			int j;

			minx = miny = minxnz = 1e100;
			maxx = maxy = -1e100;
			for (j=0; j<4; j++) {
				healpix_to_xyz(dx[j], dy[j], i, Nside, &x, &y, &z);
				xyz2radec(x, y, z, &ra, &dec);
				x = ra / (2.0 * M_PI);
				y = (asinh(tan(dec)) + M_PI) / (2.0 * M_PI);
				if (x > maxx) maxx = x;
				if (x < minx) minx = x;
				if ((x != 0.0) && (x < minxnz)) minxnz = x;
				if (y > maxy) maxy = y;
				if (y < miny) miny = y;
			}

			if (minx == 0.0) {
				if ((1.0 - minxnz) < (maxx - minx)) {
					// RA wrap-around.
					//fprintf(stderr, "HP %i: RA wrap-around: minx=%g, minxnz=%g, maxx=%g.\n", i, minx, minxnz, maxx);
					// (there are healpixes with finite extent on either side of the RA=0 line.)
					minx = 0.0;
					maxx = 1.0;
				}
			}

			bblo[0] = minx;
			bbhi[0] = maxx;
			bblo[1] = miny;
			bbhi[1] = maxy;

			if (kdtree_do_boxes_overlap(bblo, bbhi, querylow, queryhigh, 2)) {
				fprintf(stderr, "HP %i overlaps: x:[%g,%g], y:[%g,%g]\n",
						i, bblo[0], bbhi[0], bblo[1], bbhi[1]);
				il_append(hplist, i);
			}
		}

		for (i=0; i<il_size(hplist); i++) {
			int Ntotal;
			int Nleaves;
			int j;
			hp = il_get(hplist, i);

			sprintf(fn, merc_template, hp);
			fprintf(stderr, "Opening file %s...\n", fn);
			merc = merctree_open(fn);
			if (!merc) {
				fprintf(stderr, "Failed to open merctree for healpix %i.\n", hp);
				continue;
			}

			{
				real *lo, *hi;
				int ix1, ix2, iy1, iy2;
				//int ix, iy;
				lo = kdtree_get_bb_low (merc->tree, kdtree_get_root(merc->tree));
				hi = kdtree_get_bb_high(merc->tree, kdtree_get_root(merc->tree));
				fprintf(stderr, "Merctree bounding box: x:[%g,%g], y:[%g,%g]\n",
						lo[0], hi[0], lo[1], hi[1]);
				ix1 = (int)rint((lo[0] - querylow[0]) * xscale);
				ix2 = (int)rint((hi[0] - querylow[0]) * xscale);
				iy2 = h - (int)rint((lo[1] - querylow[1]) * yscale);
				iy1 = h - (int)rint((hi[1] - querylow[1]) * yscale);
				fprintf(stderr, "In pixels: x:[%i,%i], y:[%i,%i]\n", ix1, ix2, iy1, iy2);

				/*
				  if (ix1 >= 0 && ix1 < w)
				  for (iy=iy1; iy<=iy2; iy++)
				  if (iy >= 0 && iy < h)
				  fluximg[3*(iy*w+ix1)+0] += exp(-1);
				  if (ix2 >= 0 && ix2 < w)
				  for (iy=iy1; iy<=iy2; iy++)
				  if (iy >= 0 && iy < h)
				  fluximg[3*(iy*w+ix2)+0] += exp(-1);
				  if (iy1 >= 0 && iy1 < h)
				  for (ix=ix1; ix<=ix2; ix++)
				  if (ix >= 0 && ix < w)
				  fluximg[3*(iy1*w+ix)+0] += exp(-1);
				  if (iy2 >= 0 && iy2 < h)
				  for (ix=ix1; ix<=ix2; ix++)
				  if (ix >= 0 && ix < w)
				  fluximg[3*(iy2*w+ix)+0] += exp(-1);
				  for (iy=iy1; iy<iy2; iy++)
				  if (iy >= 0 && iy < h)
				  for (ix=ix1; ix<ix2; ix++)
				  if (ix >= 0 && ix < w)
				  fluximg[3*(iy*w+ix)+2] += exp(-1);
				*/
			}

			get_nodes_contained_in(merc->tree, querylow, queryhigh, nodelist);

			Ntotal = 0;
			Nleaves = 0;
			for (j=0; j<pl_size(nodelist); j++) {
				kdtree_node_t* node = pl_get(nodelist, j);
				Ntotal += kdtree_node_npoints(node);
				if (kdtree_node_is_leaf(merc->tree, node))
					Nleaves++;
			}

			fprintf(stderr, "Found %i nodes (%i leaves), total of %i points.\n", pl_size(nodelist), Nleaves, Ntotal);

			for (j=0; j<pl_size(nodelist); j++) {
				int k;
				kdtree_node_t* node = pl_get(nodelist, j);
				// for macros
				kdtree_t* kd = merc->tree;
				for (k=node->l; k<=node->r; k++) {
					int ix, iy;
					real* pt;
					merc_flux* flux;
					pt = KD_POINT(k);
					ix = (int)rint((pt[0] - querylow[0]) * xscale);
					iy = h - (int)rint((pt[1] - querylow[1]) * yscale);
					if (ix < 0 || iy < 0 || ix > w || iy > h) {
						Noob++;
						continue;
					}
					Nib++;
					flux = merc->flux + k;
					fluximg[3*(iy*w+ix) + 0] += flux->rflux;
					fluximg[3*(iy*w+ix) + 1] += flux->bflux;
					fluximg[3*(iy*w+ix) + 2] += flux->nflux;
				}
			}

			pl_remove_all(nodelist);
			merctree_close(merc);
		}
		fprintf(stderr, "%i stars outside image bounds.\n", Noob);
		fprintf(stderr, "%i stars inside image bounds.\n", Nib);

		pl_free(nodelist);
		il_free(hplist);

		{
			float rmax, bmax, nmax;
			float offset = -25;
			float minval = exp(offset);
			float rscale, bscale, nscale;
			rmax = bmax = nmax = 0.0;
			for (i=0; i<(w*h); i++) {
				if (fluximg[3*i + 0] > rmax) rmax = fluximg[3*i + 0];
				if (fluximg[3*i + 1] > bmax) bmax = fluximg[3*i + 1];
				if (fluximg[3*i + 2] > nmax) nmax = fluximg[3*i + 2];
			}
			rscale = 255.0 / (log(rmax) - offset);
			bscale = 255.0 / (log(bmax) - offset);
			nscale = 255.0 / (log(nmax) - offset);

			fprintf(stderr, "Maxes: R %g, B %g, N %g.\n", rmax, bmax, nmax);

			printf("P6 %d %d %d\n", w, h, 255);
			for (i=0; i<(w*h); i++) {
				unsigned char pix[3];
				pix[0] = (log(max(fluximg[3*i+0], minval)) - offset) * rscale;
				pix[1] = (log(max(fluximg[3*i+1], minval)) - offset) * bscale;
				pix[2] = (log(max(fluximg[3*i+2], minval)) - offset) * nscale;
				fwrite(pix, 1, 3, stdout);
			}
		}

		free(fluximg);
		return 0;
	}
}
