#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <sys/param.h>

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
    float* counts;
    float* ink;
    int i, j, w;
    double *ravals, *decvals;

	logmsg("starting.\n");

    // Find images in the image directory.
    imagefiles = dir_get_contents(image_dir, NULL, TRUE, TRUE);
    if (!imagefiles) {
        logmsg("error getting image file list.\n");
    }
    logmsg("found %i files in image directory %s.\n", sl_size(imagefiles), image_dir);

    w = args->W;

    counts = calloc(args->W * args->H, sizeof(float));
    ink = calloc(3 * args->W * args->H, sizeof(float));

    ravals  = malloc(args->W * sizeof(double));
    decvals = malloc(args->H * sizeof(double));
    for (i=0; i<w; i++)
        ravals[i] = pixel2ra(i, args);
    for (j=0; j<args->H; j++)
        decvals[j] = pixel2dec(j, args);
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
        double ramin, ramax, decmin, decmax;
        double racenter, deccenter;
        int xvals[4];
        int yvals[4];
        int xlo, xhi, ylo, yhi;
        float pixeldensity, weight;

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
            logmsg("failed to read image file %s\n", imgfn);

        logmsg("Image %s is %i x %i.\n", imgfn, W, H);

        /*{
         char* outfn;
         asprintf(&outfn, "/tmp/out-%s.png", imgfn);
         cairoutils_write_png(outfn, userimg, W, H);
         free(outfn);
         }*/

        // find the bounds in RA,Dec of this image.
        sip_pixelxy2radec(&wcs, W/2, H/2, &racenter, &deccenter);
        xvals[0] = 0;
        yvals[0] = 0;
        xvals[1] = W;
        yvals[1] = 0;
        xvals[2] = W;
        yvals[2] = H;
        xvals[3] = 0;
        yvals[3] = H;
        decmin = decmax = deccenter;
        ramin = ramax = racenter;
        for (i=0; i<4; i++) {
            double dra;
            double dx = 0;
            sip_pixelxy2radec(&wcs, xvals[i], yvals[i], &ra, &dec);
            decmin = MIN(decmin, dec);
            decmax = MAX(decmax, dec);

            // ugh, does RA wrap around?

            // "x" is intermediate world coordinate, which points in the
            // direction of decreasing RA.  The CD matrix is ~ the derivative
            // of x wrt pixel coordinates
            dx = (wcs.wcstan.cd[0][0] * (xvals[i] - W/2)) +
                (wcs.wcstan.cd[0][1] * (yvals[i] - H/2));
            // expected change in RA (we just care about the sign)

            // FIXME - comments in sip.h suggest it should be:
            //dra = -dx;
            // - but in practice it should be this:
            dra = dx;

            // If we expected RA to be bigger but it isn't, or we expected it
            // to be smaller but it isn't, then we wrapped around.
            if ((dra * (ra - racenter)) < 0.0) {
                // wrap around!
                ramin = 0.0;
                ramax = 360.0;
            } else {
                ramin = MIN(ramin, ra);
                ramax = MAX(ramax, ra);
            }
        }
        logmsg("RA,Dec range for this image: (%g to %g, %g to %g)\n",
               ramin, ramax, decmin, decmax);

        xlo = floor(ra2pixelf(ramin, args));
        xhi = ceil (ra2pixelf(ramax, args));
        // increasing DEC -> decreasing Y pixel coord
        ylo = floor(dec2pixelf(decmax, args));
        yhi = ceil (dec2pixelf(decmin, args));

        logmsg("Pixel range: (%i to %i, %i to %i)\n", xlo, xhi, ylo, yhi);

        // clamp to image bounds
        xlo = MAX(0, MIN(args->W-1, xlo));
        xhi = MAX(0, MIN(args->W-1, xhi));
        ylo = MAX(0, MIN(args->H-1, ylo));
        yhi = MAX(0, MIN(args->H-1, yhi));

        logmsg("Clamped to pixel range: (%i to %i, %i to %i)\n", xlo, xhi, ylo, yhi);

        pixeldensity = 1.0 / square(sip_pixel_scale(&wcs));
        weight = pixeldensity;

        // iterate over mercator space (ie, output pixels)
        for (j=ylo; j<=yhi; j++) {
            dec = decvals[j];
            for (i=xlo; i<=xhi; i++) {
                int pppx,pppy;
                ra = ravals[i];
                if (!sip_radec2pixelxy(&wcs, ra, dec, &imagex, &imagey))
                    continue;
                pppx = lround(imagex-1); // The -1 is because FITS uses 1-indexing for pixels. DOH
                pppy = lround(imagey-1);
                if (pppx < 0 || pppx >= W || pppy < 0 || pppy >= H)
                    continue;
				// nearest neighbour. bilinear is for weenies.
				ink[3*(j*w + i) + 0] += userimg[4*(pppy*W + pppx) + 0] * weight;
				ink[3*(j*w + i) + 1] += userimg[4*(pppy*W + pppx) + 1] * weight;
				ink[3*(j*w + i) + 2] += userimg[4*(pppy*W + pppx) + 2] * weight;
				counts[j*w + i] += weight;
			}
		}
        free(userimg);
    }

	for (j=0; j<args->H; j++) {
		for (i=0; i<w; i++) {
            uchar* pix;
            pix = pixel(i, j, img, args);
            if (counts[j*w + i]) {
                pix[0] = MAX(0, MIN(255, ink[3 * (j*w + i) + 0] / counts[j*w + i]));
                pix[1] = MAX(0, MIN(255, ink[3 * (j*w + i) + 1] / counts[j*w + i]));
                pix[2] = MAX(0, MIN(255, ink[3 * (j*w + i) + 2] / counts[j*w + i]));
            } else {
                pix[0] = 0;
                pix[1] = 0;
                pix[2] = 0;
            }
            pix[3] = 255;
        }
    }

    sl_free2(imagefiles);

    free(ravals);
    free(decvals);

    free(counts);
    free(ink);

	return 0;
}
