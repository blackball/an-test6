/*
  This file is part of the Astrometry.net suite.
  Copyright 2008 Dustin Lang.

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

#ifndef FIELD_H
#define FIELD_H

struct field_t {
    double* xy;
    int N;
};
typedef struct field_t field_t;


field_t* field_new(int N);

field_t* field_new_from_data(double* data, int N);

void field_free(field_t* f);

void field_set_xy(field_t* f, int i, const double* xy);

void field_get_xy(field_t* f, int i, double* xy);

double* field_create_copy(field_t* f);

#endif
