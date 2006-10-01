#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include "starutil.h"
#include "sip.h"

void grinder(sip_t* sip, int n)
{
	double xyzcrval[3];
	radecdeg2xyzarr(sip->crval[0],sip->crval[1],xyzcrval);

	int i;
	for (i=0; i<n; i++) {
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

		xyz[0] /= 40; // make this a small dist from crval
		xyz[1] /= 40;
		xyz[2] /= 40;

		/*
		xyz[0] = 0;
		xyz[1] = 0.01;
		xyz[2] = 0.00;
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

		printf("px=%lf, py=%lf\n", px,py);

		double aback, dback;
		pixelxy2radec(sip, px,py, &aback, &dback);
		printf("aback=%lf, dback=%lf\n", aback,dback);

		printf("a-aback=%lf, d-dback=%lf\n", a-aback,d-dback);
		printf("\n");
	}

}


int main()
{

	srand(0);

	// Simple identity
	sip_t* sip = createsip();
	grinder(sip, 10);
	sip->cd[0][0] = 5;
	sip->cd[0][1] = 0.3;
	sip->cd[1][0] = 0.4;
	sip->cd[1][1] = 3;
	grinder(sip, 10);
	return 0;
}



