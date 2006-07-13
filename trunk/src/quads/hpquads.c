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
#include "catalog.h"
#include "tic.h"
#include "fitsioutils.h"

#define OPTIONS "hf:u:l:n:o:i:p:P:c" // r

static void print_help(char* progname)
{
	printf("\nUsage:\n"
	       "  %s -f <input-filename-base> -o <output-filename-base>\n"
	       //"     [-r]            re-bin the unused stars\n"
		   "     [-c]            allow quads in the circle, not the box, defined by AB\n"
	       "     [-n <nside>]    healpix nside (default 501)\n"
	       "     [-u <scale>]    upper bound of quad scale (arcmin)\n"
	       "     [-l <scale>]    lower bound of quad scale (arcmin)\n"
		   "     [-p <passes>]   number of quad-generating passes through healpixes (inner loop)\n"
		   "     [-P <passes>]   number of quad-generating passes through healpixes (outer loop)\n"
		   "     [-i <unique-id>] set the unique ID of this index\n\n"
	       "Reads catalog (objs), writes (code, quad).\n\n"
	       , progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

static quadfile* quads;
static codefile* codes;
static catalog* cat;

// bounds of quad scale (in radians^2)
static double quad_scale_upper2;
static double quad_scale_lower2;

#define IL_BLOCKSIZE 50

static int quadnum = 0;

static bl* quadlist;
static int ndupquads = 0;

static unsigned char* nuses;

struct quad {
	uint star[4];
};
typedef struct quad quad;

int compare_quad(const void* v1, const void* v2) {
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
	//return (bl_insert_unique_sorted(quadlist, q, compare_quad) != -1);
	int ind = bl_insert_unique_sorted(quadlist, q, compare_quad);
	if (ind == -1)
		ndupquads++;
	return (ind != -1);
}

void compute_code(quad* q, double* code) {
	double *sA, *sB;
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

	sA = catalog_get_star(cat, q->star[0]);
	sB = catalog_get_star(cat, q->star[1]);
	star_midpoint(midAB, sA, sB);
	star_coords(sA, midAB, &Ax, &Ay);
	star_coords(sB, midAB, &Bx, &By);
	ABx = Bx - Ax;
	ABy = By - Ay;
	scale = (ABx * ABx) + (ABy * ABy);
	invscale = 1.0 / scale;
	costheta = (ABy + ABx) * invscale;
	sintheta = (ABy - ABx) * invscale;

	starpos = catalog_get_star(cat, q->star[2]);
	star_coords(starpos, midAB, &Dx, &Dy);
	ADx = Dx - Ax;
	ADy = Dy - Ay;
	x =  ADx * costheta + ADy * sintheta;
	y = -ADx * sintheta + ADy * costheta;
	code[0] = x;
	code[1] = y;

	starpos = catalog_get_star(cat, q->star[3]);
	star_coords(starpos, midAB, &Dx, &Dy);
	ADx = Dx - Ax;
	ADy = Dy - Ay;
	x =  ADx * costheta + ADy * sintheta;
	y = -ADx * sintheta + ADy * costheta;
	code[2] = x;
	code[3] = y;
}

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

// warning, you must guarantee iA<iB and iC<iD
static Inline void drop_quad(il* stars, int iA, int iB, int iC, int iD)
{
	int n1, n2, n3, n4;
	int i1, i2, i3, i4;
	// n1 < n2 < n3 < n4.
	n1 = MIN(iA, iC);
	n4 = MAX(iB, iD);
	n2 = MAX(iA, iC);
	n3 = MIN(iB, iD);
	if (n2 > n3) {
		int ntmp = n2;
		n2 = n3;
		n3 = ntmp;
	}

	assert(iA < iB);
	assert(iC < iD);
	assert(n1 < n2);
	assert(n2 < n3);
	assert(n3 < n4);
	assert(il_size(stars) >= 4);
	assert(il_size(stars) > iA);
	assert(il_size(stars) > iB);
	assert(il_size(stars) > iC);
	assert(il_size(stars) > iD);
	assert(iA >= 0);
	assert(iB >= 0);
	assert(iC >= 0);
	assert(iD >= 0);

	i1 = il_get(stars, n1);
	i2 = il_get(stars, n2);
	i3 = il_get(stars, n3);
	i4 = il_get(stars, n4);

	assert(nuses[i4]);
	assert(nuses[i3]);
	assert(nuses[i2]);
	assert(nuses[i1]);

	nuses[i4]--;
	if (!nuses[i4])
		il_remove(stars, n4);
	nuses[i3]--;
	if (!nuses[i3])
		il_remove(stars, n3);
	nuses[i2]--;
	if (!nuses[i2])
		il_remove(stars, n2);
	nuses[i1]--;
	if (!nuses[i1])
		il_remove(stars, n1);
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
check_scale(pquad* pq, il* stars) {
	double *sA, *sB;
	double Bx, By;
	double scale, invscale;
	double ABx, ABy;
	pq->staridA = il_get(stars, pq->iA);
	pq->staridB = il_get(stars, pq->iB);
	sA = catalog_get_star(cat, pq->staridA);
	sB = catalog_get_star(cat, pq->staridB);
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
	pq->scale_ok = 1;
	invscale = 1.0 / scale;
	pq->costheta = (ABy + ABx) * invscale;
	pq->sintheta = (ABy - ABx) * invscale;
}

static int
check_inbox(pquad* pq, int* inds, int ninds, il* stars, bool circle) {
	int i, ind;
	uint starid;
	double* starpos;
	double Dx, Dy;
	double ADx, ADy;
	double x, y;
	int destind = 0;
	for (i=0; i<ninds; i++) {
		ind = inds[i];
		starid = il_get(stars, ind);
		starpos = catalog_get_star(cat, starid);
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

static char find_a_quad(il* stars, bool circle) {
	uint numxy, iA, iB, iC, iD, newpoint;
	int rtn = 0;
	pquad* pquads;
	int* inbox;
	int ninbox;
	int i, j, k;

	numxy = il_size(stars);
	inbox =  malloc(numxy * sizeof(int));
	pquads = calloc(numxy*numxy, sizeof(pquad));

	/*
	  Each time through the "for" loop below, we consider a new
	  star ("newpoint").  First, we try building all quads that
	  have the new star on the diagonal (star B).  Then, we try
	  building all quads that have the star not on the diagonal
	  (star D).

	  Note that we keep the invariants iA < iB and iC < iD.
	*/

	for (newpoint = 0; newpoint < numxy; newpoint++) {
		pquad* pq;
		// quads with the new star on the diagonal:
		iB = newpoint;
		for (iA = 0; iA < newpoint; iA++) {
			int j,k;

			pq = pquads + iA*numxy + iB;
			pq->iA = iA;
			pq->iB = iB;
			check_scale(pq, stars);
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
					q.star[2] = il_get(stars, iC);
					q.star[3] = il_get(stars, iD);

					if (add_quad(&q)) {
						drop_quad(stars, iA, iB, iC, iD);
						rtn = 1;
						goto theend;
					}
				}
			}
			pq->inbox = malloc(numxy * sizeof(int));
			pq->ninbox = ninbox;
			memcpy(pq->inbox, inbox, ninbox * sizeof(int));
		}

		// quads with the new star not on the diagonal:
		iD = newpoint;
		for (iA = 0; iA < newpoint; iA++) {
			for (iB = iA + 1; iB < newpoint; iB++) {
				pq = pquads + iA*numxy + iB;
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
						q.star[2] = il_get(stars, iC);
						q.star[3] = il_get(stars, iD);

						if (add_quad(&q)) {
							drop_quad(stars, iA, iB, iC, iD);
							rtn = 1;
							goto theend;
						}
					}
				}
			}
		}
	}
 theend:
	for (i=0; i<(numxy*numxy); i++)
		free(pquads[i].inbox);
	free(inbox);
	free(pquads);
	return rtn;
}

static void shifted_healpix_bin_stars(int numstars, il* starindices,
									  il* pixels, int dx, int dy,
									  int Nside)
{
	int i;
	int HEALPIXES = 12 * Nside * Nside;
	int offx, offy;

	printf("Binning stars into %i healpixes...\n", HEALPIXES);
	fflush(stdout);

	// the idea is that we look at a healpix grid that is three times
	// finer in both directions, and in each pass we choose one of the nine
	// sub-pixels to be the center and we dump stars from a 3x3 grid around
	// the center into its coarser pixel.

	// (offx, offy) are the offsets you have to add to the sub-pixel
	// positions to determine which pixel they belong to.
	offx = (1 - dx);
	offy = (1 - dy);

	for (i = 0; i < numstars; i++) {
		double* starxyz;
		int hp;
		int ind;
		uint bighp, x, y;
		if (!(i % 100000)) {
			printf(".");
			fflush(stdout);
		}
		if (!starindices)
			ind = i;
		else
			ind = il_get(starindices, i);

		if (!nuses[ind])
			// this star has been used too many times already.
			continue;

		starxyz = catalog_get_star(cat, ind);
		// note the Nside*3; this is the sub-pixel.
		hp = xyztohealpix_nside(starxyz[0], starxyz[1], starxyz[2], Nside*3);
		healpix_decompose(hp, &bighp, &x, &y, Nside*3);
		// now we compute which pixel this sub-pixel belongs to.
		if (unlikely(
			// if it's on the border...
			((x == 0) || (x == (Nside*3-1)) ||
			 (y == 0) || (y == (Nside*3-1))) &&
			// and it's not the center pixel itself...
			(((x % 3) != dx) || ((y % 3) != dy)) )) {
			// this sub-pixel is on the border of its big healpix.
			// this happens rarely, so do a relatively expensive check:
			// just find its neighbours and take the first one that has the
			// right "dx" and "dy" values (ie, is the center sub-pixel in
			// this pass).
			uint nn, neigh[8];
			int j;
			nn = healpix_get_neighbours_nside(hp, neigh, Nside*3);
			for (j=0; j<nn; j++) {
				uint nx, ny, nbighp;
				healpix_decompose(neigh[j], &nbighp, &nx, &ny, Nside*3);
				if (((nx % 3) == dx) && ((ny % 3) == dy)) {
					// found the center pixel!
					// compute its corresponding normal (not sub-) pixel.
					hp = healpix_compose(nbighp, nx/3, ny/3, Nside);
					break;
				}
			}
			if (j == nn) {
				// none of the neighbours is a center pixel - this happens occasionally in
				// weird corners of the healpixes.
				continue;
			}
		} else {
			x = (x + offx) / 3;
			y = (y + offy) / 3;
			// note Nside: this is a normal pixel, not a sub-pixel.
			hp = healpix_compose(bighp, x, y, Nside);
		}
		// append this star to the appropriate normal pixel list.
		il_append(pixels + hp, ind);
	}
	printf("\n");
	fflush(stdout);
}

static void create_quads_in_pixels(int numstars, il* starindices,
								   il* pixels, int Nside,
								   int dx, int dy, int Npasses,
								   bool circle)
{
	int i;
	int HEALPIXES = 12 * Nside * Nside;
	int Ninteresting = HEALPIXES;
	char* interesting;
	unsigned char* quadsmade;
	int passes = 0;
	int nused = 0;

	shifted_healpix_bin_stars(numstars, starindices, pixels, dx, dy, Nside);

	quadsmade = calloc(HEALPIXES, sizeof(unsigned char));
	interesting = calloc(HEALPIXES, sizeof(char));

	Ninteresting = 0;
	for (i=0; i<HEALPIXES; i++) {
		if (il_size(pixels + i)) {
			interesting[i] = 1;
			Ninteresting++;
		}
	}

	while (Ninteresting) {
		int nusedthispass;
		int grass = 0;
		int hp;
		nusedthispass = 0;
		for (hp=0; hp<HEALPIXES; hp++) {
			int foundone;
			if (!interesting[hp])
				continue;
			if (((grass++) % 10000) == 0) {
				printf("+");
				fflush(stdout);
			}
			foundone = find_a_quad(pixels + hp, circle);
			if (foundone) {
				if (!il_size(pixels + hp)) {
					interesting[hp] = 0;
					Ninteresting--;
					continue;
				}
				quadsmade[hp]++;
				if (!quadsmade[hp]) {
					fprintf(stderr, "Warning, \"quadsmade\" counter overflowed for healpix %i.  Some of the stats may be wrong.\n", hp);
				}
				nused += 4;
				nusedthispass += 4;
			} else {
				interesting[hp] = 0;
				Ninteresting--;
			}
		}
		passes++;
		printf("\nEnd of pass %i: ninteresting=%i, nused=%i this pass, %i total, of %i stars\n",
		        passes, Ninteresting, nusedthispass, nused, (int)numstars);

		// HACK
		quadnum = bl_size(quadlist);
		printf("Duplicate quads: %i\n", ndupquads);

		printf("Made %i quads so far.\n", quadnum);
		if (Npasses && (passes == Npasses)) {
			printf("Maximum number of passes reached.\n");
			break;
		}
	}
	printf("\n");
	fflush(stdout);

	printf("Took %i passes.\n", passes);
	printf("Made %i quads.\n", quadnum);
	printf("Used %i stars.\n", nused);
	printf("Didn't use %i stars.\n", (int)numstars - nused);

	free(interesting);
	free(quadsmade);
}


int main(int argc, char** argv)
{
	int argchar;
	char *quadfname = NULL;
	char *codefname = NULL;
	char* catfname;
	il* pixels;
	int Nside = 501;
	int HEALPIXES;
	int i;
	//int rebin = 0;
	char* basefnin = NULL;
	char* basefnout = NULL;
	uint pass, npasses;
	uint id = 0;
	int Npasses = 0;
	bool circle = FALSE;
	int Bigpasses = 1;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'P':
			Bigpasses = atoi(optarg);
			break;
		case 'c':
			circle = TRUE;
			break;
		case 'p':
			Npasses = atoi(optarg);
			break;
		case 'i':
			id = atoi(optarg);
			break;
			/*
			  case 'r':
			  rebin = 1;
			  break;
			*/
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

	if (Nside % 3) {
		fprintf(stderr, "Warning: to be really correct you should make Nside "
			   " (-n option) a multiple of three.");
	}

	if (!id) {
		fprintf(stderr, "Warning: you should set the unique-id for this index (-i).\n");
	}

	HEALPIXES = 12 * Nside * Nside;
	{
		double hparea = 4.0 * M_PI * square(180.0 * 60.0 / M_PI) / (double)HEALPIXES;
		printf("Nside=%i.  Number of healpixes=%i.  Healpix area = %g arcmin^2, length ~ %g arcmin.\n",
		        Nside, HEALPIXES, hparea, sqrt(hparea));
	}

	tic();

	pixels = malloc(HEALPIXES * sizeof(il));
	for (i = 0; i < HEALPIXES; i++)
		il_new_existing(pixels + i, IL_BLOCKSIZE);

	catfname = mk_catfn(basefnin);
	printf("Reading catalog file %s ...\n", catfname);
	cat = catalog_open(catfname, 0);
	if (!cat) {
		fprintf(stderr, "Failed to open catalog file %s\n", catfname);
		exit( -1);
	}
	free_fn(catfname);
	printf("Catalog contains %i objects.\n", cat->numstars);

	quadfname = mk_quadfn(basefnout);
	codefname = mk_codefn(basefnout);

	printf("Writing quad file %s and code file %s\n", quadfname, codefname);

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
	// get the "HEALPIX" header from the catalog and put it in the code and quad headers.
	quads->healpix = cat->healpix;
	codes->healpix = cat->healpix;
	if (cat->healpix == -1) {
		fprintf(stderr, "Warning: catalog file does not contain \"HEALPIX\" header.  Code and quad files will not contain this header either.\n");
	}
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
	quadfname = codefname = NULL;

    codes->numstars = cat->numstars;
    codes->index_scale       = sqrt(quad_scale_upper2);
    codes->index_scale_lower = sqrt(quad_scale_lower2);

    quads->numstars = cat->numstars;
    quads->index_scale       = sqrt(quad_scale_upper2);
    quads->index_scale_lower = sqrt(quad_scale_lower2);

	quadlist = bl_new(1024, sizeof(quad));

	if (Bigpasses > 255) {
		fprintf(stderr, "Error, -P must be less than 256.\n");
		exit(-1);
	}

	nuses = malloc(cat->numstars * sizeof(unsigned char));
	for (i=0; i<cat->numstars; i++) {
		nuses[i] = Bigpasses;
	}

	npasses = 9;
	for (pass = 0; pass < npasses; pass++) {
		int dx, dy;
		dx = pass % 3;
		dy = (pass / 3) % 3;

		printf("Doing shift %i of %i: dx=%i, dy=%i.\n", pass+1, npasses, dx, dy);

		create_quads_in_pixels(cat->numstars, NULL, pixels, Nside, dx, dy,
							   Npasses, circle);

		/*
		  Who knows if this works? 

		  if (rebin) {
		  // Gather up the leftover stars and re-bin.
		  il* leftovers = il_new(IL_BLOCKSIZE);
		  for (i = 0; i < HEALPIXES; i++) {
		  il_merge_lists(leftovers, pixels + i);
		  }
		  printf("Rebinning with Nside=%i\n", Nside / 2);
		  create_quads_in_pixels(il_size(leftovers), leftovers, pixels, Nside / 2, dx, dy, );
		  il_free(leftovers);
		  }
		*/

		// empty blocklists.
		for (i = 0; i < HEALPIXES; i++)
			il_remove_all(pixels + i);
	}

	free(nuses);

	for (i=0; i<bl_size(quadlist); i++) {
		double code[4];
		quad* q = bl_access(quadlist, i);
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

	catalog_close(cat);

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

	toc();

	printf("Done.\n");
	fflush(stdout);

	free(pixels);

	return 0;
}

