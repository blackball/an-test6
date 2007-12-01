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
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <cairo.h>
#include <png.h>
#include <ppm.h>
#include <jpeglib.h>

#include "cairoutils.h"

enum imgformat {
    PPM,
    PNG,
    JPEG,
};
typedef enum imgformat imgformat;

char* cairoutils_get_color_name(int i) {
    switch (i) {
    case 0: return "red";
    case 1: return "green";
    case 2: return "brightred";
    }
    return NULL;
}

int cairoutils_parse_color(const char* color, float* r, float* g, float* b) {
    if (!strcmp(optarg, "red")) {
        *r = 1.0;
        *g = *b = 0.0;
    } else if (!strcmp(optarg, "brightred")) {
        *r = 1.0;
        *g = 0.0;
        *b = 0.2;
    } else if (!strcmp(optarg, "green")) {
        *r = *b = 0.0;
        *g = 1.0;
    } else {
        return -1;
    }
    return 0;
}

unsigned char* cairoutils_read_jpeg(const char* fn, int* pW, int* pH) {
    FILE* fid;
    unsigned char* img;
    if (!strcmp(fn, "-")) {
        return cairoutils_read_jpeg_stream(stdin, pW, pH);
    }
    fid = fopen(fn, "rb");
    if (!fid) {
        fprintf(stderr, "Failed to open file %s\n", fn);
        return NULL;
    }
    img = cairoutils_read_jpeg_stream(fid, pW, pH);
    fclose(fid);
    return img;
}

unsigned char* cairoutils_read_png(const char* fn, int* pW, int *pH) {
    FILE* fid;
    unsigned char* img;
    fid = fopen(fn, "rb");
    if (!fid) {
        fprintf(stderr, "Failed to open file %s\n", fn);
        return NULL;
    }
    img = cairoutils_read_png_stream(fid, pW, pH);
    fclose(fid);
    return img;
}

static void user_error_fn(png_structp png_ptr, png_const_charp error_msg) {
    fprintf(stderr, "PNG error: %s\n", error_msg);
}
static void user_warning_fn(png_structp png_ptr, png_const_charp warning_msg) {
    fprintf(stderr, "PNG warning: %s\n", warning_msg);
}

unsigned char* cairoutils_read_png_stream(FILE* fid, int* pW, int *pH) {
    png_structp ping;
    png_infop info;
    png_uint_32 W, H;
    unsigned char* outimg;
    png_bytepp rows;
    int j;
    int bitdepth, color_type, interlace;

    ping = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
                                  user_error_fn, user_warning_fn);
    if (!ping)
        return NULL;
    info = png_create_info_struct(ping);
    if (!info) {
        png_destroy_read_struct(&ping, NULL, NULL);
        return NULL;
    }

    png_init_io(ping, fid);
    png_read_info(ping, info);
    png_get_IHDR(ping, info, &W, &H, &bitdepth, &color_type,
                 &interlace, NULL, NULL);

    // see cairo's cairo-png.c
    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(ping);
    if (color_type == PNG_COLOR_TYPE_GRAY && bitdepth < 8)
        png_set_gray_1_2_4_to_8(ping);
    if (png_get_valid(ping, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(ping);
    if (bitdepth == 16)
        png_set_strip_16(ping);
    if (bitdepth < 8)
        png_set_packing(ping);
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(ping);
    if (interlace != PNG_INTERLACE_NONE)
        png_set_interlace_handling(ping);
    png_set_filler(ping, 0xff, PNG_FILLER_AFTER);
    png_read_update_info(ping, info);

    outimg = malloc(4 * W * H);
    rows = malloc(H * sizeof(png_bytep));
    if (!outimg || !rows) {
        free(outimg);
        png_destroy_read_struct(&ping, &info, NULL);
        return NULL;
    }
    for (j=0; j<H; j++)
        rows[j] = outimg + j*4*W;

    png_read_image(ping, rows);
    png_read_end (ping, info);

    png_destroy_read_struct(&ping, &info, NULL);
    free(rows);

    if (pW) *pW = W;
    if (pH) *pH = H;

    return outimg;
}

unsigned char* cairoutils_read_jpeg_stream(FILE* fid, int* pW, int* pH) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPLE* buffer;
    int row_stride;
    unsigned char* outimg;
    int W, H;
    int i, j;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fid);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    row_stride = cinfo.output_width * cinfo.output_components;
    buffer = malloc(row_stride * sizeof(JSAMPLE));
    W = cinfo.output_width;
    H = cinfo.output_height;
    outimg = malloc(4 * W * H);
    for (j=0; j<H; j++) {
        jpeg_read_scanlines(&cinfo, &buffer, 1);
        for (i=0; i<W; i++) {
            if (cinfo.output_components == 3) {
                outimg[4 * (j*W + i) + 0] = buffer[3*i + 0];
                outimg[4 * (j*W + i) + 1] = buffer[3*i + 1];
                outimg[4 * (j*W + i) + 2] = buffer[3*i + 2];
                outimg[4 * (j*W + i) + 3] = 255;
            } else if (cinfo.output_components == 1) {
                outimg[4 * (j*W + i) + 0] = buffer[i];
                outimg[4 * (j*W + i) + 1] = buffer[i];
                outimg[4 * (j*W + i) + 2] = buffer[i];
                outimg[4 * (j*W + i) + 3] = 255;
            }
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free(buffer);

    if (pW) *pW = W;
    if (pH) *pH = H;

    return outimg;
}

static int streamout(FILE* fout, unsigned char* img, int W, int H, int format) {
    if (format == PPM) {
        // PPM...
        int i;
        fprintf(fout, "P6 %i %i %i\n", W, H, 255);
        for (i=0; i<(H*W); i++) {
            unsigned char* pix = img + 4*i;
            if (fwrite(pix, 1, 3, fout) != 3) {
                fprintf(stderr, "Failed to write pixels for PPM output: %s\n", strerror(errno));
                return -1;
            }
        }
    } else if (format == PNG) {
        // fires an ALPHA png out to fout
        png_bytepp image_rows;
        png_structp png_ptr;
        png_infop png_info;
        int n;
        
        image_rows = malloc(sizeof(png_bytep)*H);
        for (n = 0; n < H; n++)
            image_rows[n] = (unsigned char *) img + 4*n*W;
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        png_info = png_create_info_struct(png_ptr);
        png_init_io(png_ptr, fout);
        png_set_filter(png_ptr, 0, PNG_FILTER_NONE);
        png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
        png_set_IHDR(png_ptr, png_info, W, H, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
        png_write_info(png_ptr, png_info);
        png_write_image(png_ptr, image_rows);
        png_write_end(png_ptr, png_info);
        free(image_rows);
        png_destroy_write_struct(&png_ptr, &png_info);
    } else if (format == JPEG) {
        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;
        JSAMPLE* buffer;
        int r;
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_compress(&cinfo);
        jpeg_stdio_dest(&cinfo, fout);
        cinfo.image_width = W;
        cinfo.image_height = H;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults(&cinfo);
        jpeg_set_colorspace(&cinfo, JCS_RGB);
        jpeg_simple_progression(&cinfo);
        jpeg_set_linear_quality(&cinfo, 70, FALSE);
        jpeg_start_compress(&cinfo, TRUE);
        buffer = malloc(W * 3);
        for (r=0; r<H; r++) {
            int i;
            for (i=0; i<W; i++) {
                buffer[i*3 + 0] = img[(r*W + i)*4 + 0];
                buffer[i*3 + 1] = img[(r*W + i)*4 + 1];
                buffer[i*3 + 2] = img[(r*W + i)*4 + 2];
            }
            jpeg_write_scanlines(&cinfo, &buffer, 1);
        }
        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);
        free(buffer);
    }
    return 0;
}

static int writeout(const char* outfn, unsigned char* img, int W, int H, int format) {
    FILE* fout;
    int rtn;
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
    rtn = streamout(fout, img, W, H, format);
    if (rtn)
        return rtn;
    if (!outstdout) {
        if (fclose(fout)) {
            fprintf(stderr, "Failed to close output file %s: %s\n", outfn, strerror(errno));
            return -1;
        }
    }
	return 0;
}

void cairoutils_fake_ppm_init() {
    char* fake_args[] = {"cairoutils"};
    int fake_argc = 1;
    ppm_init(&fake_argc, fake_args);
}

int cairoutils_write_ppm(const char* outfn, unsigned char* img, int W, int H) {
    return writeout(outfn, img, W, H, PPM);
}

int cairoutils_write_png(const char* outfn, unsigned char* img, int W, int H) {
    return writeout(outfn, img, W, H, PNG);
}

int cairoutils_write_jpeg(const char* outfn, unsigned char* img, int W, int H) {
    return writeout(outfn, img, W, H, JPEG);
}

int cairoutils_stream_ppm(FILE* fout, unsigned char* img, int W, int H) {
    return streamout(fout, img, W, H, PPM);
}

int cairoutils_stream_png(FILE* fout, unsigned char* img, int W, int H) {
    return streamout(fout, img, W, H, PNG);
}

int cairoutils_stream_jpeg(FILE* fout, unsigned char* img, int W, int H) {
    return streamout(fout, img, W, H, JPEG);
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

unsigned char* cairoutils_read_ppm_stream(FILE* fin, int* pW, int* pH) {
    int x,y;
    int W, H, format;
    pixval maxval;
    pixel* pixelrow;
    unsigned char* img;

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
    return img;
}

unsigned char* cairoutils_read_ppm(const char* infn, int* pW, int* pH) {
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

    img = cairoutils_read_ppm_stream(fin, pW, pH);

    if (!fromstdin) {
        fclose(fin);
    }
    return img;
}

