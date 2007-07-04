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
    hoggExMaxStep

  purpose:
    1-d, 2-component expectation maximization, fixed at zero mean.

  inputs:
    xx      - 1-d data points
    varin   - variances (one per data point) for "inlier" gaussian
    nn      - number of points
    varout  - variance (one shared by all data) for "outlier" gaussian
    *logpin - log amplitude of inlier gaussian

  outputs:
    [return value]  - log (posterior?) likelihood
    *logpin         - updated inlier amplitude

  comments / bugs:
    - Highly specialized.
    - Each data point has its *own* inlier variance.  Yes, this is
      allowed.
*/

#include <math.h>
#include "hoggTweak.h"
double hoggExMaxStep(double *xx, double *varin, int nn, double varout,
		     double *logpin)
{
  int ii;
  double logpout, thisloglike, loglike=0.0, two[2], numerator;
  logpout= log(1.0-exp(*logpin));
  for (ii= 0; ii < nn; ii++){
    two[0]= *logpin+hoggLogGaussian(xx[ii],varin[ii]);
    two[1]= logpout+hoggLogGaussian(xx[ii],varout);
    thisloglike= hoggLogsum(two,2);
    loglike+= thisloglike;
    two[0]-= thisloglike;
    if (ii == 0){
      numerator= two[0];
    } else {
      two[1]= numerator;
      numerator= hoggLogsum(two,2);
    }
  }
  *logpin= numerator-log((double) nn);
  return loglike;
}
