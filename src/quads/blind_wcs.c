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
#include <math.h>

#include "blind_wcs.h"
#include "solver_callbacks.h"
#include "mathutil.h"
#include "svd.h"

void blind_wcs_compute(MatchObj* mo, double* field, int nfield,
					   int* corr,
					   double* crval, double* crpix, double* CD) {
	double xyz[3];
	double starcmass[3];
	double fieldcmass[2];
	double cov[4];
	double U[4], V[4], S[2], R[4];
	double scale;
	int i, j, k;
	int Ncorr;
	// projected star coordinates
	double* p;
	// relative field coordinates
	double* f;
	double pcm[2];
	double fcm[2];

	// compute a simple WCS transformation:
	// -get field & star positions of the matching quad.
	for (j=0; j<3; j++)
		starcmass[j] = 0.0;
	for (j=0; j<2; j++)
		fieldcmass[j] = 0.0;
	for (i=0; i<4; i++) {
		getstarcoord(mo->star[i], xyz);
		for (j=0; j<3; j++)
			starcmass[j] += xyz[j];
		for (j=0; j<2; j++)
			fieldcmass[j] += field[mo->field[i] * 2 + j];
	}
	// -set the tangent point to be the center of mass of the matching quad.
	for (j=0; j<2; j++)
		fieldcmass[j] /= 4.0;
	normalize_3(starcmass);

	// -count how many corresponding stars there are.
	if (!corr)
		Ncorr = 4;
	else {
		Ncorr = 0;
		for (i=0; i<nfield; i++)
			if (corr[i] != -1)
				Ncorr++;
	}

	// -allocate and fill "p" and "f" arrays.
	p = malloc(Ncorr * 2 * sizeof(double));
	f = malloc(Ncorr * 2 * sizeof(double));
	j = 0;
	if (!corr) {
		// just use the four stars that compose the quad.
		for (i=0; i<4; i++) {
			getstarcoord(mo->star[i], xyz);
			star_coords(xyz, starcmass, p + 2*i, p + 2*i + 1);
			f[2*i+0] = field[mo->field[i] * 2 + 0];
			f[2*i+1] = field[mo->field[i] * 2 + 1];
		}
	} else {
		// use all correspondences.
		for (i=0; i<nfield; i++)
			if (corr[i] != -1) {
				// -project the stars around the quad center
				getstarcoord(corr[i], xyz);
				star_coords(xyz, starcmass, p + 2*j, p + 2*j + 1);
				// -grab the corresponding field coords.
				f[2*j+0] = field[2*i+0];
				f[2*j+1] = field[2*i+1];
				j++;
			}
	}

	// -compute centers of mass
	pcm[0] = pcm[1] = fcm[0] = fcm[1] = 0.0;
	for (i=0; i<Ncorr; i++)
		for (j=0; j<2; j++) {
			pcm[j] += p[2*i+j];
			fcm[j] += f[2*i+j];
		}
	pcm[0] /= (double)Ncorr;
	pcm[1] /= (double)Ncorr;
	fcm[0] /= (double)Ncorr;
	fcm[1] /= (double)Ncorr;

	// -subtract out the centers of mass.
	for (i=0; i<Ncorr; i++)
		for (j=0; j<2; j++) {
			f[2*i + j] -= fcm[j];
			p[2*i + j] -= pcm[j];
		}

	// -compute the covariance between field positions and projected
	//  positions of the corresponding stars.
	for (i=0; i<4; i++)
		cov[i] = 0.0;
	for (i=0; i<Ncorr; i++)
		for (j=0; j<2; j++)
			for (k=0; k<2; k++)
				cov[j*2 + k] += p[i*2 + k] * f[i*2 + j];
	// -run SVD
	{
		double* pcov[] = { cov, cov+2 };
		double* pU[]   = { U,   U  +2 };
		double* pV[]   = { V,   V  +2 };
		double eps, tol;
		eps = 1e-30;
		tol = 1e-30;
		svd(2, 2, 1, 1, eps, tol, pcov, S, pU, pV);
	}
	// -compute rotation matrix R = V U'
	for (i=0; i<4; i++)
		R[i] = 0.0;
	for (i=0; i<2; i++)
		for (j=0; j<2; j++)
			for (k=0; k<2; k++)
				R[i*2 + j] += V[i*2 + k] * U[j*2 + k];

	// -compute scale: make the variances equal.
	{
		double pvar, fvar;
		pvar = fvar = 0.0;
		for (i=0; i<Ncorr; i++)
			for (j=0; j<2; j++) {
				pvar += square(p[i*2 + j]);
				fvar += square(f[i*2 + j]);
			}
		scale = sqrt(pvar / fvar);
	}

	// -compute WCS parameters.
	{
		double ra, dec;
		xyz2radec(starcmass[0], starcmass[1], starcmass[2], &ra, &dec);
		crval[0] = rad2deg(ra);
		crval[1] = rad2deg(dec);
		crpix[0] = fieldcmass[0];
		crpix[1] = fieldcmass[1];
		scale = rad2deg(scale);
		CD[0] = R[2] * scale; // CD1_1
		CD[1] = R[3] * scale; // CD1_2
		CD[2] = R[0] * scale; // CD2_1
		CD[3] = R[1] * scale; // CD2_2
	}

	free(p);
	free(f);
}

qfits_header* blind_wcs_get_header(double* crval, double* crpix,
								   double* CD) {
	qfits_header* wcs = NULL;
	char val[64];

	wcs = qfits_table_prim_header_default();

	qfits_header_add(wcs, "CTYPE1 ", "RA---TAN", "TAN (gnomic) projection", NULL);
	qfits_header_add(wcs, "CTYPE2 ", "DEC--TAN", "TAN (gnomic) projection", NULL);
	sprintf(val, "%.12g", crval[0]);
	qfits_header_add(wcs, "CRVAL1 ", val, "RA  of reference point", NULL);
	sprintf(val, "%.12g", crval[1]);
	qfits_header_add(wcs, "CRVAL2 ", val, "DEC of reference point", NULL);

	sprintf(val, "%.12g", crpix[0]);
	qfits_header_add(wcs, "CRPIX1 ", val, "X reference pixel", NULL);
	sprintf(val, "%.12g", crpix[1]);
	qfits_header_add(wcs, "CRPIX2 ", val, "Y reference pixel", NULL);

	qfits_header_add(wcs, "CUNIT1 ", "deg", "X pixel scale units", NULL);
	qfits_header_add(wcs, "CUNIT2 ", "deg", "Y pixel scale units", NULL);

	// bizarrely, this only seems to work when I swap the rows of R.
	sprintf(val, "%.12g", CD[0]);
	qfits_header_add(wcs, "CD1_1", val, "Transformation matrix", NULL);
	sprintf(val, "%.12g", CD[1]);
	qfits_header_add(wcs, "CD1_2", val, " ", NULL);
	sprintf(val, "%.12g", CD[2]);
	qfits_header_add(wcs, "CD2_1", val, " ", NULL);
	sprintf(val, "%.12g", CD[3]);
	qfits_header_add(wcs, "CD2_2", val, " ", NULL);

	return wcs;
}

