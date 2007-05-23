/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Michael Blanton, Keir Mierle, David W. Hogg,
  Sam Roweis and Dustin Lang.

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
#include <string.h>
#include <math.h>
#include <assert.h>

#include "common.h"

//#define M 64
#define M 128
#define BIG 1.0e30

#define FREEALL FREEVEC(sel);FREEVEC(isel)

int high_water_mark = 0;
float* past_data = NULL;

void RadixSort11(float* array, float* sorted, unsigned int n);

float dselip(unsigned long k, unsigned long n, float *arr)
{
	if (n > high_water_mark) {
		if (past_data)
			free(past_data);
		past_data = malloc(sizeof(float)*n);
		high_water_mark = n;
		//printf("dselip watermark=%lu\n",n);
	}
	RadixSort11(arr, past_data, n);
	return past_data[k];
}
