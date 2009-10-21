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

#ifndef VERIFY_H
#define VERIFY_H

#include "kdtree.h"
#include "matchobj.h"
#include "bl.h"
#include "starkd.h"
#include "sip.h"
#include "bl.h"
#include "starxy.h"

struct verify_field_t {
    const starxy_t* field;
	// this copy is normal.
    double* xy;
	// this copy is permuted by the kdtree
    double* fieldcopy;
    kdtree_t* ftree;
};
typedef struct verify_field_t verify_field_t;

/*
  Uses the following entries in the "mo" struct:
  -wcs_valid
  -wcstan
  -center
  -radius
  -field[]
  -star[]
  -dimquads

  Sets the following:
  -nfield
  -noverlap
  -nconflict
  -nindex
  -(matchobj_compute_derived() values)
  -logodds
  -corr_field
  -corr_index
 */
void verify_hit(startree_t* skdt,
				int index_cutnside,
                MatchObj* mo,
                sip_t* sip, // if non-NULL, verify this SIP WCS.
                verify_field_t* vf,
                double verify_pix2,
                double distractors,
                double fieldW,
                double fieldH,
                double logratio_tobail,
                double logratio_toaccept,
                double logratio_tostoplooking,
                bool distance_from_quad_bonus,
                bool fake_match);

// Distractor
#define THETA_DISTRACTOR -1
// Conflict
#define THETA_CONFLICT -2

void verify_apply_ror(double* refxy, int* starids, int* p_NR,
					  int index_cutnside,
					  MatchObj* mo,
					  verify_field_t* vf,
					  double pix2,
					  double distractors,
					  double fieldW,
					  double fieldH,
					  bool do_gamma, bool fake_match,
					  double** p_testxy, double** p_sigma2s,
					  int* p_NT, int** p_perm, double* p_effA,
					  int* p_uninw, int* p_uninh);

/**
 Returns the best log-odds encountered.
 */
double verify_star_lists(const double* refxys, int NR,
						 const double* testxys, const double* testsigma2s, int NT,
						 double effective_area,
						 double distractors,
						 double logodds_bail,
						 double logodds_accept,
						 int* p_besti,
						 double** p_all_logodds, int** p_theta,
						 double* p_worstlogodds);

verify_field_t* verify_field_preprocess(const starxy_t* fieldxy);

void verify_field_free(verify_field_t* vf);

void verify_get_uniformize_scale(int cutnside, double scale, int W, int H, int* uni_nw, int* uni_nh);

void verify_uniformize_field(const double* xy, int* perm, int N,
							 double fieldW, double fieldH,
							 int nw, int nh,
							 int** p_bincounts,
							 int** p_binids);

double* verify_uniformize_bin_centers(double fieldW, double fieldH,
									  int nw, int nh);

void verify_get_quad_center(const verify_field_t* vf, const MatchObj* mo, double* centerpix,
							double* quadr2);

int verify_get_test_stars(verify_field_t* vf, MatchObj* mo,
						  double pix2, bool do_gamma,
						  bool fake_match,
						  double** p_sigma2s, int** p_perm);

void verify_get_index_stars(const double* fieldcenter, double fieldr2,
							const startree_t* skdt, const sip_t* sip, const tan_t* tan,
							double fieldW, double fieldH,
							double** p_indexradec,
							double** p_indexpix, int** p_starids, int* p_nindex);

bool* verify_deduplicate_field_stars(verify_field_t* vf, double* sigma2s, double nsigmas);

double* verify_compute_sigma2s_arr(const double* xy, int NF,
								   const double* qc, double Q2,
								   double verify_pix2, bool do_gamma);

#endif
