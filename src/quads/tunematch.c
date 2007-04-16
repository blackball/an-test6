/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

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
#include "xylist.h"
#include "verify.h"
#include "starkd.h"

static const char* OPTIONS = "hi:o:s:S:r:f:F:X:Y:w:Mu:v:";

static void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n"
			"   -i <input matchfile>\n"
			"   -o <output matchfile>\n"
			"   (-f <field (xyls) file>  OR  -F <field (xyls) template>)\n"
			"     [-X <x column name>]\n"
			"     [-Y <y column name>]\n"
			"   (-s <star kdtree>  OR  -S <star kdtree template>)\n"
			"   -r <overlap radius (arcsec)>\n"
			"   [-w <FITS WCS header output file>]\n"
			"   [-M]: write Matlab script showing the tuning\n"
			"     [-u <max-u-image-coordinate>] (defaults 1024)\n"
			"     [-v <max-v-image-coordinate>]\n"
			"\n", progname);
}

static void rotation_matrix_z(double* M, double R) {
	double cosR = cos(R);
	double sinR = sin(R);
	M[0] = cosR;
	M[1] = -sinR;
	M[2] = 0.0;
	M[3] = sinR;
	M[4] = cosR;
	M[5] = 0.0;
	M[6] = 0.0;
	M[7] = 0.0;
	M[8] = 1.0;
}

static void rotation_matrix_x(double* M, double R) {
	double cosR = cos(R);
	double sinR = sin(R);
	M[0] = 1.0;
	M[1] = 0.0;
	M[2] = 0.0;
	M[3] = 0.0;
	M[4] = cosR;
	M[5] = -sinR;
	M[6] = 0.0;
	M[7] = sinR;
	M[8] = cosR;
}

static void scale_matrix(double* M, double sx, double sy, double sz) {
	M[0] = sx;
	M[1] = 0.0;
	M[2] = 0.0;
	M[3] = 0.0;
	M[4] = sy;
	M[5] = 0.0;
	M[6] = 0.0;
	M[7] = 0.0;
	M[8] = sz;
}

static void parity_matrix_xy(double* M, bool P) {
	M[0] = (P ? 0 : 1);
	M[1] = (P ? 1 : 0);
	M[2] = 0;
	M[3] = (P ? 1 : 0);
	M[4] = (P ? 0 : 1);
	M[5] = 0;
	M[6] = 0;
	M[7] = 0;
	M[8] = 1;
}

// compute the new & improved MatchObj.
static void update_matchobj(MatchObj* mo, double* Mall,
							double* fielduv, int NF) {
	double tmp[3];
	double pu[3], pv[3], pw[3];
	double minu, maxu, minv, maxv;
	int i;

	tmp[0] = 1.0;
	tmp[1] = 0.0;
	tmp[2] = 0.0;
	matrix_vector_3(Mall, tmp, pu);

	tmp[0] = 0.0;
	tmp[1] = 1.0;
	tmp[2] = 0.0;
	matrix_vector_3(Mall, tmp, pv);

	tmp[0] = 0.0;
	tmp[1] = 0.0;
	tmp[2] = 1.0;
	matrix_vector_3(Mall, tmp, pw);

	mo->transform[0] = pu[0];
	mo->transform[3] = pu[1];
	mo->transform[6] = pu[2];
	mo->transform[1] = pv[0];
	mo->transform[4] = pv[1];
	mo->transform[7] = pv[2];
	mo->transform[2] = pw[0];
	mo->transform[5] = pw[1];
	mo->transform[8] = pw[2];

	minu = HUGE_VAL;
	maxu = -HUGE_VAL;
	minv = HUGE_VAL;
	maxv = -HUGE_VAL;
	for (i=0; i<NF; i++) {
		double u = fielduv[i*2+0];
		double v = fielduv[i*2+1];
		if (u < minu) minu = u;
		if (u > maxu) maxu = u;
		if (v < minv) minv = v;
		if (v > maxv) maxv = v;
	}

	mo->sMin[0] = pw[0] + minu * pu[0] + minv * pv[0];
	mo->sMin[1] = pw[1] + minu * pu[1] + minv * pv[1];
	mo->sMin[2] = pw[2] + minu * pu[2] + minv * pv[2];
	mo->sMinMax[0] = pw[0] + minu * pu[0] + maxv * pv[0];
	mo->sMinMax[1] = pw[1] + minu * pu[1] + maxv * pv[1];
	mo->sMinMax[2] = pw[2] + minu * pu[2] + maxv * pv[2];
	mo->sMaxMin[0] = pw[0] + maxu * pu[0] + minv * pv[0];
	mo->sMaxMin[1] = pw[1] + maxu * pu[1] + minv * pv[1];
	mo->sMaxMin[2] = pw[2] + maxu * pu[2] + minv * pv[2];
	mo->sMax[0] = pw[0] + maxu * pu[0] + maxv * pv[0];
	mo->sMax[1] = pw[1] + maxu * pu[1] + maxv * pv[1];
	mo->sMax[2] = pw[2] + maxu * pu[2] + maxv * pv[2];

}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];

	char* infn = NULL;
	char* outfn = NULL;
	char* fitsout = NULL;
	char* xylsfn = NULL;
	char* starfn = NULL;
	char* xylstemplate = NULL;
	char* startemplate = NULL;
	char* xname = NULL;
	char* yname = NULL;
	matchfile* matchin;
	matchfile* matchout;
	startree* skdt;
	xylist* xyls;
	double* fielduv = NULL;
	double* fieldxyz = NULL;

	double overlap_rad = 0.0;
	double overlap_d2;

	bool matlab = FALSE;

	double maxu = 1024;
	double maxv = 1024;

	il* slideshow_steps = il_new(32);
	int lasthp, lastff;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'u':
			maxu = atof(optarg);
			break;
		case 'v':
			maxv = atof(optarg);
			break;
		case 'M':
			matlab = TRUE;
			break;
		case 'w':
			fitsout = optarg;
			break;
		case 'X':
			xname = optarg;
			break;
		case 'Y':
			yname = optarg;
			break;
		case 'f':
			xylsfn = optarg;
			break;
		case 'F':
			xylstemplate = optarg;
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
		case 'S':
			startemplate = optarg;
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

	if (!infn || !outfn || !(starfn || startemplate) || !(xylsfn || xylstemplate)) {
		fprintf(stderr, "Must specify input and output match files, star kdtree, and field filenames.\n");
		printHelp(progname);
		exit(-1);
	}

	if (overlap_rad == 0.0) {
		fprintf(stderr, "Must specify overlap radius and threshold.\n");
		printHelp(progname);
		exit(-1);
	}

	overlap_d2 = arcsec2distsq(overlap_rad);

	il_append(slideshow_steps, 0);
	il_append(slideshow_steps, 24);

	matchin = matchfile_open(infn);
	if (!matchin) {
		fprintf(stderr, "Failed to read input matchfile %s.\n", infn);
		exit(-1);
	}

	if (starfn) {
		skdt = startree_open(starfn);
		if (!skdt) {
			fprintf(stderr, "Failed to open star kdtree %s.\n", starfn);
			exit(-1);
		}
	} else
		skdt = NULL;

	if (xylsfn) {
		xyls = xylist_open(xylsfn);
		if (!xyls) {
			fprintf(stderr, "Failed to open xylist %s.\n", xylsfn);
			exit(-1);
		}
		if (xname)
			xyls->xname = xname;
		if (yname)
			xyls->yname = yname;
	} else
		xyls = NULL;

	matchout = matchfile_open_for_writing(outfn);
	if (!matchout) {
		fprintf(stderr, "Failed to open output matchfile %s.\n", outfn);
		exit(-1);
	}
	if (matchfile_write_header(matchout)) {
		fprintf(stderr, "Failed to write output matchfile header.\n");
		exit(-1);
	}

	lasthp = lastff = -1;

	for (;;) {
		double centerxyz[3];
		double xyz[3];
		double t1[3];
		double t2[3];
		double t3[3];
		double pu[3];
		double pv[3];
		double u1, u2, v1, v2;
		double theta1, theta2;
		double diff;
		double ra, dec;
		double R, D, T, S;
		bool P;
		double len1;
		double len2;
		double r2;
		int NF;
		kdtree_qres_t* res;
		MatchObj* mo;
		int s;
		int i;

		double maxd2;
		int NI;
		double* indxyz;

		double MS[9];
		double MT[9];
		double MD[9];
		double MR[9];
		double MP[9];
		double Mtmp[9];
		double Mtmp2[9];
		double Mall[9];
		double tmp[3];
		double pw[3];

		const int STEPS = 25;
		double overlaps[STEPS+1];
		double totalweights[STEPS];

		char fn[256];

		mo = matchfile_buffered_read_match(matchin);
		if (!mo)
			break;
		if (!mo->transform_valid) {
			printf("MatchObject transform isn't valid.\n");
			continue;
		}

		if (startemplate && (mo->healpix != lasthp)) {
			if (skdt)
				startree_close(skdt);
			sprintf(fn, startemplate, mo->healpix);
			skdt = startree_open(fn);
			if (!skdt) {
				fprintf(stderr, "Failed to open star kdtree from file %s.\n", fn);
				exit(-1);
			}
			lasthp = mo->healpix;
		}

		if (xylstemplate && (mo->fieldfile != lastff)) {
			if (xyls)
				xylist_close(xyls);
			sprintf(fn, xylstemplate, mo->fieldfile);
			xyls = xylist_open(fn);
			if (!xyls) {
				fprintf(stderr, "Failed to open xylist file %s.\n", fn);
				exit(-1);
			}
			if (xname)
				xyls->xname = xname;
			if (yname)
				xyls->yname = yname;
			lastff = mo->fieldfile;
		}

		// find the field center
		star_midpoint(centerxyz, mo->sMin, mo->sMax);
		// find the field radius
		r2 = distsq(centerxyz, mo->sMin, 3);
		r2 *= 1.02;
		// find stars in that region
		res = kdtree_rangesearch_nosort(skdt->tree, centerxyz, r2);
		if (!res->nres) {
			printf("no stars found.\n");
			kdtree_free_query(res);
			continue;
		}

		// convert the transformation matrix into RA, DEC, theta, scale.
		t3[0] = xyz[0] = mo->transform[2];
		t3[1] = xyz[1] = mo->transform[5];
		t3[2] = xyz[2] = mo->transform[8];
		normalize_3(xyz);
		xyz2radec(xyz[0], xyz[1], xyz[2], &ra, &dec);

		// scale is the mean of the lengths of t1 and t2.
		t1[0] = mo->transform[0];
		t1[1] = mo->transform[3];
		t1[2] = mo->transform[6];
		t2[0] = mo->transform[1];
		t2[1] = mo->transform[4];
		t2[2] = mo->transform[7];
		len1 = vector_length_3(t1);
		len2 = vector_length_3(t2);
		S = (len1 + len2) * 0.5;
		scale_matrix(MS, S, S, 1.0);

		// we get theta by projecting pu=[1 0 0] and pv=[0 1 0] through R*D
		// and computing the projections of t1, t2 on these.
		R = M_PI_2 + ra;
		D = M_PI_2 - dec;
		rotation_matrix_z(MR, R);
		rotation_matrix_x(MD, D);

		pu[0] = 1.0;
		pu[1] = 0.0;
		pu[2] = 0.0;
		matrix_vector_3(MD, pu, tmp);
		matrix_vector_3(MR, tmp, pu);
		
		pv[0] = 0.0;
		pv[1] = 1.0;
		pv[2] = 0.0;
		matrix_vector_3(MD, pv, tmp);
		matrix_vector_3(MR, tmp, pv);

		pw[0] = 0.0;
		pw[1] = 0.0;
		pw[2] = 1.0;
		matrix_vector_3(MD, pw, tmp);
		matrix_vector_3(MR, tmp, pw);

		printf("\n");
		printf("t3=[ % 8.3g, % 8.3g, % 8.3g ];\n", t3[0], t3[1], t3[2]);
		printf("pw=[ % 8.3g, % 8.3g, % 8.3g ];\n", pw[0], pw[1], pw[2]);
		printf("\n");

		// note, pu and pv have length 1.
		u1 = dot_product_3(t1, pu);
		v1 = dot_product_3(t1, pv);
		u2 = dot_product_3(t2, pu);
		v2 = dot_product_3(t2, pv);
		theta1 = atan2(v1, u1);
		theta2 = atan2(v2, u2);

		printf("theta1=%g, theta2=%g.\n", rad2deg(theta1), rad2deg(theta2));

		diff = fabs(theta1 - theta2);
		if (diff > M_PI)
			diff = 2.0*M_PI - diff;
		// if they're not within 10 degrees of being 90 degrees apart,
		// warn.
		if (fabs(rad2deg(diff) - 90) > 10) {
			printf("warning, theta1 and theta2 should be nearly orthogonal.\n");
		}

		// parity: is theta2 90 degrees clockwise or anti-clockwise of theta1?
		diff = theta2 - theta1;
		if (diff < -M_PI)
			diff += 2.0 * M_PI;
		if (diff > M_PI)
			diff -= 2.0 * M_PI;
		printf("theta2 is %g after theta1.\n", rad2deg(diff));
		P = (diff < 0.0);

		if (P)
			T = theta2;
		else
			T = theta1;
		parity_matrix_xy(MP, P);

		printf("RA,DEC=(%g, %g) degrees, scale=%g arcsec/pixel,",
			   rad2deg(ra), rad2deg(dec), rad2arcsec(S));
		printf("theta=%g, parity=%i.\n",
			   rad2deg(T), (int)P);
		printf("Initial overlap: %g\n", mo->overlap);

		rotation_matrix_z(MT, T);

		NF = xylist_n_entries(xyls, mo->fieldnum);
		if (NF <= 0) {
			printf("Failed to get field %i.\n", mo->fieldnum);
			continue;
		}
		fielduv = realloc(fielduv, NF * 2 * sizeof(double));
		xylist_read_entries(xyls, mo->fieldnum, 0, NF, fielduv);

		printf("NF=%i\n", NF);

		fieldxyz = realloc(fieldxyz, NF * 3 * sizeof(double));

		NI = res->nres;
		indxyz = res->results.d;
		printf("NI=%i\n", NI);

		// 5 sigmas
		maxd2 = 25.0 * overlap_d2;

		matrix_matrix_3(MS, MP, Mtmp);
		matrix_matrix_3(MT, Mtmp, Mtmp2);
		matrix_matrix_3(MD, Mtmp2, Mtmp);
		matrix_matrix_3(MR, Mtmp, Mall);

		overlaps[0] = mo->overlap;

		// "xyz" is the field origin.

		for (s=0; s<STEPS; s++) {
			double u,v;
			int j;
			double totalweight;
			double totalscale;
			double totaltheta;
			double totalxyz[3];
			double step;
			double lenxyz;

			// project each field object...
			for (i=0; i<NF; i++) {
				u = fielduv[i*2 + 0];
				v = fielduv[i*2 + 1];
				tmp[0] = u;
				tmp[1] = v;
				tmp[2] = 1.0;
				matrix_vector_3(Mall, tmp, fieldxyz + i*3);
				normalize_3(fieldxyz + i*3);
			}

			totalweight = 0.0;
			totalscale  = 0.0;
			totaltheta  = 0.0;
			totalxyz[0] = totalxyz[1] = totalxyz[2] = 0.0;

			for (i=0; i<NF; i++) {
				double* fxyz;
				double rf;
				double frel[3];

				fxyz = fieldxyz + i*3;

				frel[0] = fxyz[0] - xyz[0];
				frel[1] = fxyz[1] - xyz[1];
				frel[2] = fxyz[2] - xyz[2];
				rf = vector_length_3(frel);

				for (j=0; j<NI; j++) {
					double* ixyz;
					double d2;
					double ri;
					double weight;
					double dtheta;
					double cross[3];
					double irel[3];

					ixyz = indxyz + j*3;
					d2 = distsq(fxyz, ixyz, 3);
					if (d2 > maxd2)
						continue;

					irel[0] = ixyz[0] - xyz[0];
					irel[1] = ixyz[1] - xyz[1];
					irel[2] = ixyz[2] - xyz[2];
					ri = vector_length_3(irel);

					weight = exp(-0.5 * (d2 / overlap_d2));
					totalweight += weight;

					totalscale += log(ri / rf) * weight;

					cross_product(frel, irel, cross);
					dtheta = asin(dot_product_3(cross, xyz) / (rf * ri));
					totaltheta += weight * dtheta;

					totalxyz[0] += weight * (ixyz[0] - fxyz[0]);
					totalxyz[1] += weight * (ixyz[1] - fxyz[1]);
					totalxyz[2] += weight * (ixyz[2] - fxyz[2]);

					/*
					  printf("f=%i, i=%i: dist: %g (%g sigmas), weight %g\n", i, j, sqrt(d2),
					  sqrt(d2 / overlap_d2), weight);
					*/
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
			printf("theta: %g degrees\n", rad2deg(totaltheta));
			lenxyz = distsq2arcsec(vector_length_squared_3(totalxyz));
			printf("xyz: (%g,%g,%g)   (%g arcsec)\n", totalxyz[0], totalxyz[1], totalxyz[2], lenxyz);
			printf("\n");

			step = 0.5;

			if (matlab && il_contains(slideshow_steps, s)) {
				double ox, oy;
				double x, y;
				double pu[3], pv[3], puv[3];
				double pux, puy, pvx, pvy, puvx, puvy;
				double pixelsx = maxu;
				double pixelsy = maxv;

				if (s == il_get(slideshow_steps, 0)) {
					fprintf(stderr, "fprintf('File %i, Field %i.\\n');\n",
							mo->fieldfile, mo->fieldnum);
					fprintf(stderr, "index=[");
					for (j=0; j<NI; j++) {
						star_coords(indxyz + j*3, centerxyz, &x, &y);
						fprintf(stderr, "%g,%g;", x, y);
					}
					fprintf(stderr, "];\n");
				}

				star_coords(xyz, centerxyz, &ox, &oy);
				fprintf(stderr, "origin%i=[%g,%g];\n", s, ox, oy);
				fprintf(stderr, "field%i=[", s);
				for (i=0; i<NF; i++) {
					star_coords(fieldxyz + i*3, centerxyz, &x, &y);
					fprintf(stderr, "%g,%g;", x, y);
				}
				fprintf(stderr, "];\n");
				pu[0] = pixelsx;
				pu[1] = 0.0;
				pu[2] = 1.0;
				pv[0] = 0.0;
				pv[1] = pixelsy;
				pv[2] = 1.0;
				puv[0] = pixelsx;
				puv[1] = pixelsy;
				puv[2] = 1.0;
				matrix_vector_3(Mall, pu, tmp);
				star_coords(tmp,  centerxyz, &pux,  &puy);
				matrix_vector_3(Mall, pv, tmp);
				star_coords(tmp,  centerxyz, &pvx,  &pvy);
				matrix_vector_3(Mall, puv, tmp);
				star_coords(tmp,  centerxyz, &puvx,  &puvy);
				fprintf(stderr, "uvx%i=[%g,%g,%g,%g,%g];\n", s, ox, pux, puvx, pvx, ox);
				fprintf(stderr, "uvy%i=[%g,%g,%g,%g,%g];\n", s, oy, puy, puvy, pvy, oy);
			}

			printf("Old RA,DEC=(%g, %g), scale=%g arcsec/pixel, theta=%g degrees.\n",
				   rad2deg(ra), rad2deg(dec), rad2arcsec(S), rad2deg(T));

			xyz[0] += step * totalxyz[0];
			xyz[1] += step * totalxyz[1];
			xyz[2] += step * totalxyz[2];
			normalize_3(xyz);
			xyz2radec(xyz[0], xyz[1], xyz[2], &ra, &dec);

			R = M_PI_2 + ra;
			D = M_PI_2 - dec;
			T += step * totaltheta;
			S *= pow(totalscale, step);

			rotation_matrix_z(MR, R);
			rotation_matrix_x(MD, D);
			rotation_matrix_z(MT, T);
			scale_matrix(MS, S, S, 1.0);
			matrix_matrix_3(MS, MP, Mtmp);
			matrix_matrix_3(MT, Mtmp, Mtmp2);
			matrix_matrix_3(MD, Mtmp2, Mtmp);
			matrix_matrix_3(MR, Mtmp, Mall);

			printf("New RA,DEC=(%g, %g), scale=%g arcsec/pixel, theta=%g degrees.\n\n",
				   rad2deg(ra), rad2deg(dec), rad2arcsec(S), rad2deg(T));

			update_matchobj(mo, Mall, fielduv, NF);
			{
				int match;
				int unmatch;
				int conflict;
				verify_hit(skdt->tree, mo, fielduv, NF, overlap_d2,
						   &match, &unmatch, &conflict, NULL, NULL, NULL);
			}
			printf("Overlap: %g\n", mo->overlap);

			totalweights[s] = totalweight;
			overlaps[s+1] = mo->overlap;

		}

		if (matlab) {
			// slide show.
			fprintf(stderr, "totalweights=[");
			for (s=0; s<STEPS; s++)
				fprintf(stderr, "%g,", totalweights[s]);
			fprintf(stderr, "];\n");
			fprintf(stderr, "overlaps=[");
			for (s=0; s<=STEPS; s++)
				fprintf(stderr, "%g,", overlaps[s]);
			fprintf(stderr, "];\n");
			fprintf(stderr, "clf;\n");
			fprintf(stderr, "subplot(6, 1, 1);\n");
			for (i=0; i<il_size(slideshow_steps); i++) {
				s = il_get(slideshow_steps, i);
				fprintf(stderr, "subplot(6, 1, 1);\n");
				fprintf(stderr, "plot([1:%i],totalweights,'r-',%i,totalweights(%i),'ro'); "
						"a=axis; a(1)=0; a(2)=%i; axis(a); ylabel('Total probability');\n",
						STEPS, s+1, s+1, STEPS+1);
				fprintf(stderr, "subplot(6, 1, 2);");
				fprintf(stderr, "plot([0:%i],overlaps,'b-',%i,overlaps(%i),'ro'); "
						"a=axis; a(1)=0; a(2)=%i; axis(a); ylabel('Overlap'); xlabel('Step');\n",
						STEPS, s+1, s+2, STEPS+1);
				fprintf(stderr, "subplot(6, 1, [3:6]);");
				fprintf(stderr, "p=plot(origin%i(1),origin%i(2),'r*', "
						"field%i(:,1), field%i(:,2), 'b.',"
						"index(:,1), index(:,2), 'ro',"
						"uvx%i, uvy%i, 'm-');\n", s, s, s, s, s, s);
				fprintf(stderr, "set(p(3), 'MarkerSize', 5);\n");
				fprintf(stderr, "axis equal;\n");
				fprintf(stderr, "input('\\nround %i');\n", s);
			}
		}

		kdtree_free_query(res);

		tmp[0] = 1.0;
		tmp[1] = 0.0;
		tmp[2] = 0.0;
		matrix_vector_3(Mall, tmp, pu);

		tmp[0] = 0.0;
		tmp[1] = 1.0;
		tmp[2] = 0.0;
		matrix_vector_3(Mall, tmp, pv);

		tmp[0] = 0.0;
		tmp[1] = 0.0;
		tmp[2] = 1.0;
		matrix_vector_3(Mall, tmp, pw);

		printf("\n");
		printf("t1=[ % 8.3g, % 8.3g, % 8.3g ];\n", t1[0], t1[1], t1[2]);
		printf("pu=[ % 8.3g, % 8.3g, % 8.3g ];\n", pu[0], pu[1], pu[2]);
		printf("\n");
		printf("t2=[ % 8.3g, % 8.3g, % 8.3g ];\n", t2[0], t2[1], t2[2]);
		printf("pv=[ % 8.3g, % 8.3g, % 8.3g ];\n", pv[0], pv[1], pv[2]);
		printf("\n");
		printf("t3=[ % 8.3g, % 8.3g, % 8.3g ];\n", t3[0], t3[1], t3[2]);
		printf("pw=[ % 8.3g, % 8.3g, % 8.3g ];\n", pw[0], pw[1], pw[2]);
		printf("\n");

		matchfile_write_match(matchout, mo);

		if (fitsout) {
			FILE* fid = fopen(fitsout, "wb");
			qfits_header* hdr;
			char val[64];
			if (!fid) {
				fprintf(stderr, "Failed to open file %s to write FITS header: %s\n",
						fitsout, strerror(errno));
				exit(-1);
			}
			hdr = qfits_header_default();

			qfits_header_add(hdr, "BITPIX", "8", NULL, NULL);
			qfits_header_add(hdr, "NAXIS", "0", "No image", NULL);
			qfits_header_add(hdr, "EXTEND", "T", "May be FITS extensions", NULL);

			qfits_header_add(hdr, "CRPIX1 ", "0", "X reference pixel", NULL);
			qfits_header_add(hdr, "CRPIX2 ", "0", "Y reference pixel", NULL);
			sprintf(val, "%.12g", rad2deg(S));
			qfits_header_add(hdr, "CDELT1 ", val, "X pixel scale (deg/pix)", NULL);
			qfits_header_add(hdr, "CDELT2 ", val, "Y pixel scale (deg/pix)", NULL);
			qfits_header_add(hdr, "CUNIT1 ", "deg", "X pixel scale units", NULL);
			qfits_header_add(hdr, "CUNIT2 ", "deg", "Y pixel scale units", NULL);
			sprintf(val, "%.12g", cos(T));
			qfits_header_add(hdr, "PC1_1 ", val, "XY rotation matrix (cos T)", NULL);
			qfits_header_add(hdr, "PC2_2 ", val, "XY rotation matrix (cos T)", NULL);
			sprintf(val, "%.12g", -sin(T));
			qfits_header_add(hdr, "PC1_2 ", val, "XY rotation matrix (-sin T)", NULL);
			sprintf(val, "%.12g", sin(T));
			qfits_header_add(hdr, "PC2_1 ", val, "XY rotation matrix (sin T)", NULL);

			qfits_header_add(hdr, "CTYPE1 ", "RA---TAN", "TAN (gnomic) projection", NULL);
			qfits_header_add(hdr, "CTYPE2 ", "DEC--TAN", "TAN (gnomic) projection", NULL);
			sprintf(val, "%.12g", rad2deg(ra));
			qfits_header_add(hdr, "CRVAL1 ", val, "RA  of reference point", NULL);
			sprintf(val, "%.12g", rad2deg(dec));
			qfits_header_add(hdr, "CRVAL2 ", val, "DEC of reference point", NULL);

			if (qfits_header_dump(hdr, fid)) {
				fprintf(stderr, "Failed to write FITS header to file %s.\n", fitsout);
				exit(-1);
			}

			qfits_header_destroy(hdr);
			if (fclose(fid)) {
				fprintf(stderr, "Failed to close file %s to write FITS header: %s\n",
						fitsout, strerror(errno));
				exit(-1);
			}
		}

	}

	free(fielduv);
	free(fieldxyz);

	if (xyls)
		xylist_close(xyls);
	matchfile_close(matchin);
	if (skdt)
		startree_close(skdt);

	if (matchfile_fix_header(matchout) ||
		matchfile_close(matchout)) {
		fprintf(stderr, "Failed to close output matchfile %s.\n", outfn);
		exit(-1);
	}

	return 0;
}

