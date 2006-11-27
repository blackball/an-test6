/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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

#ifndef VERIFY_H
#define VERIFY_H

#include "kdtree.h"
#include "matchobj.h"
#include "xylist.h"
#include "bl.h"

/**
   correspondences: for each field object (up to the maximum index that
   .    we test, which is the minimum of the number of field objects and
   .    the number of index objects in the field), if there is an index
   .    object in range, then its index is set.  Otherwise, the array is
   .    untouched, so you should initialize it to, eg, -1 before calling.
 */
void verify_hit(kdtree_t* startree, MatchObj* mo, double* field,
				int nfield, double verify_dist2,
				int* pmatches, int* punmatches, int* pconflicts,
				il* indexstars, dl* bestd2s, int* correspondences);


#endif
