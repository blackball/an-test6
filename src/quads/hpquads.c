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
#include "bl.h"
#include "kdtree.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"
#include "tic.h"
#include "fitsioutils.h"
#include "qfits.h"
#include "permutedsort.h"

#include "bt.h"

#define OPTIONS "hf:u:l:n:o:i:cr:x:y:" /* p:P: */

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
		   "     [-i <unique-id>] set the unique ID of this index\n\n"
	       "Reads skdt, writes {code, quad}.\n\n"
	       , progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

static quadfile* quads;
static codefile* codes;
static kdtree_t* startree;
static int* invperm;

// bounds of quad scale (in radians^2)
static double quad_scale_upper2;
static double quad_scale_lower2;

static int quadnum = 0;

//static bl* quadlist;
static bt* quadlist;
static int ndupquads = 0;

static unsigned char* nuses;

struct quad {
	uint star[4];
};
typedef struct quad quad;

static void* mymalloc(int n) {
	void* rtn = malloc(n);
	if (!rtn) {
		fprintf(stderr, "Failed to malloc %i.\n", n);
		exit(-1);
	}
	return rtn;
}

/*
  static void* mycalloc(int n, int sz) {
  void* rtn = calloc(n, sz);
  if (!rtn) {
  fprintf(stderr, "Failed to calloc %i * %i.\n", n, sz);
  exit(-1);
  }
  return rtn;
  }
  static void* myrealloc(void* p, int n) {
  void* rtn = realloc(p, n);
  if (!rtn) {
  fprintf(stderr, "Failed to realloc %i.\n", n);
  exit(-1);
  }
  return rtn;
  }
*/

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

static bool add_quad(quad* q) {
	/*
	  int ind = bl_insert_unique_sorted(quadlist, q, compare_quad);
	  if (ind == -1)
	  ndupquads++;
	  return (ind != -1);
	*/
	bool okay = bt_insert(quadlist, q, TRUE, compare_quads);
	if (!okay)
		ndupquads++;
	return okay;
}

void compute_code(quad* q, double* code) {
	double *sA, *sB, *sC, *sD;
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

	sA = startree->data + 3 * invperm[q->star[0]];
	sB = startree->data + 3 * invperm[q->star[1]];
	sC = startree->data + 3 * invperm[q->star[2]];
	sD = startree->data + 3 * invperm[q->star[3]];

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

static void
check_scale(pquad* pq, double* stars, int* starids, int Nstars) {
	double *sA, *sB;
	double Bx, By;
	double scale, invscale;
	double ABx, ABy;
	sA = stars + pq->iA * 3;
	sB = stars + pq->iB * 3;

	// HACK - we can check scale without doing this projections
	// HACK - also, we can determine whether the quad center is in the
	//   healpix using just the average - don't need to normalize it - no sqrt.

	star_midpoint(pq->midAB, sA, sB);
	star_coords(sA, pq->midAB, &pq->Ax, &pq->Ay);
	star_coords(sB, pq->midAB, &Bx, &By);
	ABx = Bx - pq->Ax;
	ABy = By - pq->Ay;
	scale = (ABx * ABx) + (ABy * ABy);

	if ((scale < quad_scale_lower2) ||
		(scale > quad_scale_upper2)) {
		pq->scale_ok = 0;
		return;
	}
	pq->staridA = starids[pq->iA];
	pq->staridB = starids[pq->iB];
	pq->scale_ok = 1;
	invscale = 1.0 / scale;
	pq->costheta = (ABy + ABx) * invscale;
	pq->sintheta = (ABy - ABx) * invscale;
}

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
static pquad* cq_pquads;
static int* cq_inbox;

static int create_quad(double* stars, int* starinds, int Nstars,
					   bool circle,
					   double* origin, double* vx, double* vy,
					   double maxdot1, double maxdot2) {
	uint iA, iB, iC, iD, newpoint;
	int rtn = 0;
	int ninbox;
	int i, j, k;
	int* inbox;
	pquad* pquads;

	// ensure the arrays are large enough...
	if (Nstars > Ncq) {
		Ncq = Nstars;
		cq_inbox =  mymalloc(Nstars * sizeof(int));
		cq_pquads = mymalloc(Nstars * Nstars * sizeof(pquad));
	}
	inbox = cq_inbox;
	pquads = cq_pquads;
	memset(pquads, 0, Nstars*Nstars*sizeof(pquad));

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
			double dot1, dot2;
			pq = pquads + iA*Nstars + iB;
			pq->iA = iA;
			pq->iB = iB;
			check_scale(pq, stars, starinds, Nstars);
			if (!pq->scale_ok)
				continue;

			// is the midpoint of AB inside this healpix?
			dot1 =
				(pq->midAB[0] - origin[0]) * vx[0] +
				(pq->midAB[1] - origin[1]) * vx[1] +
				(pq->midAB[2] - origin[2]) * vx[2];
			if (dot1 < 0.0 || dot1 > maxdot1) {
				pq->scale_ok = 0;
				continue;
			}
			dot2 =
				(pq->midAB[0] - origin[0]) * vy[0] +
				(pq->midAB[1] - origin[1]) * vy[1] +
				(pq->midAB[2] - origin[2]) * vy[2];
			if (dot2 < 0.0 || dot2 > maxdot2) {
				pq->scale_ok = 0;
				continue;
			}

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
							goto theend;
						}
					}
				}
			}
		}
	}
 theend:
	for (i=0; i<(Nstars*Nstars); i++)
		free(pquads[i].inbox);
	return rtn;
}

int main(int argc, char** argv) {
	int argchar;
	char *quadfname;
	char *codefname;
	char *skdtfname;
	qfits_header* hdr;
	int Nside = 501;
	int HEALPIXES;
	int i;
	char* basefnin = NULL;
	char* basefnout = NULL;
	int xpass, ypass;
	uint id = 0;
	int xpasses = 1;
	int ypasses = 1;
	bool circle = FALSE;
	int hp;
	int Nreuse = 3;
	double* hp00;
	double* hpvx;
	double* hpvy;
	double* hpmaxdot1;
	double* hpmaxdot2;
	double radius2;
	int* perm = NULL;
	int j, d;
	int* inds = NULL;
	double* stars = NULL;
	int lastgrass = 0;
	int Nhighwater = 0;
	unsigned char* tryhealpix;
	int Ntry, Ntrystart;
	int nquads;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
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
			quad_scale_upper2 = atof(optarg);
			quad_scale_upper2 = square(arcmin2rad(quad_scale_upper2));
			break;
		case 'l':
			quad_scale_lower2 = atof(optarg);
			quad_scale_lower2 = square(arcmin2rad(quad_scale_lower2));
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

	HEALPIXES = 12 * Nside * Nside;
	printf("Nside=%i.  Nside^2=%i.  Number of healpixes=%i.  Healpix side length ~ %g arcmin.\n",
		   Nside, Nside*Nside, HEALPIXES, healpix_side_length_arcmin(Nside));

	tic();

	skdtfname = mk_streefn(basefnin);
	printf("Reading star kdtree %s ...\n", skdtfname);
	startree = kdtree_fits_read_file(skdtfname);
	if (!startree) {
		fprintf(stderr, "Failed to open star kdtree %s\n", skdtfname);
		exit( -1);
	}
	hdr = qfits_header_read(skdtfname);
	if (!hdr) {
		fprintf(stderr, "Failed to read FITS header from kdtree file %s.\n", skdtfname);
		exit(-1);
	}
	free_fn(skdtfname);
	printf("Star tree contains %i objects.\n", startree->ndata);

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
	hp = qfits_header_getint(hdr, "HEALPIX", -1);
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

    codes->numstars = startree->ndata;
    codes->index_scale       = sqrt(quad_scale_upper2);
    codes->index_scale_lower = sqrt(quad_scale_lower2);

    quads->numstars = startree->ndata;
    quads->index_scale       = sqrt(quad_scale_upper2);
    quads->index_scale_lower = sqrt(quad_scale_lower2);

	//quadlist = bl_new(1024, sizeof(quad));
	quadlist = bt_new(sizeof(quad), 256);

	if (Nreuse > 255) {
		fprintf(stderr, "Error, reuse (-r) must be less than 256.\n");
		exit(-1);
	}
	nuses = mymalloc(startree->ndata * sizeof(unsigned char));
	for (i=0; i<startree->ndata; i++)
		nuses[i] = Nreuse;

	printf("Computing healpix centers...\n");

	hp00 = mymalloc(3 * HEALPIXES * sizeof(double));
	hpvx = mymalloc(3 * HEALPIXES * sizeof(double));
	hpvy = mymalloc(3 * HEALPIXES * sizeof(double));
	hpmaxdot1 = mymalloc(HEALPIXES * sizeof(double));
	hpmaxdot2 = mymalloc(HEALPIXES * sizeof(double));

	for (i=0; i<HEALPIXES; i++)
		healpix_to_xyz_lex(0.0, 0.0, i, Nside,
						   hp00 + i*3 + 0,
						   hp00 + i*3 + 1,
						   hp00 + i*3 + 2);

	printf("Computing healpix bounds...\n");
	for (i=0; i<HEALPIXES; i++) {
		uint bighp, x, y;
		double x1,y1,z1;
		double x2,y2,z2;
		double norm[3];
		double perp1[3];
		double perp2[3];

		x1 = hp00[i*3 + 0];
		y1 = hp00[i*3 + 1];
		z1 = hp00[i*3 + 2];

		healpix_decompose_lex(i, &bighp, &x, &y, Nside);

		if (x == Nside-1)
			healpix_to_xyz_lex(1.0, 0.0, i, Nside, &x2, &y2, &z2);
		else {
			uint hp = healpix_compose_lex(bighp, x+1, y, Nside);
			x2 = hp00[hp*3 + 0];
			y2 = hp00[hp*3 + 1];
			z2 = hp00[hp*3 + 2];
		}
		hpvx[i*3 + 0] = x2 - x1;
		hpvx[i*3 + 1] = y2 - y1;
		hpvx[i*3 + 2] = z2 - z1;

		if (y == Nside-1)
			healpix_to_xyz_lex(0.0, 1.0, i, Nside, &x2, &y2, &z2);
		else {
			uint hp = healpix_compose_lex(bighp, x, y+1, Nside);
			x2 = hp00[hp*3 + 0];
			y2 = hp00[hp*3 + 1];
			z2 = hp00[hp*3 + 2];
		}
		hpvy[i*3 + 0] = x2 - x1;
		hpvy[i*3 + 1] = y2 - y1;
		hpvy[i*3 + 2] = z2 - z1;

		cross_product(hpvx + i*3, hpvy + i*3, norm);
		cross_product(hpvx + i*3, norm, perp1);
		cross_product(hpvy + i*3, norm, perp2);
		hpmaxdot1[i] =
			hpvy[i*3+0]*perp1[0] +
			hpvy[i*3+1]*perp1[1] +
			hpvy[i*3+2]*perp1[2];
		hpmaxdot2[i] = 
			hpvx[i*3+0]*perp2[0] +
			hpvx[i*3+1]*perp2[1] +
			hpvx[i*3+2]*perp2[2];
		hpvx[i*3+0] = perp1[0];
		hpvx[i*3+1] = perp1[1];
		hpvx[i*3+2] = perp1[2];
		hpvy[i*3+0] = perp2[0];
		hpvy[i*3+1] = perp2[1];
		hpvy[i*3+2] = perp2[2];
	}

	{
		double hprad = sqrt(0.5 * (hpvx[0]*hpvx[0] + hpvx[1]*hpvx[1] + hpvx[2]*hpvx[2]));
		double quadscale = 0.5 * sqrt(arc2distsq(sqrt(quad_scale_upper2)));
		// 1.01 for a bit of safety.  we'll look at a few extra stars.
		radius2 = square(1.01 * (hprad + quadscale));

		printf("Healpix radius %g arcsec, quad scale %g arcsec, total %g arcsec\n",
			   distsq2arcsec(hprad*hprad),
			   distsq2arcsec(quadscale*quadscale),
			   distsq2arcsec(radius2));
	}

	tryhealpix = malloc(HEALPIXES * sizeof(unsigned char));
	memset(tryhealpix, 1, HEALPIXES * sizeof(unsigned char));
	Ntry = HEALPIXES;

	for (xpass=0; xpass<xpasses; xpass++) {
		for (ypass=0; ypass<ypasses; ypass++) {
			double dxfrac, dyfrac;
			kdtree_qres_t* res;
			int nthispass;
			int nnostars;
			int nyesstars;
			int nnounused;

			int ntried = 0;

			int nstarstotal = 0;
			int ncounted = 0;

			dxfrac = xpass / (double)xpasses;
			dyfrac = ypass / (double)ypasses;

			printf("Pass %i of %i.\n", xpass * ypasses + ypass + 1, xpasses * ypasses);
			nthispass = 0;
			nnostars = 0;
			nyesstars = 0;
			nnounused = 0;
			lastgrass = 0;
			Ntrystart = Ntry;

			for (i=0; i<HEALPIXES; i++) {
				int N;
				int destind;
				double centre[3];

				if (!tryhealpix[i])
					continue;

				if ((ntried * 80 / Ntrystart) != lastgrass) {
					printf(".");
					fflush(stdout);
					lastgrass = ntried * 80 / Ntrystart;
				}
				ntried++;

				centre[0] = hp00[i*3+0] + hpvx[i*3+0] * dxfrac + hpvy[i*3+0] * dyfrac;
				centre[1] = hp00[i*3+1] + hpvx[i*3+1] * dxfrac + hpvy[i*3+1] * dyfrac;
				centre[2] = hp00[i*3+2] + hpvx[i*3+2] * dxfrac + hpvy[i*3+2] * dyfrac;

				res = kdtree_rangesearch_nosort(startree, centre, radius2);

				// here we could check whether stars are in the box
				// defined by the healpix boundaries plus quadscale.

				N = res->nres;
				if (N < 4) {
					kdtree_free_query(res);
					nnostars++;
					tryhealpix[i] = 0;
					Ntry--;
					continue;
				}

				nyesstars++;

				// remove stars that have no "nuses" left.
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
					nnounused++;
					tryhealpix[i] = 0;
					Ntry--;
					continue;
				}

				// sort the stars in increasing order of index - assume
				// that this corresponds to decreasing order of brightness.

				if (N > Nhighwater) {
					// enlarge arrays.
					free(perm);
					free(inds);
					free(stars);
					perm  = mymalloc(N * sizeof(int));
					inds  = mymalloc(N * sizeof(int));
					stars = mymalloc(N * 3 * sizeof(double));
					Nhighwater = N;
				}

				nstarstotal += N;
				ncounted++;

				for (j=0; j<N; j++)
					perm[j] = j;

				permuted_sort_set_params(res->inds, sizeof(int), compare_ints);
				permuted_sort(perm, N);

				for (j=0; j<N; j++) {
					inds[j] = res->inds[perm[j]];
					for (d=0; d<3; d++)
						stars[j*3+d] = res->results[perm[j]*3+d];
				}

				kdtree_free_query(res);

				if (create_quad(stars, inds, N, circle,
								hp00 + i*3, hpvx + i*3, hpvy + i*3,
								hpmaxdot1[i], hpmaxdot2[i]))
					nthispass++;
			}
			printf("\n");

			printf("Each non-empty healpix had on average %g stars.\n",
				   nstarstotal / (double)ncounted);

			printf("Made %i quads (out of %i healpixes) this pass.\n",
				   nthispass, HEALPIXES);
			printf("  %i healpixes had no stars.\n", nnostars);
			printf("  %i healpixes had some stars.\n", nyesstars);
			printf("  %i healpixes had only stars that had been overused.\n", nnounused);

			// HACK
			//quadnum = bl_size(quadlist);
			quadnum = bt_size(quadlist);
			printf("Duplicate quads: %i\n", ndupquads);
			printf("Made %i quads so far.\n", quadnum);
		}
	}

	free(tryhealpix);
	free(stars);
	free(inds);
	free(perm);
	free(hp00);
	free(hpvx);
	free(hpvy);
	free(hpmaxdot1);
	free(hpmaxdot2);
	free(nuses);

	printf("Writing quads...\n");

	invperm = mymalloc(startree->ndata * sizeof(int));
	if (!invperm) {
		fprintf(stderr, "Failed to allocate mem for invperm.\n");
		exit(-1);
	}
	kdtree_inverse_permutation(startree, invperm);

	nquads = bt_size(quadlist);
	for (i=0; i<nquads; i++) {
		double code[4];
		//quad* q = bl_access(quadlist, i);
		quad* q = bt_access(quadlist, i);
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

	free(invperm);
	kdtree_close(startree);

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

	bt_free(quadlist);

	toc();
	printf("Done.\n");
	return 0;
}

