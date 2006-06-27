#ifndef HEALPIX_H
#define HEALPIX_H

#include <sys/types.h>

#include "keywords.h"

/**
   The HEALPix paper:

   http://www.journals.uchicago.edu/ApJ/journal/issues/ApJ/v622n2/61342/61342.web.pdf
*/


/**
   The following two functions convert (ra,dec) or (x,y,z) into the
   base-level healpix in the range [0, 11].

   RA, DEC in radians.
*/
__const int radectohealpix(double ra, double dec);

__const int xyztohealpix(double x, double y, double z);

/**
   The following functions convert (ra,dec) or (x,y,z) into a finely-
   pixelized healpix index, according to the HIERARCHICAL scheme,
   in the range [0, 12 Nside^2).

   RA, DEC in radians.
 */
__const uint radectohealpix_nside(double ra, double dec, uint Nside);

__const uint xyztohealpix_nside(double x, double y, double z, uint Nside);

uint healpix_get_neighbours_nside(uint pix, uint* neighbour, uint Nside);

void healpix_to_xyz(double dx, double dy, uint hp, uint Nside,
                    double* rx, double *ry, double *rz);

void healpix_decompose(uint finehp, uint* bighp, uint* x, uint* y, uint Nside);

__const uint healpix_compose(uint bighp, uint x, uint y, uint Nside);

#endif
