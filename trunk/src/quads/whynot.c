#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>

#include "kdtree.h"
#include "kdtree_fits_io.h"
#include "kdtree_io.h"
#include "kdtree_access.h"
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "matchobj.h"
#include "catalog.h"
#include "hitlist_healpix.h"
#include "tic.h"
#include "quadfile.h"
#include "idfile.h"
#include "intmap.h"
#include "xylist.h"
#include "rdlist.h"
#include "qidxfile.h"
#include "verify.h"
#include "qfits.h"
#include "ioutils.h"

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s\n", progname);
}

void solve_fields();

int read_parameters();

#define DEFAULT_CODE_TOL .002
#define DEFAULT_PARITY_FLIP FALSE

// params:
char *fieldfname = NULL, *treefname = NULL;
char *quadfname = NULL, *catfname = NULL;
char* qidxfname = NULL;
char *rdlsfname = NULL;
char* startreefname = NULL;
char *idfname = NULL;
char* xcolname;
char* ycolname;
char* racolname = "RA";
char* deccolname = "DEC";
bool parity = DEFAULT_PARITY_FLIP;
double codetol = DEFAULT_CODE_TOL;

int startdepth = 0;
int enddepth = 0;

double funits_lower = 0.0;
double funits_upper = 0.0;
double index_scale;
double index_scale_lower;
double index_scale_lower_factor = 0.0;

bool agreement = FALSE;
int nagree = 4;
int maxnagree = 0;
double agreetol = 0.0;

bool do_verify;
int nagree_toverify;
double verify_dist2;
float overlap_tosolve;
int min_ninfield;

double nearbyrad = 3.0;

int threads = 1;
il* fieldlist = NULL;

int do_correspond = 1;

bool circle = FALSE;

catalog* cat;
idfile* id;
quadfile* quads;
kdtree_t *codetree;
xylist* xyls;
rdlist* rdls;
kdtree_t* startree;
qidxfile* qidx;

int* inverse_perm = NULL;

int* code_inverse_perm = NULL;

double arcsec_per_pixel_lower, arcsec_per_pixel_upper;
double minAB, maxAB;

intmap* fieldtoind;
intmap* indtofield;

struct quadmatch {
	uint quadnum;
	uint indexstar[4];
	uint fieldstar[4];
	uint fieldstar_sorted[4];
};
typedef struct quadmatch quadmatch;

quadmatch* qms;
int Nqms;

int compare_uints(const void* v1, const void* v2) {
	const uint u1 = *(uint*)v1;
	const uint u2 = *(uint*)v2;
	if (u1 < u2)
		return -1;
	if (u1 > u2)
		return 1;
	return 0;
}

int compare_last_fieldobj(const void* v1, const void* v2) {
	const quadmatch* q1 = v1;
	const quadmatch* q2 = v2;
	if (q1->fieldstar_sorted[3] > q2->fieldstar_sorted[3])
		return 1;
	if (q1->fieldstar_sorted[3] < q2->fieldstar_sorted[3])
		return -1;
	return 0;
}

// find min and max coordinates in this field;
// place them in "cornerpix"
void find_corners(xy *thisfield, xy *cornerpix) {
	double minx, maxx, miny, maxy;
	double x, y;
	uint i;

	minx = miny = 1e308;
	maxx = maxy = -1e308;
	
	for (i=0; i<xy_size(thisfield); i++) {
		x = xy_refx(thisfield, i);
		y = xy_refy(thisfield, i);
		if (x < minx) minx = x;
		if (x > maxx) maxx = x;
		if (y < miny) miny = y;
		if (y > maxy) maxy = y;
	}

    xy_setx(cornerpix, 0, minx);
    xy_sety(cornerpix, 0, miny);
    xy_setx(cornerpix, 1, maxx);
    xy_sety(cornerpix, 1, maxy);

    xy_setx(cornerpix, 2, minx);
    xy_sety(cornerpix, 2, maxy);
    xy_setx(cornerpix, 3, maxx);
    xy_sety(cornerpix, 3, miny);
}

void getstarcoord(uint iA, double *sA) {
	memcpy(sA, startree->data + inverse_perm[iA] * DIM_STARS,
		   DIM_STARS * sizeof(double));
}

int quadnum;

void findable_quad(quadmatch* qm, xy* thisfield, xy* cornerpix,
				   hitlist* hits, bool verbose) {
	double Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
	double dx, dy;
	double costheta, sintheta, scale, xxtmp;
	xy *ABCDpix;
	double* qcode;
	double dist;
	double code[4];
	double d, c;
	int nagree, listind;

	MatchObj mo;
	MatchObj* mocopy;

	int i, matches, unmatches, conflicts;

	double star[12];
	double field[8];

	int iA = qm->fieldstar[0];
	int iB = qm->fieldstar[1];
	int iC = qm->fieldstar[2];
	int iD = qm->fieldstar[3];

	bool badquad = FALSE;

	fprintf(stderr, "  %i: (%i, %i, %i, %i)\n",
			qm->quadnum,
			qm->fieldstar_sorted[0],
			qm->fieldstar_sorted[1],
			qm->fieldstar_sorted[2],
			qm->fieldstar_sorted[3]);

	Ax = xy_refx(thisfield, iA);
	Ay = xy_refy(thisfield, iA);
	Bx = xy_refx(thisfield, iB);
	By = xy_refy(thisfield, iB);
	dx = Bx - Ax;
	dy = By - Ay;
	scale = dx*dx + dy*dy;
	printf("quadscale(%i)=%g;\n", quadnum+1, sqrt(scale));
	if ((scale < square(minAB)) || (scale > square(maxAB))) {
		fprintf(stderr, "    Quad scale %g: not in allowable range [%g, %g].\n", scale, minAB, maxAB);
		return;
	}

	ABCDpix = mk_xy(DIM_QUADS);
	xy_setx(ABCDpix, 0, Ax);
	xy_sety(ABCDpix, 0, Ay);
	xy_setx(ABCDpix, 1, Bx);
	xy_sety(ABCDpix, 1, By);

	costheta = (dx + dy) / scale;
	sintheta = (dy - dx) / scale;

	Cx = xy_refx(thisfield, iC);
	Cy = xy_refy(thisfield, iC);
	xy_setx(ABCDpix, 2, Cx);
	xy_sety(ABCDpix, 2, Cy);
	Cx -= Ax;
	Cy -= Ay;
	xxtmp = Cx;
	Cx = Cx * costheta + Cy * sintheta;
	Cy = -xxtmp * sintheta + Cy * costheta;
	if (circle) {
		double r = (Cx*Cx - Cx) + (Cy*Cy - Cy);
		if (r > 0.0) {
			fprintf(stderr, "    Field star C is not in the circle.\n");
			return;
		}
	} else {
		if ((Cx > 1.0) || (Cx < 0.0) ||
			(Cy > 1.0) || (Cy < 0.0)) {
			fprintf(stderr, "    Field star C is not in the box.\n");
			return;
		}
	}

	Dx = xy_refx(thisfield, iD);
	Dy = xy_refy(thisfield, iD);
	xy_setx(ABCDpix, 3, Dx);
	xy_sety(ABCDpix, 3, Dy);
	Dx -= Ax;
	Dy -= Ay;
	xxtmp = Dx;
	Dx = Dx * costheta + Dy * sintheta;
	Dy = -xxtmp * sintheta + Dy * costheta;
	if (circle) {
		double r = (Dx*Dx - Dx) + (Dy*Dy - Dy);
		if (r > 0.0) {
			fprintf(stderr, "    Field star D is not in the circle.\n");
			return;
		}
	} else {
		if ((Dx > 1.0) || (Dx < 0.0) ||
			(Dy > 1.0) || (Dy < 0.0)) {
			fprintf(stderr, "    Field star D is not in the box.\n");
			return;
		}
	}

	code[0] = Cx;
	code[1] = Cy;
	code[2] = Dx;
	code[3] = Dy;

	qcode = codetree->data +
		code_inverse_perm[qm->quadnum] * codetree->ndim;
	dist = sqrt(distsq(qcode, code, codetree->ndim));
	fprintf(stderr, "    Dist to quad %i: %g%s\n",
			qm->quadnum, dist,
			(dist < codetol) ? "   ==> HIT!" : "");

	printf("codedist(%i)=%g;\n", quadnum+1, dist);
	printf("codepassed(%i)=%i;\n", quadnum+1, (dist<codetol)?1:0);

	if (dist > codetol)
		badquad = TRUE;

	mo.quadno = qm->quadnum;
	mo.star[0] = qm->indexstar[0];
	mo.star[1] = qm->indexstar[1];
	mo.star[2] = qm->indexstar[2];
	mo.star[3] = qm->indexstar[3];
	mo.field[0] = qm->fieldstar[0];
	mo.field[1] = qm->fieldstar[1];
	mo.field[2] = qm->fieldstar[2];
	mo.field[3] = qms->fieldstar[3];
	mo.code_err = square(dist);

	getstarcoord(mo.star[0], star+0*3);
	getstarcoord(mo.star[1], star+1*3);
	getstarcoord(mo.star[2], star+2*3);
	getstarcoord(mo.star[3], star+3*3);

	for (i=0; i<4; i++) {
		field[i*2 + 0] = xy_refx(ABCDpix, i);
		field[i*2 + 1] = xy_refy(ABCDpix, i);
	}

	fit_transform(star, field, 4, mo.transform);
	image_to_xyz(xy_refx(cornerpix, 0), xy_refy(cornerpix, 0), mo.sMin,    mo.transform);
	image_to_xyz(xy_refx(cornerpix, 1), xy_refy(cornerpix, 1), mo.sMax,    mo.transform);
	image_to_xyz(xy_refx(cornerpix, 2), xy_refy(cornerpix, 2), mo.sMinMax, mo.transform);
	image_to_xyz(xy_refx(cornerpix, 3), xy_refy(cornerpix, 3), mo.sMaxMin, mo.transform);
	mo.transform_valid = TRUE;

	// scale checking
	// fieldunitsupper/lower is in arcseconds/pixel
	d  = square(mo.sMax[0] - mo.sMin[0]);
	d += square(mo.sMax[1] - mo.sMin[1]);
	d += square(mo.sMax[2] - mo.sMin[2]);
	// convert 'd' from squared distance on the unit sphere
	// to arcseconds...
	d = distsq2arc(d);
	d = rad2deg(d) * 60.0 * 60.0;
	c  = square(xy_refy(cornerpix, 1) - xy_refy(cornerpix, 0));
	c += square(xy_refx(cornerpix, 1) - xy_refx(cornerpix, 0));
	c = sqrt(c);
	if ((d/c > arcsec_per_pixel_upper) || (d/c < arcsec_per_pixel_lower)) {
		// this quad has invalid scale.
		fprintf(stderr, "    This quad has invalid scale: %g not in [%g, %g].\n",
				d/c, arcsec_per_pixel_lower, arcsec_per_pixel_upper);
		return;
	}

	mocopy = malloc(sizeof(MatchObj));
	memcpy(mocopy, &mo, sizeof(MatchObj));
	{
		int i, nfield;
		double* fld;
		nfield = xy_size(thisfield);
		fld = malloc(nfield*2*sizeof(double));
		for (i=0; i<nfield; i++) {
			fld[i*2  ] = xy_refx(thisfield, i);
			fld[i*2+1] = xy_refy(thisfield, i);
		}
		verify_hit(startree, mocopy, fld, nfield, verify_dist2, &matches, &unmatches, &conflicts, NULL);
		free(fld);
	}
	if (verbose) {
		fprintf(stderr, "    Verify: %4.1f%%: %i matches, %i unmatches, %i conflicts.\n",
				100.0 * mocopy->overlap, matches, unmatches, conflicts);
	}

	hitlist_healpix_compute_vector(mocopy);
	if (verbose) {
		fprintf(stderr, "    Checking agreement to list...\n");
		hitlist_healpix_print_dists_to_lists(hits, mocopy);
	}
	{
		double closest = hitlist_healpix_closest_dist_to_lists(hits, mocopy);
		printf("agreedist(%i)=%g;\n", quadnum+1, closest>0.0 ? distsq2arcsec(closest) : 0.0);
	}

	if (badquad)
		nagree = 0;
	else
		nagree = hitlist_healpix_add_hit(hits, mocopy, &listind);
	fprintf(stderr, "    -> %i agree, overlap %4.1f%%.\n", nagree, 100.0 * mocopy->overlap);
	if (mocopy->ninfield < min_ninfield)
		fprintf(stderr, "    -> warning, ninfield %i is less than required %i\n",
				mocopy->ninfield, min_ninfield);

	printf("nagree(%i)=%i;\n", quadnum+1, nagree);
	printf("overlap(%i)=%g;\n", quadnum+1, mocopy->overlap);
	printf("ninfield(%i)=%i;\n", quadnum+1, mocopy->ninfield);

	if (badquad)
		free(mocopy);
}

void why_not() {
	double last_utime, last_stime;
	double utime, stime;
	int nfields;
	int fieldind;
	double transform[9];
	xy* cornerpix = mk_xy(4);

	get_resource_stats(&last_utime, &last_stime, NULL);

	if (funits_upper != 0.0) {
		arcsec_per_pixel_upper = funits_upper;
		minAB = index_scale_lower / funits_upper;
		fprintf(stderr, "Set minAB to %g\n", minAB);
	}
	if (funits_lower != 0.0) {
		arcsec_per_pixel_lower = funits_lower;
		maxAB = index_scale / funits_lower;
		fprintf(stderr, "Set maxAB to %g\n", maxAB);
	}

	nfields = xyls->nfields;

	for (fieldind=0; fieldind<il_size(fieldlist); fieldind++) {
		xy *thisfield = NULL;
		int fieldnum;
		rd* thisrd;

		double nearbyd2;
		int i;

		il* indstars;
		il* indquads;

		hitlist* hlhits;

		fieldnum = il_get(fieldlist, fieldind);
		if (fieldnum >= nfields) {
			fprintf(stderr, "Field %i does not exist (nfields=%i).\n", fieldnum, nfields);
			continue;
		}

		thisfield = xylist_get_field(xyls, fieldnum);
		thisrd = rdlist_get_field(rdls, fieldnum);
		if (!thisfield || !thisrd) {
			fprintf(stderr, "Couldn't get xylist or rdlist for field %i\n", fieldnum);
			continue;
		}

		assert(xy_size(thisfield) == rd_size(thisrd));

		find_corners(thisfield, cornerpix);

		{
			double starpos[3*rd_size(thisrd)];
			double fieldpos[2*xy_size(thisfield)];

			printf("field_xy=[");
			for (i=0; i<xy_size(thisfield); i++) {
				double x, y;
				x = xy_refx(thisfield, i);
				y = xy_refy(thisfield, i);
				printf("%g,%g;", x, y);
				fieldpos[i*2] = x;
				fieldpos[i*2+1] = y;
			}
			printf("];\n");
			printf("field_radec=[");
			for (i=0; i<xy_size(thisrd); i++) {
				double ra, dec;
				ra  = rd_refra(thisrd, i);
				dec = rd_refdec(thisrd, i);
				printf("%g,%g;", ra, dec);
				radec2xyzarr(deg2rad(ra), deg2rad(dec), starpos + i*3);
			}
			printf("];\n");

			fit_transform(starpos, fieldpos, xy_size(thisfield), transform);
			//inverse_3by3(transform);
		}

		intmap_clear(fieldtoind);
		intmap_clear(indtofield);

		indstars = il_new(16);
		indquads = il_new(16);

		// for each object in the field, find nearby stars in the index.
		nearbyd2 = arcsec2distsq(nearbyrad);
		printf("conflicts=[");
		for (i=0; i<rd_size(thisrd); i++) {
			double xyz[3];
			double ra, dec;
			kdtree_qres_t* res;
			ra = rd_refra(thisrd, i);
			dec = rd_refdec(thisrd, i);
			radec2xyzarr(deg2rad(ra), deg2rad(dec), xyz);
			res = kdtree_rangesearch(startree, xyz, nearbyd2);
			assert(res);
			assert(res->nres == 0 || res->nres == 1);
			if (!res->nres)
				continue;

			if (intmap_add(fieldtoind, i, res->inds[0])) {
				fprintf(stderr, "Conflicting mapping (f->i): %i -> %i\n", i, res->inds[0]);
			}
			if (intmap_add(indtofield, res->inds[0], i)) {
				fprintf(stderr, "Conflicting mapping (i->f): %i -> %i\n", res->inds[0], i);
				printf("%i,", i);
			}
			il_append(indstars, res->inds[0]);

			kdtree_free_query(res);
		}
		printf("];\n");

		fprintf(stderr, "Of %i field stars, %i have nearby index stars.  %i index stars have nearby field objects.\n",
				rd_size(thisrd), intmap_length(fieldtoind), intmap_length(indtofield));

		fprintf(stderr, "Field stars that are indexed:\n");
		for (i=0; i<intmap_length(fieldtoind); i++) {
			int fld;
			intmap_get_entry(fieldtoind, i, &fld, NULL);
			fprintf(stderr, "%i ", fld);
		}
		fprintf(stderr, "\n");

		printf("field_indexed = [");
		for (i=0; i<intmap_length(fieldtoind); i++) {
			int fld;
			intmap_get_entry(fieldtoind, i, &fld, NULL);
			printf("%i, ", fld);
		}
		printf("];\n");

		printf("indexed_radec = [");
		for (i=0; i<intmap_length(indtofield); i++) {
			int ind;
			double xyz[3], ra, dec;
			intmap_get_entry(indtofield, i, &ind, NULL);
			getstarcoord(ind, xyz);
			ra = xy2ra(xyz[0], xyz[1]);
			dec = z2dec(xyz[2]);
			printf("%g,%g;", rad2deg(ra), rad2deg(dec));
 		}
		printf("];\n");

		{
			MatchObj mo;
			il* infield = il_new(256);
			double* fld;
			int i, nfield, ind;
			double xyz[3], ra, dec;
			nfield = xy_size(thisfield);
			fld = malloc(nfield*2*sizeof(double));
			for (i=0; i<nfield; i++) {
				fld[i*2  ] = xy_refx(thisfield, i);
				fld[i*2+1] = xy_refy(thisfield, i);
			}
			memset(&mo, 0, sizeof(mo));
			memcpy(mo.transform, transform, 9*sizeof(double));
			mo.transform_valid = TRUE;
			image_to_xyz(xy_refx(cornerpix, 0), xy_refy(cornerpix, 0), mo.sMin,    transform);
			image_to_xyz(xy_refx(cornerpix, 1), xy_refy(cornerpix, 1), mo.sMax,    transform);
			image_to_xyz(xy_refx(cornerpix, 2), xy_refy(cornerpix, 2), mo.sMinMax, transform);
			image_to_xyz(xy_refx(cornerpix, 3), xy_refy(cornerpix, 3), mo.sMaxMin, transform);

			verify_hit(startree, &mo, fld, nfield, verify_dist2,
					   NULL, NULL, NULL, infield);

			// make "infield" just be the ones that aren't in "indexed_radec".
			printf("%% infield=");
			il_print(infield);
			printf("\n");
			for (i=0; i<intmap_length(indtofield); i++) {
				int ind;
				intmap_get_entry(indtofield, i, &ind, NULL);
				if (il_remove_value(infield, ind) == -1) {
					printf("%% failed to remove index %i from infield\n", ind);
				}
			}
			printf("%% infield=");
			il_print(infield);
			printf("\n");
			printf("infield_radec = [");
			for (i=0; i<il_size(infield); i++) {
				ind = il_get(infield, i);
				getstarcoord(ind, xyz);
				ra = xy2ra(xyz[0], xyz[1]);
				dec = z2dec(xyz[2]);
				printf("%g,%g;", rad2deg(ra), rad2deg(dec));
			}
			printf("];\n");

			il_free(infield);
		}
		
		for (i=0; i<il_size(indstars); i++) {
			uint* quadlist, nquads;
			uint star = il_get(indstars, i);
			int j;
			if (qidxfile_get_quads(qidx, star, &quadlist, &nquads)) {
				fprintf(stderr, "Failed to read qidxfile for star %i.\n", star);
				exit(-1);
			}
			
			fprintf(stderr, "Star %u [field obj %i] is in %i quads:\n", star, intmap_get(indtofield, star, -1), nquads);
			for (j=0; j<nquads; j++) {
				uint sA, sB, sC, sD;
				quadfile_get_starids(quads, quadlist[j], &sA, &sB, &sC, &sD);
				fprintf(stderr, "  stars (%u,%u,%u,%u), field objs [%i,%i,%i,%i]\n",
						sA, sB, sC, sD, 
						intmap_get(indtofield,sA,-1), intmap_get(indtofield,sB,-1),
						intmap_get(indtofield,sC,-1), intmap_get(indtofield,sD,-1));
			}

			/*
			  fprintf(stderr, "Index star %i (field %i) is in quads:\n  ",
			  star, intmap_get(indtofield, star, -1));
			  for (j=0; j<nquads; j++) {
			  fprintf(stderr, "%i ", quads[j]);
			  }
			  fprintf(stderr, "\n");
			*/
			for (j=0; j<nquads; j++) {
				il_insert_ascending(indquads, quadlist[j]);
			}
		}

		/*
		  fprintf(stderr, "Partially indexed quads:\n  ");
		  for (i=0; i<il_size(indquads); i++) {
		  fprintf(stderr, "%i ", il_get(indquads, i));
		  }
		  fprintf(stderr, "\n");
		*/

		for (i=0; i<il_size(indquads)-3; i++) {
			if (il_get(indquads, i) != il_get(indquads, i+3)) {
				// not a string of four equal quads.
				il_remove(indquads, i);
				i--;
				continue;
			}
		}

		/*
		  fprintf(stderr, "Index-to-field map:\n");
		  for (i=0; i<intmap_length(indtofield); i++) {
		  int from, to;
		  intmap_get_entry(indtofield, i, &from, &to);
		  fprintf(stderr, "  %i -> %i\n", from, to);
		  }
		*/

		for (i=0; i<il_size(indquads); i++) {
			int quad = il_get(indquads, i);
			uint sA, sB, sC, sD;
			if (i) {
				// check for duplicates.
				if (quad == il_get(indquads, i-1)) {
					il_remove(indquads, i);
					i--;
					continue;
				}
			}
			quadfile_get_starids(quads, quad, &sA, &sB, &sC, &sD);
			/*
			  if ((intmap_get(indtofield, sA, -1) == -1) ||
			  (intmap_get(indtofield, sB, -1) == -1) ||
			  (intmap_get(indtofield, sC, -1) == -1) ||
			  (intmap_get(indtofield, sD, -1) == -1)) {
			*/
			if ((intmap_get_inverse(fieldtoind, sA, -1) == -1) ||
				(intmap_get_inverse(fieldtoind, sB, -1) == -1) ||
				(intmap_get_inverse(fieldtoind, sC, -1) == -1) ||
				(intmap_get_inverse(fieldtoind, sD, -1) == -1)) {
				il_remove(indquads, i);
				i--;
			}
		}

		fprintf(stderr, "Field %i.\n", fieldnum);
		fprintf(stderr, "Indexed quads:\n");

		Nqms = il_size(indquads);
		qms = malloc(Nqms * sizeof(quadmatch));

		/*
		  printf("quads=[");
		  for (i=0; i<il_size(indquads); i++) {
		  int quad = il_get(indquads, i);
		  uint sA, sB, sC, sD;
		  uint fA, fB, fC, fD;
		  quadfile_get_starids(quads, quad, &sA, &sB, &sC, &sD);
		  fA = intmap_get_inverse(fieldtoind, sA, -1);
		  fB = intmap_get_inverse(fieldtoind, sB, -1);
		  fC = intmap_get_inverse(fieldtoind, sC, -1);
		  fD = intmap_get_inverse(fieldtoind, sD, -1);
		  printf("%i,%i,%i,%i;", fA, fB, fC, fD);
		  }
		  printf("];\n");
		*/
		
		for (i=0; i<il_size(indquads); i++) {
			int quad = il_get(indquads, i);
			uint sA, sB, sC, sD;

			quadfile_get_starids(quads, quad, &sA, &sB, &sC, &sD);

			qms[i].quadnum = quad;
			qms[i].indexstar[0] = sA;
			qms[i].indexstar[1] = sB;
			qms[i].indexstar[2] = sC;
			qms[i].indexstar[3] = sD;

			// ??
			/*
			  qms[i].fieldstar[0] = intmap_get(indtofield, sA, -1);
			  qms[i].fieldstar[1] = intmap_get(indtofield, sB, -1);
			  qms[i].fieldstar[2] = intmap_get(indtofield, sC, -1);
			  qms[i].fieldstar[3] = intmap_get(indtofield, sD, -1);
			*/
			qms[i].fieldstar[0] = intmap_get_inverse(fieldtoind, sA, -1);
			qms[i].fieldstar[1] = intmap_get_inverse(fieldtoind, sB, -1);
			qms[i].fieldstar[2] = intmap_get_inverse(fieldtoind, sC, -1);
			qms[i].fieldstar[3] = intmap_get_inverse(fieldtoind, sD, -1);

			qms[i].fieldstar_sorted[0] = qms[i].fieldstar[0];
			qms[i].fieldstar_sorted[1] = qms[i].fieldstar[1];
			qms[i].fieldstar_sorted[2] = qms[i].fieldstar[2];
			qms[i].fieldstar_sorted[3] = qms[i].fieldstar[3];
			qsort(qms[i].fieldstar_sorted, 4, sizeof(uint), compare_uints);

			fprintf(stderr, "   %i: index (%i, %i, %i, %i), field (%i, %i, %i, %i), sorted (%i, %i, %i, %i)\n",
					quad, sA, sB, sC, sD,
					qms[i].fieldstar[0],
					qms[i].fieldstar[1],
					qms[i].fieldstar[2],
					qms[i].fieldstar[3],
					qms[i].fieldstar_sorted[0],
					qms[i].fieldstar_sorted[1],
					qms[i].fieldstar_sorted[2],
					qms[i].fieldstar_sorted[3]);
		}

		qsort(qms, Nqms, sizeof(quadmatch), compare_last_fieldobj);

		fprintf(stderr, "\n");

		fprintf(stderr, "Findable quads:\n");
		
		hlhits = hitlist_healpix_new(agreetol /* *sqrt(2.0)*/);
		//hlall = hitlist_healpix_new(agreetol /* *sqrt(2.0)*/);
		hlhits->do_correspond = do_correspond;

		printf("quads=[");
		for (i=0; i<Nqms; i++) {
			uint* fs = qms[i].fieldstar;
			printf("%i,%i,%i,%i;", fs[0], fs[1], fs[2], fs[3]);
		}
		printf("];\n");

		printf("quadscale=[];\n");
		printf("codedist=[];\n");
		printf("codepassed=[];\n");
		printf("overlap=[];\n");
		printf("ninfield=[];\n");
		printf("nagree=[];\n");
		printf("agreedist=[];\n");
		printf("codetol=%g;\n", codetol);
		printf("agreetol=%g;\n", agreetol);

		for (i=0; i<Nqms; i++) {
			quadnum = i;
			findable_quad(qms+i, thisfield, cornerpix, hlhits, FALSE);
			fprintf(stderr, "\n");
		}

		fprintf(stderr, "---------------------------------------\n");

		hitlist_healpix_clear(hlhits);

		for (i=0; i<Nqms; i++) {
			findable_quad(qms+i, thisfield, cornerpix, hlhits, TRUE);
			fprintf(stderr, "\n");
		}

		hitlist_healpix_clear(hlhits);

		free_xy(thisfield);
		free_rd(thisrd);

		il_free(indstars);
		il_free(indquads);

		free(qms);

		hitlist_healpix_free(hlhits);

		get_resource_stats(&utime, &stime, NULL);
		fprintf(stderr, "    spent %g s user, %g s system, %g s total.\n",
				(utime - last_utime), (stime - last_stime), (stime - last_stime + utime - last_utime));
		last_utime = utime;
		last_stime = stime;
	}
}

int main(int argc, char *argv[]) {
    uint numfields;
	char* progname = argv[0];
	int i;

    if (argc != 1) {
		printHelp(progname);
		exit(-1);
    }

	fieldlist = il_new(256);

	fieldtoind = intmap_new(INTMAP_MANY_TO_ONE);
	indtofield = intmap_new(INTMAP_ONE_TO_ONE);

	qfits_err_statset(1);

	for (;;) {
		
		tic();

		fieldfname = NULL;
		treefname = NULL;
		startreefname = NULL;
		quadfname = NULL;
		catfname = NULL;
		idfname = NULL;
		parity = DEFAULT_PARITY_FLIP;
		codetol = DEFAULT_CODE_TOL;
		startdepth = 0;
		enddepth = 0;
		il_remove_all(fieldlist);
		funits_lower = 0.0;
		funits_upper = 0.0;
		index_scale = 0.0;
		index_scale_lower_factor = 0.0;
		agreement = FALSE;
		nagree = 4;
		maxnagree = 0;
		agreetol = 0.0;
		cat = NULL;
		quads = NULL;
		startree = NULL;
		xcolname = strdup("ROWC");
		ycolname = strdup("COLC");
		verify_dist2 = 0.0;
		nagree_toverify = 0;
		overlap_tosolve = 0.0;
		min_ninfield = 0;

		if (read_parameters()) {
			break;
		}

		if (agreement && (agreetol <= 0.0)) {
			fprintf(stderr, "If you set 'agreement', you must set 'agreetol'.\n");
			exit(-1);
		}

		fprintf(stderr, "%s params:\n", progname);
		fprintf(stderr, "fieldfname %s\n", fieldfname);
		fprintf(stderr, "rdlsfname %s\n", fieldfname);
		fprintf(stderr, "treefname %s\n", treefname);
		fprintf(stderr, "startreefname %s\n", startreefname);
		fprintf(stderr, "quadfname %s\n", quadfname);
		fprintf(stderr, "catfname %s\n", catfname);
		fprintf(stderr, "idfname %s\n", idfname);
		fprintf(stderr, "parity %i\n", parity);
		fprintf(stderr, "codetol %g\n", codetol);
		fprintf(stderr, "startdepth %i\n", startdepth);
		fprintf(stderr, "enddepth %i\n", enddepth);
		fprintf(stderr, "fieldunits_lower %g\n", funits_lower);
		fprintf(stderr, "fieldunits_upper %g\n", funits_upper);
		fprintf(stderr, "index_lower %g\n", index_scale_lower_factor);
		fprintf(stderr, "agreement %i\n", agreement);
		fprintf(stderr, "num-to-agree %i\n", nagree);
		fprintf(stderr, "max-num-to-agree %i\n", maxnagree);
		fprintf(stderr, "agreetol %g\n", agreetol);
		fprintf(stderr, "verify_dist %g\n", rad2arcsec(distsq2arc(verify_dist2)));
		fprintf(stderr, "nagree_toverify %i\n", nagree_toverify);
		fprintf(stderr, "overlap_tosolve %f\n", overlap_tosolve);
		fprintf(stderr, "min_ninfield %i\n", min_ninfield);
		fprintf(stderr, "xcolname %s\n", xcolname);
		fprintf(stderr, "ycolname %s\n", ycolname);

		fprintf(stderr, "fields ");
		for (i=0; i<il_size(fieldlist); i++)
			fprintf(stderr, "%i ", il_get(fieldlist, i));
		fprintf(stderr, "\n");

		if (!treefname || !fieldfname || (codetol < 0.0)) {
			fprintf(stderr, "Invalid params... this message is useless.\n");
			exit(-1);
		}

		// Read .xyls file...
		fprintf(stderr, "Reading fields file %s...", fieldfname);
		fflush(stderr);
		xyls = xylist_open(fieldfname);
		if (!xyls) {
			fprintf(stderr, "Failed to read xylist.\n");
			exit(-1);
		}
		numfields = xyls->nfields;
		fprintf(stderr, "got %u fields.\n", numfields);
		if (parity) {
			fprintf(stderr, "  Flipping parity (swapping row/col image coordinates).\n");
			xyls->parity = 1;
		}
		xyls->xname = xcolname;
		xyls->yname = ycolname;

		// Read .rdls file...
		fprintf(stderr, "Reading rdls file %s...", rdlsfname);
		fflush(stderr);
		rdls = rdlist_open(rdlsfname);
		if (!rdls) {
			fprintf(stderr, "Failed to read rdls file.\n");
			exit(-1);
		}
		rdls->xname = racolname;
		rdls->yname = deccolname;

		// Read .ckdt file...
		fprintf(stderr, "Reading code KD tree from %s...", treefname);
		fflush(stderr);
		codetree = kdtree_fits_read_file(treefname);
		if (!codetree)
			exit(-1);
		fprintf(stderr, "done\n    (%d quads, %d nodes, dim %d).\n",
				codetree->ndata, codetree->nnodes, codetree->ndim);
		{
			qfits_header* hdr;
			hdr = qfits_header_read(treefname);
			circle = qfits_header_getboolean(hdr, "CIRCLE", 0);
			qfits_header_destroy(hdr);
		}
		if (circle)
			fprintf(stderr, "CKDT has the CIRCLE property.\n");

		fprintf(stderr, "Computing inverse permutation...\n");
		fflush(stderr);
		code_inverse_perm = malloc(codetree->ndata * sizeof(int));
		kdtree_inverse_permutation(codetree, code_inverse_perm);

		// Read .qidx file...
		fprintf(stderr, "Reading qidxfile %s...\n", qidxfname);
		fflush(stderr);
		qidx = qidxfile_open(qidxfname, 0);
		if (!qidx) {
			fprintf(stderr, "Failed to open qidx file.\n");
			exit(-1);
		}

		// Read .quad file...
		fprintf(stderr, "Reading quads file %s...\n", quadfname);
		quads = quadfile_open(quadfname, 0);
		if (!quads) {
			fprintf(stderr, "Couldn't read quads file %s\n", quadfname);
			exit(-1);
		}
		index_scale = quadfile_get_index_scale_arcsec(quads);
		index_scale_lower = quadfile_get_index_scale_lower_arcsec(quads);

        if ((index_scale_lower != 0.0) && (index_scale_lower_factor != 0.0) &&
            ((index_scale_lower_factor * index_scale) != index_scale_lower)) {
            fprintf(stderr, "You specified an index_scale, but the quad file contained a different index_scale_lower entry.\n");
			fprintf(stderr, "Quad file: %g.  Your spec: %g\n", index_scale_lower, index_scale_lower_factor * index_scale);
            fprintf(stderr, "Overriding your specification.\n");
        }
        if ((index_scale_lower == 0.0) && (index_scale_lower_factor != 0.0)) {
            index_scale_lower = index_scale * index_scale_lower_factor;
        }

		fprintf(stderr, "Index scale: %g arcmin, %g arcsec\n", index_scale/60.0, index_scale);

		// Read .skdt file...
		fprintf(stderr, "Reading star KD tree from %s...", startreefname);
		fflush(stderr);
		startree = kdtree_fits_read_file(startreefname);
		if (!startree)
			fprintf(stderr, "Star kdtree not found or failed to read.\n");
		else
			fprintf(stderr, "done\n");

		do_verify = startree && (verify_dist2 > 0.0);

		if (startree) {
			  inverse_perm = malloc(startree->ndata * sizeof(int));
			  kdtree_inverse_permutation(startree, inverse_perm);
			  cat = NULL;
		} else {
			// Read .objs file...
			cat = catalog_open(catfname, 0);
			if (!cat) {
				fprintf(stderr, "Couldn't open catalog %s.\n", catfname);
				exit(-1);
			}
		}

		id = idfile_open(idfname, 0);
		if (!id) {
			fprintf(stderr, "Couldn't open id file %s.\n", idfname);
			//exit(-1);
		}

		// Do it!

		why_not();

		free(xcolname);
		free(ycolname);

		xylist_close(xyls);
		xylist_close(rdls);

		free_fn(fieldfname);
		free_fn(treefname);
		free_fn(quadfname);
		free_fn(catfname);
		free_fn(idfname);
		free_fn(startreefname);

		kdtree_close(codetree);
		if (startree)
			kdtree_close(startree);
		if (cat)
			catalog_close(cat);
		free(inverse_perm);
		free(code_inverse_perm);
		if (id)
			idfile_close(id);
		quadfile_close(quads);

		toc();
	}

	il_free(fieldlist);

	intmap_free(fieldtoind);
	intmap_free(indtofield);

	return 0;
}

int read_parameters() {
	for (;;) {
		char buffer[10240];
		char* nextword;
		fprintf(stderr, "\nAwaiting your command:\n");
		fflush(stderr);
		if (!fgets(buffer, sizeof(buffer), stdin)) {
			return -1;
		}
		// strip off newline
		if (buffer[strlen(buffer) - 1] == '\n')
			buffer[strlen(buffer) - 1] = '\0';

		fprintf(stderr, "Command: %s\n", buffer);
		fflush(stderr);

		if (is_word(buffer, "help", &nextword)) {
			fprintf(stderr, "Commands:\n"
					"    index <index-file-name>\n"
					"    match <match-file-name>\n"
					"    field <field-file-name>\n"
					"    fields [<field-number> or <start range>/<end range>...]\n"
					"    sdepth <start-field-object>\n"
					"    depth <end-field-object>\n"
					"    parity <0 || 1>\n"
					"    tol <code-tolerance>\n"
					"    fieldunits_lower <arcsec-per-pixel>\n"
					"    fieldunits_upper <arcsec-per-pixel>\n"
					"    index_lower <index-size-lower-bound-fraction>\n"
					"    agreement\n"
					"    nagree <min-to-agree>\n"
					"    maxnagree <max-to-agree>\n"
					"    agreetol <agreement-tolerance (arcsec)>\n"
					"    verify_dist <early-verification-dist (arcsec)>\n"
					"    nagree_toverify <nagree>\n"
					"    overlap_tosolve <overlap>\n"
					"    run\n"
					"    help\n"
					"    quit\n");
		} else if (is_word(buffer, "do_correspond ", &nextword)) {
			do_correspond = atoi(nextword);
		} else if (is_word(buffer, "xcol ", &nextword)) {
			xcolname = strdup(nextword);
		} else if (is_word(buffer, "ycol ", &nextword)) {
			ycolname = strdup(nextword);
		} else if (is_word(buffer, "racol ", &nextword)) {
			racolname = strdup(nextword);
		} else if (is_word(buffer, "deccol ", &nextword)) {
			deccolname = strdup(nextword);
		} else if (is_word(buffer, "agreement", &nextword)) {
			agreement = TRUE;
		} else if (is_word(buffer, "threads ", &nextword)) {
			threads = atoi(nextword);
		} else if (is_word(buffer, "nagree ", &nextword)) {
			nagree = atoi(nextword);
		} else if (is_word(buffer, "maxnagree ", &nextword)) {
			maxnagree = atoi(nextword);
		} else if (is_word(buffer, "agreetol ", &nextword)) {
			agreetol = atof(nextword);
		} else if (is_word(buffer, "index ", &nextword)) {
			char* fname = nextword;
			qidxfname = mk_qidxfn(fname);
			treefname = mk_ctreefn(fname);
			quadfname = mk_quadfn(fname);
			catfname = mk_catfn(fname);
			idfname = mk_idfn(fname);
			startreefname = mk_streefn(fname);
		} else if (is_word(buffer, "verify_dist ", &nextword)) {
			double d = atof(nextword);
			verify_dist2 = arcsec2distsq(d);
		} else if (is_word(buffer, "nagree_toverify ", &nextword)) {
			nagree_toverify = atoi(nextword);
		} else if (is_word(buffer, "overlap_tosolve ", &nextword)) {
			overlap_tosolve = atof(nextword);
		} else if (is_word(buffer, "min_ninfield ", &nextword)) {
			min_ninfield = atoi(nextword);
		} else if (is_word(buffer, "field ", &nextword)) {
			char* fname = nextword;
			fieldfname = mk_fieldfn(fname);
		} else if (is_word(buffer, "rdls ", &nextword)) {
			rdlsfname = strdup(nextword);
		} else if (is_word(buffer, "nearby ", &nextword)) {
			nearbyrad = atof(nextword);
		} else if (is_word(buffer, "sdepth ", &nextword)) {
			int d = atoi(nextword);
			startdepth = d;
		} else if (is_word(buffer, "depth ", &nextword)) {
			int d = atoi(nextword);
			enddepth = d;
		} else if (is_word(buffer, "tol ", &nextword)) {
			double t = atof(nextword);
			codetol = t;
		} else if (is_word(buffer, "parity ", &nextword)) {
			int d = atoi(nextword);
			parity = (d ? TRUE : FALSE);
		} else if (is_word(buffer, "index_lower ", &nextword)) {
			double d = atof(nextword);
			index_scale_lower_factor = d;
		} else if (is_word(buffer, "fieldunits_lower ", &nextword)) {
			double d = atof(nextword);
			funits_lower = d;
		} else if (is_word(buffer, "fieldunits_upper ", &nextword)) {
			double d = atof(nextword);
			funits_upper = d;
		} else if (is_word(buffer, "fields ", &nextword)) {
			char* str = nextword;
			char* endp;
			int i, firstfld = -1;
			for (;;) {
				int fld = strtol(str, &endp, 10);
				if (str == endp) {
					// non-numeric value
					fprintf(stderr, "Couldn't parse: %.20s [etc]\n", str);
					break;
				}
				if (firstfld == -1) {
					il_insert_unique_ascending(fieldlist, fld);
				} else {
					if (firstfld > fld) {
						fprintf(stderr, "Ranges must be specified as <start>/<end>: %i/%i\n", firstfld, fld);
					} else {
						for (i=firstfld+1; i<=fld; i++) {
							il_insert_unique_ascending(fieldlist, i);
						}
					}
					firstfld = -1;
				}
				if (*endp == '/')
					firstfld = fld;
				if (*endp == '\0')
					// end of string
					break;
				str = endp + 1;
			}
		} else if (is_word(buffer, "run", &nextword)) {
			return 0;
		} else if (is_word(buffer, "quit", &nextword)) {
			return 1;
		} else {
			fprintf(stderr, "I didn't understand that command.\n");
			fflush(stderr);
		}
	}
}

