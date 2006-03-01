#include <math.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "healpix.h"
#include "starutil.h"
#include "blocklist.h"
#include "fileutil.h"

#define OPTIONS "hf:s:l:"

extern char *optarg;
extern int optind, opterr, optopt;

FILE *catfid  = NULL;
FILE *quadfid = NULL;
FILE *codefid = NULL;

double* catalogue;
void*  mmap_cat = NULL;
size_t mmap_cat_size = 0;

sidx maxstar;

// bounds of quad scale (in radians^2)
double quad_scale_upper2;
double quad_scale_lower2;

int quadnum = 0;

void print_help(char* progname) {
    printf("\nUsage:\n"
		   "  %s -f <filename-base>\n"
		   "     [-s <scale>]         (default scale is 5 arcmin)\n"
		   "     [-l <range>]         (lower bound on scale of quads - fraction of the scale; default 0)\n"
		  "\n"
		   , progname);
}


double* get_star(int sid) {
	return catalogue + sid * DIM_STARS;
}

int get_resource_stats(double* p_usertime, double* p_systime, long* p_maxrss) {
	struct rusage usage;
	if (getrusage(RUSAGE_SELF, &usage)) {
		fprintf(stderr, "getrusage failed: %s\n", strerror(errno));
		return 1;
	}
	if (p_usertime) {
		*p_usertime = usage.ru_utime.tv_sec + 1e-6 * usage.ru_utime.tv_usec;
	}
	if (p_systime) {
		*p_systime = usage.ru_stime.tv_sec + 1e-6 * usage.ru_stime.tv_usec;
	}
	if (p_maxrss) {
		*p_maxrss = usage.ru_maxrss;
	}
	return 0;
}

/* computes the 2D coordinates (x,y)  that star s would have in a
   TANGENTIAL PROJECTION defined by (centred at) star r.     */
void star_coords_2(double *s, double *r, double *x, double *y) {
	double sdotr = s[0] * r[0] + s[1] * r[1] + s[2] * r[2];
	if (sdotr <= 0.0) {
		fprintf(stderr, "ERROR (star_coords) -- s dot r <=0; undefined projection.\n");
		return;
	}

	if (r[2] == 1.0) {
		*x = s[0] / s[2];
		*y = s[1] / s[2];
	} else if (r[2] == -1.0) {
		*x = s[0] / s[2];
		*y = -s[1] / s[2];
	} else {
		double etax, etay, etaz, xix, xiy, xiz, eta_norm;
		// eta is a vector perpendicular to r
		etax = -r[1];
		etay = + r[0];
		etaz = 0.0;
		eta_norm = sqrt(etax * etax + etay * etay);
		etax /= eta_norm;
		etay /= eta_norm;
		// xi =  r cross eta
		xix = -r[2] * etay;
		xiy = r[2] * etax;
		xiz = r[0] * etay - r[1] * etax;

		*x = s[0] * xix / sdotr +
			s[1] * xiy / sdotr +
			s[2] * xiz / sdotr;
		*y = s[0] * etax / sdotr +
			s[1] * etay / sdotr;
	}
}

void star_midpoint_2(double* mid, double* A, double* B) {
	double len;
	// we don't actually need to divide by 2 because
	// we immediately renormalize it...
	mid[0] = A[0] + B[0];
	mid[1] = A[1] + B[1];
	mid[2] = A[2] + B[2];
	len = sqrt(square(mid[0]) + square(mid[1]) + square(mid[2]));
	mid[0] /= len;
	mid[1] /= len;
	mid[2] /= len;
}

// warning, you must guarantee iA<iB and iC<iD
void drop_quad(blocklist* stars, int iA, int iB, int iC, int iD) {
	blocklist_remove_index(stars, iA);
	blocklist_remove_index(stars, iB - 1);
	if (iC < iA)
		iC--;
	if (iC < iB)
		iC--;
	blocklist_remove_index(stars, iC);
	if (iD < iA)
		iD--;
	if (iD < iB)
		iD--;
	blocklist_remove_index(stars, iD-1);
}

int try_quads(int iA, int iB, int* iCs, int* iDs, int ncd,
			  char* inbox, int maxind, blocklist* stars) {
    int i;
    int iC=0, iD;
    double Ax, Ay, Bx, By, Cx, Cy;
	double ABx, ABy, ACx, ACy;
	double cx = 0.0, cy = 0.0, dx = 0.0, dy = 0.0;
    double costheta, sintheta, scale;
	double *sA, *sB, *sC;
	double midAB[3];
	int ninbox = 0;
	int staridA, staridB, staridC=0, staridD;

	staridA = blocklist_int_access(stars, iA);
	staridB = blocklist_int_access(stars, iB);
	sA = get_star(staridA);
	sB = get_star(staridB);

	star_midpoint_2(midAB, sA, sB);
	star_coords_2(sA, midAB, &Ax, &Ay);
	star_coords_2(sB, midAB, &Bx, &By);

	ABx = Bx - Ax;
	ABy = By - Ay;

	scale = (ABx*ABx) + (ABy*ABy);

	if ((scale < quad_scale_lower2) ||
		(scale > quad_scale_upper2))
		return 0;

	costheta = (ABx + ABy) / scale;
	sintheta = (ABy - ABx) / scale;

    // check which C, D points are inside the square.
    for (i=0; i<maxind; i++) {
		if (!inbox[i]) continue;

		iD = i;
		staridD = blocklist_int_access(stars, i);
		sC = get_star(staridD);
		star_coords_2(sC, midAB, &Cx, &Cy);
		ACx = Cx - Ax;
		ACy = Cy - Ay;
		dx =  ACx * costheta + ACy * sintheta;
		dy = -ACx * sintheta + ACy * costheta;

		if ((dx >= 1.0) || (dx <= 0.0) ||
			(dy >= 1.0) || (dy <= 0.0)) {
			continue;
		}

		ninbox++;
		if (ninbox == 1) {
			cx = dx;
			cy = dy;
			staridC = staridD;
			iC = iD;
		} else {
			writeonecode(codefid, cx, cy, cx, cy);
			writeonequad(quadfid, staridA, staridB, staridC, staridD);
			quadnum++;
			drop_quad(stars, iA, iB, iC, iD);
			return 1;
		}
	}
	return 0;
}

bool find_a_quad(blocklist* stars) {
    sidx numxy, iA, iB, iC, iD, newpoint;
    int *iCs, *iDs;
    char *iunion;
    int ncd;

	numxy = blocklist_count(stars);

	iCs = alloca(numxy * numxy * sizeof(int));
	iDs = alloca(numxy * numxy * sizeof(int));
	iunion = alloca(numxy * sizeof(char));

	/*
	  Each time through the "for" loop below, we consider a new
	  star ("newpoint").  First, we try building all quads that
	  have the new star on the diagonal (star B).  Then, we try
	  building all quads that have the star not on the diagonal
	  (star D).

	  In each of the loops, we first consider a pair of A, B stars,
	  and gather all the stars that could be C and D stars.  We add
	  the indices of the C and D stars to the arrays "iCs" and "iDs".
	  That is, iCs[0] and iDs[0] contain the first pair of C,D stars.
	  Both arrays can contain duplicates.

	  We also mark "iunion" of these indices to TRUE, so if index "i"
	  appears in "iCs" or "iDs", then "iunion[i]" is true.

	  We then call "try_quads" with stars A,B and the sets of stars
	  C,D.  For each element in "iunion" that is true, "try_quads"
	  checks whether that star is inside the box formed by AB; if not,
	  it marks iunion back to FALSE.  If then iterates through "iCs"
	  and "iDs", skipping any for which the corresponding "iunion"
	  element is FALSE.
	*/

	// We keep the invariants that iA < iB and iC < iD.
	// We try the A<->B and C<->D permutation in try_all_points.
	for (newpoint=0; newpoint<numxy; newpoint++) {
		// quads with the new star on the diagonal:
		iB = newpoint;
		for (iA=0; iA<newpoint; iA++) {
			ncd = 0;
			memset(iunion, 0, newpoint);
			for (iC=0; iC<newpoint; iC++) {
				if ((iC == iA) || (iC == iB))
					continue;
				iunion[iC] = 1;
				for (iD=iC+1; iD<newpoint; iD++) {
					if ((iD == iA) || (iD == iB))
						continue;
					iCs[ncd] = iC;
					iDs[ncd] = iD;
					ncd++;
				}
			}
			// note: "newpoint" is used as an upper-bound on the largest
			// TRUE element in "iunion".
			if (try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint, stars)) {
				return 1;
			}
		}

		// quads with the new star not on the diagonal:
		iD = newpoint;
		for (iA=0; iA<newpoint; iA++) {
			for (iB=iA+1; iB<newpoint; iB++) {
				ncd = 0;
				memset(iunion, 0, newpoint+1);
				iunion[iD] = 1;
				for (iC=0; iC<newpoint; iC++) {
					if ((iC == iA) || (iC == iB))
						continue;
					iunion[iC] = 1;
					iCs[ncd] = iC;
					iDs[ncd] = iD;
					ncd++;
				}
				// note: "newpoint+1" is used because "iunion[newpoint]" is TRUE.
				if (try_quads(iA, iB, iCs, iDs, ncd, iunion, newpoint+1, stars)) {
					return 1;
				}
			}
		}
	}
	return 0;
}





int main(int argc, char** argv) {
    int argchar;
    double ramin, ramax, decmin, decmax;

    char *catfname  = NULL;
    char *quadfname = NULL;
    char *codefname = NULL;
	char readStatus;
	dimension Dim_Stars;
	off_t endoffset;

	time_t starttime, endtime;
	off_t cposmarker;
	blocklist* pixels;
	int Nside = 512;
	int HEALPIXES = 12*Nside*Nside;
	int i;
	bool* interesting;
	int Ninteresting = HEALPIXES;
	int passes = 0;
	sidx numstars;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'h':
			print_help(argv[0]);
			exit(0);
		case 'f':
			catfname  = mk_catfn(optarg);
			quadfname = mk_quadfn(optarg);
			codefname = mk_codefn(optarg);
			break;
		case 's':
			quad_scale_upper2 = atof(optarg);
			if (quad_scale_upper2 == 0.0) {
				printf("Couldn't parse desired scale \"%s\"\n", optarg);
				exit(-1);
			}
			quad_scale_upper2 = square(arcmin2rad(quad_scale_upper2));
			break;
		case 'l':
			quad_scale_lower2 = atof(optarg);
			quad_scale_lower2 = square(arcmin2rad(quad_scale_lower2));
			break;
		default:
			return -1;
		}

	starttime = time(NULL);

	pixels = malloc(HEALPIXES * sizeof(blocklist));
	for (i=0; i<HEALPIXES; i++) {
		blocklist_int_init(pixels + i, 50);
	}

	interesting = malloc(HEALPIXES * sizeof(bool));
	memset(interesting, 1, HEALPIXES);


	// Read .objs file...
	fopenin(catfname, catfid);
	free_fn(catfname);
	catfname = NULL;
	readStatus = read_objs_header(catfid, &numstars, &Dim_Stars,
								  &ramin, &ramax, &decmin, &decmax);
	if (readStatus == READ_FAIL) {
		exit(-1);
	}
	cposmarker = ftello(catfid);
	// check that the catalogue file is the right size.
	fseeko(catfid, 0, SEEK_END);
	endoffset = ftello(catfid) - cposmarker;
	maxstar = endoffset / (DIM_STARS * sizeof(double));
	if (maxstar != numstars) {
		fprintf(stderr, "Error: numstars=%li (specified in .objs file header) does "
				"not match maxstars=%li (computed from size of .objs file).\n",
				numstars, maxstar);
		exit(-1);
	}

	mmap_cat_size = endoffset;
	mmap_cat = mmap(0, mmap_cat_size, PROT_READ, MAP_SHARED, fileno(catfid), 0);
	if (mmap_cat == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap catalogue file: %s\n", strerror(errno));
		exit(-1);
	}
	fclose(catfid);
	catalogue = (double*)(((char*)(mmap_cat)) + cposmarker);

	fopenout(quadfname, quadfid);
	fopenout(codefname, codefid);

	// we have to write an updated header after we've processed all the quads.
	write_code_header(codefid, 0, numstars, DIM_CODES, sqrt(quad_scale_upper2));
	write_quad_header(quadfid, 0, numstars, DIM_QUADS, sqrt(quad_scale_upper2));


	for (i=0; i<numstars; i++) {
		double* starxyz;
		int hp;
		starxyz  = get_star(i);
		hp = xyztohealpix_nside(starxyz[0], starxyz[1], starxyz[2], Nside);
		blocklist_int_append(pixels + hp, i);
	}

	while (Ninteresting) {
		for (i=0; i<HEALPIXES; i++) {
			if (!interesting[i])
				continue;
			if (!find_a_quad(pixels + i)) {
				interesting[i] = 0;
				Ninteresting--;
			}
		}
		passes++;
		printf("End of pass %i: ninteresting=%i\n",
			   passes, Ninteresting);
	}

	printf("Took %i passes.\n", passes);


	munmap(mmap_cat, mmap_cat_size);

	// fix output file headers.
	fix_code_header(codefid, quadnum);
	fix_quad_header(quadfid, quadnum);

	// close .code and .quad files
	if (fclose(codefid)) {
		printf("Couldn't write code output file: %s\n", strerror(errno));
		exit(-1);
	}
	if (fclose(quadfid)) {
		printf("Couldn't write quad output file: %s\n", strerror(errno));
		exit(-1);
	}

	{
		double utime, stime;
		long rss;
		int dtime;
		endtime = time(NULL);
		dtime = (int)(endtime - starttime);
		if (!get_resource_stats(&utime, &stime, &rss)) {
			fprintf(stderr, "Finished: used %g s user, %g s system (%g s total), %i s wall time, max rss %li\n",
					utime, stime, utime+stime, dtime, rss);
		}
	}

	printf("Done.\n");
	fflush(stdout);
	return 0;
}

