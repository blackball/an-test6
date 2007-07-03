/*
  license:
    This file is part of the Astrometry.net suite.
    Copyright 2007 David W. Hogg.

    The Astrometry.net suite is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, version 2.

    The Astrometry.net suite is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Astrometry.net suite ; if not, write to the Free
    Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
    02110-1301 USA

  name:
    hoggLogGaussian

  purpose:
    evaluate a Gaussian with zero mean, in log
*/
#include <math.h>
#include "hoggMath.h"
double hoggLogGaussian(double xx,double var)
{
  return (-0.5*log(2.0*PI*var)-0.5*xx*xx/var);
}
