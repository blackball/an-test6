#include <stdio.h>
#include <math.h>

#include "ioutils.h"
#include "tilerender.h"
#include "render_image.h"
#include "sip_qfits.h"
#include "cairoutils.h"

char* image_dirs[] = {
	"/home/gmaps/gmaps-rdls/",
	"/home/gmaps/ontheweb-data/",
	"."
};

int render_image(unsigned char* img, render_args_t* args) {
	int i;
	int imw, imh;
	sip_t wcs;
	qfits_header* wcshead = NULL;
	double ra, dec;
	double imagex, imagey;
	int j,w;
    char fn[256];
    bool gotit = FALSE;
    unsigned char* userimg;

	fprintf(stderr, "render_image: Starting image render\n");

	/* Search in the image paths for the image */
	for (i=0; i<sizeof(image_dirs)/sizeof(char*); i++) {
		snprintf(fn, sizeof(fn), "%s/%s", image_dirs[i], args->imagefn);
		fprintf(stderr, "render_image: Trying file: %s\n", fn);
        if (!file_readable(fn))
            continue;
        gotit = TRUE;
        fprintf(stderr, "render_image: success: image %s opened\n", fn);
        break;
	}
	if (!gotit) {
		fprintf(stderr, "render_image: Failed to open image %s.\n", args->imagefn);
		return -1;
	}

    cairoutils_fake_ppm_init();
    userimg = cairoutils_read_ppm(fn, &imw, &imh);
    // note, this returns RGBA.
    if (!userimg) {
        fprintf(stderr, "Failed to read input image %s.\n", fn);
        return -1;
    }
    //cairoutils_rgba_to_argb32(img, W, H);

    // Read WCS.
	for (i=0; i<sizeof(image_dirs)/sizeof(char*); i++) {
		char fn[256];
		snprintf(fn, sizeof(fn), "%s/%s", image_dirs[i], args->imwcsfn);
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

    qfits_header_destroy(wcshead);

	// want to iterate over mercator space (ie, output pixels)
	w = args->W;
	for (j=0; j<args->H; j++) {
		for (i=0; i<w; i++) {
            int pppx,pppy;
			uchar* pix;
			ra = pixel2ra(i, args);
			dec = pixel2dec(j, args);
			pix = pixel(i, j, img, args);
			if (!sip_radec2pixelxy(&wcs, ra, dec, &imagex, &imagey)) {
				// transparent.
				pix[3] = 0;
				continue;
			}
			pppx = lround(imagex-1); // The -1 is because FITS uses 1-indexing for pixels. DOH
			pppy = lround(imagey-1);
			if (pppx >= 0 && pppx < imw &&
				pppy >= 0 && pppy < imh) {
				// nearest neighbour. bilinear is for weenies.
				pix[0] = userimg[4 * (imw * pppy + pppx) + 0];
				pix[1] = userimg[4 * (imw * pppy + pppx) + 1];
				pix[2] = userimg[4 * (imw * pppy + pppx) + 2];
                // pix[3] = min(255, pix[0]+pix[1]+pix[2]);
				pix[3] = 255;
			} else {
				// transparent.
				pix[3] = 0;
			}
		}
	}
    free(userimg);

	return 0;
}
