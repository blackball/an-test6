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

#ifndef AN_CATALOG_H
#define AN_CATALOG_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>


#include "qfits.h"
#include "ioutils.h"
#include "fitstable.h"

#define AN_FILETYPE_ANCAT "ANCAT"

enum an_sources {
	AN_SOURCE_UNKNOWN,
	AN_SOURCE_USNOB,
	AN_SOURCE_TYCHO2,
	AN_SOURCE_2MASS
};

struct an_observation {
	unsigned char catalog;
	unsigned char band;
	int id;
	float mag;
	float sigma_mag;
};
typedef struct an_observation an_observation;

#define AN_N_OBSERVATIONS 5

struct an_entry {
	uint64_t id;
    // POSITION
    // In degrees
	double ra;
    // In degrees
	double dec;
    // In degrees
	float sigma_ra;
    // In degrees
	float sigma_dec;

    // PROPER MOTION
    // In arcsec/yr.
	float motion_ra;
    // In arcsec/yr.
	float motion_dec;
    // In arcsec/yr
	float sigma_motion_ra;
    // In arcsec/yr
	float sigma_motion_dec;

	uint8_t nobs;
	an_observation obs[AN_N_OBSERVATIONS];
};
typedef struct an_entry an_entry;

#define AN_FITS_COLUMNS 35

struct an_catalog {
    //
    fitstable_t* ft;
	// buffered reading
	bread br;
};
typedef struct an_catalog an_catalog;

an_catalog* an_catalog_open(char* fn);

an_catalog* an_catalog_open_for_writing(char* fn);

qfits_header* an_catalog_get_primary_header(const an_catalog* cat);

int an_catalog_sync(an_catalog* cat);

int an_catalog_write_headers(an_catalog* cat);

int an_catalog_fix_headers(an_catalog* cat);

int an_catalog_read_entries(an_catalog* cat, uint offset,
							uint count, an_entry* entries);

an_entry* an_catalog_read_entry(an_catalog* cat);

int an_catalog_count_entries(an_catalog* cat);

int an_catalog_close(an_catalog* cat);

int an_catalog_write_entry(an_catalog* cat, an_entry* entry);

int64_t an_catalog_get_id(int catversion, int64_t starid);

void an_catalog_set_blocksize(an_catalog* cat, int block);

#endif
