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
    hoggLogsum

  purpose:
    Return log of sum of elements, passed in as logs.

  comments / bugs:
    - There is a *much* better way to do this with binary operations.
*/

#include <math.h>
double hoggLogsum(double * list,int nn)
{
  int ii;
  double maxl,sum;
  maxl= list[0];
  for (ii=1;ii<nn;ii++) maxl= (maxl > list[ii]) ? maxl : list[ii];
  sum= 0.0;
  for (ii=0;ii<nn;ii++) sum+= exp(list[ii]-maxl);
  return (log(sum)+maxl);
}
