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

#ifndef CAIRO_UTILS_H
#define CAIRO_UTILS_H

#include <stdio.h>

void cairoutils_argb32_to_rgba(unsigned char* img, int W, int H);

void cairoutils_rgba_to_argb32(unsigned char* img, int W, int H);

// You must call ppm_init()
unsigned char* cairoutils_read_ppm(const char* infn, int* pW, int* pH);

int cairoutils_write_ppm(const char* outfn, unsigned char* img, int W, int H);

int cairoutils_write_png(const char* outfn, unsigned char* img, int W, int H);

int cairoutils_stream_ppm(FILE* fout, unsigned char* img, int W, int H);

int cairoutils_stream_png(FILE* fout, unsigned char* img, int W, int H);

int cairoutils_parse_color(const char* color, float* r, float* g, float* b);

char* cairoutils_get_color_name(int i);

#endif

