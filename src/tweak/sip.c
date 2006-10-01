#include <math.h>
#include <stdio.h>
#include "sip.h"
#include "starutil.h"

// Convert a ra,dec in degrees to coordinates in the image
void pixelxy2radec(sip_t* sip, double px, double py, double *a, double *d)
{

	// Get pixel coordinates relative to reference pixel
	double u = px - sip->crpix[0];
	double v = py - sip->crpix[1];

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
	u += fuv;
	v += guv;

	// Get intermediate world coordinates
	double x = sip->cd[0][0] * u + sip->cd[0][1] * v;
	double y = sip->cd[1][0] * u + sip->cd[1][1] * v;

	// Take r to be the threespace vector of crval
	double rx, ry, rz;
	radec2xyz(deg2rad(sip->crval[0]), deg2rad(sip->crval[1]), &rx, &ry, &rz);

	// Form i = r cross north pole, which is in direction of x
	double ix = ry;
	double iy = -rx;
	//     iz = 0 because the the north pole is at (0,0,1)
	double norm = sqrt(ix*ix + iy*iy);
	ix /= norm;
	iy /= norm;

	// Form j = r cross i, which is in the direction of y
	double jx =  rx*rz;
	double jy =  rz*ry;
	double jz = -rx*rx - ry*ry;
	norm = sqrt(jx*jx + jy*jy + jz*jz);
	jx /= norm;
	jy /= norm;
	jz /= norm;

	// Form the point on the tangent plane relative to observation point,
	// and normalize back onto the unit sphere
	double wx = ix*x + jx*y;
	double wy = iy*x + jy*y;
	double wz =        jz*y; // iz = 0
	norm = sqrt(wx*wx + wy*wy + wz*wz);
	wx /= norm;
	wy /= norm;
	wz /= norm;

	// We're done!
	xyz2radec(wx,wy,wz,a,d);
}

// Convert a ra,dec in degrees to coordinates in the image
void radec2pixelxy(sip_t* sip, double a, double d, double *px, double *py)
{
	// Invert CD
	double cdi[2][2];
	double det = sip->cd[0][0]*sip->cd[1][1] - sip->cd[0][1]*sip->cd[1][0]; 
	cdi[0][0] =  cdi[1][1] / det;
	cdi[0][1] = -cdi[1][0] / det;
	cdi[1][0] = -cdi[0][1] / det;
	cdi[1][1] =  cdi[0][0] / det;

	// Calculate intermediate world coordinates (x,y) on the tangent plane
	double xyzpt[3];
	radecdeg2xyzarr(a,d,xyzpt);
	double xyzcrval[3];
	radecdeg2xyzarr(sip->crval[0],sip->crval[1],xyzcrval);
	double x,y;
	star_coords(xyzpt, xyzcrval, &x, &y);

	// Linear pixel coordinates
	double U = cdi[0][0]*x + cdi[0][1]*y;
	double V = cdi[1][0]*x + cdi[1][1]*y;

	// Invert SIP distortion
	// Sanity check:
	if (sip->a_order != 0 && sip->ap_order == 0) {
		fprintf(stderr, "suspicious inversion; no inversion SIP coeffs "
				"yet there are forward SIP coeffs\n");
	}
	int p, q;
	double fUV=0.;
	for (p=0; p<sip->ap_order; p++)
		for (q=0; q<sip->ap_order; q++)
			fUV += sip->ap[p][q]*pow(U,p)*pow(V,q);
	double gUV=0.;
	for (p=0; p<sip->bp_order; p++) 
		for (q=0; q<sip->bp_order; q++) 
			gUV += sip->bp[p][q]*pow(U,p)*pow(V,q);

	double u = U + fUV;
	double v = V + gUV;

	// Readd crpix to get pixel coordinates
	*px = u + sip->crpix[0];
	*py = v + sip->crpix[1];
}
