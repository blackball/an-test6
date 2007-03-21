#include <math.h>
#include <stdio.h>
#include <assert.h>
#include "sip.h"
#include "starutil.h"
#include "mathutil.h"

sip_t* sip_create() {
	sip_t* sip = calloc(1, sizeof(sip_t));

	sip->wcstan.cd[0][0] = 1;
	sip->wcstan.cd[0][1] = 0;
	sip->wcstan.cd[1][0] = 0;
	sip->wcstan.cd[1][1] = 1;

	return sip;
}

void sip_free(sip_t* sip) {
	free(sip);
}

// Pixels to RA,Dec in degrees.
void sip_pixelxy2radec(sip_t* sip, double px, double py,
					   double *ra, double *dec)
{
	double U, V;
	// Get pixel coordinates relative to reference pixel
	double u = px - sip->wcstan.crpix[0];
	double v = py - sip->wcstan.crpix[1];
	sip_calc_distortion(sip, u, v, &U, &V);
	U += sip->wcstan.crpix[0];
	V += sip->wcstan.crpix[1];
	// Run a normal TAN conversion on the distorted pixel coords.
	tan_pixelxy2radec(&(sip->wcstan), U, V, ra, dec);
}

// Pixels to RA,Dec in degrees.
void tan_pixelxy2radec(tan_t* tan, double px, double py, double *ra, double *dec)
{
	double xyz[3];
	tan_pixelxy2xyzarr(tan, px, py, xyz);
	// We're done!
	xyzarr2radec(xyz, ra,dec);
	*ra = rad2deg(*ra);
	*dec = rad2deg(*dec);
}

// Pixels to XYZ unit vector.
void tan_pixelxy2xyzarr(tan_t* tan, double px, double py, double *xyz)
{
	// Get pixel coordinates relative to reference pixel
	double U = px - tan->crpix[0];
	double V = py - tan->crpix[1];

	// Get intermediate world coordinates
	double x = tan->cd[0][0] * U + tan->cd[0][1] * V;
	double y = tan->cd[1][0] * U + tan->cd[1][1] * V;
	//	printf("x=%lf y=%lf\n",x,y);

	// Mysterious! Who knows, but negating these coordinates makes WCStools match with SIP. 
	x = -deg2rad(x);
	y = -deg2rad(y);

	// Take r to be the threespace vector of crval
	double rx, ry, rz;
	radec2xyz(deg2rad(tan->crval[0]), deg2rad(tan->crval[1]), &rx, &ry, &rz);
	//	printf("rx=%lf ry=%lf rz=%lf\n",rx,ry,rz);

	// Form i = r cross north pole, which is in direction of z
	double ix = ry;
	double iy = -rx;
	//     iz = 0 because the the north pole is at (0,0,1)
	double norm = hypot(ix, iy);
	ix /= norm;
	iy /= norm;
	//	printf("ix=%lf iy=%lf iz=0.0\n",ix,iy);
	//	printf("r.i = %lf\n",ix*rx+iy*ry);

	// Form j = r cross i, which is in the direction of y
	double jx =  rx*rz;
	double jy =  rz*ry;
	double jz = -rx*rx - ry*ry;
	// norm should already be 1, but normalize anyway
	normalize(&jx, &jy, &jz);
	//	printf("jx=%lf jy=%lf jz=%lf\n",jx,jy,jz);
	//	printf("r.j = %lf\n",jx*rx+jy*ry+jz*rz);
	//	printf("i.j = %lf\n",ix*jx+iy*jy);

	// Form the point on the tangent plane relative to observation point,
	// and normalize back onto the unit sphere
	double wx = ix*x + jx*y + rx;
	double wy = iy*x + jy*y + ry;
	double wz =        jz*y + rz; // iz = 0
	normalize(&wx, &wy, &wz);
	//	printf("wx=%lf wy=%lf wz=%lf\n",wx,wy,wz);

	xyz[0] = wx;
	xyz[1] = wy;
	xyz[2] = wz;
}

// RA,Dec in degrees to Pixels.
void sip_radec2pixelxy(sip_t* sip, double ra, double dec, double *px, double *py)
{
	double u, v;
	double U, V;

	tan_radec2pixelxy(&(sip->wcstan), ra, dec, px, py);

	// Subtract crpix, invert SIP distortion, add crpix.

	// Sanity check:
	if (sip->a_order != 0 && sip->ap_order == 0) {
		fprintf(stderr, "suspicious inversion; no inversion SIP coeffs "
				"yet there are forward SIP coeffs\n");
	}

	U = *px - sip->wcstan.crpix[0];
	V = *py - sip->wcstan.crpix[1];

	sip_calc_inv_distortion(sip, U, V, &u, &v);

	*px = u + sip->wcstan.crpix[0];
	*py = v + sip->wcstan.crpix[1];
}

// xyz unit vector to Pixels.
void   tan_xyzarr2pixelxy(tan_t* tan, double* xyzpt, double *px, double *py)
{
	// Invert CD
	double cdi[2][2];
	double inv_det = 1.0/(tan->cd[0][0]*tan->cd[1][1] - tan->cd[0][1]*tan->cd[1][0]); 
	cdi[0][0] =  tan->cd[1][1] * inv_det;
	cdi[0][1] = -tan->cd[0][1] * inv_det;
	cdi[1][0] = -tan->cd[1][0] * inv_det;
	cdi[1][1] =  tan->cd[0][0] * inv_det;

	/*
	printf(":: %lf\n",  (tan->cd[0][0]*cdi[0][0] + tan->cd[0][1]*cdi[1][0] ));
	printf(":: %lf\n",  (tan->cd[0][0]*cdi[0][1] + tan->cd[0][1]*cdi[1][1] ));
	printf(":: %lf\n",  (tan->cd[1][0]*cdi[0][0] + tan->cd[1][1]*cdi[1][0] ));
	printf(":: %lf\n",  (tan->cd[1][0]*cdi[0][1] + tan->cd[1][1]*cdi[1][1] ));
	*/

	//assert( (tan->cd[0][0]*cdi[0][0] + tan->cd[0][1]*cdi[1][0] ) == 1.0);
	//assert( (tan->cd[0][0]*cdi[0][1] + tan->cd[0][1]*cdi[1][1] ) == 0.0);
	//assert( (tan->cd[1][0]*cdi[0][0] + tan->cd[1][1]*cdi[1][0] ) == 0.0);
	//assert( (tan->cd[1][0]*cdi[0][1] + tan->cd[1][1]*cdi[1][1] ) == 1.0);

	// FIXME be robust near the poles
	// Calculate intermediate world coordinates (x,y) on the tangent plane
	double xyzcrval[3];
	radecdeg2xyzarr(tan->crval[0],tan->crval[1],xyzcrval);
	double x,y;
	star_coords(xyzpt, xyzcrval, &y, &x);

	// Switch intermediate world coordinates into degrees
	x = rad2deg(x);
	y = rad2deg(y);

	// Linear pixel coordinates
	double U = cdi[0][0]*x + cdi[0][1]*y;
	double V = cdi[1][0]*x + cdi[1][1]*y;

	// Re-add crpix to get pixel coordinates
	*px = U + tan->crpix[0];
	*py = V + tan->crpix[1];
}

// RA,Dec in degrees to Pixels.
void tan_radec2pixelxy(tan_t* tan, double a, double d, double *px, double *py)
{
	double xyzpt[3];
	radecdeg2xyzarr(a,d,xyzpt);
	tan_xyzarr2pixelxy(tan, xyzpt, px, py);
}

void sip_calc_distortion(sip_t* sip, double u, double v, double* U, double *V)
{
	// Do SIP distortion (in relative pixel coordinates)
	// See the sip_t struct definition in header file for details
	int p, q;
	double fuv=0.;
	for (p=0; p<=sip->a_order; p++)
		for (q=0; q<=sip->a_order; q++)
			if (p+q <= sip->a_order && !(p==0&&q==0))
				fuv += sip->a[p][q]*pow(u,p)*pow(v,q);
	double guv=0.;
	for (p=0; p<=sip->b_order; p++) 
		for (q=0; q<=sip->b_order; q++) 
			if (p+q <= sip->b_order && !(p==0&&q==0)) 
				guv += sip->b[p][q]*pow(u,p)*pow(v,q);
	*U = u + fuv;
	*V = v + guv;
}

void sip_calc_inv_distortion(sip_t* sip, double U, double V, double* u, double *v)
{
	int p, q;
	double fUV=0.;
	for (p=0; p<=sip->ap_order; p++)
		for (q=0; q<=sip->ap_order; q++)
			fUV += sip->ap[p][q]*pow(U,p)*pow(V,q);
	double gUV=0.;
	for (p=0; p<=sip->bp_order; p++) 
		for (q=0; q<=sip->bp_order; q++) 
			gUV += sip->bp[p][q]*pow(U,p)*pow(V,q);

	*u = U + fUV;
	*v = V + gUV;
}

double tan_det_cd(tan_t* tan) {
	return (tan->cd[0][0]*tan->cd[1][1] - tan->cd[0][1]*tan->cd[1][0]);
}

double sip_det_cd(sip_t* sip) {
	return (sip->wcstan.cd[0][0]*sip->wcstan.cd[1][1] - sip->wcstan.cd[0][1]*sip->wcstan.cd[1][0]);
}

void sip_print(sip_t* sip)
{

	printf("SIP Structure:\n");
	printf("crval[0]=%lf\n", sip->wcstan.crval[0]);
	printf("crval[1]=%lf\n", sip->wcstan.crval[1]);
	printf("crpix[0]=%lf\n", sip->wcstan.crpix[0]);
	printf("crpix[1]=%lf\n", sip->wcstan.crpix[1]);

	printf("cd00=%le\n", sip->wcstan.cd[0][0]);
	printf("cd01=%le\n", sip->wcstan.cd[0][1]);
	printf("cd10=%le\n", sip->wcstan.cd[1][0]);
	printf("cd11=%le\n", sip->wcstan.cd[1][1]);

	if (sip->a_order > 0) {
		int p, q;
		for (p=0; p<=sip->a_order; p++)
			for (q=0; q<=sip->a_order; q++)
				if (p+q <= sip->a_order && !(p==0&&q==0))
					 printf("a%d%d=%le\n", p,q,sip->a[p][q]);
	}
	if (sip->b_order > 0) {
		int p, q;
		for (p=0; p<=sip->b_order; p++)
			for (q=0; q<=sip->b_order; q++)
				if (p+q <= sip->b_order && !(p==0&&q==0))
					 printf("b%d%d=%le\n", p,q,sip->b[p][q]);
	}

	double det = sip_det_cd(sip);
	double pixsc = 3600*sqrt(fabs(det));
	printf("det(CD)=%le [arcsec]\n", det);
	printf("sqrt(det(CD))=%le [arcsec]\n", pixsc);


	printf("\n");
}

