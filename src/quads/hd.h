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

#include "kdtree.h"

struct hd_entry_t {
    // J2000.0 degrees
    double ra;
    double dec;

    int hd;
};
typedef struct hd_entry_t hd_entry_t;

struct hd_catalog {
    char* fn;
    kdtree_t* kd;
};
typedef struct hd_catalog hd_catalog_t;

hd_catalog_t* henry_draper_open(const char* fn);

void henry_draper_close(hd_catalog_t* hd);

bl* henry_draper_get(hd_catalog_t* hd,
                     double racenter, double deccenter,
                     double radius_in_arcsec);

