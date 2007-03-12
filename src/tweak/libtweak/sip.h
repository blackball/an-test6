#ifndef ANWCS_H
#define ANWCS_H

#define SIP_MAXORDER 10

// WCS TAN header.
typedef struct tan_s {

	// World coordinate of the tangent point, in ra,dec.
	double crval[2];

	// Tangent point location in pixel (CCD) coordinates
	// This may not be in the image; consider a telescope with an array of
	// CCD's, where the tangent point is in one or none of the CCD's.
	double crpix[2];

	// Matrix for the linear transformation of relative pixel coordinates
	// (u,v) onto "intermediate world coordinates", which are in degrees
	// (x,y). The x,y coordinates are on the tangent plane. If the SIP
	// terms are all zero, then the equation to get from pixel coordinates
	// to intermediate world coordinates is:
	//
	//   u = pixel_x - crpix0
	//   v = pixel_y - crpix1
	// 
	//   x  = [cd00 cd01] * u
	//   y    [cd10 cd11]   v
	// 
	// where x,y are in intermediate world coordinates (i.e. x points
	// along negative ra and y points to positive dec) and u,v are in pixel
	// coordinates.
	double cd[2][2];

} tan_t;

// Flat structure for minimal SIP wcs. This structure should contain enough
// information to effectively represent any image, provided it is possible to
// convert that image's projecton to a TAN projection and the distortion to SIP
// distortion.
typedef struct sip_s {

	// A basic TAN header.
	tan_t wcstan;

	// Forward SIP coefficients
	// The transformation from relative pixel coordinates to intermediate
	// world coordinates[1] is:
	// 
	//   x  = [cd00 cd01] * (u + f(u,v))
	//   y    [cd10 cd11]   (v + g(u,v))
	// 
	// where
	//                           p    q
	//   f(u,v) = SUM a[p][q] * u  * v  ,  p+q <= a_order
	//            p,q
        //
	//                           p    q
	//   f(u,v) = SUM b[p][q] * u  * v  ,  p+q <= b_order
	//            p,q
	// 
	// [1] The SIP convention for representing distortion in FITS image
	// headers. D. L. Shupe, M.Moshir, J. Li, D. Makovoz, R. Narron, R. N.
	// Hook. Astronomical Data Analysis Software and Systems XIV.
	// http://ssc.spitzer.caltech.edu/postbcd/doc/shupeADASS.pdf
	//
	// Note: These matricies are larger than they strictly need to be
	// because aij = 0 if i+j > a_order and similarily for b.
	// 
	// Note: The convention for indicating that no SIP polynomial is
	// present is to simply set [ab]_order to zero.
	int a_order, b_order;
	double a[SIP_MAXORDER][SIP_MAXORDER];
	double b[SIP_MAXORDER][SIP_MAXORDER];

	// Inverse SIP coefficients
	// To convert from world coordinates back into image coordinates, the
	// inverse transformation may be stored. To convert from intermediate
	// world coordinates, first we calculate the linear pixel coordinates:
	// 
	//                   -1
	//    U = [cd00 cd01]    * x
	//    V   [cd10 cd11]      y
	//
	// Then, the original pixel coordinates are computed as:
	// 
	//                            p    q
	//   u  = U + SUM ap[p][q] * U  * V  ,  p+q <= ap_order
	//            p,q
        //
	//                            p    q
	//   v  = V + SUM bp[p][q] * U  * V  ,  p+q <= ap_order
	//            p,q
	// 
	// Note: ap_order does not necessarily equal a_order, because the
	// inverse of a nth-order polynomial may be of higer order than n.
	// 
	// Note: The convention for indicating that no inverse SIP polynomial
	// is present is to simply set [ab]p_order to zero.
	int ap_order, bp_order;
	double ap[SIP_MAXORDER][SIP_MAXORDER];
	double bp[SIP_MAXORDER][SIP_MAXORDER];
} sip_t;

sip_t* sip_create(void);
void   sip_free(sip_t* sip);
void   sip_pixelxy2radec(sip_t* sip, double px, double py, double *a, double *d);
void   sip_radec2pixelxy(sip_t* sip, double a, double d, double *px, double *py);
double tan_det_cd(tan_t* tan);
double sip_det_cd(sip_t* sip);
void   sip_calc_inv_distortion(sip_t* sip, double U, double V, double* u, double *v);
void   sip_calc_distortion(sip_t* sip, double u, double v, double* U, double *V);
      
void   tan_pixelxy2xyzarr(tan_t* tan, double px, double py, double *xyz);
void   tan_pixelxy2radec(tan_t* wcs_tan, double px, double py, double *ra, double *dec);
void   tan_radec2pixelxy(tan_t* wcs_tan, double ra, double dec, double *px, double *py);
void   sip_print(sip_t*);

#endif
