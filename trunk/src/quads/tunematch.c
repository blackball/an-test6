#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "matchfile.h"
#include "kdtree.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"
#include "xylist.h"

static const char* OPTIONS = "hi:o:s:r:t:m:f:X:Y:";

static void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n"
			"   -i <input matchfile>\n"
			"   -o <output matchfile>\n"
			"   -f <field (xyls) file>\n"
			"     [-X <x column name>]\n"
			"     [-Y <y column name>]\n"
			"   -s <star kdtree>\n"
			"   -r <overlap radius (arcsec)>\n"
			"   -t <overlap threshold>\n"
			"   -m <min field objects required>\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	/*
	  char** inputfiles = NULL;
	  int ninputfiles = 0;
	  int i;
	*/
	char* infn = NULL;
	char* outfn = NULL;
	char* xylsfn = NULL;
	char* starfn = NULL;
	char* xname = NULL;
	char* yname = NULL;
	matchfile* matchin;
	matchfile* matchout;
	kdtree_t* startree;
	xylist* xyls;
	double* fielduv = NULL;
	double* fieldxyz = NULL;

	double overlap_thresh = 0.0;
	double overlap_rad = 0.0;
	int min_ninfield = 0;

	double overlap_d2;



    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'X':
			xname = optarg;
			break;
		case 'Y':
			yname = optarg;
			break;
		case 'f':
			xylsfn = optarg;
			break;
		case 'i':
			infn = optarg;
			break;
		case 'o':
			outfn = optarg;
			break;
		case 's':
			starfn = optarg;
			break;
		case 't':
			overlap_thresh = atof(optarg);
			break;
		case 'm':
			min_ninfield = atoi(optarg);
			break;
		case 'r':
			overlap_rad = atof(optarg);
			break;
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}

	if (!infn || !outfn || !starfn || !xylsfn) {
		fprintf(stderr, "Must specify input and output match files, star kdtree, and field filenames.\n");
		printHelp(progname);
		exit(-1);
	}

	if (overlap_rad == 0.0 || overlap_thresh == 0.0) {
		fprintf(stderr, "Must specify overlap radius and threshold.\n");
		printHelp(progname);
		exit(-1);
	}

	overlap_d2 = arcsec2distsq(overlap_rad);

	matchin = matchfile_open(infn);
	if (!matchin) {
		fprintf(stderr, "Failed to read input matchfile %s.\n", infn);
		exit(-1);
	}

	startree = kdtree_fits_read_file(starfn);
	if (!startree) {
		fprintf(stderr, "Failed to open star kdtree %s.\n", starfn);
		exit(-1);
	}

	xyls = xylist_open(xylsfn);
	if (!xyls) {
		fprintf(stderr, "Failed to open xylist %s.\n", xylsfn);
		exit(-1);
	}
	if (xname)
		xyls->xname = xname;
	if (yname)
		xyls->yname = yname;

	matchout = matchfile_open_for_writing(outfn);
	if (!matchout) {
		fprintf(stderr, "Failed to open output matchfile %s.\n", outfn);
		exit(-1);
	}
	if (matchfile_write_header(matchout)) {
		fprintf(stderr, "Failed to write output matchfile header.\n");
		exit(-1);
	}

	for (;;) {
		double xyz[3];
		double t1[3];
		double t2[3];
		double pu[3];
		double pv[3];
		double u1, u2, v1, v2;
		double theta1, theta2;
		double diff;
		double ra, dec;
		double R, D, T, S, P;
		double len1;
		double len2;
		double r2;
		int NF;
		kdtree_qres_t* res;
		MatchObj* mo;
		int s;
		int i;

		double cosT, sinT;
		double cosD, sinD;
		double cosR, sinR;
		double maxd2;
		int NI;
		double* indxyz;

		mo = matchfile_buffered_read_match(matchin);
		if (!mo)
			break;
		if (!mo->transform_valid) {
			printf("MatchObject transform isn't valid.\n");
			continue;
		}

		// find the field center
		star_midpoint(xyz, mo->sMin, mo->sMax);
		// find the field radius
		r2 = distsq(xyz, mo->sMin, 3);
		r2 *= 1.02;
		// find stars in that region
		res = kdtree_rangesearch_nosort(startree, xyz, r2);
		if (!res->nres) {
			printf("no stars found.\n");
			//kdtree_free_query(res);
			//continue;
		}

		// convert the transformation matrix into RA, DEC, theta, scale.
		xyz[0] = mo->transform[2];
		xyz[1] = mo->transform[5];
		xyz[2] = mo->transform[8];
		normalize_3(xyz);
		xyz2radec(xyz[0], xyz[1], xyz[2], &ra, &dec);

		// scale is the mean of the lengths of t1 and t2.
		t1[0] = mo->transform[0];
		t1[1] = mo->transform[3];
		t1[2] = mo->transform[6];
		t2[0] = mo->transform[1];
		t2[1] = mo->transform[4];
		t2[2] = mo->transform[7];
		len1 = sqrt(t1[0]*t1[0] + t1[1]*t1[1] + t1[2]*t1[2]);
		len2 = sqrt(t2[0]*t2[0] + t2[1]*t2[1] + t2[2]*t2[2]);
		S = (len1 + len2) * 0.5;

		// we get theta by projecting pu=[1 0 0] and pv=[0 1 0] through R*D*P
		// and computing the projections of t1, t2 on these.
		R = M_PI_2 - ra;
		D = M_PI_2 - dec;
		cosR = cos(R);
		sinR = sin(R);
		cosD = cos(D);
		sinD = sin(D);

		pu[0] = cosR;
		pu[1] = -sinR;
		pu[2] = 0.0;
		pv[0] = cosD * sinR;
		pv[1] = cosD * cosR;
		pv[2] = -sinD;
		// note, pu and pv have length 1.

		u1 = t1[0]*pu[0] + t1[1]*pu[1] + t1[2]*pu[2];
		v1 = t1[0]*pv[0] + t1[1]*pv[1] + t1[2]*pv[2];
		u2 = t2[0]*pu[0] + t2[1]*pu[1] + t2[2]*pu[2];
		v2 = t2[0]*pv[0] + t2[1]*pv[1] + t2[2]*pv[2];
		theta1 = atan2(v1, u1);
		theta2 = atan2(v2, u2);
		diff = fabs(theta1 - theta2);
		if (diff > M_PI)
			diff = 2.0*M_PI - diff;
		// if they're not within 10 degrees of being 90 degrees apart,
		// warn.
		if (fabs(rad2deg(diff) - 90) > 10) {
			printf("warning, theta1 and theta2 should be nearly orthogonal.\n");
		}
		T = theta1;
		// parity: is theta2 90 degrees clockwise or anti-clockwise of theta1?
		diff = theta2 - theta1;
		if (diff < -M_PI)
			diff += 2.0 * M_PI;
		if (diff > M_PI)
			diff -= 2.0 * M_PI;
		P = (diff > 0.0) ? 1.0 : 0.0;

		printf("RA,DEC=(%g, %g) degrees, scale=%g arcsec/pixel,",
			   rad2deg(ra), rad2deg(dec), S * 180/M_PI * 60 * 60);
		printf("theta=%g, parity=%i.\n",
			   rad2deg(T), (int)P);

		cosT = cos(T);
		sinT = sin(T);

		NF = xylist_n_entries(xyls, mo->fieldnum);
		if (NF <= 0) {
			fprintf(stderr, "Failed to get field %i.\n", mo->fieldnum);
			continue;
		}
		fielduv = realloc(fielduv, NF * 2 * sizeof(double));
		xylist_read_entries(xyls, mo->fieldnum, 0, NF, fielduv);

		printf("NF=%i\n", NF);

		fieldxyz = realloc(fieldxyz, NF * 3 * sizeof(double));

		NI = res->nres;
		indxyz = res->results;
		printf("NI=%i\n", NI);

		// 5 sigmas
		maxd2 = 25.0 * overlap_d2;

		// "xyz" is the field origin.

		for (s=0; s<10; s++) {
			double u,v;
			int j;

			double totalweight;
			double totalscale;
			double totaltheta;
			double totalxyz[3];
			double step;

			// project each field object...
			for (i=0; i<NF; i++) {
				double tu, tv;
				double d1, d2, d3;
				// HACK - should precompute these matrix elements...
				u = fielduv[i*2 + 0];
				v = fielduv[i*2 + 1];
				tu =  S*u*cosT + S*v*sinT;
				tv = -S*u*sinT + S*v*cosT;
				d1 = tu;
				d2 =  cosD * tv + sinD;
				d3 = -sinD * tv + cosD;
				fieldxyz[i*3+0] = d1 *  cosR + d2 * sinR;
				fieldxyz[i*3+1] = d1 * -sinR + d2 * cosR;
				fieldxyz[i*3+2] = d3;
			}

			totalweight = 0.0;
			totalscale  = 0.0;
			totaltheta  = 0.0;
			totalxyz[0] = totalxyz[1] = totalxyz[2] = 0.0;

			for (i=0; i<NF; i++) {
				double* fxyz;
				double rf;
				double fu, fv;
				double ftheta;
				fxyz = fieldxyz + i*3;
				rf = sqrt(distsq(fxyz, xyz, 3));
				fu = (fxyz[0]-xyz[0])*pu[0] + (fxyz[1]-xyz[1])*pu[1] + (fxyz[2]-xyz[2])*pu[2];
				fv = (fxyz[0]-xyz[0])*pv[0] + (fxyz[1]-xyz[1])*pv[1] + (fxyz[2]-xyz[2])*pv[2];
				ftheta = atan2(fv, fu);

				for (j=0; j<NI; j++) {
					double* ixyz;
					double d2;
					double ri;
					double iu, iv;
					double itheta;
					double weight;
					double dtheta;

					ixyz = indxyz + j*3;
					d2 = distsq(fxyz, ixyz, 3);
					if (d2 > maxd2)
						continue;

					ri = sqrt(distsq(ixyz, xyz, 3));

					weight = exp(-0.5 * (d2 / overlap_d2));
					totalweight += weight;

					printf("f=%i, i=%i: dist: %g (%g sigmas), weight %g\n", i, j, sqrt(d2),
						   sqrt(d2 / overlap_d2), weight);
					//printf("index scale: %g\nfield scale: %g\n", ri, rf);
					//printf("scale change factor: %g\n", (ri / rf));
					totalscale += log(ri / rf) * weight;

					iu = (ixyz[0]-xyz[0])*pu[0] + (ixyz[1]-xyz[1])*pu[1] + (ixyz[2]-xyz[2])*pu[2];
					iv = (ixyz[0]-xyz[0])*pv[0] + (ixyz[1]-xyz[1])*pv[1] + (ixyz[2]-xyz[2])*pv[2];

					/*
					  printf("field (u,v) = (%g,%g) or (%g,%g)\n",
					  fu, fv, fielduv[i*2], fielduv[i*2+1]);
					*/
					/*
					  printf("field (u,v) = (%g,%g)\n", fu, fv);
					  printf("index (u,v) = (%g,%g)\n", iu, iv);
					*/
					itheta = atan2(iv, iu);
					//printf("field theta=%g\nindex theta=%g\n", ftheta, itheta);

					dtheta = itheta - ftheta;
					if (dtheta > M_PI)
						dtheta = 2.0 * M_PI - dtheta;
					if (dtheta < -M_PI)
						dtheta += 2.0 * M_PI;
					totaltheta += dtheta * weight;

					totalxyz[0] = weight * (ixyz[0] - fxyz[0]);
					totalxyz[1] = weight * (ixyz[1] - fxyz[1]);
					totalxyz[2] = weight * (ixyz[2] - fxyz[2]);
				}
			}

			totalscale /= totalweight;
			totaltheta /= totalweight;
			totalxyz[0] /= totalweight;
			totalxyz[1] /= totalweight;
			totalxyz[2] /= totalweight;

			totalscale = exp(totalscale);

			printf("\n");
			printf("Total weight: %g\n", totalweight);
			printf("Total change:\n");
			printf("=============\n");
			printf("scale: %g\n", totalscale);
			printf("theta: %g\n", totaltheta);
			printf("xyz: (%g,%g,%g)\n", totalxyz[0], totalxyz[1], totalxyz[2]);
			printf("\n");

			step = 0.5;

			xyz[0] += step * totalxyz[0];
			xyz[1] += step * totalxyz[1];
			xyz[2] += step * totalxyz[2];
			normalize_3(xyz);
			xyz2radec(xyz[0], xyz[1], xyz[2], &ra, &dec);
			R = M_PI_2 - ra;
			D = M_PI_2 - dec;
			T += step * totaltheta;
			S *= pow(totalscale, step);

			cosR = cos(R);
			sinR = sin(R);
			cosD = cos(D);
			sinD = sin(D);
			cosT = cos(T);
			sinT = sin(T);

			pu[0] = cosR;
			pu[1] = -sinR;
			pu[2] = 0.0;
			pv[0] = cosD * sinR;
			pv[1] = cosD * cosR;
			pv[2] = -sinD;
		}


		kdtree_free_query(res);
	}

	free(fielduv);
	free(fieldxyz);

	xylist_close(xyls);
	matchfile_close(matchin);
	kdtree_close(startree);

	if (matchfile_fix_header(matchout) ||
		matchfile_close(matchout)) {
		fprintf(stderr, "Failed to close output matchfile %s.\n", outfn);
		exit(-1);
	}

	return 0;
}

