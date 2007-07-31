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

#include <assert.h>

#include "constellations.h"

#include "stellarium-constellations.c"

int constellations_n() {
	return constellations_N;
}

static void check_const_num(int i) {
	assert(i >= 0);
	assert(i < constellations_N);
}

static void check_star_num(int i) {
	assert(i >= 0);
	assert(i < stars_N);
}

const char* constellations_get_shortname(int c) {
	check_const_num(c);
	return shortnames[c];
}

int constellations_get_nlines(int c) {
	check_const_num(c);
	return constellation_nlines[c];
}

il* constellations_get_lines(int c) {
	il* list;
	const int* lines;
	int i;
	check_const_num(c);
	list = il_new(16);
	lines = constellation_lines[c];
	for (i=0; i<2*constellation_nlines[c]; i++) {
		il_append(list, lines[i]);
	}
	return list;
}

il* constellations_get_unique_stars(int c) {
	il* uniq;
	const int* lines;
	int i;
	check_const_num(c);
	uniq = il_new(16);
	lines = constellation_lines[c];
	for (i=0; i<2*constellation_nlines[c]; i++) {
		il_insert_unique_ascending(uniq, lines[i]);
	}
	return uniq;
}

void constellations_get_line(int c, int i, int* ep1, int* ep2) {
	const int* lines;
	check_const_num(c);
	assert(i >= 0);
	assert(i < constellation_nlines[c]);
	lines = constellation_lines[c];
	*ep1 = lines[2*i];
	*ep2 = lines[2*i+1];
}

dl* constellations_get_lines_radec(int c) {
	dl* list;
	const int* lines;
	int i;
	check_const_num(c);
	list = dl_new(16);
	lines = constellation_lines[c];
	for (i=0; i<constellation_nlines[c]; i++) {
		int star = lines[i];
		const double* radec = star_positions + star*2;
		dl_append(list, radec[0]);
		dl_append(list, radec[1]);
	}
	return list;
}

void constellations_get_star_radec(int s, double* ra, double* dec) {
	const double* radec;
	check_star_num(s);
	radec = star_positions + s*2;
	*ra = radec[0];
	*dec = radec[1];
}

