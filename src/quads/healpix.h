#ifndef HEALPIX_H_
#define HEALPIX_H_

/**
   The HEALPix paper:

   http://www.journals.uchicago.edu/ApJ/journal/issues/ApJ/v622n2/61342/61342.web.pdf
*/

int radectohealpix(double ra, double dec);

int radectohealpix_nside(double ra, double dec, int nside);

int xyztohealpix(double x, double y, double z);

#endif
