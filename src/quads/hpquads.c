#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>

#include "healpix.h"
#include "starutil.h"
#include "codefile.h"
#include "fileutil.h"
#include "mathutil.h"
#include "quadfile.h"
#include "kdtree.h"
#include "kdtree_access.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"
#include "tic.h"
#include "fitsioutils.h"
#include "qfits.h"
#include "permutedsort.h"
#include "bt.h"
#include "rdlist.h"
#include "histogram.h"
#include "starkd.h"

#define OPTIONS "hf:u:l:n:o:i:cr:x:y:F:R"

static void print_help(char* progname)
{
	printf("\nUsage:\n"
	       "  %s -f <input-filename-base> -o <output-filename-base>\n"
		   "     [-c]            allow quads in the circle, not the box, defined by AB\n"
	       "     [-n <nside>]    healpix nside (default 501)\n"
	       "     [-u <scale>]    upper bound of quad scale (arcmin)\n"
	       "     [-l <scale>]    lower bound of quad scale (arcmin)\n"
		   "     [-x <x-passes>] number of passes in the x direction\n"
		   "     [-y <y-passes>] number of passes in the y direction\n"
		   "     [-r <reuse-times>] number of times a star can be used.\n"
		   "     [-R]            make a second pass through healpixes in which quads couldn't be made,\n"
		   "                     removing the \"-r\" restriction on the number of times a star can be used.\n"
		   "     [-i <unique-id>] set the unique ID of this index\n\n"
		   "     [-F <failed-rdls-file>] write the centers of the healpixes in which quads can't be made.\n"
	       "Reads skdt, writes {code, quad}.\n\n"
	       , progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

static quadfile* quads;
static codefile* codes;
static startree* starkd;

// bounds of quad scale (in radians^2)
static double quad_scale_upper2;
static double quad_scale_lower2;

// bounds of quad scale, in distance between AB on the sphere.
static double quad_dist2_upper;
static double quad_dist2_lower;

struct quad {
	uint star[4];
};
typedef struct quad quad;

static int Nquads;
static quad* quadlist;
static bt* bigquadlist;

static int ndupquads = 0;
static int nbadscale = 0;
static int nbadcenter = 0;
static int nabok = 0;

static unsigned char* nuses;

static void* mymalloc(int n) {
	void* rtn = malloc(n);
	if (!rtn) {
		fprintf(stderr, "Failed to malloc %i.\n", n);
		assert(0);
		exit(-1);
	}
	return rtn;
}

static int compare_ints(const void* v1, const void* v2) {
	int i1 = *(int*)v1;
	int i2 = *(int*)v2;
	if (i1 < i2) return -1;
	if (i1 > i2) return 1;
	return 0;
}

static int compare_quads(const void* v1, const void* v2) {
	const quad* q1 = v1;
	const quad* q2 = v2;
	int i;
	for (i=0; i<4; i++) {
		if (q1->star[i] > q2->star[i])
			return 1;
		if (q1->star[i] < q2->star[i])
			return -1;
	}
	return 0;
}

static bool firstpass;

static bool add_quad(quad* q) {
	if (!firstpass) {
		bool dup = bt_contains(bigquadlist, q, compare_quads);
		if (dup) {
			ndupquads++;
			return FALSE;
		}
	}
	quadlist[Nquads++] = *q;
	return TRUE;
}

static void compute_code(quad* q, double* code) {
	double sA[3], sB[3], sC[3], sD[3];
	double Bx, By;
	double scale, invscale;
	double ABx, ABy;
	double* starpos;
	double Dx, Dy;
	double ADx, ADy;
	double x, y;
	double midAB[3];
	double Ax, Ay;
	double costheta, sintheta;

	if (startree_get(starkd, q->star[0], sA) ||
		startree_get(starkd, q->star[1], sB) ||
		startree_get(starkd, q->star[2], sC) ||
		startree_get(starkd, q->star[3], sD)) {
		fprintf(stderr, "Failed to get stars belonging to a quad.\n");
		exit(-1);
	}

	star_midpoint(midAB, sA, sB);
	star_coords(sA, midAB, &Ax, &Ay);
	star_coords(sB, midAB, &Bx, &By);
	ABx = Bx - Ax;
	ABy = By - Ay;
	scale = (ABx * ABx) + (ABy * ABy);
	invscale = 1.0 / scale;
	costheta = (ABy + ABx) * invscale;
	sintheta = (ABy - ABx) * invscale;

	starpos = sC;
	star_coords(starpos, midAB, &Dx, &Dy);
	ADx = Dx - Ax;
	ADy = Dy - Ay;
	x =  ADx * costheta + ADy * sintheta;
	y = -ADx * sintheta + ADy * costheta;
	code[0] = x;
	code[1] = y;

	starpos = sD;
	star_coords(starpos, midAB, &Dx, &Dy);
	ADx = Dx - Ax;
	ADy = Dy - Ay;
	x =  ADx * costheta + ADy * sintheta;
	y = -ADx * sintheta + ADy * costheta;
	code[2] = x;
	code[3] = y;
}

static Inline void drop_quad(int iA, int iB, int iC, int iD) {
	nuses[iA]--;
	nuses[iB]--;
	nuses[iC]--;
	nuses[iD]--;
}

struct potential_quad {
	bool scale_ok;
	int iA, iB;
	uint staridA, staridB;
	double midAB[3];
	double Ax, Ay;
	double costheta, sintheta;
	int* inbox;
	int ninbox;
};
typedef struct potential_quad pquad;

// is the AB distance right?
// is the midpoint of AB inside the healpix?
Noinline
static void
check_scale_and_midpoint(pquad* pq, double* stars, int* starids, int Nstars,
						 double* origin, double* vx, double* vy,
						 double maxdot1, double maxdot2) {
	double *sA, *sB;
	double Bx, By;
	double invscale;
	double ABx, ABy;
	double dot1, dot2;
	double s2;
	int d;
	double avgAB[3];
	double invlen;

	sA = stars + pq->iA * 3;
	sB = stars + pq->iB * 3;

	// s2: squared AB dist
	s2 = 0.0;
	for (d=0; d<3; d++)
		s2 += (sA[d] - sB[d])*(sA[d] - sB[d]);
	if ((s2 > quad_dist2_upper) ||
		(s2 < quad_dist2_lower)) {
		pq->scale_ok = 0;
		nbadscale++;
		return;
	}

	// avgAB: mean of A,B.  (note, it's NOT on the sphere)
	for (d=0; d<3; d++)
		avgAB[d] = 0.5 * (sA[d] + sB[d]);

	dot1 =
		(avgAB[0] - origin[0]) * vx[0] +
		(avgAB[1] - origin[1]) * vx[1] +
		(avgAB[2] - origin[2]) * vx[2];
	if (dot1 < 0.0 || dot1 > maxdot1) {
		pq->scale_ok = 0;
		nbadcenter++;
		return;
	}
	dot2 =
		(avgAB[0] - origin[0]) * vy[0] +
		(avgAB[1] - origin[1]) * vy[1] +
		(avgAB[2] - origin[2]) * vy[2];
	if (dot2 < 0.0 || dot2 > maxdot2) {
		pq->scale_ok = 0;
		nbadcenter++;
		return;
	}

	pq->scale_ok = 1;
	pq->staridA = starids[pq->iA];
	pq->staridB = starids[pq->iB];

	invlen = 1.0 / sqrt(avgAB[0]*avgAB[0] + avgAB[1]*avgAB[1] + avgAB[2]*avgAB[2]);
	pq->midAB[0] = avgAB[0] * invlen;
	pq->midAB[1] = avgAB[1] * invlen;
	pq->midAB[2] = avgAB[2] * invlen;
	star_coords(sA, pq->midAB, &pq->Ax, &pq->Ay);
	star_coords(sB, pq->midAB, &Bx, &By);
	ABx = Bx - pq->Ax;
	ABy = By - pq->Ay;
	invscale = 1.0 / (ABx*ABx + ABy*ABy);
	pq->costheta = (ABy + ABx) * invscale;
	pq->sintheta = (ABy - ABx) * invscale;

	nabok++;
}

Noinline
static int
check_inbox(pquad* pq, int* inds, int ninds, double* stars, bool circle) {
	int i, ind;
	double* starpos;
	double Dx, Dy;
	double ADx, ADy;
	double x, y;
	int destind = 0;
	for (i=0; i<ninds; i++) {
		ind = inds[i];
		starpos = stars + ind*3;
		star_coords(starpos, pq->midAB, &Dx, &Dy);
		ADx = Dx - pq->Ax;
		ADy = Dy - pq->Ay;
		x =  ADx * pq->costheta + ADy * pq->sintheta;
		y = -ADx * pq->sintheta + ADy * pq->costheta;
		if (circle) {
			// make sure it's in the circle centered at (0.5, 0.5)...
			// (x-1/2)^2 + (y-1/2)^2   <=   r^2
			// x^2-x+1/4 + y^2-y+1/4   <=   (1/sqrt(2))^2
			// x^2-x + y^2-y + 1/2     <=   1/2
			// x^2-x + y^2-y           <=   0
			double r = (x*x - x) + (y*y - y);
			if (r > 0.0)
				continue;
		} else {
			// make sure it's in the box...
			if ((x > 1.0) || (x < 0.0) ||
				(y > 1.0) || (y < 0.0)) {
				continue;
			}
		}
		inds[destind] = ind;
		destind++;
	}
	return destind;
}

static int Ncq = 0;
static pquad* cq_pquads = NULL;
static int* cq_inbox = NULL;

Noinline
static int create_quad(double* stars, int* starinds, int Nstars,
					   bool circle,
					   double* origin, double* vx, double* vy,
					   double maxdot1, double maxdot2) {
	uint iA=0, iB, iC, iD, newpoint;
	int rtn = 0;
	int ninbox;
	int i, j, k;
	int* inbox;
	pquad* pquads;
	int iAalloc;

	// ensure the arrays are large enough...
	if (Nstars > Ncq) {
		free(cq_inbox);
		free(cq_pquads);
		Ncq = Nstars;
		cq_inbox =  mymalloc(Nstars * sizeof(int));
		cq_pquads = mymalloc(Nstars * Nstars * sizeof(pquad));
	}
	inbox = cq_inbox;
	pquads = cq_pquads;

	/*
	  Each time through the "for" loop below, we consider a new
	  star ("newpoint").  First, we try building all quads that
	  have the new star on the diagonal (star B).  Then, we try
	  building all quads that have the star not on the diagonal
	  (star D).

	  Note that we keep the invariants iA < iB and iC < iD.
	*/

	for (newpoint=0; newpoint<Nstars; newpoint++) {
		pquad* pq;
		// quads with the new star on the diagonal:
		iB = newpoint;
		for (iA = 0; iA < newpoint; iA++) {
			pq = pquads + iA*Nstars + iB;
			pq->inbox = NULL;
			pq->ninbox = 0;
			pq->iA = iA;
			pq->iB = iB;
			check_scale_and_midpoint(pq, stars, starinds, Nstars,
									 origin, vx, vy, maxdot1, maxdot2);
			if (!pq->scale_ok)
				continue;

			ninbox = 0;
			for (iC = 0; iC < newpoint; iC++) {
				if ((iC == iA) || (iC == iB))
					continue;
				inbox[ninbox] = iC;
				ninbox++;
			}
			ninbox = check_inbox(pq, inbox, ninbox, stars, circle);
			for (j=0; j<ninbox; j++) {
				iC = inbox[j];
				for (k=j+1; k<ninbox; k++) {
					quad q;
					iD = inbox[k];
					q.star[0] = pq->staridA;
					q.star[1] = pq->staridB;
					q.star[2] = starinds[iC];
					q.star[3] = starinds[iD];

					if (add_quad(&q)) {
						drop_quad(q.star[0], q.star[1], q.star[2], q.star[3]);
						rtn = 1;
						goto theend;
					}
				}
			}
			pq->inbox = mymalloc(Nstars * sizeof(int));
			pq->ninbox = ninbox;
			memcpy(pq->inbox, inbox, ninbox * sizeof(int));
		}
		iAalloc = iA;

		// quads with the new star not on the diagonal:
		iD = newpoint;
		for (iA = 0; iA < newpoint; iA++) {
			for (iB = iA + 1; iB < newpoint; iB++) {
				pq = pquads + iA*Nstars + iB;
				if (!pq->scale_ok)
					continue;
				inbox[0] = iD;
				if (!check_inbox(pq, inbox, 1, stars, circle))
					continue;
				pq->inbox[pq->ninbox] = iD;
				pq->ninbox++;
				ninbox = pq->ninbox;

				for (j=0; j<ninbox; j++) {
					iC = pq->inbox[j];
					for (k=j+1; k<ninbox; k++) {
						quad q;
						iD = pq->inbox[k];
						q.star[0] = pq->staridA;
						q.star[1] = pq->staridB;
						q.star[2] = starinds[iC];
						q.star[3] = starinds[iD];

						if (add_quad(&q)) {
							drop_quad(q.star[0], q.star[1], q.star[2], q.star[3]);
							rtn = 1;
							iA = iAalloc;
							goto theend;
						}
					}
				}
			}
		}
	}
 theend:
	for (i=0; i<imin(Nstars, newpoint+1); i++) {
		int lim = (i == newpoint) ? iA : i;
		for (j=0; j<lim; j++) {
			pquad* pq = pquads + j*Nstars + i;
			free(pq->inbox);
		}
	}

	return rtn;
}

static int* perm = NULL;
static int* inds = NULL;
static double* stars = NULL;

static bool find_stars_and_vectors(int hp, int Nside, double radius2,
								   int* p_nostars, int* p_yesstars,
								   int* p_nounused, int* p_nstarstotal,
								   int* p_ncounted,
								   int* p_N,
								   double* centre, double* perp1,
								   double* perp2,
								   double* p_maxdot1, double* p_maxdot2,
								   double dxfrac, double dyfrac,
								   bool* p_failed_nostars) {
	static int Nhighwater = 0;
	double origin[3];
	double vx[3];
	double vy[3];
	double normal[3];
	int d;
	kdtree_qres_t* res;
	int j, N;
	int destind;

	healpix_to_xyzarr_lex(0.0, 0.0, hp, Nside, origin);
	healpix_to_xyzarr_lex(1.0, 0.0, hp, Nside, vx);
	healpix_to_xyzarr_lex(0.0, 1.0, hp, Nside, vy);
	for (d=0; d<3; d++) {
		vx[d] -= origin[d];
		vy[d] -= origin[d];
		centre[d] = origin[d] + dxfrac*vx[d] + dyfrac*vy[d];
	}

	res = kdtree_rangesearch_nosort(starkd->tree, centre, radius2);

	// here we could check whether stars are in the box
	// defined by the healpix boundaries plus quadscale.

	N = res->nres;
	if (N < 4) {
		kdtree_free_query(res);
		if (p_nostars)
			(*p_nostars)++;
		if (p_N) *p_N = N;
		if (p_failed_nostars)
			*p_failed_nostars = TRUE;
		return FALSE;
	}
	if (p_yesstars)
		(*p_yesstars)++;

	// remove stars that have been used up.
	destind = 0;
	for (j=0; j<N; j++) {
		if (!nuses[res->inds[j]])
			continue;
		res->inds[destind] = res->inds[j];
		for (d=0; d<3; d++)
			res->results[destind*3+d] = res->results[j*3+d];
		destind++;
	}
	N = destind;
	if (N < 4) {
		kdtree_free_query(res);
		if (p_nounused)
			(*p_nounused)++;
		if (p_N) *p_N = N;
		return FALSE;
	}
	if (p_nstarstotal)
		(*p_nstarstotal) += N;
	if (p_ncounted)
		(*p_ncounted)++;

	// sort the stars in increasing order of index - assume
	// that this corresponds to decreasing order of brightness.

	// ensure the arrays are big enough...
	if (N > Nhighwater) {
		free(perm);
		free(inds);
		free(stars);
		perm  = mymalloc(N * sizeof(int));
		inds  = mymalloc(N * sizeof(int));
		stars = mymalloc(N * 3 * sizeof(double));
		Nhighwater = N;
	}
	// find permutation that sorts by index...
	for (j=0; j<N; j++)
		perm[j] = j;
	permuted_sort_set_params(res->inds, sizeof(int), compare_ints);
	permuted_sort(perm, N);
	// apply the permutation...
	for (j=0; j<N; j++) {
		inds[j] = res->inds[perm[j]];
		for (d=0; d<3; d++)
			stars[j*3+d] = res->results[perm[j]*3+d];
	}
	kdtree_free_query(res);

	// compute the projection vectors
	cross_product(vx, vy, normal);
	cross_product(normal, vx, perp1);
	cross_product(vy, normal, perp2);
	*p_maxdot1 = *p_maxdot2 = 0.0;
	for (d=0; d<3; d++) {
		*p_maxdot1 += vy[d] * perp1[d];
		*p_maxdot2 += vx[d] * perp2[d];
	}
	if (p_N) *p_N = N;
	return TRUE;
}

int main(int argc, char** argv) {
	int argchar;
	char *quadfname;
	char *codefname;
	char *skdtfname;
	int Nside = 501;
	int HEALPIXES;
	int i;
	char* basefnin = NULL;
	char* basefnout = NULL;
	char* failedrdlsfn = NULL;
	rdlist* failedrdls = NULL;
	int xpass, ypass;
	uint id = 0;
	int xpasses = 1;
	int ypasses = 1;
	bool circle = FALSE;
	int hp;
	int Nreuse = 3;
	double radius2;
	int lastgrass = 0;
	int* hptotry;
	int Nhptotry;
	int nquads;
	double rads;
	double hprad;
	double quadscale;
	bool noreuse_pass = FALSE;
	il* noreuse_hps = NULL;

	dl* nostars_radec = NULL;
	dl* noreuse_radec = NULL;
	dl* noquads_radec = NULL;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'R':
			noreuse_pass = TRUE;
			break;
		case 'F':
			failedrdlsfn = optarg;
			break;
		case 'r':
			Nreuse = atoi(optarg);
			break;
		case 'c':
			circle = TRUE;
			break;
		case 'x':
			xpasses = atoi(optarg);
			break;
		case 'y':
			ypasses = atoi(optarg);
			break;
		case 'i':
			id = atoi(optarg);
			break;
		case 'n':
			Nside = atoi(optarg);
			break;
		case 'h':
			print_help(argv[0]);
			exit(0);
		case 'f':
			basefnin = optarg;
			break;
		case 'o':
			basefnout = optarg;
			break;
		case 'u':
			rads = arcmin2rad(atof(optarg));
			quad_scale_upper2 = square(rads);
			quad_dist2_upper = square(tan(rads));
			break;
		case 'l':
			rads = arcmin2rad(atof(optarg));
			quad_scale_lower2 = square(rads);
			quad_dist2_lower = square(tan(rads));
			break;
		default:
			return -1;
		}

	if (!basefnin || !basefnout) {
		fprintf(stderr, "Specify in & out base filenames, bonehead!\n");
		print_help(argv[0]);
		exit( -1);
	}

	if (!id) {
		fprintf(stderr, "Warning: you should set the unique-id for this index (-i).\n");
	}

	if (failedrdlsfn) {
		failedrdls = rdlist_open_for_writing(failedrdlsfn);
		if (!failedrdls) {
			fprintf(stderr, "Failed to open file %s to write failed-quads RDLS.\n", failedrdlsfn);
			exit(-1);
		}
		if (rdlist_write_header(failedrdls)) {
			fprintf(stderr, "Failed to write header of failed RDLS file.\n");
		}
	}

	HEALPIXES = 12 * Nside * Nside;
	printf("Nside=%i.  Nside^2=%i.  Number of healpixes=%i.  Healpix side length ~ %g arcmin.\n",
		   Nside, Nside*Nside, HEALPIXES, healpix_side_length_arcmin(Nside));

	tic();

	skdtfname = mk_streefn(basefnin);
	printf("Reading star kdtree %s ...\n", skdtfname);
	starkd = startree_open(skdtfname);
	if (!starkd) {
		fprintf(stderr, "Failed to open star kdtree %s\n", skdtfname);
		exit( -1);
	}
	free_fn(skdtfname);
	printf("Star tree contains %i objects.\n", starkd->tree->ndata);

	quadfname = mk_quadfn(basefnout);
	codefname = mk_codefn(basefnout);

	printf("Will write to quad file %s and code file %s\n", quadfname, codefname);

    quads = quadfile_open_for_writing(quadfname);
	if (!quads) {
		fprintf(stderr, "Couldn't open file %s to write quads.\n", quadfname);
		exit(-1);
	}
    codes = codefile_open_for_writing(codefname);
	if (!codes) {
		fprintf(stderr, "Couldn't open file %s to write codes.\n", quadfname);
		exit(-1);
	}

	if (id) {
		quads->indexid = id;
		codes->indexid = id;
	}

	// get the "HEALPIX" header from the skdt and put it in the code and quad headers.
	hp = qfits_header_getint(starkd->header, "HEALPIX", -1);
	if (hp == -1) {
		fprintf(stderr, "Warning: skdt does not contain \"HEALPIX\" header.  Code and quad files will not contain this header either.\n");
	}

	quads->healpix = hp;
	codes->healpix = hp;
	qfits_header_add(quads->header, "HISTORY", "hpquads command line:", NULL, NULL);
	fits_add_args(quads->header, argv, argc);
	qfits_header_add(quads->header, "HISTORY", "(end of hpquads command line)", NULL, NULL);

	qfits_header_add(codes->header, "HISTORY", "hpquads command line:", NULL, NULL);
	fits_add_args(codes->header, argv, argc);
	qfits_header_add(codes->header, "HISTORY", "(end of hpquads command line)", NULL, NULL);

	qfits_header_add(quads->header, "CXDX", "T", "All codes have the property cx<=dx.", NULL);
	qfits_header_add(codes->header, "CXDX", "T", "All codes have the property cx<=dx.", NULL);

	qfits_header_add(quads->header, "CIRCLE", (circle ? "T" : "F"), 
					 (circle ? "Stars C,D live in the circle defined by AB."
					  :        "Stars C,D live in the box defined by AB."), NULL);
	qfits_header_add(codes->header, "CIRCLE", (circle ? "T" : "F"), 
					 (circle ? "Stars C,D live in the circle defined by AB."
					  :        "Stars C,D live in the box defined by AB."), NULL);

	// add placeholders...
	for (i=0; i<(xpasses * ypasses); i++) {
		char key[64];
		sprintf(key, "PASS%i", i+1);
		qfits_header_add(codes->header, key, "-1", "placeholder", NULL);
		qfits_header_add(quads->header, key, "-1", "placeholder", NULL);
	}

    if (quadfile_write_header(quads)) {
        fprintf(stderr, "Couldn't write headers to quads file %s\n", quadfname);
        exit(-1);
    }
    if (codefile_write_header(codes)) {
        fprintf(stderr, "Couldn't write headers to code file %s\n", codefname);
        exit(-1);
    }

	free_fn(quadfname);
	free_fn(codefname);

    codes->numstars = starkd->tree->ndata;
    codes->index_scale       = sqrt(quad_scale_upper2);
    codes->index_scale_lower = sqrt(quad_scale_lower2);

    quads->numstars = starkd->tree->ndata;
    quads->index_scale       = sqrt(quad_scale_upper2);
    quads->index_scale_lower = sqrt(quad_scale_lower2);

	bigquadlist = bt_new(sizeof(quad), 256);

	if (Nreuse > 255) {
		fprintf(stderr, "Error, reuse (-r) must be less than 256.\n");
		exit(-1);
	}
	nuses = mymalloc(starkd->tree->ndata * sizeof(unsigned char));
	for (i=0; i<starkd->tree->ndata; i++)
		nuses[i] = Nreuse;

	hprad = sqrt(0.5 * arcsec2distsq(healpix_side_length_arcmin(Nside) * 60.0));
	quadscale = 0.5 * sqrt(quad_dist2_upper);
	// 1.01 for a bit of safety.  we'll look at a few extra stars.
	radius2 = square(1.01 * (hprad + quadscale));

	printf("Healpix radius %g arcsec, quad scale %g arcsec, total %g arcsec\n",
		   distsq2arcsec(hprad*hprad),
		   distsq2arcsec(quadscale*quadscale),
		   distsq2arcsec(radius2));

	// if the SKDT had the HEALPIX property, then it includes stars that are
	// within that healpix and within a small margin around it.
	// Try fine-grained healpixes that are either within that big healpix
	// or neighbouring it.  That's not exactly right, since we don't really
	// know how big the margin is, but in reality it's probably what we want
	// to do.
	if (hp != -1) {
		bool* try = calloc(HEALPIXES, sizeof(bool));
		for (i=0; i<HEALPIXES; i++) {
			uint bighp, x, y;
			healpix_decompose_lex(i, &bighp, &x, &y, Nside);
			if (bighp != hp)
				continue;
			try[i] = TRUE;
			if ((x == 0) || (y == 0) || (x == Nside-1) || (y == Nside-1)) {
				uint neigh[8];
				uint nneigh;
				int k;
				nneigh = healpix_get_neighbours_nside(i, neigh, Nside);
				for (k=0; k<nneigh; k++)
					try[neigh[k]] = TRUE;
			}
		}
		Nhptotry = 0;
		for (i=0; i<HEALPIXES; i++)
			if (try[i])
				Nhptotry++;
		hptotry = malloc(Nhptotry * sizeof(int));
		Nhptotry = 0;
		for (i=0; i<HEALPIXES; i++)
			if (try[i])
				hptotry[Nhptotry++] = i;
		free(try);
	} else {
		// try all healpixes.
		hptotry = malloc(HEALPIXES * sizeof(int));
		for (i=0; i<HEALPIXES; i++)
			hptotry[i] = i;
		Nhptotry = HEALPIXES;
	}

	quadlist = malloc(Nhptotry * sizeof(quad));

	if (noreuse_pass)
		noreuse_hps = il_new(1024);

	if (failedrdls) {
		nostars_radec = dl_new(1024);
		noreuse_radec = dl_new(1024);
		noquads_radec = dl_new(1024);
	}

	firstpass = TRUE;

	for (xpass=0; xpass<xpasses; xpass++) {
		for (ypass=0; ypass<ypasses; ypass++) {
			double dxfrac, dyfrac;
			int nthispass;
			int nnostars;
			int nyesstars;
			int nnounused;
			int nstarstotal = 0;
			int ncounted = 0;

			histogram* histnstars = histogram_new_nbins(0.0, 100.0, 100);
			histogram* histnstars_failed = histogram_new_nbins(0.0, 100.0, 100);

			dxfrac = xpass / (double)xpasses;
			dyfrac = ypass / (double)ypasses;

			printf("Pass %i of %i.\n", xpass * ypasses + ypass + 1, xpasses * ypasses);
			nthispass = 0;
			nnostars = 0;
			nyesstars = 0;
			nnounused = 0;
			lastgrass = 0;
			Nquads = 0;
			nbadscale = 0;
			nbadcenter = 0;
			nabok = 0;
			ndupquads = 0;

			printf("Trying %i healpixes.\n", Nhptotry);

			for (i=0; i<Nhptotry; i++) {
				double centre[3];
				double perp1[3];
				double perp2[3];
				double maxdot1, maxdot2;
				double radec[2];
				int hp;
				int N;
				bool ok;
				bool failed_nostars;

				hp = hptotry[i];

				if ((i * 80 / Nhptotry) != lastgrass) {
					printf(".");
					fflush(stdout);
					lastgrass = i * 80 / Nhptotry;
				}

				failed_nostars = FALSE;
				ok = find_stars_and_vectors(hp, Nside, radius2,
											&nnostars, &nyesstars,
											&nnounused, &nstarstotal,
											&ncounted,
											&N, centre, perp1, perp2,
											&maxdot1, &maxdot2,
											dxfrac, dyfrac,
											&failed_nostars);

				if (failedrdls) {
					xyz2radec(centre[0], centre[1], centre[2], radec, radec+1);
					radec[0] = rad2deg(radec[0]);
					radec[1] = rad2deg(radec[1]);
				}

				if (!ok) {
					if (failed_nostars) {
						if (failedrdls) {
							dl_append(nostars_radec, radec[0]);
							dl_append(nostars_radec, radec[1]);
						}
					} else {
						if (noreuse_pass)
							il_append(noreuse_hps, hp);
						if (failedrdls) {
							dl_append(noreuse_radec, radec[0]);
							dl_append(noreuse_radec, radec[1]);
						}
					}
					histogram_add(histnstars_failed, (double)N);
					continue;
				}

				if (create_quad(stars, inds, N, circle,
								centre, perp1, perp2, maxdot1, maxdot2)) {
					histogram_add(histnstars, (double)N);
					nthispass++;
				} else {
					if (noreuse_pass)
						il_append(noreuse_hps, hp);
					if (failedrdls) {
						dl_append(noquads_radec, radec[0]);
						dl_append(noquads_radec, radec[1]);
					}
					histogram_add(histnstars_failed, (double)N);
				}
			}
			printf("\n");

			printf("Number of stars per healpix histogram:\n");
			printf("hist_nstars=");
			histogram_print_matlab(histnstars, stdout);
			printf("\n");
			printf("hist_nstars_bins=");
			histogram_print_matlab_bin_centers(histnstars, stdout);
			printf("\n");

			printf("Number of stars per healpix, for failed healpixes:\n");
			printf("hist_nstars_failed=");
			histogram_print_matlab(histnstars_failed, stdout);
			printf("\n");
			printf("hist_nstars_failed_bins=");
			histogram_print_matlab_bin_centers(histnstars_failed, stdout);
			printf("\n");

			histogram_free(histnstars);
			histogram_free(histnstars_failed);

			printf("Each non-empty healpix had on average %g stars.\n",
				   nstarstotal / (double)ncounted);

			printf("Made %i quads (out of %i healpixes) this pass.\n",
				   nthispass, Nhptotry);
			printf("  %i healpixes had no stars.\n", nnostars);
			printf("  %i healpixes had only stars that had been overused.\n", nnounused);
			printf("  %i healpixes had some stars.\n", nyesstars);
			printf("  %i AB pairs had bad scale.\n", nbadscale);
			printf("  %i AB pairs had bad center.\n", nbadcenter);
			printf("  %i AB pairs were ok.\n", nabok);
			printf("  %i quads were duplicates.\n", ndupquads);

			{
				char key[64];
				char val[64];
				sprintf(key, "PASS%i", xpass * ypasses + ypass + 1);
				sprintf(val, "%i", nthispass);
				qfits_header_mod(codes->header, key, val, "quads created in this pass");
				qfits_header_mod(quads->header, key, val, "quads created in this pass");
			}

			// HACK -
			// sort the quads in "quadlist", then insert them into
			// "bigquadlist" ?

			printf("Made %i quads so far.\n", bt_size(bigquadlist) + Nquads);

			if (failedrdls) {
				int j;
				dl* lists[] = { nostars_radec, noreuse_radec, noquads_radec };
				int l;

				for (l=0; l<3; l++) {
					dl* list = lists[l];
					if (rdlist_write_new_field(failedrdls)) {
						fprintf(stderr, "Failed to start a new field in failed RDLS file.\n");
						exit(-1);
					}
					for (j=0; j<dl_size(list); j+=2) {
						double radec[2];
						radec[0] = dl_get(list, j);
						radec[1] = dl_get(list, j+1);
						if (rdlist_write_entries(failedrdls, radec, 1)) {
							fprintf(stderr, "Failed to write failed-RDLS entries.\n");
							exit(-1);
						}
					}
					if (rdlist_fix_field(failedrdls)) {
						fprintf(stderr, "Failed to fix a field in failed RDLS file.\n");
						exit(-1);
					}
				}
			}

			if ((xpass == xpasses-1) &&
				(ypass == ypasses-1))
				break;

			printf("Merging quads...\n");
			for (i=0; i<Nquads; i++) {
				quad* q = quadlist + i;
				bt_insert(bigquadlist, q, FALSE, compare_quads);
			}

			/*
			  printf("bt height is %i\n", bt_height(bigquadlist));
			  {
			  double opth = log(bt_size(bigquadlist) / (double)bigquadlist->blocksize) / log(2.0);
			  int nleaves;
			  printf("(optimal height is %g.)\n", opth);
			  nleaves = bt_count_leaves(bigquadlist);
			  printf("bt contains %i leaves.\n", nleaves);
			  printf("leaves are on average %.1f%% full.\n", 100.0 * bt_size(bigquadlist) / (double)(nleaves * bigquadlist->blocksize));
			  }
			*/

			firstpass = FALSE;

		}
	}
	free(hptotry);

	if (failedrdls) {
		dl_free(nostars_radec);
		dl_free(noreuse_radec);
		dl_free(noquads_radec);
	}


	if (noreuse_pass) {
		int i;
		int nfailed1 = 0;
		int nfailed2 = 0;
		int nmade = 0;
		lastgrass = -1;
		printf("Making no-limit-on-number-of-times-a-star-can-be-used pass.\n");

		for (i=0; i<starkd->tree->ndata; i++)
			nuses[i] = 255;

		if (failedrdls) {
			if (rdlist_write_new_field(failedrdls)) {
				fprintf(stderr, "Failed to start a new field in failed RDLS file.\n");
				exit(-1);
			}
		}

		for (i=0; i<il_size(noreuse_hps); i++) {
			double centre[3];
			double perp1[3];
			double perp2[3];
			double maxdot1, maxdot2;
			int N;
			int hp = il_get(noreuse_hps, i);

			if ((i * 80 / il_size(noreuse_hps)) != lastgrass) {
				printf(".");
				fflush(stdout);
				lastgrass = i * 80 / il_size(noreuse_hps);
			}

			if (!find_stars_and_vectors(hp, Nside, radius2,
										NULL, NULL, NULL, NULL, NULL,
										&N, centre, perp1, perp2,
										&maxdot1, &maxdot2, 0.0, 0.0,
										NULL)) {
				nfailed1++;
				goto failedhp2;
			}
			if (!create_quad(stars, inds, N, circle,
							 centre, perp1, perp2, maxdot1, maxdot2)) {
				nfailed2++;
				goto failedhp2;
			}
			nmade++;
			continue;

		failedhp2:
			if (failedrdls) {
				double radec[2];
				xyz2radec(centre[0], centre[1], centre[2], radec, radec+1);
				radec[0] = rad2deg(radec[0]);
				radec[1] = rad2deg(radec[1]);
				if (rdlist_write_entries(failedrdls, radec, 1)) {
					fprintf(stderr, "Failed to write failed-RDLS entries.\n");
					exit(-1);
				}
			}
		}

		printf("\n");
		printf("Tried %i healpixes.\n", il_size(noreuse_hps));
		printf("Failed at point 1: %i.\n", nfailed1);
		printf("Failed at point 2: %i.\n", nfailed2);
		printf("Made: %i\n", nmade);

		il_free(noreuse_hps);

		if (failedrdls) {
			if (rdlist_fix_field(failedrdls)) {
				fprintf(stderr, "Failed to fix a field in failed RDLS file.\n");
				exit(-1);
			}
		}
	}

	free(cq_pquads);
	free(cq_inbox);

	free(stars);
	free(inds);
	free(perm);
	free(nuses);

	printf("Writing quads...\n");

	// add the quads from the big-quadlist
	nquads = bt_size(bigquadlist);
	for (i=0; i<nquads; i++) {
		double code[4];
		quad* q = bt_access(bigquadlist, i);
		compute_code(q, code);
		// here we add the invariant cx <= dx.
		if (code[0] <= code[2]) {
			codefile_write_code(codes, code[0], code[1], code[2], code[3]);
			quadfile_write_quad(quads, q->star[0], q->star[1], q->star[2], q->star[3]);
		} else {
			codefile_write_code(codes, code[2], code[3], code[0], code[1]);
			quadfile_write_quad(quads, q->star[0], q->star[1], q->star[3], q->star[2]);
		}
	}
	// add the quads that were made during the final round.
	for (i=0; i<Nquads; i++) {
		double code[4];
		quad* q = quadlist + i;
		compute_code(q, code);
		// here we add the invariant cx <= dx.
		if (code[0] <= code[2]) {
			codefile_write_code(codes, code[0], code[1], code[2], code[3]);
			quadfile_write_quad(quads, q->star[0], q->star[1], q->star[2], q->star[3]);
		} else {
			codefile_write_code(codes, code[2], code[3], code[0], code[1]);
			quadfile_write_quad(quads, q->star[0], q->star[1], q->star[3], q->star[2]);
		}
	}
	free(quadlist);

	startree_close(starkd);

	// fix output file headers.
	if (quadfile_fix_header(quads) ||
		quadfile_close(quads)) {
		fprintf(stderr, "Couldn't write quad output file: %s\n", strerror(errno));
		exit( -1);
	}
	if (codefile_fix_header(codes) ||
		codefile_close(codes)) {
		fprintf(stderr, "Couldn't write code output file: %s\n", strerror(errno));
		exit( -1);
	}

	if (failedrdls) {
		if (rdlist_fix_header(failedrdls) ||
			rdlist_close(failedrdls)) {
			fprintf(stderr, "Failed to fix header of failed RDLS file.\n");
		}
	}

	bt_free(bigquadlist);

	toc();
	printf("Done.\n");
	return 0;
}

	
