/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Keir Mierle.

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

#ifndef SIP_UTIL_H

// libwcs:
#include "fitsfile.h"
#include "wcs.h"

#include "sip.h"
#include "fitsio.h"

typedef struct WorldCoor wcs_t;
void copy_wcs_into_sip(wcs_t* wcs, sip_t* sip);
wcs_t* get_wcs_from_fits_header(char* buf);
wcs_t* load_wcs_from_fitsio(fitsfile* fptr);
sip_t* load_sip_from_fitsio(fitsfile* fptr);

#endif //SIP_UTIL_H
