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

#ifndef BLIND_WCS_H
#define BLIND_WCS_H

#include "qfits.h"
#include "matchobj.h"
#include "sip.h"

void blind_wcs_compute(MatchObj* mo, double* field, int nfield,
					   int* correspondences,
					   // outputs:
					   tan_t* wcstan);

qfits_header* blind_wcs_get_header(tan_t* wcstan);

#endif
