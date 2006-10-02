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
	sip->cd[0][1] = wcs->cd[1];
	sip->cd[1][0] = wcs->cd[2];
	sip->cd[1][1] = wcs->cd[3];
}

void grinder(sip_t* sip, wcs_t* wcs, int n, char* name)
{

	copy_wcs_into_sip(wcs,sip);

	double xyzcrval[3];
	radecdeg2xyzarr(sip->crval[0],sip->crval[1],xyzcrval);

	printf("TEST:::::::\n");
	int i;
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

		xyz[0] /= 100; // make this a small dist from crval
		xyz[1] /= 100;
		xyz[2] /= 100;

		/*
		xyz[0] = 0.0;
		xyz[1] = 0.01;
		xyz[2] = 0.0;
		*/

		xyz[0] += xyzcrval[0];
		xyz[1] += xyzcrval[1];
		xyz[2] += xyzcrval[2];

		// normalize it again
		norm = sqrt(xyz[0]*xyz[0]+xyz[1]*xyz[1]+xyz[2]*xyz[2]);
		xyz[0] /= norm;
		xyz[1] /= norm;
		xyz[2] /= norm;

		double a, d;
		xyz2radec(xyz[0], xyz[1], xyz[2], &a, &d);

		a = rad2deg(a);
		d = rad2deg(d);

		printf("a=%lf, d=%lf\n", a,d);

		double px, py;
		radec2pixelxy(sip, a,d,&px,&py);

		printf("px =%lf, py =%lf\n", px,py);

		int offscr;
		double wcspx,wcspy;
		wcs2pix(wcs, a, d, &wcspx, &wcspy, &offscr);
		printf("wpx=%lf, wpy=%lf, offscr=%d\n", wcspx,wcspy, offscr);

		double epsilon = 0.00001;
		if (abs(px-wcspx) > epsilon)
			printf("** FAILURE X wcs -> pix \n");
		if (abs(py-wcspy) > epsilon)
			printf("** FAILURE Y wcs -> pix \n");

		double aback, dback;
		double wcsa, wcsd;
		pixelxy2radec(sip, wcspx,wcspy, &aback, &dback);
		pix2wcs(wcs, a, d, &wcsa, &wcsd);
		printf("a=%lf, dback=%lf\n", aback,dback);
		//printf("a-aback=%lf, d-dback=%lf\n", a-aback,d-dback);
		if (abs(aback-wcsa) > epsilon)
			printf("** FAILURE A pix -> wcs\n");
		if (abs(dback-wcsd) > epsilon)
			printf("** FAILURE D pix -> wcs\n");
		printf("\n");
	}
	free(wcs);
}


int main()
{

	srand(0);
	sip_t* sip = createsip();
	grinder(sip, getwcs(fobj059), 2, "fobj059 without TNX");
	grinder(sip, getwcs(simple), 2, "identity transform");
	grinder(sip, getwcs(simple2), 2, "identity transform");
	return 0;
}



