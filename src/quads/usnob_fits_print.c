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

#include <stdio.h>

#include "usnob_fits.h"

int main(int argc, char** args) {
	char* fn;
	int i;
	usnob_fits* usnob;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <filename>\n", args[0]);
		exit(-1);
	}

	fn = args[1];

	usnob = usnob_fits_open(fn);
	if (!usnob) {
		fprintf(stderr, "Couldn't open %s\n", fn);
		exit(-1);
	}

	for (i=0; i<usnob->table->nc; i++) {
		usnob_entry entry;
		int ob;
		if (usnob_fits_read_entries(usnob, i, 1, &entry)) {
			fprintf(stderr, "Failed reading %i.\n", i);
			exit(-1);
		}
		printf("RA, DEC (%g, %g), mu_{RA,DEC} (%g, %g), sigma_mu_{RA,DEC} (%g, %g)\n",
			   entry.ra, entry.dec, entry.mu_ra, entry.mu_dec,
			   entry.sigma_mu_ra, entry.sigma_mu_dec);
		printf("    sigma_fit_{RA,DEC} (%g, %g), sigma_{RA,DEC} (%g, %g), mu_prob %g\n",
			   entry.sigma_ra_fit, entry.sigma_dec_fit,
			   entry.sigma_ra, entry.sigma_dec,
			   entry.mu_prob);
		printf("    epoch %g, ndetections %i\n",
			   entry.epoch, (int)entry.ndetections);
		printf("    diffraction = %i, ", entry.diffraction_spike);
		printf("motion = %i, ", entry.motion_catalog);
		printf("ys4 = %i\n", entry.ys4);
		printf("    USNO-B id = %u (slice=%i, index=%i)\n",
			   entry.usnob_id, usnob_get_slice(&entry), usnob_get_index(&entry));
		for (ob=0; ob<5; ob++) {
			struct observation o = entry.obs[ob];
			printf("    obs %i: mag %g, star/galaxy %i, calib %i, pmm %u\n",
				   ob, o.mag, (int)o.star_galaxy, (int)o.calibration,
				   o.pmmscan);
			printf("            survey %i, field %i, {xi,eta}_resid (%g, %g)\n",
				   (int)o.survey, (int)o.field, o.xi_resid, o.eta_resid);
		}
	}

	usnob_fits_close(usnob);
	return 0;
}
