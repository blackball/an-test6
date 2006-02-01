#include <math.h>
#include <stdio.h>

#include "healpix.h"

int main(int argc, char** args) {
    int rastep, decstep;
    int Nra = 100;
    int Ndec = 100;
    double ra, dec;
    int healpix;

    printf("radechealpix=zeros(%i,3);\n", Nra*Ndec);
    for (rastep=0; rastep<Nra; rastep++) {
	ra = ((double)rastep / (double)(Nra-1)) * 2.0 * M_PI;

	for (decstep=0; decstep<Ndec; decstep++) {
 	    dec = (((double)decstep / (double)(Ndec-1)) * M_PI) - M_PI/2.0;

	    healpix = radectohealpix(ra, dec);

	    printf("radechealpix(%i,:)=[%g,%g,%i];\n", 
		   (rastep*Ndec) + decstep + 1, ra, dec, healpix);
	}
    }
    printf("ra=radechealpix(:,1);\n");
    printf("dec=radechealpix(:,2);\n");
    printf("healpix=radechealpix(:,3);\n");
    
    return 0;
}
