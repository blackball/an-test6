#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#include "starutil.h"
#include "sip.h"
#include "libwcs/wcs.h"
#include "libwcs/fitsfile.h"

typedef struct WorldCoor wcs_t;

char simple[] = 
"XTENSION= 'IMAGE   '           / Extension type                                 "
"BITPIX  =                  -32 / Bits per pixel                                 "
"NAXIS   =                    2 / Number of image axes                           "
"NAXIS1  =                 2048 / Length of axis 1                               "
"NAXIS2  =                 4096 / Length of axis 2                               "
"BSCALE  =                    1 / Scale factor for unsigned short                "
"BZERO   =                32768 / Zero shift for unsigned short                  "
"PCOUNT  =                    0 / Number of bytes following image matrix         "
"EQUINOX =                2000. / Equinox of WCS                                 "
"WCSDIM  =                    2 / WCS dimensionality                             "
"CTYPE1  = 'RA---TAN'           / Coordinate type                                "
"CTYPE2  = 'DEC--TAN'           / Coordinate type                                "
"CRVAL1  =                  0.0 / Coordinate reference value                     "
"CRVAL2  =                  0.0 / Coordinate reference value                     "
"CRPIX1  =                  0.0 / Coordinate reference pixel                     "
"CRPIX2  =                  0.0 / Coordinate reference pixel                     "
"CD1_1   =                  1.0 / Coordinate matrix                              "
"CD2_1   =                  0.0 / Coordinate matrix                              "
"CD1_2   =                  0.0 / Coordinate matrix                              "
"CD2_2   =                  1.0 / Coordinate matrix                              ";

char simple2[] = 
"XTENSION= 'IMAGE   '           / Extension type                                 "
"BITPIX  =                  -32 / Bits per pixel                                 "
"NAXIS   =                    2 / Number of image axes                           "
"NAXIS1  =                 2048 / Length of axis 1                               "
"NAXIS2  =                 4096 / Length of axis 2                               "
"BSCALE  =                    1 / Scale factor for unsigned short                "
"BZERO   =                32768 / Zero shift for unsigned short                  "
"PCOUNT  =                    0 / Number of bytes following image matrix         "
"EQUINOX =                2000. / Equinox of WCS                                 "
"WCSDIM  =                    2 / WCS dimensionality                             "
"CTYPE1  = 'RA---TAN'           / Coordinate type                                "
"CTYPE2  = 'DEC--TAN'           / Coordinate type                                "
"CRVAL1  =                  0.0 / Coordinate reference value                     "
"CRVAL2  =                  0.0 / Coordinate reference value                     "
"CRPIX1  =                  6.0 / Coordinate reference pixel                     "
"CRPIX2  =                  6.0 / Coordinate reference pixel                     "
"CD1_1   =                  1.0 / Coordinate matrix                              "
"CD2_1   =                  0.0 / Coordinate matrix                              "
"CD1_2   =                  0.0 / Coordinate matrix                              "
"CD2_2   =                  1.0 / Coordinate matrix                              ";

char simple3[] = 
"XTENSION= 'IMAGE   '           / Extension type                                 "
"BITPIX  =                  -32 / Bits per pixel                                 "
"NAXIS   =                    2 / Number of image axes                           "
"NAXIS1  =                 2048 / Length of axis 1                               "
"NAXIS2  =                 4096 / Length of axis 2                               "
"BSCALE  =                    1 / Scale factor for unsigned short                "
"BZERO   =                32768 / Zero shift for unsigned short                  "
"PCOUNT  =                    0 / Number of bytes following image matrix         "
"EQUINOX =                2000. / Equinox of WCS                                 "
"WCSDIM  =                    2 / WCS dimensionality                             "
"CTYPE1  = 'RA---TAN'           / Coordinate type                                "
"CTYPE2  = 'DEC--TAN'           / Coordinate type                                "
"CRVAL1  =                  5.0 / Coordinate reference value                     "
"CRVAL2  =                  5.0 / Coordinate reference value                     "
"CRPIX1  =                  0.0 / Coordinate reference pixel                     "
"CRPIX2  =                  0.0 / Coordinate reference pixel                     "
"CD1_1   =                  1.0 / Coordinate matrix                              "
"CD2_1   =                  0.0 / Coordinate matrix                              "
"CD1_2   =                  0.0 / Coordinate matrix                              "
"CD2_2   =                  1.0 / Coordinate matrix                              ";

char simple4[] = 
"XTENSION= 'IMAGE   '           / Extension type                                 "
"BITPIX  =                  -32 / Bits per pixel                                 "
"NAXIS   =                    2 / Number of image axes                           "
"NAXIS1  =                 2048 / Length of axis 1                               "
"NAXIS2  =                 4096 / Length of axis 2                               "
"BSCALE  =                    1 / Scale factor for unsigned short                "
"BZERO   =                32768 / Zero shift for unsigned short                  "
"PCOUNT  =                    0 / Number of bytes following image matrix         "
"EQUINOX =                2000. / Equinox of WCS                                 "
"WCSDIM  =                    2 / WCS dimensionality                             "
"CTYPE1  = 'RA---TAN'           / Coordinate type                                "
"CTYPE2  = 'DEC--TAN'           / Coordinate type                                "
"CRVAL1  =                  0.0 / Coordinate reference value                     "
"CRVAL2  =                  0.0 / Coordinate reference value                     "
"CRPIX1  =                  0.0 / Coordinate reference pixel                     "
"CRPIX2  =                  0.0 / Coordinate reference pixel                     "
"CD1_1   =                  1.0 / Coordinate matrix                              "
"CD2_1   =                  0.3 / Coordinate matrix                              "
"CD1_2   =                  0.6 / Coordinate matrix                              "
"CD2_2   =                  1.0 / Coordinate matrix                              ";

char fobj059[] = 
"XTENSION= 'IMAGE   '           / Extension type                                 "
"BITPIX  =                  -32 / Bits per pixel                                 "
"NAXIS   =                    2 / Number of image axes                           "
"NAXIS1  =                 2048 / Length of axis 1                               "
"NAXIS2  =                 4096 / Length of axis 2                               "
"BSCALE  =                    1 / Scale factor for unsigned short                "
"BZERO   =                32768 / Zero shift for unsigned short                  "
"PCOUNT  =                    0 / Number of bytes following image matrix         "
"EQUINOX =                2000. / Equinox of WCS                                 "
"WCSDIM  =                    2 / WCS dimensionality                             "
"CTYPE1  = 'RA---TAN'           / Coordinate type                                "
"CTYPE2  = 'DEC--TAN'           / Coordinate type                                "
"CRVAL1  =            158.70829 / Coordinate reference value                     "
"CRVAL2  =            51.919442 / Coordinate reference value                     "
"CRPIX1  =            4354.0869 / Coordinate reference pixel                     "
"CRPIX2  =            3995.8926 / Coordinate reference pixel                     "
"CD1_1   =        8.3414411e-07 / Coordinate matrix                              "
"CD2_1   =       -7.1192765e-05 / Coordinate matrix                              "
"CD1_2   =       -7.1533145e-05 / Coordinate matrix                              "
"CD2_2   =        5.6420051e-07 / Coordinate matrix                              ";

// Page 1117 of Calabretta & Greisen (2002)
char paperII[] = 
"XTENSION= 'IMAGE   '           / Extension type                                 "
"BITPIX  =                  -32 / Bits per pixel                                 "
"NAXIS   =                    2 / Number of image axes                           "
"NAXIS1  =                 4096 / Length of axis 1                               "
"NAXIS2  =                 4096 / Length of axis 2                               "
"BSCALE  =                    1 / Scale factor for unsigned short                "
"BZERO   =                32768 / Zero shift for unsigned short                  "
"PCOUNT  =                    0 / Number of bytes following image matrix         "
"EQUINOX =                2000. / Equinox of WCS                                 "
"WCSDIM  =                    2 / WCS dimensionality                             "
"CTYPE1  = 'RA---TAN'           / Coordinate type                                "
"CTYPE2  = 'DEC--TAN'           / Coordinate type                                "
"CRVAL1  =            145.30459 / Coordinate reference value                     "
"CRVAL2  =            8.5738600 / Coordinate reference value                     "
"CRPIX1  =            2048.5000 / Coordinate reference pixel                     "
"CRPIX2  =            2048.5000 / Coordinate reference pixel                     "
"CDELT1  =           -0.0002778 / Coordinate reference pixel                     "
"CDELT2  =            0.0002778 / Coordinate reference pixel                     "
"CD1_1   =                  1.0 / Coordinate matrix                              "
"CD2_1   =                  0.0 / Coordinate matrix                              "
"CD1_2   =                  0.0 / Coordinate matrix                              "
"CD2_2   =                  1.0 / Coordinate matrix                              ";

char simple5[] = 
"XTENSION= 'IMAGE   '           / Extension type                                 "
"BITPIX  =                  -32 / Bits per pixel                                 "
"NAXIS   =                    2 / Number of image axes                           "
"NAXIS1  =                 8192 / Length of axis 1                               "
"NAXIS2  =                 8192 / Length of axis 2                               "
"BSCALE  =                    1 / Scale factor for unsigned short                "
"BZERO   =                32768 / Zero shift for unsigned short                  "
"PCOUNT  =                    0 / Number of bytes following image matrix         "
"EQUINOX =                2000. / Equinox of WCS                                 "
"WCSDIM  =                    2 / WCS dimensionality                             "
"CTYPE1  = 'RA---TAN'           / Coordinate type                                "
"CTYPE2  = 'DEC--TAN'           / Coordinate type                                "
"CRVAL1  =                 60.0 / Coordinate reference value                     "
"CRVAL2  =                 45.0 / Coordinate reference value                     "
"CRPIX1  =                  0.0 / Coordinate reference pixel                     "
"CRPIX2  =                  0.0 / Coordinate reference pixel                     "
"CD1_1   =                  0.0 / Coordinate matrix                              "
"CD2_1   =          -0.00027777 / Coordinate matrix                              "
"CD1_2   =          -0.00027777 / Coordinate matrix                              "
"CD2_2   =                  0.0 / Coordinate matrix                              ";

double epsilon = 0.00001;

wcs_t* getwcs(char* buf)
{
	wcs_t* wcs = wcsninit(buf, strlen(buf));
	assert(wcs);
	return wcs;
}

void copy_wcs_into_sip(wcs_t* wcs, sip_t* sip)
{
	sip->crval[0] = wcs->crval[0];
	sip->crval[1] = wcs->crval[1];
	sip->crpix[0] = wcs->crpix[0];
	sip->crpix[1] = wcs->crpix[1];
	sip->cd[0][0] = wcs->cd[0];
	sip->cd[0][1] = wcs->cd[2];
	sip->cd[1][0] = wcs->cd[1];
	sip->cd[1][1] = wcs->cd[3];
}

void tryad(sip_t* sip, wcs_t* wcs, double a, double d, char* name,
		double *opx, double *opy)
{
	double px, py;
	radec2pixelxy(sip, a,d,&px,&py);

	int offscr;
	double wcspx,wcspy;
	wcs2pix(wcs, a, d, &wcspx, &wcspy, &offscr);

	if (opx) *opx = wcspx;
	if (opy) *opy = wcspy;

	printf("AD Test: %s\n",name);
	printf("px =%lf, py =%lf\n", px,py);
	printf("wpx=%lf, wpy=%lf, offscr=%d\n", wcspx,wcspy, offscr);

	if (fabs(px-wcspx) > epsilon)
		printf("** FAILURE X wcs -> pix \n");
	if (fabs(py-wcspy) > epsilon)
		printf("** FAILURE Y wcs -> pix \n");
}

void tryxy(sip_t* sip, wcs_t* wcs, double px, double py, char* name)
{
	printf("XY Test: %s\n",name);
	double a, d;
	double wcsa, wcsd;
	pixelxy2radec(sip, px,py, &a, &d);
	pix2wcs(wcs, px, py, &wcsa, &wcsd);
	printf("sa=%lf, sd=%lf\n", a,d);
	printf("wa=%lf, wd=%lf\n", wcsa,wcsd);
	if (fabs(a-wcsa) > epsilon)
		printf("** FAILURE A on pix -> wcs\n");
	if (fabs(d-wcsd) > epsilon)
		printf("** FAILURE D on pix -> wcs\n");
	printf("\n");
}

void grinder(sip_t* sip, wcs_t* wcs, int n, char* name)
{

	copy_wcs_into_sip(wcs,sip);

	double xyzcrval[3];
	radecdeg2xyzarr(sip->crval[0],sip->crval[1],xyzcrval);

	printf("------------------------------------------\n");
	printf("TEST::::::: %s\n", name);
	int i;
	double a, d;

	printf("Make sure pixel origin goes to same place\n");
	tryxy(sip,wcs,0,0,"  ");

	for (i=0; i<n; i++) {
		printf("case: \n");
		double xyz[3];
		xyz[0] = rand() / (double)RAND_MAX - 0.5;
		xyz[1] = rand() / (double)RAND_MAX - 0.5;
		xyz[2] = rand() / (double)RAND_MAX - 0.5;

		//xyz[0] = 1;
		//xyz[1] = 0;
		//xyz[2] = 0;

		double norm = sqrt(xyz[0]*xyz[0]+xyz[1]*xyz[1]+xyz[2]*xyz[2]);
		xyz[0] /= norm;
		xyz[1] /= norm;
		xyz[2] /= norm;

		xyz[0] /= 400; // make this a small dist from crval
		xyz[1] /= 400;
		xyz[2] /= 400;

		if (i==0) {
			printf("Trying reference point first\n");
			xyz[0] = 0.0;
			xyz[1] = 0.0;
			xyz[2] = 0.0;
		}


		xyz[0] += xyzcrval[0];
		xyz[1] += xyzcrval[1];
		xyz[2] += xyzcrval[2];

		// normalize it again
		norm = sqrt(xyz[0]*xyz[0]+xyz[1]*xyz[1]+xyz[2]*xyz[2]);
		xyz[0] /= norm;
		xyz[1] /= norm;
		xyz[2] /= norm;

		xyz2radec(xyz[0], xyz[1], xyz[2], &a, &d);

		a = rad2deg(a);
		d = rad2deg(d);

		printf("a=%lf, d=%lf\n", a,d);

		double px,py;
		tryad(sip,wcs,a,d,"...", &px, &py);
		tryxy(sip,wcs,px,py, "..");
	}
	free(wcs);
}


int main()
{
	srand(0);
	int n=6;
	sip_t* sip = createsip();
	//grinder(sip, getwcs(fobj059), n, "fobj059 without TNX");
	//grinder(sip, getwcs(simple) , n, "identity transform");
	//grinder(sip, getwcs(simple2), n, "non-zero crpix");
	//grinder(sip, getwcs(simple3), n, "non-zero crval");
	//grinder(sip, getwcs(simple4), n, "non-rotation identity");
	grinder(sip, getwcs(simple5), n, "Simple case at 60,40deg");
	return 0;
}



