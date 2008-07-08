/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2008 Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <stdio.h>
#include <stdlib.h>

#include "sip_qfits.h"
#include "an-bool.h"
#include "qfits.h"
#include "starutil.h"
#include "bl.h"
#include "xylist.h"
#include "rdlist.h"
#include "errors.h"

int wcs_rd2xy(const char* wcsfn, const char* rdlsfn, const char* xylsfn,
              const char* racol, const char* deccol, bool forcetan,
              il* fields) {
	xylist_t* xyls = NULL;
	rdlist_t* rdls = NULL;
	sip_t sip;
	int i;
    bool alloced_fields = FALSE;

	// read WCS.
	if (forcetan) {
		memset(&sip, 0, sizeof(sip_t));
		if (!tan_read_header_file(wcsfn, &(sip.wcstan))) {
			ERROR("Failed to parse TAN header from %s", wcsfn);
			return -1;
		}
	} else {
		if (!sip_read_header_file(wcsfn, &sip)) {
			ERROR("Failed to parse SIP header from %s", wcsfn);
            return -1;
		}
	}

	// read RDLS.
	rdls = rdlist_open(rdlsfn);
	if (!rdls) {
		ERROR("Failed to read an RA,Dec list from file %s", rdlsfn);
        goto bailout;
	}
	if (racol)
        rdlist_set_raname(rdls, racol);
	if (deccol)
		rdlist_set_decname(rdls, deccol);

	// write XYLS.
	xyls = xylist_open_for_writing(xylsfn);
	if (!xyls) {
		ERROR("Failed to open file %s to write XYLS", xylsfn);
        goto bailout;
	}
	if (xylist_write_primary_header(xyls)) {
		ERROR("Failed to write header to XYLS file %s", xylsfn);
        goto bailout;
	}

    if (!fields) {
        alloced_fields = TRUE;
        fields = il_new(16);
    }
	if (!il_size(fields)) {
		// add all fields.
		int NF = rdlist_n_fields(rdls);
		for (i=1; i<=NF; i++)
			il_append(fields, i);
	}

	for (i=0; i<il_size(fields); i++) {
		int fieldnum = il_get(fields, i);
		int j;
        starxy_t xy;
        rd_t rd;

        if (!rdlist_read_field_num(rdls, fieldnum, &rd)) {
			ERROR("Failed to read rdls field %i", fieldnum);
            goto bailout;
        }

        starxy_alloc_data(&xy, rd_n(&rd), FALSE, FALSE);

		if (xylist_write_header(xyls)) {
			ERROR("Failed to write xyls field header");
            goto bailout;
		}

		for (j=0; j<rd_n(&rd); j++) {
			double x, y, ra, dec;
            ra  = rd_getra (&rd, j);
            dec = rd_getdec(&rd, j);
			if (!sip_radec2pixelxy(&sip, ra, dec, &x, &y)) {
				ERROR("Point RA,Dec = (%g,%g) projects to the opposite side of the sphere", ra, dec);
				continue;
			}
            starxy_set(&xy, j, x, y);
		}
        if (xylist_write_field(xyls, &xy)) {
            ERROR("Failed to write xyls field");
            goto bailout;
        }
		if (xylist_fix_header(xyls)) {
            ERROR("Failed to fix xyls field header");
            goto bailout;
		}
        xylist_next_field(xyls);

        starxy_free_data(&xy);
        rd_free_data(&rd);
	}

	if (xylist_fix_primary_header(xyls) ||
		xylist_close(xyls)) {
		ERROR("Failed to fix header of XYLS file");
        goto bailout;
	}

	if (rdlist_close(rdls)) {
		ERROR("Failed to close RDLS file");
        goto bailout;
	}
    return 0;

 bailout:
    if (alloced_fields)
        il_free(fields);
    if (rdls)
        rdlist_close(rdls);
    if (xyls)
        xylist_close(xyls);
    return -1;
}

