/*
  This file is part of the Astrometry.net suite.
  Copyright 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <stdint.h>

#include <cairo.h>
#include <png.h>
#include <ppm.h>

// fires an ALPHA png out to fout
static void write_png(unsigned char * img, int w, int h, FILE* fout)
{
    png_bytepp image_rows;
    png_structp png_ptr;
    png_infop png_info;
    int n;

    image_rows = malloc(sizeof(png_bytep)*h);
    for (n = 0; n < h; n++)
        image_rows[n] = (unsigned char *) img + 4*n*w;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_info = png_create_info_struct(png_ptr);
    png_init_io(png_ptr, fout);
    png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);

    png_set_IHDR(png_ptr, png_info, w, h, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, png_info);

    png_write_image(png_ptr, image_rows);
    png_write_end(png_ptr, png_info);

    free(image_rows);

    png_destroy_write_struct(&png_ptr, &png_info);
}

static int writeout(const char* outfn, unsigned char* img, int W, int H, int ppm) {
    FILE* fout;
    int outstdout = !strcmp(outfn, "-");
    if (outstdout) {
        fout = stdout;
    } else {
        fout = fopen(outfn, "wb");
        if (!fout) {
            fprintf(stderr, "Failed to open output file %s: %s\n", outfn, strerror(errno));
            return -1;
        }
    }

    if (ppm) {
        int i;
        // PPM...
        fprintf(fout, "P6 %i %i %i\n", W, H, 255);
        for (i=0; i<(H*W); i++) {
            unsigned char* pix = img + 4*i;
            if (fwrite(pix, 1, 3, fout) != 3) {
                fprintf(stderr, "Failed to write pixels for PPM output.\n");
                return -1;
            }
        }
    } else {
        write_png(img, W, H, fout);
    }

    if (!outstdout) {
        if (fclose(fout)) {
            fprintf(stderr, "Failed to close output file %s: %s\n", outfn, strerror(errno));
            return -1;
        }
    }
	return 0;
}

int cairoutils_write_ppm(const char* outfn, unsigned char* img, int W, int H) {
    return writeout(outfn, img, W, H, 1);
}

int cairoutils_write_png(const char* outfn, unsigned char* img, int W, int H) {
    return writeout(outfn, img, W, H, 0);
}

void cairoutils_argb32_to_rgba(unsigned char* img, int W, int H) {
    int i;
    for (i=0; i<(H*W); i++) {
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
}

void cairoutils_rgba_to_argb32(unsigned char* img, int W, int H) {
    int i;
    for (i=0; i<(H*W); i++) {
        unsigned char r,g,b,a;
        uint32_t* ipix;
        r = img[4*i + 0];
        g = img[4*i + 1];
        b = img[4*i + 2];
        a = img[4*i + 3];
        ipix = (uint32_t*)(img + 4*i);
        *ipix = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

unsigned char* cairoutils_read_ppm(const char* infn, int* pW, int* pH) {
    int x,y;
    int W, H, format;
    pixval maxval;
    pixel* pixelrow;
    FILE* fin;
    int fromstdin;
    unsigned char* img;

    fromstdin = !strcmp(infn, "-");
    if (!fromstdin) {
        fin = fopen(infn, "rb");
        if (!fin) {
            fprintf(stderr, "Failed to read input image %s: %s\n", infn, strerror(errno));
            return NULL;
        }
    } else {
        fin = stdin;
    }
    ppm_readppminit(fin, &W, &H, &maxval, &format);
    pixelrow = ppm_allocrow(W);
    //printf("%i x %i, maxval %i, format 0x%x\n", C, R, maxval, format);
    if (pW) *pW = W;
    if (pH) *pH = H;

    // Allocate image.
    img = malloc(4 * W * H);
    if (!img) {
        fprintf(stderr, "Failed to allocate an image of size %ix%i x 4\n", W, H);
        return NULL;
    }
    for (y=0; y<H; y++) {
        ppm_readppmrow(fin, pixelrow, W, maxval, format);
        for (x=0; x<W; x++) {
            unsigned char a,r,g,b;
            pixel p;
            if (maxval == 255)
                p = pixelrow[x];
            else
                PPM_DEPTH(p, pixelrow[x], maxval, 255);
            a = 255;
            r = PPM_GETR(p);
            g = PPM_GETG(p);
            b = PPM_GETB(p);

            img[(y*W + x)*4 + 0] = r;
            img[(y*W + x)*4 + 1] = g;
            img[(y*W + x)*4 + 2] = b;
            img[(y*W + x)*4 + 3] = a;
        }
    }
    ppm_freerow(pixelrow);
    if (!fromstdin) {
        fclose(fin);
    }
    return img;
}

