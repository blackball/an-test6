#include <math.h>
#include <stdio.h>
#include <assert.h>
#include "sip.h"
#include "starutil.h"

sip_t* createsip() {
	sip_t* sip = malloc(sizeof(sip_t));

	sip->crval[0] = 0;
	sip->crval[1] = 0;

	sip->crpix[0] = 0;
	sip->crpix[1] = 0;

	sip->cd[0][0] = 1;
	sip->cd[0][1] = 0;
	sip->cd[1][0] = 0;
	sip->cd[1][1] = 1;

	sip->a_order = 0;
	sip->b_order = 0;
	sip->ap_order = 0;
	sip->bp_order = 0;

	return sip;
}

// Convert a ra,dec in degrees to coordinates in the image
void pixelxy2radec(sip_t* sip, double px, double py, double *a, double *d)
{

	// Get pixel coordinates relative to reference pixel
	//double u = deg2rad(px) - sip->crpix[0];
	//double v = deg2rad(py) - sip->crpix[1];
	double u = px - sip->crpix[0];
	double v = py - sip->crpix[1];

	// Convert to radians?? Apparently!

	double U, V;
	sip_calc_distortion(sip, u, v, &U, &V);
//	printf("u=%lf v=%lf\n",u,v);

	// Get intermediate world coordinates
	double x = sip->cd[0][0] * U + sip->cd[0][1] * V;
	double y = sip->cd[1][0] * U + sip->cd[1][1] * V;
//	printf("x=%lf y=%lf\n",x,y);

	// Mysterious! Who knows, but negating these coordinates makes WCStools match with SIP. 
	x = -deg2rad(x);
	y = -deg2rad(y);

	// Take r to be the threespace vector of crval
	double rx, ry, rz;
	radec2xyz(deg2rad(sip->crval[0]), deg2rad(sip->crval[1]), &rx, &ry, &rz);
//	printf("rx=%lf ry=%lf rz=%lf\n",rx,ry,rz);

	// Form i = r cross north pole, which is in direction of z
	double ix = ry;
	double iy = -rx;
	//     iz = 0 because the the north pole is at (0,0,1)
	double norm = sqrt(ix*ix + iy*iy);
	ix /= norm;
	iy /= norm;
//	printf("ix=%lf iy=%lf iz=0.0\n",ix,iy);
//	printf("r.i = %lf\n",ix*rx+iy*ry);

	// Form j = r cross i, which is in the direction of y
	double jx =  rx*rz;
	double jy =  rz*ry;
	double jz = -rx*rx - ry*ry;
	// norm should already be 1, but normalize anyway
	norm = sqrt(jx*jx + jy*jy + jz*jz);
	jx /= norm;
	jy /= norm;
	jz /= norm;
//	printf("jx=%lf jy=%lf jz=%lf\n",jx,jy,jz);
//	printf("r.j = %lf\n",jx*rx+jy*ry+jz*rz);
//	printf("i.j = %lf\n",ix*jx+iy*jy);

	// Form the point on the tangent plane relative to observation point,
	// and normalize back onto the unit sphere
	double wx = ix*x + jx*y + rx;
	double wy = iy*x + jy*y + ry;
	double wz =        jz*y + rz; // iz = 0
	norm = sqrt(wx*wx + wy*wy + wz*wz);
	wx /= norm;
	wy /= norm;
	wz /= norm;
//	printf("wx=%lf wy=%lf wz=%lf\n",wx,wy,wz);

	// We're done!
	xyz2radec(wx,wy,wz,a,d);
	*a = rad2deg(*a);
	*d = rad2deg(*d);
}

// Convert a ra,dec in degrees to coordinates in the image
void radec2pixelxy(sip_t* sip, double a, double d, double *px, double *py)
{
	// Invert CD
	double cdi[2][2];
	double inv_det = 1.0/(sip->cd[0][0]*sip->cd[1][1] - sip->cd[0][1]*sip->cd[1][0]); 
	cdi[0][0] =  sip->cd[1][1] * inv_det;
	cdi[0][1] = -sip->cd[0][1] * inv_det;
	cdi[1][0] = -sip->cd[1][0] * inv_det;
	cdi[1][1] =  sip->cd[0][0] * inv_det;

	/*
	printf(":: %lf\n",  (sip->cd[0][0]*cdi[0][0] + sip->cd[0][1]*cdi[1][0] ));
	printf(":: %lf\n",  (sip->cd[0][0]*cdi[0][1] + sip->cd[0][1]*cdi[1][1] ));
	printf(":: %lf\n",  (sip->cd[1][0]*cdi[0][0] + sip->cd[1][1]*cdi[1][0] ));
	printf(":: %lf\n",  (sip->cd[1][0]*cdi[0][1] + sip->cd[1][1]*cdi[1][1] ));
	*/

	//assert( (sip->cd[0][0]*cdi[0][0] + sip->cd[0][1]*cdi[1][0] ) == 1.0);
	//assert( (sip->cd[0][0]*cdi[0][1] + sip->cd[0][1]*cdi[1][1] ) == 0.0);
	//assert( (sip->cd[1][0]*cdi[0][0] + sip->cd[1][1]*cdi[1][0] ) == 0.0);
	//assert( (sip->cd[1][0]*cdi[0][1] + sip->cd[1][1]*cdi[1][1] ) == 1.0);

	// FIXME be robust near the poles
	// Calculate intermediate world coordinates (x,y) on the tangent plane
	double xyzpt[3];
	radecdeg2xyzarr(a,d,xyzpt);
	double xyzcrval[3];
	radecdeg2xyzarr(sip->crval[0],sip->crval[1],xyzcrval);
	double x,y;
	star_coords(xyzpt, xyzcrval, &y, &x);

	// Switch intermediate world coordinates into degrees
	x = rad2deg(x);
	y = rad2deg(y);

	// Linear pixel coordinates
	double U = cdi[0][0]*x + cdi[0][1]*y;
	double V = cdi[1][0]*x + cdi[1][1]*y;

	// Invert SIP distortion
	// Sanity check:
	if (sip->a_order != 0 && sip->ap_order == 0) {
		fprintf(stderr, "suspicious inversion; no inversion SIP coeffs "
				"yet there are forward SIP coeffs\n");
	}

	double u, v;
	sip_calc_inv_distortion(sip, U, V, &u, &v);

	// Re-add crpix to get pixel coordinates
	*px = u + sip->crpix[0];
	*py = v + sip->crpix[1];
}

void sip_calc_distortion(sip_t* sip, double u, double v, double* U, double *V)
{
	// Do SIP distortion (in relative pixel coordinates)
	// See the sip_t struct definition in header file for details
	int p, q;
	double fuv=0.;
	for (p=0; p<sip->a_order; p++)
		for (q=0; q<sip->a_order; q++)
			if (p+q <= sip->a_order && !(p==0&&q==0))
				fuv += sip->a[p][q]*pow(u,p)*pow(v,q);
	double guv=0.;
	for (p=0; p<sip->b_order; p++) 
		for (q=0; q<sip->b_order; q++) 
			if (p+q <= sip->b_order && !(p==0&&q==0)) 
				guv += sip->b[p][q]*pow(u,p)*pow(v,q);
	*U = u + fuv;
	*V = v + guv;
}

void sip_calc_inv_distortion(sip_t* sip, double U, double V, double* u, double *v)
{
	int p, q;
	double fUV=0.;
	for (p=0; p<sip->ap_order; p++)
		for (q=0; q<sip->ap_order; q++)
			fUV += sip->ap[p][q]*pow(U,p)*pow(V,q);
	double gUV=0.;
	for (p=0; p<sip->bp_order; p++) 
		for (q=0; q<sip->bp_order; q++) 
			gUV += sip->bp[p][q]*pow(U,p)*pow(V,q);

	*u = U + fUV;
	*v = V + gUV;
}

double sip_det_cd(sip_t* sip)
{
	return (sip->cd[0][0]*sip->cd[1][1] - sip->cd[0][1]*sip->cd[1][0]); 
}
