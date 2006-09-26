#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>

#define TRUE 1
#include "ppm.h"
#include "pnm.h"
#undef bool

#include "an_catalog.h"
#include "kdtree/kdtree.h"
#include "kdtree/kdtree_macros.h"
#include "kdtree/kdtree_access.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"
#include "merctree.h"
#include "bl.h"

#define OPTIONS "x:y:X:Y:w:h:l:f"

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

static void get_nodes_contained_in(kdtree_t* kd,
								   real* querylow, real* queryhigh,
								   pl* nodelist) {
	get_nodes_in(kd, kdtree_get_root(kd), querylow, queryhigh, nodelist);
}

static void expand_nodes(kdtree_t* kd, kdtree_node_t* node,
						 pl* leaflist, pl* pixlist,
						 real* qlo, real* qhi,
						 int w, int h) {
	// check if this whole box fits inside a pixel.
	int xp0, xp1, yp0, yp1;
	real *bblo, *bbhi;
	bblo = kdtree_get_bb_low (kd, node);
	bbhi = kdtree_get_bb_high(kd, node);
	xp0 = (int)floor((double)w * (bblo[0] - qlo[0]) / (qhi[0] - qlo[0]));
	xp1 = (int)ceil ((double)w * (bbhi[0] - qlo[0]) / (qhi[0] - qlo[0]));
	yp0 = (int)floor((double)h * (bblo[1] - qlo[1]) / (qhi[1] - qlo[1]));
	yp1 = (int)ceil ((double)h * (bbhi[1] - qlo[1]) / (qhi[1] - qlo[1]));
	if ((xp1 - xp0 <= 1) && (yp1 - yp0 <= 1)) {
		//fprintf(stderr, "Node fits in a single pixel!\n");
		pl_append(pixlist, node);
		return;
	}
	if (kdtree_node_is_leaf(kd, node)) {
		pl_append(leaflist, node);
		return;
	}
	expand_nodes(kd, kdtree_get_child1(kd, node), leaflist, pixlist, qlo, qhi, w, h);
	expand_nodes(kd, kdtree_get_child2(kd, node), leaflist, pixlist, qlo, qhi, w, h);
}

static int count_points_in_list(pl* list) {
	int j;
	int N = 0;
	for (j=0; j<pl_size(list); j++) {
		kdtree_node_t* node = pl_get(list, j);
		N += kdtree_node_npoints(node);
	}
	return N;
}

static void addstar(float* fluximg, int x, int y, int W, int H,
					float rflux, float gflux, float bflux) {
	/*
	  int dx[]      = {  -2,  -1,   0,   1,   2,   0,   0,   0,   0,   1,   1,  -1,  -1 };
	  int dy[]      = {   0,   0,   0,   0,   0,  -2,  -1,   1,   2,   1,  -1,   1,  -1 };
	  float scale[] = { 5e-2, 0.1, 1.0, 0.1, 5e-2, 5e-2, 0.1, 0.1, 5e-2, 8e-1, 8e-1, 8e-1, 8e-1 };
	*/
	int dx[] = { -1,  0,  1,  0,  0 };
	int dy[] = {  0,  0,  0, -1,  1 };
	float scale[] = { 1, 1, 1, 1, 1 };
	int i;
	for (i=0; i<sizeof(dx)/sizeof(int); i++) {
		if ((x + dx[i] < 0) || (x + dx[i] >= W)) continue;
		if ((y + dy[i] < 0) || (y + dy[i] >= H)) continue;
		fluximg[3*((y+dy[i])*W+(x+dx[i])) + 0] += rflux * scale[i];
		fluximg[3*((y+dy[i])*W+(x+dx[i])) + 1] += gflux * scale[i];
		fluximg[3*((y+dy[i])*W+(x+dx[i])) + 2] += bflux * scale[i];
	}
}

int main(int argc, char *argv[]) {
    int argchar;
	bool gotx, goty, gotX, gotY, gotw, goth;
	double x0=0.0, x1=0.0, y0=0.0, y1=0.0;
	int w=0, h=0;
	char* map_template = "/h/42/dstn/local/maps/usnob-zoom%i.ppm";
	char* merc_template = "/h/42/dstn/local/maps/merc/merc_hp%03i.fits";
	//char* merc_template = "/h/42/dstn/local/maps/merc/merc_hp365.fits";
	char fn[256];
	int i;
	double lines = 0.0;

	double px0, py0, px1, py1;
	double pixperx, pixpery;
	double xzoom, yzoom;
	int zoomlevel;
	int xpix0, xpix1, ypix0, ypix1;

	bool forcetree = FALSE;

	gotx = goty = gotX = gotY = gotw = goth = FALSE;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'f':
			forcetree = TRUE;
			break;
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

	pixperx = (double)w / (px1 - px0);
	pixpery = (double)h / (py1 - py0);

	fprintf(stderr, "Projected X range: [%g, %g]\n", px0, px1);
	fprintf(stderr, "Projected Y range: [%g, %g]\n", py0, py1);
	fprintf(stderr, "X,Y pixel scale: %g, %g.\n", pixperx, pixpery);
	xzoom = pixperx / 256.0;
	yzoom = pixpery / 256.0;
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

	xpix0 = px0 * pixperx;
	ypix0 = py0 * pixpery;
	xpix1 = px1 * pixperx;
	ypix1 = py1 * pixpery;

	fprintf(stderr, "Pixel positions: (%i,%i), (%i,%i) vs (%i,%i)\n", xpix0, ypix0, xpix0+w, ypix0+h, xpix1, ypix1);

	xpix1 = xpix0 + w;
	ypix1 = ypix0 + h;

	if (!forcetree && (zoomlevel <= 5)) {
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
				px = (int)rint(((deg2rad(r) - (x0 + x0wrap)) / (2.0*M_PI)) * pixperx);
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
				py = (int)rint( ((M_PI + asinh(tan(deg2rad(d)))) / (2.0*M_PI)) * pixpery) - ypix0;
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
		// flip the image vertically here at the last step.
		for (y=h-1; y>=0; y--)
			fwrite(outimg + y*w*3, 1, w*3, stdout);
		//fwrite(outimg, 1, w*h*3, stdout);

		free(outimg);

		return 0;
	} else {
		int Nside = 9;
		int HP;
		int hp;
		il* hplist;
		merctree* merc;
		real querylow[2], queryhigh[2];
		real wraplow[2], wraphigh[2];
		pl* nodelist;
		pl* leaflist;
		pl* pixlist;
		float* fluximg;
		float xscale, yscale;
		int Noob;
		int Nib;
		int Nstars;
		bool wrapra;

		wrapra = (px1 > 1.0);

		HP = 12 * Nside * Nside;
		hplist = il_new(32);

		if (wrapra) {
			querylow[0] = px0;
			queryhigh[0] = 1.0;
			querylow[1] = py0;
			queryhigh[1] = py1;
			wraplow[0] = 0.0;
			wraphigh[0] = px1 - 1.0;
			wraplow[1] = py0;
			wraphigh[1] = py1;
		} else {
			querylow[0] = px0;
			querylow[1] = py0;
			queryhigh[0] = px1;
			queryhigh[1] = py1;
		}
		nodelist = pl_new(1024);
		leaflist = pl_new(1024);
		pixlist = pl_new(1024);

		fluximg = calloc(w*h*3, sizeof(float));
		if (!fluximg) {
			fprintf(stderr, "Failed to allocate flux image.\n");
			exit(-1);
		}

		xscale = pixperx;
		yscale = pixpery;

		Noob = 0;
		Nib = 0;
		// number of stars in the image.
		Nstars = 0;

		fprintf(stderr, "Query: x:[%g,%g] y:[%g,%g]\n",
				querylow[0], queryhigh[0], querylow[1], queryhigh[1]);
		if (wrapra)
			fprintf(stderr, "Query': x:[%g,%g] y:[%g,%g]\n",
					wraplow[0], wraphigh[0], wraplow[1], wraphigh[1]);


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

			/*
			  if (!((queryhigh[1] < bblo[1]) || (bbhi[1] < querylow[1]))) {
			  // Y range overlaps.
			  if (wrapra)
			  fprintf(stderr, "HP %i: bb [%g,%g]; query [%g,%g], wrap [%g,%g].\n",
			  i, bblo[0], bbhi[0], querylow[0], queryhigh[0], wraplow[0], wraphigh[0]);
			  }
			*/

			if (kdtree_do_boxes_overlap(bblo, bbhi, querylow, queryhigh, 2) ||
				(wrapra && kdtree_do_boxes_overlap(bblo, bbhi, wraplow, wraphigh, 2))) {
				fprintf(stderr, "HP %i overlaps: x:[%g,%g], y:[%g,%g]\n",
						i, bblo[0], bbhi[0], bblo[1], bbhi[1]);
				il_append(hplist, i);
			}
		}

		for (i=0; i<il_size(hplist); i++) {
			/*
			  int Ntotal;
			  int Nleaves;
			*/
			int j;
			int iwnode, iwpix, iwleaf;
			hp = il_get(hplist, i);

			sprintf(fn, merc_template, hp);
			fprintf(stderr, "Opening file %s...\n", fn);
			merc = merctree_open(fn);
			if (!merc) {
				fprintf(stderr, "Failed to open merctree for healpix %i.\n", hp);
				continue;
			}

			/*{
				real *lo, *hi;
				int ix1, ix2, iy1, iy2;
				//int ix, iy;
				lo = kdtree_get_bb_low (merc->tree, kdtree_get_root(merc->tree));
				hi = kdtree_get_bb_high(merc->tree, kdtree_get_root(merc->tree));
				fprintf(stderr, "Merctree bounding box: x:[%g,%g], y:[%g,%g]\n",
						lo[0], hi[0], lo[1], hi[1]);
				ix1 = (int)rint((lo[0] - querylow[0]) * xscale);
				ix2 = (int)rint((hi[0] - querylow[0]) * xscale);
				// flip the image vertically
				iy2 = h - (int)rint((lo[1] - querylow[1]) * yscale);
				iy1 = h - (int)rint((hi[1] - querylow[1]) * yscale);
				fprintf(stderr, "In pixels: x:[%i,%i], y:[%i,%i]\n", ix1, ix2, iy1, iy2);

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
				  }*/

			//get_nodes_contained_in(merc->tree, querylow, queryhigh, nodelist, leaflist);
			get_nodes_contained_in(merc->tree, querylow, queryhigh, nodelist);
			iwnode = pl_size(nodelist);
			if (wrapra) {
				get_nodes_contained_in(merc->tree, wraplow, wraphigh, nodelist);
				//fprintf(stderr, "Found %i nodes in wrapped-around region.\n", (pl_size(nodelist) - iwnode));
			}

			/*
			  fprintf(stderr, "Found %i nodes (%i points) + %i leaves (%i points), total of %i points.\n",
			  pl_size(nodelist), count_points_in_list(nodelist),
			  pl_size(leaflist), count_points_in_list(leaflist),
			  count_points_in_list(nodelist) + count_points_in_list(leaflist));
			*/
			fprintf(stderr, "%i nodes (%i points) overlap the tile.\n", pl_size(nodelist), count_points_in_list(nodelist));
			//fprintf(stderr, "%i leaves (%i points) overlap the tile.\n", pl_size(leaflist), count_points_in_list(leaflist));

			iwpix = iwleaf = INT_MAX;

			for (j=0; j<pl_size(nodelist); j++) {
				kdtree_node_t* node = pl_get(nodelist, j);
				if (j == iwnode) {
					iwleaf = pl_size(leaflist);
					iwpix  = pl_size(pixlist);
				}
				expand_nodes(merc->tree, node, leaflist, pixlist,
							 (j >= iwnode ? wraplow  : querylow),
							 (j >= iwnode ? wraphigh : queryhigh),
							 w, h);
			}
			pl_remove_all(nodelist);

			fprintf(stderr, "%i single-pixel nodes (%i points).\n", pl_size(pixlist), count_points_in_list(pixlist));
			fprintf(stderr, "%i multi-pixel leaves (%i points).\n", pl_size(leaflist), count_points_in_list(leaflist));
			/*
			  if (wrapra) {
			  fprintf(stderr, "(%i single-pixel nodes, %i multi-pixel in the wrapped-around region.)\n",
			  (pl_size(pixlist) - iwpix), (pl_size(leaflist) - iwleaf));
			  }
			*/

			/*
			  fprintf(stderr, "Expanded to %i leaves (%i points) + %i single-pixel nodes (%i points)\n",
			  pl_size(leaflist), count_points_in_list(leaflist),
			  pl_size(pixlist), count_points_in_list(pixlist));
			*/

			/*
			  Ntotal = 0;
			  Nleaves = 0;
			  for (j=0; j<pl_size(nodelist); j++) {
			  kdtree_node_t* node = pl_get(nodelist, j);
			  Ntotal += kdtree_node_npoints(node);
			  if (kdtree_node_is_leaf(merc->tree, node))
			  Nleaves++;
			  }
			  fprintf(stderr, "Found %i nodes (%i leaves), total of %i points.\n", pl_size(nodelist), Nleaves, Ntotal);
			*/

			for (j=0; j<pl_size(pixlist); j++) {
				kdtree_node_t* node = pl_get(pixlist, j);
				// for macros
				kdtree_t* kd = merc->tree;
				int ix, iy;
				real* pt;
				merc_flux* flux;
				int nodenum;

				pt = KD_POINT(node->l);
				if (j >= iwpix) {
					ix = (int)rint(((pt[0] - wraplow[0]) + (1.0 - querylow[0])) * xscale);
				} else {
					ix = (int)rint((pt[0] - querylow[0]) * xscale);
				}
				if (ix < 0 || ix >= w) {
					Noob++;
					continue;
				}
				// flip vertically
				iy = h - (int)rint((pt[1] - querylow[1]) * yscale);
				if (iy < 0 || iy >= h) {
					Noob++;
					continue;
				}
				Nib++;
				Nstars += kdtree_node_npoints(node);
				nodenum = kdtree_node_to_nodeid(kd, node);
				flux = &(merc->stats[nodenum].flux);
				fluximg[3*(iy*w+ix) + 0] += flux->rflux;
				fluximg[3*(iy*w+ix) + 1] += flux->bflux;
				fluximg[3*(iy*w+ix) + 2] += flux->nflux;
			}
			pl_remove_all(pixlist);

			for (j=0; j<pl_size(leaflist); j++) {
				int k;
				kdtree_node_t* node = pl_get(leaflist, j);
				// for macros
				kdtree_t* kd = merc->tree;

				/*{
					float xp0, xp1, yp0, yp1;
					real *bblo, *bbhi;
					real *qlo = querylow;
					real *qhi = queryhigh;
					bblo = kdtree_get_bb_low (kd, node);
					bbhi = kdtree_get_bb_high(kd, node);
					xp0 = ((double)w * (bblo[0] - qlo[0]) / (qhi[0] - qlo[0]));
					xp1 = ((double)w * (bbhi[0] - qlo[0]) / (qhi[0] - qlo[0]));
					yp0 = ((double)h * (bblo[1] - qlo[1]) / (qhi[1] - qlo[1]));
					yp1 = ((double)h * (bbhi[1] - qlo[1]) / (qhi[1] - qlo[1]));
					fprintf(stderr, "xp [%g,%g], yp [%g,%g], area %g, N %i\n", xp0, xp1, yp0, yp1, (xp1-xp0)*(yp1-yp0), kdtree_node_npoints(node));
					}*/

				for (k=node->l; k<=node->r; k++) {
					int ix, iy;
					real* pt;
					merc_flux* flux;
					pt = KD_POINT(k);
					if (j >= iwleaf) {
						ix = (int)rint(((pt[0] - wraplow[0]) + (1.0 - querylow[0])) * xscale);
					} else {
						ix = (int)rint((pt[0] - querylow[0]) * xscale);
					}
					if (ix < 0 || ix >= w) {
						Noob++;
						continue;
					}
					// flip vertically
					iy = h - (int)rint((pt[1] - querylow[1]) * yscale);
					if (iy < 0 || iy >= h) {
						Noob++;
						continue;
					}
					Nib++;
					Nstars++;
					flux = merc->flux + k;
					addstar(fluximg, ix, iy, w, h, flux->rflux, flux->bflux, flux->nflux);
				}
			}
			pl_remove_all(leaflist);

			merctree_close(merc);
		}
		fprintf(stderr, "%i points outside image bounds.\n", Noob);
		fprintf(stderr, "%i points inside image bounds.\n", Nib);
		fprintf(stderr, "%i stars inside image bounds.\n", Nstars);

		pl_free(nodelist);
		pl_free(leaflist);
		pl_free(pixlist);
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
