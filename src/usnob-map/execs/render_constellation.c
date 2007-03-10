#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <sys/mman.h>

#include <cairo/cairo.h>

#include "tilerender.h"
#include "render_constellation.h"
#include "starutil.h"
#include "mathutil.h"
#include "bl.h"

static char* const_dirs[] = {
	".",
	"/usr/share/stellarium/data/sky_cultures/western", // Debian
	"/home/gmaps/usnob-map/execs" // FIXME
};

static char* hipparcos_fn = "hipparcos.fab";
static char* hip_dirs[] = {
	".",
	"/usr/share/stellarium/data", // Debian
	"/home/gmaps/usnob-map/execs"
};

// size of entries in Stellarium's hipparcos.fab file.
static int HIP_SIZE = 15;
// byte offset to the first element in Stellarium's hipparcos.fab file.
static int HIP_OFFSET = 4;

#if __BYTE_ORDER == __BIG_ENDIAN
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif
// Stellarium writes things in little-endian format.
static inline void swap(void* p, int nbytes) {
#if IS_BIG_ENDIAN
	int i;
	unsigned char* c = p;
	for (i=0; i<(nbytes/2); i++) {
		unsigned char tmp = c[i];
		c[i] = c[nbytes-(i+1)];
		c[nbytes-(i+1)] = tmp;
	}
#else
	return;
#endif
}
static inline void swap_32(void* p) {
	return swap(p, 4);
}

// ra,dec in degrees.
static void draw_segmented_line(double ra1, double dec1,
								double ra2, double dec2,
								int SEGS,
								cairo_t* cairo, render_args_t* args) {
	int i, s, k;
	double xyz1[3], xyz2[3];
	bool wrap;

	radecdeg2xyzarr(ra1, dec1, xyz1);
	radecdeg2xyzarr(ra2, dec2, xyz2);

	wrap = (fabs(ra1 - ra2) >= 180.0);

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
			xyzarr2radec(xyz, &ra, &dec);
			mx = ra2merc(ra);
			if (wrap) {
				// in the first pass we draw the left side (mx>0.5)
				if ((i==0) && (mx < 0.5)) mx += 1.0;
				// in the second pass we draw the right side (wx<0.5)
				if ((i==1) && (mx > 0.5)) mx -= 1.0;
			}
			px = xmerc2pixelf(mx, args);
			py = dec2pixelf(rad2deg(dec), args);

			if (s==0)
				cairo_move_to(cairo, px, py);
			else
				cairo_line_to(cairo, px, py);
		}
	}
}

typedef union {
	uint32_t i;
	float    f;
} intfloat;

static void hip_get_radec(unsigned char* hip, int star1,
						  double* ra, double* dec) {
	intfloat ifval;
	ifval.i = *((uint32_t*)(hip + HIP_SIZE * star1));
	swap_32(&ifval.i);
	*ra = ifval.f;
	// Stellarium stores RA in hours...
	*ra *= (360.0 / 24.0);
	ifval.i = *((uint32_t*)(hip + HIP_SIZE * star1 + 4));
	swap_32(&ifval.i);
	*dec = ifval.f;
}


int render_constellation(unsigned char* img, render_args_t* args) {
	int i;
	FILE* fconst = NULL;
	srand(0);

	fprintf(stderr, "render_constellation: Starting.\n");

	if (!args->constfn)
		args->constfn = "constellationship.fab";

	for (i=0; i<sizeof(const_dirs)/sizeof(char*); i++) {
		char fn[256];
		snprintf(fn, sizeof(fn), "%s/%s", const_dirs[i], args->constfn);
		fprintf(stderr, "render_constellation: Trying file: %s\n", fn);
		fconst = fopen(fn, "rb");
		if (fconst)
			break;
	}
	if (!fconst) {
		fprintf(stderr, "render_constellation: couldn't open any constellation files.\n");
		return -1;
	}

	FILE* fhip = NULL;
	for (i=0; i<sizeof(hip_dirs)/sizeof(char*); i++) {
		char fn[256];
		snprintf(fn, sizeof(fn), "%s/%s", hip_dirs[i], hipparcos_fn);
		fprintf(stderr, "render_constellation: Trying hip file: %s\n", fn);
		fhip = fopen(fn, "rb");
		if (fhip)
			break;
	}
	if (!fhip) {
		fprintf(stderr, "render_constellation: unhip\n");
		return -1;
	}

	{
		cairo_t* cairo;
		cairo_surface_t* target;
		double lw = args->linewidth;
		int i;
		uint32_t nstars;
		size_t mapsize;
		void* map;
		unsigned char* hip;
		int c;

		// first 32-bit int: 
		if (fread(&nstars, 4, 1, fhip) != 1) {
			fprintf(stderr, "render_constellation: failed to read nstars.\n");
			return -1;
		}
		swap_32(&nstars);
		fprintf(stderr, "render_constellation: Found %i Hipparcos stars\n", nstars);

		mapsize = nstars * HIP_SIZE + HIP_OFFSET;
		map = mmap(0, mapsize, PROT_READ, MAP_SHARED, fileno(fhip), 0);
		hip = ((unsigned char*)map) + HIP_OFFSET;
		//fprintf(stderr, "mapsize: %i\n", mapsize);

		target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32,
													 args->W, args->H, args->W*4);
		cairo = cairo_create(target);
		cairo_set_line_width(cairo, lw);
		cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
		cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
		cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
		//cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 1.0);

		for (c=0;; c++) {
			char shortname[16];
			int nlines;
			int i;
			il* uniqstars = il_new(16);
			double cmass[3];
			double ra,dec;
			double px,py;

			if (fscanf(fconst, "%s %d ", shortname, &nlines) != 2) {
				fprintf(stderr, "failed parse name+nlines\n");
				return -1;
			}
			//fprintf(stderr, "Name: %s.  Nlines %i.\n", shortname, nlines);

			unsigned char r = (rand() % 128) + 127;
			unsigned char g = (rand() % 128) + 127;
			unsigned char b = (rand() % 128) + 127;

			for (i=0; i<nlines; i++) {
				int star1, star2;
				double ra1, dec1, ra2, dec2;
				int SEGS=20;

				if (fscanf(fconst, " %d %d", &star1, &star2) != 2) {
					fprintf(stderr, "failed parse star1+star2\n");
					return -1;
				}

				il_insert_unique_ascending(uniqstars, star1);
				il_insert_unique_ascending(uniqstars, star2);

				// RA,DEC are the first two elements: 32-bit floats
				// (little-endian)
				hip_get_radec(hip, star1, &ra1, &dec1);
				hip_get_radec(hip, star2, &ra2, &dec2);

				cairo_set_source_rgba(cairo, r/255.0,g/255.0,b/255.0,0.8);
				draw_segmented_line(ra1, dec1, ra2, dec2, SEGS, cairo, args);
				cairo_stroke(cairo);
			}

			// find center of mass.
			cmass[0] = cmass[1] = cmass[2] = 0.0;
			for (i=0; i<il_size(uniqstars); i++) {
				double xyz[3];
				hip_get_radec(hip, il_get(uniqstars, i), &ra, &dec);
				radecdeg2xyzarr(ra, dec, xyz);
				cmass[0] += xyz[0];
				cmass[1] += xyz[1];
				cmass[2] += xyz[2];
			}
			cmass[0] /= il_size(uniqstars);
			cmass[1] /= il_size(uniqstars);
			cmass[2] /= il_size(uniqstars);
			xyzarr2radec(cmass, &ra, &dec);
			px = ra2pixel(rad2deg(ra), args);
			py = dec2pixel(rad2deg(dec), args);

			//cairo_select_font_face(cairo, "DejaVuSans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_select_font_face(cairo, "helvetica", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
			cairo_set_font_size(cairo, 10.0);
			cairo_move_to(cairo, px, py);
			cairo_show_text(cairo, shortname);

			fscanf(fconst, "\n");
			if (feof(fconst))
				break;
		}
		fprintf(stderr, "render_constellations: Read %i constellations.\n", c);

		munmap(map, mapsize);

		fclose(fconst);
		fclose(fhip);

		// Cairo's uint32 ARGB32 format is a little different than what we need,
		// which is uchar R,G,B,A.
		for (i=0; i<(args->H*args->W); i++) {
			unsigned char r,g,b,a;
			uint32_t ipix = *((uint32_t*)(img + 4*i));
			a = (ipix >> 24) & 0xff;
			r = (ipix >> 16) & 0xff;
			g = (ipix >>  8) & 0xff;
			b = (ipix      ) & 0xff;
			img[4*i + 0] = r;
			img[4*i + 1] = g;
			img[4*i + 2] = b;
			img[4*i + 3] = a;
		}

		cairo_surface_destroy(target);
		cairo_destroy(cairo);
	}

	return 0;
}
