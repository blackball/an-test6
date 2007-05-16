/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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
#include "sip.h"
#include "sip_qfits.h"

void blind_wcs_compute_2(double* starxyz,
						 double* fieldxy,
						 int N,
						 // output:
						 tan_t* tan,
						 double* p_scale) {
	int i, j, k;
	double star_cm[3] = {0, 0, 0};
	double field_cm[2] = {0, 0};
	double cov[4] = {0, 0, 0, 0};
	double U[4], V[4], S[2];
	double R[4] = {0, 0, 0, 0};
	double scale;
	// projected star coordinates
	double* p;
	// relative field coordinates
	double* f;
	double pcm[2] = {0, 0};

	// -get field & star centers-of-mass of the matching quad.
	//  (this will become the tangent point)
	for (i=0; i<N; i++) {
		star_cm[0] += starxyz[i*3 + 0];
		star_cm[1] += starxyz[i*3 + 1];
		star_cm[2] += starxyz[i*3 + 2];
		field_cm[0] += fieldxy[i*2 + 0];
		field_cm[1] += fieldxy[i*2 + 1];
	}
	field_cm[0] /= (double)N;
	field_cm[1] /= (double)N;
	normalize_3(star_cm);

	// -allocate and fill "p" and "f" arrays.
	p = malloc(N * 2 * sizeof(double));
	f = malloc(N * 2 * sizeof(double));
	j = 0;

	for (i=0; i<N; i++) {
		// -project the stars around the quad center of mass
		star_coords(starxyz + i*3, star_cm, p + 2*i, p + 2*i + 1);
		// -grab the corresponding field coords
		f[2*i+0] = fieldxy[2*i+0] - field_cm[0];
		f[2*i+1] = fieldxy[2*i+1] - field_cm[1];
	}

	// -compute the center of mass of the projected stars and subtract it out.
	//  This will be close to zero, but we need it to be exactly zero
	//  before we start rigid Procrustes
	for (i=0; i<N; i++) {
		pcm[0] += p[2*i + 0];
		pcm[1] += p[2*i + 1];
	}
	pcm[0] /= (double)N;
	pcm[1] /= (double)N;
	//printf("pcm=(%g, %g) pixels\n", pcm[0], pcm[1]);
	for (i=0; i<N; i++) {
		p[2*i + 0] -= pcm[0];
		p[2*i + 1] -= pcm[1];
	}

	// -compute the covariance between field positions and projected
	//  positions of the corresponding stars.
	for (i=0; i<N; i++)
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
		for (i=0; i<N; i++)
			for (j=0; j<2; j++) {
				pvar += square(p[i*2 + j]);
				fvar += square(f[i*2 + j]);
			}
		scale = sqrt(pvar / fvar);
	}

	// -compute WCS parameters.
	xyzarr2radecdegarr(star_cm, tan->crval);
	tan->crpix[0] = field_cm[0] + pcm[0];
	tan->crpix[1] = field_cm[1] + pcm[1];
	scale = rad2deg(scale);
	// The CD rows are reversed from R because star_coords and the Intermediate
	// World Coordinate System consider x and y to be exchanged.
	tan->cd[0][0] = R[2] * scale; // CD1_1
	tan->cd[0][1] = R[3] * scale; // CD1_2
	tan->cd[1][0] = R[0] * scale; // CD2_1
	tan->cd[1][1] = R[1] * scale; // CD2_2

	/*
	 * Did I add pcm in the right direction?
	 * I've never seen it make more of a difference than machine epsilon.
	  {
		double rmsA = 0.0;
		double rmsB = 0.0;
		double rmsC = 0.0;
		tan_t tan_tst;
		memcpy(&tan_tst, tan, sizeof(tan_t));

		tan_tst.crpix[0] = field_cm[0] + pcm[0];
		tan_tst.crpix[1] = field_cm[1] + pcm[1];

		for (i=0; i<N; i++) {
			double x, y;
			tan_xyzarr2pixelxy(&tan_tst, starxyz + i*3, &x, &y);
			rmsA += square(x - fieldxy[i*2 + 0]) + square(y - fieldxy[i*2 + 1]);
		}
		rmsA = sqrt(rmsA / (double)N);

		tan_tst.crpix[0] = field_cm[0];
		tan_tst.crpix[1] = field_cm[1];

		for (i=0; i<N; i++) {
			double x, y;
			tan_xyzarr2pixelxy(&tan_tst, starxyz + i*3, &x, &y);
			rmsB += square(x - fieldxy[i*2 + 0]) + square(y - fieldxy[i*2 + 1]);
		}
		rmsB = sqrt(rmsB / (double)N);

		tan_tst.crpix[0] = field_cm[0] - pcm[0];
		tan_tst.crpix[1] = field_cm[1] - pcm[1];

		for (i=0; i<N; i++) {
			double x, y;
			tan_xyzarr2pixelxy(&tan_tst, starxyz + i*3, &x, &y);
			rmsC += square(x - fieldxy[i*2 + 0]) + square(y - fieldxy[i*2 + 1]);
		}
		rmsC = sqrt(rmsC / (double)N);

		printf("rmsA = %.16g pix.\n", rmsA);
		printf("rmsB = %.16g pix.\n", rmsB);
		printf("rmsC = %.16g pix.\n", rmsC);
	}
	*/

	if (p_scale) *p_scale = scale;

	free(p);
	free(f);
}


void blind_wcs_compute(MatchObj* mo, double* field, int nfield,
					   int* corr,
					   tan_t* tan) {
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
		tan->crval[0] = rad2deg(ra);
		tan->crval[1] = rad2deg(dec);
		tan->crpix[0] = fieldcmass[0];
		tan->crpix[1] = fieldcmass[1];
		scale = rad2deg(scale);
		tan->cd[0][0] = R[2] * scale; // CD1_1
		tan->cd[0][1] = R[3] * scale; // CD1_2
		tan->cd[1][0] = R[0] * scale; // CD2_1
		tan->cd[1][1] = R[1] * scale; // CD2_2
	}

	free(p);
	free(f);
}

qfits_header* blind_wcs_get_header(tan_t* tan) {
	return tan_create_header(tan);
}

qfits_header* blind_wcs_get_sip_header(sip_t* sip) {
	return sip_create_header(sip);
}

