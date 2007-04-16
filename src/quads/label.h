/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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

unsigned char label_image_char(unsigned char* source, int W, int H, unsigned char* dest);
int  label_image_int(unsigned char* source, int W, int H, int* dest);

struct label_info {
	int label;
	int xlow;
	int xhigh;
	int ylow;
	int yhigh;
	int mass;
	float xcenter;
	float ycenter;

	int value;
};
typedef struct label_info label_info;

label_info* label_image_super(unsigned char* source, int W, int H, int* dest, int* N);

label_info* label_image_super_2(unsigned char* source, int W, int H, int* dest, int* N);

inline float roundness(label_info* label, int* labelim, int WW);
