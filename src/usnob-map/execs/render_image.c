#include <stdio.h>
#include <math.h>

#include "tilerender.h"
#include "render_image.h"
#include "sip_qfits.h"

char* image_dirs[] = {
	"/home/gmaps/gmaps-rdls/",
	"/home/gmaps/ontheweb-data/"
};

int render_image(unsigned char* img, render_args_t* args) {

	fprintf(stderr, "render_image: Starting image render\n");

	/* Search in the image paths for the image */
	int i;
	FILE* f;
	for (i=0; i<sizeof(image_dirs)/sizeof(char*); i++) {
		char fn[256];
		snprintf(fn, sizeof(fn), "%s/%s", image_dirs[i], args->imagefn);
		fprintf(stderr, "render_image: Trying file: %s\n", fn);
		f = fopen(fn, "rb");
		if (f) {
			fprintf(stderr, "render_image: success: image %s opened\n", fn);
			break;
		} 
	}
	if (!f) {
		fprintf(stderr, "render_image: Failed to open image %s.\n", args->imagefn);
		return -1;
	}

	int imw, imh;
	// THIS IS COMPLETELY DEPENDENT ON IMAGEMAGICK's 'convert' PPM OUTPUT!
	// The actual ppm 'standard' allows any whitespace between the tokens
	// P6 w h vpp data, but for fscanf we only accept one form, which is
	// what 'convert' outptus.
	fscanf(f, "P6");
	fprintf(stderr, "render_image: position after fscanf: %ld\n", ftell(f));
	fscanf(f, "%d %d\n255\n", &imw, &imh);
	fprintf(stderr, "render_image: position after fscanf: %ld\n", ftell(f));
	fprintf(stderr, "render_image: got imwid=%d, imheight=%d\n", imw, imh);
	unsigned char* imbuf = malloc(imw*imh*3);
	fread(imbuf, 1, imw*imh*3, f);
	fprintf(stderr, "render_image: position after fscanf: %ld\n", ftell(f));

	// READ THE WCS
	// read wcs into tan structure. IGNORES SIP.
	sip_t wcs;


	qfits_header* wcshead = NULL;
	for (i=0; i<sizeof(image_dirs)/sizeof(char*); i++) {
		char fn[256];
		snprintf(fn, sizeof(fn), "%s/%s", image_dirs[i], args->wcsfn);
		fprintf(stderr, "render_image: Trying wcs file: %s\n", fn);
		wcshead = qfits_header_read(fn);
		if (wcshead) {
			fprintf(stderr, "render_image: wcs opened ok\n");
			break;
		} else {
			fprintf(stderr, "render_image: wcs didn't open\n");
		}
	}
	if (!wcshead) {
		fprintf(stderr, "render_image: couldn't open any wcs files\n");
		return -1;
	}

	if (!sip_read_header(wcshead, &wcs)) {
		fprintf(stderr, "render_image: failed to read WCS file.\n");
		return -1;
	}

	// want to iterate over mercator space 
	double ra, dec;
	double imagex, imagey;
	int j;
	int w = args->W;
	for (j=0; j<args->H; j++) {
		for (i=0; i<w; i++) {
			ra = pixel2ra(i, args);
			dec = pixel2dec(j, args);
			sip_radec2pixelxy(&wcs, ra, dec, &imagex, &imagey);
			int pppx = lround(imagex);
			int pppy = lround(imagey);
			uchar* pix = pixel(i, j, img, args);
			if (pppx >= 0 && pppx < imw &&
				pppy >= 0 && pppy < imh) {
				// nearest neighbour. bilinear is for weenies.
				pix[0] = imbuf[3 * (imw * pppy + pppx) + 0];
				pix[1] = imbuf[3 * (imw * pppy + pppx) + 1];
				pix[2] = imbuf[3 * (imw * pppy + pppx) + 2];
				pix[3] = 255;
			} else {
				// transparent.
				pix[3] = 0;
			}
		}
	}
	return 0;
}
