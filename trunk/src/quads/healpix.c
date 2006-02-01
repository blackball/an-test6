#include <math.h>

#include "healpix.h"
#include "starutil.h"

/*
  We should be able to do some simple trig to convert.
  Just go via xyz for now.
*/
int radectohealpix(double ra, double dec) {
    double x, y, z;
    x = radec2x(ra, dec);
    y = radec2y(ra, dec);
    z = radec2z(ra, dec);
    return xyztohealpix(x, y, z);
}

int xyztohealpix(double x, double y, double z) {
    double phi;
    double phioverpi;
    double twothirds = 2.0 / 3.0;

    phi = atan2(y, x);
    if (phi < 0.0)
	phi += 2.0 * M_PI;

    phioverpi = phi / M_PI;

    if ((z >= twothirds) || (z <= -twothirds)) {
	// definitely in the polar regions.
	int offset;
	int pix;

	// the north polar healpixes are 0,1,2,3
	// the south polar healpixes are 8,9,10,11
	if (z >= twothirds)
	    offset = 0;
	else
	    offset = 8;

	pix = (int)(phioverpi * 2.0);

	return offset + pix;

    } else {
	// could be polar or equatorial.
	double phimod;
	int offset;
	double z1, z2;

	offset = (int)(phioverpi * 2.0);
	phimod = phioverpi - offset * 0.5;

	z1 =  twothirds - (8.0 / 3.0) * phimod;
	z2 = -twothirds + (8.0 / 3.0) * phimod;

	if ((z >= z1) && (z >= z2)) {
	    // north polar
	    return offset;
	} else if ((z <= z1) && (z <= z2)) {
	    // south polar
	    return offset + 8;
	} else if (phimod < 0.25) {
	    // left equatorial
	    return offset + 4;
	} else {
	    // right equatorial
	    return ((offset+1)%4) + 4;
	}

	// the polar pixel center is at (z,phi/pi) = (2/3, 1/4)
	// the left pixel center is at (z,phi/pi) = (0, 0)
	// the right pixel center is at (z,phi/pi) = (0, 1/2)
    }
}
