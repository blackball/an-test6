/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

#ifndef PERMUTED_SORT_H
#define PERMUTED_SORT_H

/*
  This uses global variables and is NOT thread-safe.

  It's really an ugly hack, but it's one that was repeated in several places
  so needed to be extracted.  At least it's just ugly in one place now...
*/

void permuted_sort_set_params(void* realarray, int array_stride,
							  int (*compare)(const void*, const void*));

void permuted_sort(int* perm, int Nperm);


/*
  Some sort functions that might come in handy:
 */
int compare_doubles(const void* v1, const void* v2);

int compare_floats(const void* v1, const void* v2);

int compare_doubles_desc(const void* v1, const void* v2);

int compare_floats_desc(const void* v1, const void* v2);

#endif
