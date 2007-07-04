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
    hoggLeastSquareFit

  purpose:
    perform a straightforward least-square fit for parameters aa in the
    equation yy = xx.aa

  inputs:
    yy    - nn array of data points
    AA    - nn x mm matrix of model functions evaluated at points (read
            source for the element order)
    nn    - number of data points
    mm    - number of parameters

  outputs:
    [return value] - chi-squared
    xx             - mm array fit parameters

  comments:
    - Uses gsl library in a very clunky way.
    - Doesn't allow for points to have different weights.
    - Doesn't allow for points to have different errors.
    - Doesn't allow for nontrivial covariance matrix.
    - Doesn't do anything for you if are running very similar LSFs
      over and over again (ie, it keeps allocing and de-allocing, and
      it keeps stuffing and unstuffing vectors and matrices).
*/
#include <math.h>
#include <assert.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix_double.h>
#include <gsl/gsl_vector_double.h>
#include <gsl/gsl_blas.h>
#include "hoggMath.h"
double hoggLeastSquareFit(double *yy, double *AA, double *xx, int nn, int mm)
{
  int status, ii, jj, kk;
  double chisq=0.0, foo, bar;
  gsl_vector *RR, *BB, *XX, *resid;
  gsl_matrix *QQ;
  /* allocate space */
  RR = gsl_vector_alloc(mm);
  BB = gsl_vector_alloc(nn);
  XX = gsl_vector_alloc(mm);
  resid = gsl_vector_alloc(nn);
  QQ = gsl_matrix_alloc(nn, mm);
  /* pack matrices */
  for (kk=0; kk<nn; kk++){
    gsl_vector_set(BB, kk, yy[kk]);
    for (ii=0; ii<mm; ii++) gsl_matrix_set(QQ, kk, ii, AA[kk+nn*ii]);
  }
  /* solve equations */
  status = gsl_linalg_QR_decomp(QQ, RR);
  status = gsl_linalg_QR_lssolve(QQ, RR, BB, XX, resid);
  /* unpack matrices */
  for (ii=0; ii<mm; ii++) xx[ii] = gsl_vector_get(XX,ii);
  /* compute chisq */
  for (kk=0; kk<nn; kk++){
    foo = yy[kk];
    for (ii=0; ii<mm; ii++) foo -= xx[ii]*AA[kk+nn*ii];
    chisq += square(foo);
  }
  /* free space and return */
  gsl_vector_free(RR);
  gsl_vector_free(BB);
  gsl_vector_free(XX);
  gsl_vector_free(resid);
  gsl_matrix_free(QQ);
  return chisq;
}
