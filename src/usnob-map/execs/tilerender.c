/*
   This file is part of the Astrometry.net suite.
   Copyright 2007 Keir Mierle and Dustin Lang.

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
#include <unistd.h>
#include <sys/param.h>
#include <assert.h>
#include <stdarg.h>

#include <zlib.h>

#include "an-bool.h"
#include "tilerender.h"
#include "starutil.h"
#include "cairoutils.h"
#include "ioutils.h"

#include "render_tycho.h"
#include "render_gridlines.h"
#include "render_usnob.h"
#include "render_rdls.h"
#include "render_boundary.h"
#include "render_constellation.h"
#include "render_messier.h"
#include "render_solid.h"
#include "render_collection.h"

/**
  This program gets called by "tile.php" in response to a client requesting a map
  tile.  The coordinates of the tile are specified as a range of RA and DEC values.

  The RA coordinates are passed as    -x <lower-RA> -X <upper-RA>
  The DEC coordinates are             -y <lower-DEC> -Y <upper-DEC>
  The width and height in pixels are  -w <width> -h <height>
  */

const char* OPTIONS = "x:y:X:Y:w:h:l:i:W:c:sag:r:N:F:L:B:I:RMC:pk:zdV:D:nS:J";


/* All render layers must go in here */
char* layernames[] = {
	"tycho",
	"grid",
	"usnob",
	"rdls",
	"boundary",
	"constellation",
	"messier",

	"clean",
	"dirty",
	"solid",

	"apod",
	"userimage"
};
render_func_t renderers[] = {
	render_tycho,
	render_gridlines,
	render_usnob,
	render_rdls,
	render_boundary,
	render_constellation,
	render_messier,

	render_usnob,
	render_usnob,
	render_solid,

	render_collection,
	render_collection,
};

static void
ATTRIB_FORMAT(printf,1,2)
logmsg(char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "tilerender: ");
    vfprintf(stderr, format, args);
    va_end(args);
}

static void default_rdls_args(render_args_t* args) {
	// Set the other RDLS-related args if they haven't been set already.
	if (pl_size(args->rdlscolors) < pl_size(args->rdlsfns))
		pl_append(args->rdlscolors, NULL);
	if (il_size(args->Nstars) < pl_size(args->rdlsfns))
		il_append(args->Nstars, 0);
	if (il_size(args->fieldnums) < pl_size(args->rdlsfns))
		il_append(args->fieldnums, 0);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
	int argchar;
	int gotx, goty, gotX, gotY, gotw, goth;
	double xzoom;
	unsigned char* img;
	render_args_t args;
	pl* layers;
	int i;
	bool inmerc = 0;
    bool writejpeg = FALSE;

	memset(&args, 0, sizeof(render_args_t));

	// default args:
	args.colorcor = 1.44;
	args.linewidth = 2.0;

	args.rdlsfns = pl_new(4);
	args.rdlscolors = pl_new(4);
	args.Nstars = il_new(4);
	args.fieldnums = il_new(4);

	layers = pl_new(16);
	gotx = goty = gotX = gotY = gotw = goth = FALSE;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
        case 'J':
            writejpeg = TRUE;
            break;
        case 'S':
				args.filelist = strdup(optarg);
				break;
			case 'n':
				args.density = TRUE;
				break;
			case 'D':
				args.cachedir = strdup(optarg);
				break;
			case 'V':
				args.version = optarg;
				break;
			case 'z':
				args.zoomright = TRUE;
				break;
			case 'd':
				args.zoomdown = TRUE;
				break;
			case 'p':
				args.nopre = TRUE;
				break;
			case 'C':
				args.cmap = strdup(optarg);
				break;
			case 'M':
				inmerc = TRUE;
				break;
			case 'R':
				args.makerawfloatimg = 1;
				break;
			case 'B':
				args.dashbox = atof(optarg);
				break;
			case 'F':
				il_append(args.fieldnums, atoi(optarg));
				break;
			case 'N':
				il_append(args.Nstars, atoi(optarg));
				break;
			case 'r':
				default_rdls_args(&args);
				pl_append(args.rdlsfns, strdup(optarg));
				break;
			case 'k':
				pl_append(args.rdlscolors, strdup(optarg));
				break;
			case 'i':
				args.imagefn = strdup(optarg);
				break;
			case 'W':
				args.wcsfn = strdup(optarg);
				break;
			case 'I':
				args.imwcsfn = strdup(optarg);
				break;
			case 'c':
				args.colorcor = atof(optarg);
				break;
			case 's':
				args.arc = TRUE;
				break;
			case 'a':
				args.arith = TRUE;
				break;
			case 'g':
				args.gain = atof(optarg);
				break;
			case 'l':
				pl_append(layers, strdup(optarg));
				break;
			case 'L':
				args.linewidth = atof(optarg);
				break;
			case 'x':
				args.ramin = atof(optarg);
				gotx = TRUE;
				break;
			case 'X':
				args.ramax = atof(optarg);
				goty = TRUE;
				break;
			case 'y':
				args.decmin = atof(optarg);
				gotX = TRUE;
				break;
			case 'Y':
				args.decmax = atof(optarg);
				gotY = TRUE;
				break;
			case 'w':
				args.W = atoi(optarg);
				gotw = TRUE;
				break;
			case 'h':
				args.H = atoi(optarg);
				goth = TRUE;
				break;
		}

	if (!(gotx && goty && gotX && gotY && gotw && goth)) {
		logmsg("tilecache: Invalid inputs: need ");
		if (!gotx) logmsg("-x ");
		if (!gotX) logmsg("-X ");
		if (!goty) logmsg("-y ");
		if (!gotY) logmsg("-Y ");
		if (!gotw) logmsg("-w ");
		if (!goth) logmsg("-h ");
		logmsg("\n");
		exit(-1);
	}

	default_rdls_args(&args);

    if (args.W > 1024 || args.H > 1024) {
        logmsg("tilecache: Width or height too large (limit 1024)\n");
        exit(-1);
    }

	logmsg("tilecache: BEGIN TILECACHE\n");

	if (inmerc) {
		// -x -X -y -Y were given in Mercator coordinates - convert to deg.
		// this is for cases where it's more convenient to specify the coords
		// in Merc coords (eg prerendering)
		args.ramin  = merc2radeg(args.ramin);
		args.ramax  = merc2radeg(args.ramax);
		args.decmin = merc2decdeg(args.decmin);
		args.decmax = merc2decdeg(args.decmax);
	}

	// min ra -> max merc.
	args.xmercmax =  radeg2merc(args.ramin);
	args.xmercmin =  radeg2merc(args.ramax);
	args.ymercmin = decdeg2merc(args.decmin);
	args.ymercmax = decdeg2merc(args.decmax);

	// The y mercator position can end up *near* but not exactly
	// equal to the boundary conditions... clamp.
	args.ymercmin = MAX(0.0, args.ymercmin);
	args.ymercmax = MIN(1.0, args.ymercmax);

	args.xpixelpermerc = (double)args.W / (args.xmercmax - args.xmercmin);
	args.ypixelpermerc = (double)args.H / (args.ymercmax - args.ymercmin);
	args.xmercperpixel = 1.0 / args.xpixelpermerc;
	args.ymercperpixel = 1.0 / args.ypixelpermerc;

	// TEST
	/*
	  {
	  double x, y;
	  double mx, my;
	  double ra, dec;
	  double ra2, dec2;
	  double mx2, my2;
	  double x2, y2;
	  double x3, y3;

	  x = y = 0;
	  mx = xpixel2mercf(x, &args);
	  my = ypixel2mercf(y, &args);
	  ra = merc2radeg(mx);
	  dec = merc2decdeg(my);
	  ra2 = pixel2ra(x, &args);
	  dec2 = pixel2dec(y, &args);
	  mx2 = radeg2merc(ra);
	  my2 = decdeg2merc(dec);
	  x2 = xmerc2pixelf(mx2, &args);
	  y2 = ymerc2pixelf(my2, &args);
	  x3 = ra2pixelf(ra, &args);
	  y3 = dec2pixelf(dec, &args);

	  logmsg("Pixel (%g,%g) -> Merc (%g, %g) -> RA,Dec (%g, %g) or (%g, %g)\n"
	  "  -> Merc (%g, %g) -> Pixel (%g, %g) or (%g, %g).\n",
	  x, y, mx, my, ra, dec, ra2, dec2, mx2, my2, x2, y2, x3, y3);

	  x = y = 255;
	  mx = xpixel2mercf(x, &args);
	  my = ypixel2mercf(y, &args);
	  ra = merc2radeg(mx);
	  dec = merc2decdeg(my);
	  ra2 = pixel2ra(x, &args);
	  dec2 = pixel2dec(y, &args);
	  mx2 = radeg2merc(ra);
	  my2 = decdeg2merc(dec);
	  x2 = xmerc2pixelf(mx2, &args);
	  y2 = ymerc2pixelf(my2, &args);
	  x3 = ra2pixelf(ra, &args);
	  y3 = dec2pixelf(dec, &args);

	  logmsg("Pixel (%g,%g) -> Merc (%g, %g) -> RA,Dec (%g, %g) or (%g, %g)\n"
	  "  -> Merc (%g, %g) -> Pixel (%g, %g) or (%g, %g).\n",
	  x, y, mx, my, ra, dec, ra2, dec2, mx2, my2, x2, y2, x3, y3);
	  }
	*/

	xzoom = args.xpixelpermerc / 256.0;
	args.zoomlevel = (int)rint(log(fabs(xzoom)) / log(2.0));
	logmsg("tilecache: zoomlevel: %d\n", args.zoomlevel);

	// Allocate a black image.
	img = calloc(4 * args.W * args.H, 1);

	// Rescue boneheads.
	if (!pl_size(layers)) {
		logmsg("tilecache: Do you maybe want to try rendering some layers?\n");
	}

	for (i=0; i<pl_size(layers); i++) {
		int j, k;
		int NR = sizeof(layernames) / sizeof(char*);
		char* layer = pl_get(layers, i);
		bool gotit = FALSE;
		uchar* thisimg = calloc(4 * args.W * args.H, 1);

		for (j=0; j<NR; j++) {
			if (!strcmp(layer, layernames[j])) {
				args.currentlayer = layernames[j];
				if (renderers[j](thisimg, &args)) {
					logmsg("tilecache: Renderer \"%s\" failed.\n", layernames[j]);
				} else {
					logmsg("tilecache: Renderer \"%s\" succeeded.\n", layernames[j]);
				}
				gotit = TRUE;
				break;
			}
		}
		// Save a different kind of bonehead.
		if (!gotit) {
			logmsg("tilecache: No renderer found for layer \"%s\".\n", layer);
		}

		// Composite.
		for (j=0; j<args.H; j++) {
			for (k=0; k<args.W; k++) {
				float alpha;
				uchar* newpix = pixel(k, j, thisimg, &args);
				uchar* accpix = pixel(k, j, img, &args);
				alpha = newpix[3] / 255.0;

				accpix[0] = accpix[0]*(1.0 - alpha) + newpix[0] * alpha;
				accpix[1] = accpix[1]*(1.0 - alpha) + newpix[1] * alpha;
				accpix[2] = accpix[2]*(1.0 - alpha) + newpix[2] * alpha;
				accpix[3] = MIN(255, accpix[3] + newpix[3]);
			}
		}

		free(thisimg);
	}

	if (args.makerawfloatimg) {
		fwrite(args.rawfloatimg, sizeof(float), args.W * args.H * 3, stdout);
		free(args.rawfloatimg);
	} else {
        if (writejpeg)
            cairoutils_stream_jpeg(stdout, img, args.W, args.H);
		else
            cairoutils_stream_png(stdout, img, args.W, args.H);
	}

	free(img);

	for (i=0; i<pl_size(args.rdlsfns); i++) {
		char* str = pl_get(args.rdlsfns, i);
		free(str);
	}
	pl_free(args.rdlsfns);
	for (i=0; i<pl_size(args.rdlscolors); i++) {
		char* str = pl_get(args.rdlscolors, i);
		free(str);
	}
	pl_free(args.rdlscolors);
	il_free(args.Nstars);
	il_free(args.fieldnums);

	free(args.imagefn);
	free(args.wcsfn);
	free(args.imwcsfn);
	free(args.cmap);
	for (i=0; i<pl_size(layers); i++) {
		char* str = pl_get(layers, i);
		free(str);
	}
	pl_free(layers);

	logmsg("tilecache: END TILECACHE\n");

	return 0;
}

/*
 We need to flip RA somewhere...

 We convert Longitude to RA to Mercator to Pixels.

 We choose to insert the flip in the conversion from RA to Mercator.
*/

double xpixel2mercf(double pix, render_args_t* args) {
	return args->xmercmin + pix * args->xmercperpixel;
}

double ypixel2mercf(double pix, render_args_t* args) {
	return args->ymercmax - pix * args->ymercperpixel;
}

double xmerc2pixelf(double x, render_args_t* args) {
	return (x - args->xmercmin) * args->xpixelpermerc;
}

double ymerc2pixelf(double y, render_args_t* args) {
	return (args->ymercmax - y) * args->ypixelpermerc;
}

////// The following are just composed of simpler conversions.

// RA in degrees
int ra2pixel(double ra, render_args_t* args) {
	return xmerc2pixel(radeg2merc(ra), args);
}

// DEC in degrees
int dec2pixel(double dec, render_args_t* args) {
	return ymerc2pixel(decdeg2merc(dec), args);
}

// RA in degrees
double ra2pixelf(double ra, render_args_t* args) {
	return xmerc2pixelf(radeg2merc(ra), args);
}

// DEC in degrees
double dec2pixelf(double dec, render_args_t* args) {
	return ymerc2pixelf(decdeg2merc(dec), args);
}

// to RA in degrees
double pixel2ra(double pix, render_args_t* args) {
	return merc2radeg(xpixel2mercf(pix, args));
}

// to DEC in degrees
double pixel2dec(double pix, render_args_t* args) {
	return merc2decdeg(ypixel2mercf(pix, args));
}

int xmerc2pixel(double x, render_args_t* args) {
	return (int)floor(xmerc2pixelf(x, args));
}

int ymerc2pixel(double y, render_args_t* args) {
	return (int)floor(ymerc2pixelf(y, args));
}

int in_image(int x, int y, render_args_t* args) {
	return (x >= 0 && x < args->W && y >=0 && y < args->H);
}

int in_image_margin(int x, int y, int margin, render_args_t* args) {
	return (x >= -margin && x < (args->W + margin) && y >= -margin && y < (args->H + margin));
}

uchar* pixel(int x, int y, uchar* img, render_args_t* args) {
	return img + 4*(y*args->W + x);
}

// draw a line in Mercator space, handling wrap-around if necessary.
void draw_line_merc(double mx1, double my1, double mx2, double my2,
		cairo_t* cairo, render_args_t* args) {
	cairo_move_to(cairo, xmerc2pixel(mx1, args), ymerc2pixel(my1, args));
	cairo_line_to(cairo, xmerc2pixel(mx2, args), ymerc2pixel(my2, args));
	if (MIN(mx1,mx2) < 0) {
		cairo_move_to(cairo, xmerc2pixel(mx1+1, args), ymerc2pixel(my1, args));
		cairo_line_to(cairo, xmerc2pixel(mx2+1, args), ymerc2pixel(my2, args));
	}
	if (MAX(mx1,mx2) > 1) {
		cairo_move_to(cairo, xmerc2pixel(mx1-1, args), ymerc2pixel(my1, args));
		cairo_line_to(cairo, xmerc2pixel(mx2-1, args), ymerc2pixel(my2, args));
	}
}

// ra,dec in degrees.
void draw_segmented_line(double ra1, double dec1,
		double ra2, double dec2,
		int SEGS,
		cairo_t* cairo, render_args_t* args) {
	int i, s, k;
	double xyz1[3], xyz2[3];
	bool wrap;

	radecdeg2xyzarr(ra1, dec1, xyz1);
	radecdeg2xyzarr(ra2, dec2, xyz2);

	wrap = (fabs(ra1 - ra2) >= 180.0) ||
		(args->ramin < 0) || (args->ramax > 360.0);

	// Draw segmented line.
	for (i=0; i<(1 + (wrap?1:0)); i++) {
		for (s=0; s<SEGS; s++) {
			double xyz[3], frac;
			double ra, dec;
			double mx;
			double px, py;
			frac = (double)s / (double)(SEGS-1);
			for (k=0; k<3; k++)
				xyz[k] = xyz1[k]*(1.0-frac) + xyz2[k]*frac;
			normalize_3(xyz);
			xyzarr2radecdeg(xyz, &ra, &dec);
			mx = radeg2merc(ra);

			if (wrap) {
				// in the first pass we draw the left side (mx>0.5)
				if ((i==0) && (mx < 0.5)) mx += 1.0;
				// in the second pass we draw the right side (wx<0.5)
				if ((i==1) && (mx > 0.5)) mx -= 1.0;
			}
			px = xmerc2pixelf(mx, args);
			py = dec2pixelf(dec, args);

			if (s==0)
				cairo_move_to(cairo, px, py);
			else
				cairo_line_to(cairo, px, py);
		}
	}
}

static int cache_get_filename(render_args_t* args,
		const char* cachedomain, const char* key,
		char* fn, int fnlen) {
	if (snprintf(fn, fnlen, "%s/%s/%s", args->cachedir, cachedomain, key) > fnlen) {
		logmsg("Filename truncated in cache_load/cache_save.\n");
		return -1;
	}
	return 0;
}

void* cache_load(render_args_t* args,
		const char* cachedomain, const char* key, int* length) {
	char fn[1024];
	unsigned char* buf;
	size_t len;
	uint32_t typeid;
	unsigned char* orig;
	int rtn;
	uLong origlen;
	uint32_t* ubuf;

	if (!args->cachedir)
		return NULL;
	if (cache_get_filename(args, cachedomain, key, fn, sizeof(fn))) {
		return NULL;
	}
	if (!file_exists(fn))
		return NULL;
	buf = file_get_contents(fn, &len, FALSE);
	if (!buf) {
		logmsg("Failed to read file contents in cache_load.\n");
		return NULL;
	}
	if (len < 2*sizeof(uint32_t)) {
		logmsg("Cache file too small: \"%s\n", fn);
		free(buf);
		return NULL;
	}

    // Pull the two header values off the front...
    ubuf = (uint32_t*)buf;
	// Grab typeid.
	typeid = ubuf[0];
	// Grab original (uncompressed) length.
	origlen = ubuf[1];

	if (typeid != 1) {
		logmsg("File \"%s\" does not have typeid 1.\n", fn);
		free(buf);
		return NULL;
	}
	orig = malloc(origlen);
	if (!orig) {
		logmsg("Failed to allocate %i bytes for uncompressed cache file \"%s\".\n", (int)origlen, fn);
		free(buf);
		return NULL;
	}
	if (length)
		*length = origlen;
	//logmsg("Origlen as described by the cache file: %d\n", ulen);
	//logmsg("File size as determined by file_get_contents() = %d\n", len);
    rtn = uncompress(orig, &origlen, buf + 2*sizeof(uint32_t), len - 2*sizeof(uint32_t));
	free(buf);
	if (rtn != Z_OK) {
		logmsg("Failed to uncompress() file \"%s\": %s\n", fn, zError(rtn));
		free(orig);
		return NULL;
	}
	return orig;
}

int cache_save(render_args_t* args,
		const char* cachedomain, const char* key,
		const void* data, int length) {
	char fn[1024];
	FILE* fid;
	uint32_t typeid;
	unsigned char* compressed = NULL;
	uLong complen;
	uint32_t ulen;
	int rtn;

	if (!args->cachedir)
		return -1;
	if (cache_get_filename(args, cachedomain, key, fn, sizeof(fn))) {
		return -1;
	}
	fid = fopen(fn, "wb");
	if (!fid) {
		logmsg("Failed to open cache file \"%s\": %s\n", fn, strerror(errno));
		goto cleanup;
	}

	complen = compressBound(length);
	compressed = malloc(complen + 2*sizeof(uint32_t));
	if (!compressed) {
		logmsg("Failed to allocate compressed cache buffer\n");
		goto cleanup;
	}

	// first four bytes: type id
	typeid = 1;
	if (fwrite(&typeid, sizeof(uint32_t), 1, fid) != 1) {
		logmsg("Failed to write cache file \"%s\": %s\n", fn, strerror(errno));
		goto cleanup;
	}
	ulen = length;
	if (fwrite(&ulen, sizeof(uint32_t), 1, fid) != 1) {
		logmsg("Failed to write cache file \"%s\": %s\n", fn, strerror(errno));
		goto cleanup;
	}
	rtn = compress(compressed, &complen, data, length);
	if (rtn != Z_OK) {
		logmsg("compress() error: %s\n", zError(rtn));
		goto cleanup;
	}
	if (fwrite(compressed, 1, complen, fid) != complen) {
		logmsg("Failed to write cache file \"%s\": %s\n", fn, strerror(errno));
		goto cleanup;
	}
	if (fclose(fid)) {
		logmsg("Failed to close cache file \"%s\": %s\n", fn, strerror(errno));
		goto cleanup;
	}

	free(compressed);
	return 0;

cleanup:
	free(compressed);
	if (fid)
		fclose(fid);
	unlink(fn);
	return -1;
}


