#include <stdio.h>
#include <math.h>
#include <stdarg.h>

#include "ioutils.h"
#include "tilerender.h"
#include "render_image.h"
#include "sip_qfits.h"
#include "cairoutils.h"
#include "keywords.h"

//char* image_dir = "/home/gmaps/apod-solves";
//char* wcs_dir = "/home/gmaps/apod-solves";

//char* image_dir = "/u/dstn/raid2/APOD/dstn/2006-solved";

char* image_dir = "/tmp/imgs";

static void
ATTRIB_FORMAT(printf,1,2)
logmsg(char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "render_collection: ");
    vfprintf(stderr, format, args);
    va_end(args);
}

int render_collection(unsigned char* img, render_args_t* args) {
    int I;
    sl* imagefiles;
    int* counts;
    int* ink;
    int i, j, w;

	logmsg("starting.\n");

    // Find images in the image directory.
    imagefiles = dir_get_contents(image_dir, NULL, TRUE, TRUE);
    if (!imagefiles) {
        logmsg("error getting image file list.\n");
    }
    logmsg("found %i files in image directory %s.\n", sl_size(imagefiles), image_dir);

    counts = calloc(args->W * args->H, sizeof(int));
    ink = calloc(3 * args->W * args->H, sizeof(int));

    w = args->W;
    for (I=0; I<sl_size(imagefiles); I++) {
        char* imgfn;
        char* wcsfn;
        char* dot;
        bool jpeg, png;
        qfits_header* hdr;
        sip_t wcs;
        unsigned char* userimg = NULL;
        int W, H;
        sip_t* res;
        double ra, dec;
        double imagex, imagey;

        imgfn = sl_get(imagefiles, I);
        dot = strrchr(imgfn, '.');
        if (!dot) {
            logmsg("filename %s has no suffix.\n", imgfn);
            continue;
        }
        jpeg = png = FALSE;
        if (!strcasecmp(dot+1, "jpg") ||
            !strcasecmp(dot+1, "jpeg")) {
            jpeg = TRUE;
        } else if (!strcasecmp(dot+1, "png")) {
            png = TRUE;
        } else {
            //logmsg("filename %s doesn't end with jpeg or png.\n", imgfn);
            continue;
        }
        asprintf_safe(&wcsfn, "%.*s.wcs", dot - imgfn, imgfn);

        hdr = qfits_header_read(wcsfn);
        if (!hdr) {
            logmsg("failed to read WCS header from %s\n", wcsfn);
            continue;
        }
        free(wcsfn);
        res = sip_read_header(hdr, &wcs);
        qfits_header_destroy(hdr);
        if (!res) {
            logmsg("failed to parse SIP header from %s\n", wcsfn);
            continue;
        }

        if (jpeg)
            userimg = cairoutils_read_jpeg(imgfn, &W, &H);
        else if (png)
            userimg = cairoutils_read_png(imgfn, &W, &H);

        if (!userimg)
            logmsg("failod to read image file %s\n", imgfn);

        logmsg("Image %s is %i x %i.\n", imgfn, W, H);

        // want to iterate over mercator space (ie, output pixels)
        for (j=0; j<args->H; j++) {
            for (i=0; i<w; i++) {
                int pppx,pppy;
                ra = pixel2ra(i, args);
                dec = pixel2dec(j, args);
                if (!sip_radec2pixelxy(&wcs, ra, dec, &imagex, &imagey))
                    continue;
                pppx = lround(imagex-1); // The -1 is because FITS uses 1-indexing for pixels. DOH
                pppy = lround(imagey-1);
                if (pppx < 0 || pppx >= W || pppy < 0 || pppy >= H)
                    continue;
				// nearest neighbour. bilinear is for weenies.
				ink[3*(j*w + i) + 0] += userimg[4 * (W * pppy + pppx) + 0];
				ink[3*(j*w + i) + 1] += userimg[4 * (W * pppy + pppx) + 1];
				ink[3*(j*w + i) + 2] += userimg[4 * (W * pppy + pppx) + 2];

                // FIXME - pixel density
				counts[j*w + i] += 1;
			}
		}
        free(userimg);
    }

	for (j=0; j<args->H; j++) {
		for (i=0; i<w; i++) {
            uchar* pix;
            pix = pixel(i, j, img, args);
            if (counts[j*w + i]) {
                pix[0] = ink[3 * (j*w + i) + 0] / counts[j*w + i];
                pix[1] = ink[3 * (j*w + i) + 1] / counts[j*w + i];
                pix[2] = ink[3 * (j*w + i) + 2] / counts[j*w + i];
            } else {
                pix[0] = 0;
                pix[1] = 0;
                pix[2] = 0;
            }
            pix[3] = 255;
        }
    }

    sl_free(imagefiles);

    free(counts);
    free(ink);

	return 0;
}
