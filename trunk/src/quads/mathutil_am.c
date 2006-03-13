#include "mathutil_am.h"

/* applies the given transform to uu,vv to produce xx,yy,zz and puts
   these values into the star object s */
// DEPRECATE
void image_to_xyz_old(double uu, double vv, star *s, double *transform)
{
	double x, y, z;
	double length;
	if (s == NULL || transform == NULL)
		return ;

	x = uu * transform[0] +
		vv * transform[1] +
		transform[2];

	y = uu * transform[3] +
		vv * transform[4] +
		transform[5];

	z = uu * transform[6] +
		vv * transform[7] +
		transform[8];

	length = sqrt(x*x + y*y + z*z);

	x /= length;
	y /= length;
	z /= length;

	star_set(s, 0, x);
	star_set(s, 1, y);
	star_set(s, 2, z);
}

/* takes four image locations from ABCDpix, maps them to stars ABCD
   using the order given, and finally fits a transform to go
   from image locations to xyz positions based on these four datapoints */
double *fit_transform_old(xy *ABCDpix, char order, star *A, star *B, star *C, star *D)
{
	double det, uu, uv, vv, sumu, sumv;
	char oA = 0, oB = 1, oC = 2, oD = 3;
	double Au, Av, Bu, Bv, Cu, Cv, Du, Dv;
	double matQ[9];
	double matR[12];

	if (order == ABCD_ORDER) {
		oA = 0;
		oB = 1;
		oC = 2;
		oD = 3;
	}
	if (order == BACD_ORDER) {
		oA = 1;
		oB = 0;
		oC = 2;
		oD = 3;
	}
	if (order == ABDC_ORDER) {
		oA = 0;
		oB = 1;
		oC = 3;
		oD = 2;
	}
	if (order == BADC_ORDER) {
		oA = 1;
		oB = 0;
		oC = 3;
		oD = 2;
	}

	// image plane coordinates of A,B,C,D
	Au = xy_refx(ABCDpix, oA);
	Av = xy_refy(ABCDpix, oA);
	Bu = xy_refx(ABCDpix, oB);
	Bv = xy_refy(ABCDpix, oB);
	Cu = xy_refx(ABCDpix, oC);
	Cv = xy_refy(ABCDpix, oC);
	Du = xy_refx(ABCDpix, oD);
	Dv = xy_refy(ABCDpix, oD);

	//fprintf(stderr,"Image ABCD = (%lf,%lf) (%lf,%lf) (%lf,%lf) (%lf,%lf)\n",
	//  	    Au,Av,Bu,Bv,Cu,Cv,Du,Dv);

	// define M to be the 3x4 matrix [Au,Bu,Cu,Du;ones(1,4)]
	// define X to be the 3x4 matrix [Ax,Bx,Cx,Dx;Ay,By,Cy,Dy;Az,Bz,Cz,Dz]

	// set Q to be the 3x3 matrix  M*M'
	uu = Au * Au + Bu * Bu + Cu * Cu + Du * Du;
	uv = Au * Av + Bu * Bv + Cu * Cv + Du * Dv;
	vv = Av * Av + Bv * Bv + Cv * Cv + Dv * Dv;
	sumu = Au + Bu + Cu + Du;
	sumv = Av + Bv + Cv + Dv;
	matQ[0] = uu;
	matQ[1] = uv;
	matQ[2] = sumu;
	matQ[3] = uv;
	matQ[4] = vv;
	matQ[5] = sumv;
	matQ[6] = sumu;
	matQ[7] = sumv;
	matQ[8] = 4.0;

	// take the inverse of Q in-place, so Q=inv(M*M')
	det = inverse_3by3(matQ);

	//fprintf(stderr,"det=%.12g\n",det);
	if (det < 0)
		fprintf(stderr, "WARNING (fit_transform) -- determinant<0\n");

	if (det == 0.0) {
		fprintf(stderr, "ERROR (fit_transform) -- determinant zero\n");
		return (NULL);
	}

	//fprintf(stderr, "det=%g\n", det);

	// set R to be the 4x3 matrix M'*inv(M*M')=M'*Q
	matR[0]  = matQ[0] * Au + matQ[3] * Av + matQ[6];
	matR[1]  = matQ[1] * Au + matQ[4] * Av + matQ[7];
	matR[2]  = matQ[2] * Au + matQ[5] * Av + matQ[8];
	matR[3]  = matQ[0] * Bu + matQ[3] * Bv + matQ[6];
	matR[4]  = matQ[1] * Bu + matQ[4] * Bv + matQ[7];
	matR[5]  = matQ[2] * Bu + matQ[5] * Bv + matQ[8];
	matR[6]  = matQ[0] * Cu + matQ[3] * Cv + matQ[6];
	matR[7]  = matQ[1] * Cu + matQ[4] * Cv + matQ[7];
	matR[8]  = matQ[2] * Cu + matQ[5] * Cv + matQ[8];
	matR[9]  = matQ[0] * Du + matQ[3] * Dv + matQ[6];
	matR[10] = matQ[1] * Du + matQ[4] * Dv + matQ[7];
	matR[11] = matQ[2] * Du + matQ[5] * Dv + matQ[8];

	// set Q to be the 3x3 matrix X*R

	matQ[0] = star_ref(A, 0) * matR[0] + star_ref(B, 0) * matR[3] +
	          star_ref(C, 0) * matR[6] + star_ref(D, 0) * matR[9];
	matQ[1] = star_ref(A, 0) * matR[1] + star_ref(B, 0) * matR[4] +
	          star_ref(C, 0) * matR[7] + star_ref(D, 0) * matR[10];
	matQ[2] = star_ref(A, 0) * matR[2] + star_ref(B, 0) * matR[5] +
	          star_ref(C, 0) * matR[8] + star_ref(D, 0) * matR[11];

	matQ[3] = star_ref(A, 1) * matR[0] + star_ref(B, 1) * matR[3] +
	          star_ref(C, 1) * matR[6] + star_ref(D, 1) * matR[9];
	matQ[4] = star_ref(A, 1) * matR[1] + star_ref(B, 1) * matR[4] +
	          star_ref(C, 1) * matR[7] + star_ref(D, 1) * matR[10];
	matQ[5] = star_ref(A, 1) * matR[2] + star_ref(B, 1) * matR[5] +
	          star_ref(C, 1) * matR[8] + star_ref(D, 1) * matR[11];

	matQ[6] = star_ref(A, 2) * matR[0] + star_ref(B, 2) * matR[3] +
	          star_ref(C, 2) * matR[6] + star_ref(D, 2) * matR[9];
	matQ[7] = star_ref(A, 2) * matR[1] + star_ref(B, 2) * matR[4] +
	          star_ref(C, 2) * matR[7] + star_ref(D, 2) * matR[10];
	matQ[8] = star_ref(A, 2) * matR[2] + star_ref(B, 2) * matR[5] +
		      star_ref(C, 2) * matR[8] + star_ref(D, 2) * matR[11];

	{
		double* copyQ = (double*)malloc(9 * sizeof(double));
		memcpy(copyQ, matQ, 9 * sizeof(double));
		return copyQ;
	}
}

