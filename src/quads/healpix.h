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

#ifndef HEALPIX_H
#define HEALPIX_H

#include <sys/types.h>

#include "keywords.h"

/**
   The HEALPix paper:

   http://www.journals.uchicago.edu/ApJ/journal/issues/ApJ/v622n2/61342/61342.web.pdf
*/


/**
   Converts a lexicographical healpix index into a ring index.
*/
Const int healpix_lex_to_ring(uint hp, uint Nside);

/**
   Decomposes a ring index into the ring number (rings contain healpixels of equal
   latitude) and longitude index.  Pixels within a ring have longitude index starting
   at zero for the first pixel with RA >= 0.  Different rings contain different numbers
   of healpixels.
 */
void healpix_ring_decompose(uint ring_index, uint Nside, uint* p_ring, uint* p_longind);

/**
   The following two functions convert (ra,dec) or (x,y,z) into the
   base-level healpix in the range [0, 11].

   RA, DEC in radians.
*/
Const int radectohealpix(double ra, double dec);

Const int xyztohealpix(double x, double y, double z);

Const double healpix_side_length_arcmin(uint Nside);

/**
   The following functions convert (ra,dec) or (x,y,z) into a finely-
   pixelized healpix index, according to the HIERARCHICAL scheme,
   in the range [0, 12 Nside^2).

   RA, DEC in radians.
 */
Const uint radectohealpix_nside(double ra, double dec, uint Nside);

Const uint xyztohealpix_nside(double x, double y, double z, uint Nside);

/**
   Finds the fine-scale healpixes neighbouring the given
   "pix".  Healpixes in the interior of a large healpix will
   have eight neighbours; pixels near the edges can have fewer.

   Make sure the "neighbour" array has space for at least eight
   neighbours.

   Returns the number of neighbours.
 */
uint healpix_get_neighbours_nside(uint pix, uint* neighbour, uint Nside);

void healpix_to_xyz(double dx, double dy, uint hp, uint Nside,
                    double* rx, double *ry, double *rz);

/**
   Given a fine-scale healpix number, computes the base healpix and (x,y)
   coordinates within that healpix.  Uses the Heirarchical numbering scheme
   for "Nside" values that are a power of 4, and the Lexicographical method
   otherwise.

   See also healpix_decompose_lex.
 */
void healpix_decompose(uint finehp, uint* bighp, uint* x, uint* y, uint Nside);

/**
   Returns the fine-scale healpix number given a large-scale healpix
   and (x,y) coordinates within the large healpix.

   This uses the Hierarchical numbering scheme if "Nside" is a power of 4,
   and a lexicographical number for other "Nside" values.

   See also healpix_compose_lex, which always uses the lexicographical scheme.
 */
Const uint healpix_compose(uint bighp, uint x, uint y, uint Nside);

/**
   Given a fine-scale healpix number, computes the large-scale healpix and (x,y)
   coordinates within that healpix.
 */
void healpix_decompose_lex(uint finehp, uint* bighp, uint* x, uint* y, uint Nside);

/**
   Computes the fine-scale healpix number of a pixel in large-scale healpix
   "bighp" and position (x,y) within the healpix.
 */
Const uint healpix_compose_lex(uint bighp, uint x, uint y, uint Nside);

void healpix_to_xyz_lex(double dx, double dy, uint hp, uint Nside,
						double* p_x, double *p_y, double *p_z);

void healpix_to_xyzarr_lex(double dx, double dy, uint hp, uint Nside,
						   double* xyz);

#endif
