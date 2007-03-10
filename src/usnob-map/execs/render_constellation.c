#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <sys/mman.h>

#include <cairo/cairo.h>

#include "tilerender.h"
#include "render_constellation.h"
#include "sip_qfits.h"

static char* const_dirs[] = {
	".",
	"/home/gmaps/usnob-map/execs" // FIXME
};

static char* hipparcos_fn = "/home/gmaps/usnob-map/execs/hipparcos.fab";
//static char* hipparcos_fn = "hipparcos.fab";

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

	FILE* fhip = fopen(hipparcos_fn, "rb");
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

		if (fread(&nstars, 4, 1, fhip) != 1) {
			fprintf(stderr, "render_constellation: failed to read nstars.\n");
			return -1;
		}
		fprintf(stderr, "render_constellation: Found %i Hipparcos stars\n", nstars);

		mapsize = nstars * 15 + 4;
		map = mmap(0, mapsize, PROT_READ, MAP_SHARED, fileno(fhip), 0);
		hip = ((unsigned char*)map) + 4;

		target = cairo_image_surface_create_for_data(img, CAIRO_FORMAT_ARGB32,
													 args->W, args->H, args->W*4);
		cairo = cairo_create(target);
		cairo_set_line_width(cairo, lw);
		cairo_set_line_join(cairo, CAIRO_LINE_JOIN_BEVEL);
		cairo_set_antialias(cairo, CAIRO_ANTIALIAS_GRAY);
		cairo_set_source_rgb(cairo, 1.0, 1.0, 1.0);
		//cairo_set_source_rgba(cairo, 1.0, 1.0, 1.0, 1.0);

		while (1) {
			char shortname[16];
			int nlines;
			int i;
			if (fscanf(fconst, "%s %d ", shortname, &nlines) != 2) {
				fprintf(stderr, "failed parse name+nlines\n");
				return -1;
			}

			unsigned char r = (rand() % 128) + 127;
			unsigned char g = (rand() % 128) + 127;
			unsigned char b = (rand() % 128) + 127;

			for (i=0; i<nlines; i++) {
				int star1, star2;
				//uint32_t ival;
				float ra1, dec1, ra2, dec2;
				double px1, py1, px2, py2;
				if (fscanf(fconst, " %d %d", &star1, &star2) != 2) {
					fprintf(stderr, "failed parse star1+star2\n");
					return -1;
				}
				// FIXME: ENDIAN
				//ival = *((uint32_t*)(map + 15 * star1));
				
				ra1  = *((float*)(hip + 15 * star1));
				dec1 = *((float*)(hip + 15 * star1 + 4));
				ra2  = *((float*)(hip + 15 * star2));
				dec2 = *((float*)(hip + 15 * star2 + 4));

				ra1 *= (360.0 / 24.0);
				ra2 *= (360.0 / 24.0);

				// Fix wraparound
				if (fabs(ra1-ra2) > 50) {
					double ramin = fmin(ra1,ra2);
					double ramax = fmin(ra1,ra2);
					ra1 = ramax - 360.0;
					ra2 = ramin;
				}

				px1 = ra2pixel(ra1, args);
				px2 = ra2pixel(ra2, args);
				py1 = dec2pixel(dec1, args);
				py2 = dec2pixel(dec2, args);

				cairo_set_source_rgba(cairo, r/255.0,g/255.0,b/255.0,0.8);

				cairo_move_to(cairo, px1, py1);
				cairo_line_to(cairo, px2, py2);
				cairo_stroke(cairo);
				
			}
			fscanf(fconst, "\n");
			if (feof(fconst))
				break;
		}

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
