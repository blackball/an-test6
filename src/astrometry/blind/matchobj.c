/*
  This file is part of the Astrometry.net suite.
  Copyright 2010 Dustin Lang.

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

#include <sys/param.h>

#include "sip.h"
#include "matchobj.h"
#include "starutil.h"
#include "log.h"
#include "verify.h"

void matchobj_log_hit_miss(int* theta, int* testperm, int nbest, int nfield, int loglev) {
	int i;
	for (i=0; i<MIN(nfield, 100); i++) {
		int ti = (testperm ? theta[testperm[i]] : theta[i]);
		if (ti == THETA_DISTRACTOR) {
			loglevel(loglev, "-");
		} else if (ti == THETA_CONFLICT) {
			loglevel(loglev, "c");
		} else if (ti == THETA_FILTERED) {
			loglevel(loglev, "f");
		} else if (ti == THETA_BAILEDOUT) {
			loglevel(loglev, " bail");
			break;
		} else if (ti == THETA_STOPPEDLOOKING) {
			loglevel(loglev, " stopped");
			break;
		} else {
			loglevel(loglev, "+");
		}
		if (i+1 == nbest) {
			loglevel(loglev, "(best)");
		}
	}
}


void matchobj_print(MatchObj* mo, int loglvl) {
	double ra,dec;
	loglevel(loglvl, "  log-odds ratio %g (%g), %i match, %i conflict, %i distractors, %i index.\n",
			 mo->logodds, exp(mo->logodds), mo->nmatch, mo->nconflict, mo->ndistractor, mo->nindex);
	xyzarr2radecdeg(mo->center, &ra, &dec);
	loglevel(loglvl, "  RA,Dec = (%g,%g), pixel scale %g arcsec/pix.\n",
			 ra, dec, mo->scale);
	loglevel(loglvl, "  Hit/miss: ");
	matchobj_log_hit_miss(mo->theta, mo->testperm, mo->nbest, mo->nfield, loglvl);
	loglevel(loglvl, "\n");
}

void matchobj_compute_derived(MatchObj* mo) {
	int mx;
	int i;
	mx = 0;
	for (i=0; i<mo->dimquads; i++)
		mx = MAX(mx, mo->field[i]);
	mo->objs_tried = mx+1;
	if (mo->wcs_valid)
		mo->scale = tan_pixel_scale(&(mo->wcstan));
    mo->radius = deg2dist(mo->radius_deg);
	mo->nbest = mo->nmatch + mo->ndistractor + mo->nconflict;
}

/*void matchobj_log_verify_hit_miss(MatchObj* mo, int loglevel) {
 verify_log_hit_miss(mo->theta, mo->nbest, mo->nfield, loglevel);
 }
 */

const char* matchobj_get_index_name(MatchObj* mo) {
	if (!mo->index)
		return NULL;
	return mo->index->indexname;
}
