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
#include "fileutil.h"
#include "blocklist.h"

#define OPTIONS "hcqf:s:l:x"

extern char *optarg;
extern int optind, opterr, optopt;

void accept_quad(int quadnum, sidx iA, sidx iB, sidx iC, sidx iD,
                 double Cx, double Cy, double Dx, double Dy);

FILE *quadfid = NULL;
FILE *codefid = NULL;
FILE *catfid  = NULL;

bool use_mmap = TRUE;

// when not mmap'd
off_t cposmarker;
// when mmap'd
double* catalogue;

void*  mmap_cat = NULL;
size_t mmap_cat_size = 0;

sidx maxstar;

// this is an array of lists, one for each star.
// each list contains the indices of the quads to which
// that star belongs.
blocklist** quadlist = NULL;

sidx numstars;
int nstarsdone = 0;
int lastpercent = 0;
int lastcount = 0;
int justcount = 0;
int quiet = 0;
bool writeqidx = TRUE;

// max radius of the search (in radians)
double radius;

// lower bound of quad scale (fraction of radius)
double lower = 0.0;


void print_help(char* progname) {
    printf("\nUsage:\n"
		   "  %s -f <filename-base>\n"
		   "     [-s <scale>]         (default scale is 5 arcmin)\n"
		   "     [-l <range>]         (lower bound on scale of quads - fraction of the scale; default 0)\n"
		   "     [-c]                 (just count the quads, don't write anything)\n"
		   "     [-q]                 (be quiet!  No progress reports)\n"
		   "     [-x]                 (don't write quad index .qidx file)\n"
		   "\n"
		   , progname);
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

// the star ids in each healpix.  size HEALPIXES * NSTARS.
// healpix i, pass j is in starids[j*HEALPIXES + i].
int* starids;

// how many stars per healpix?
//int PASSES = 2;
int PASSES = 5;

// how many healpixes?
//int Nside = 512;
#define Nside 512
int HEALPIXES = 12*Nside*Nside;

int get_starid(int hp, int pass) {
	return starids[pass*HEALPIXES + hp];
}

void set_starid(int hp, int pass, int starid) {
	starids[pass*HEALPIXES + hp] = starid;
}

int get_first_valid_starid(int hp) {
	int p;
	int id;
	for (p=0; p<PASSES; p++) {
		if ((id = get_starid(hp, p)) != -1)
			return id;
	}
	return -1;
}

void set_first_valid_starid_invalid(int hp) {
	int p;
	int id;
	for (p=0; p<PASSES; p++) {
		if ((id = get_starid(hp, p)) != -1)
			set_starid(hp, p, -1);
	}
}

void get_star(int sid, double* xyz) {
	if (use_mmap) {
		memcpy(xyz, catalogue + sid * DIM_STARS, DIM_STARS*sizeof(double));
	} else {
		fseekocat(sid, cposmarker, catfid);
		//fseeko(catfid, cposmarker + i*DIM_STARS*sizeof(double), SEEK_SET);
		fread(xyz, sizeof(double), DIM_STARS, catfid);
	}
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

int can_create_quad(int s1, int s2, int s3, int s4, double* quad, int* stars) {
	double p1[3];
	double p2[3];
	double p3[3];
	double p4[3];
	double midAB[3];
	double *pA, *pB, *pC, *pD;
    // projected coordinates:
    double Ax, Ay, Bx, By, Cx, Cy, Dx, Dy;
    double AAx, AAy;
    double ABx, ABy;
    double scale, costheta, sintheta;
	double maxdist2;
	double d2;
	int i, j;
	int inds[4];
	int bestinds[4];
	double* points[4];
	double cx, cy, dx, dy;
	double ACx, ACy;
	double ADx, ADy;
	int starids[4];

	points[0] = p1;
	points[1] = p2;
	points[2] = p3;
	points[3] = p4;

	starids[0] = s1;
	starids[1] = s2;
	starids[2] = s3;
	starids[3] = s4;

	get_star(s1, p1);
	get_star(s2, p2);
	get_star(s3, p3);
	get_star(s4, p4);

	// find largest dist...
	maxdist2 = 0.0;
	bestinds[0] = -1;

	// which star is A?
	for (i=0; i<4; i++) {
		inds[0] = i;
		// which star is B?
		for (j=i+1; j<4; j++) {
			int k;
			// tmp
			double *pA, *pB;

			inds[1] = j;

			pA = points[inds[0]];
			pB = points[inds[1]];
			star_coords_2(pA, pA, &AAx, &AAy);
			star_coords_2(pB, pA, &ABx, &ABy);
			d2 = (ABx-AAx)*(ABx-AAx) + (ABy-AAy)*(ABy-AAy);

			//d2 = distsq(points[inds[0]], points[inds[1]], 3);

			if (d2 > radius)

			if (d2 < maxdist2)
				continue;

			//if ((distsq < minrsq) || (distsq > maxrsq)) {


			// which star is C?
			for (k=0; k<4; k++) {
				if ((k != inds[0]) && (k != inds[1])) {
					inds[2] = k;
					break;
				}
			}
			// which star is D?
			for (; k<4; k++) {
				if ((k != inds[0]) && (k != inds[1]) && (k != inds[2])) {
					inds[3] = k;
					break;
				}
			}

			for (k=0; k<4; k++) {
				bestinds[k] = inds[k];
			}
		}
	}

	if (bestinds[0] == -1)
		return 0;

	pA = points[bestinds[0]];
	pB = points[bestinds[1]];
	pC = points[bestinds[2]];
	pD = points[bestinds[3]];

	star_midpoint_2(midAB, pA, pB);
	star_coords_2(pA, midAB, &Ax, &Ay);
	star_coords_2(pB, midAB, &Bx, &By);

	ABx = Bx - Ax;
	ABy = By - Ay;

	scale = (ABx*ABx) + (ABy*ABy);
	costheta = (ABx + ABy) / scale;
	sintheta = (ABy - ABx) / scale;
	
	star_coords_2(pC, midAB, &Cx, &Cy);
	ACx = Cx - Ax;
	ACy = Cy - Ay;
	cx =  ACx * costheta + ACy * sintheta;
	cy = -ACx * sintheta + ACy * costheta;

	star_coords_2(pD, midAB, &Dx, &Dy);
	ADx = Dx - Ax;
	ADy = Dy - Ay;
	dx =  ADx * costheta + ADy * sintheta;
	dy = -ADx * sintheta + ADy * costheta;

	// quad is [cx,cy,dx,dy]

	quad[0] = cx;
	quad[1] = cy;
	quad[2] = dx;
	quad[3] = dy;

	stars[0] = starids[bestinds[0]];
	stars[1] = starids[bestinds[1]];
	stars[2] = starids[bestinds[2]];
	stars[3] = starids[bestinds[3]];

	return 1;
}

int main(int argc, char *argv[]) {
    char* progname;
    int argchar; //  opterr = 0;
    double ramin, ramax, decmin, decmax;

    char *catfname  = NULL;
    char *quadfname = NULL;
    char *codefname = NULL;
    char *qidxfname = NULL;

    FILE *qidxfid = NULL;
	char readStatus;
	dimension Dim_Stars;
	off_t endoffset;
    int hdrlength = 0;

	int i, nquads = 0;
	int p;

	time_t starttime, endtime;

	double* starxyz = NULL;

	int quadnum = 0;


	starttime = time(NULL);

    progname = argv[0];

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'h':
			print_help(progname);
			exit(0);
		case 'x':
			writeqidx = FALSE;
			break;
		case 'c':
			justcount = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'f':
			catfname  = mk_catfn(optarg);
			quadfname = mk_quadfn(optarg);
			codefname = mk_codefn(optarg);
			qidxfname = mk_qidxfn(optarg);
			break;
		case 's':
			radius = atof(optarg);
			if (radius == 0.0) {
				printf("Couldn't parse desired scale \"%s\"\n", optarg);
				exit(-1);
			}
			radius = arcmin2rad(radius);
			break;
		case 'l':
			lower = atof(optarg);
			if (lower > 1.0) {
				printf("You really don't want to make lower > 1.\n");
				exit(-1);
			}
			break;
		default:
			return (OPT_ERR);
		}



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
	if (use_mmap) {
		mmap_cat_size = endoffset;
		mmap_cat = mmap(0, mmap_cat_size, PROT_READ, MAP_SHARED, fileno(catfid), 0);
		if (mmap_cat == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap catalogue file: %s\n", strerror(errno));
			exit(-1);
		}
		fclose(catfid);
		catalogue = (double*)(((char*)(mmap_cat)) + cposmarker);
	}


	fopenout(quadfname, quadfid);
	fopenout(codefname, codefid);

	// we have to write an updated header after we've processed all the quads.
	{
		uint nelems = 1000000000;
		hdrlength = 10;
		write_code_header(codefid, nelems, numstars,
						  DIM_CODES, radius);
		write_quad_header(quadfid, nelems, numstars,
						  DIM_QUADS, radius);
	}

	if (writeqidx) {
		quadlist = (blocklist**)malloc(numstars * sizeof(blocklist*));
		memset(quadlist, 0, numstars * sizeof(blocklist*));
	}

	starids = (int*)malloc(HEALPIXES * PASSES * sizeof(int));
	for (i=0; i<(HEALPIXES * PASSES); i++) {
		starids[i] = -1;
	}

	
	if (!use_mmap) {
		starxyz = (double*)malloc(3 * sizeof(double));
	}

	for (i=0; i<numstars; i++) {
		int hp;

		if (i % 100000 == 99999) {
			fprintf(stderr, ".");
			fflush(stderr);
		}
		get_star(i, starxyz);

		hp = xyztohealpix_nside(starxyz[0], starxyz[1],
								starxyz[2], Nside);
		for (p=0; p<PASSES; p++) {
			if (get_starid(hp, p) == -1) {
				set_starid(hp, p, i);
				break;
			}
		}
	}
	fprintf(stderr, "\n");
	fflush(stderr);

	for (p=0; p<PASSES; p++) {
		int nfound = 0;
		for (i=0; i<HEALPIXES; i++) {
			if (get_starid(i, p) != -1) {
				nfound++;
			}
		}
		fprintf(stderr, "Pass %i: found stars in %i of %i healpixes (%i %%).\n",
				p, nfound, HEALPIXES, (int)rint(100.0 * nfound / (double)HEALPIXES));
	}

	for (i=0; i<HEALPIXES; i++) {
		int neigh[8];
		int nn;
		int s1, s2, s3, s4;
		double code[4];
		int starids[4];

		/*
		  for (p=0; p<PASSES; p++) {
		  s1 = get_starid(i, p);
		  if (s1 == -1) continue;
		  }
		  if (p == PASSES) continue;
		*/

		s1 = get_first_valid_starid(i);
		if (s1 == -1) continue;

		nn = healpix_get_neighbours_nside(i, neigh, Nside);

		// try to create the quad containing this star plus
		// neighbours 0, 1, and 2.
		s2 = get_first_valid_starid(neigh[0]);
		s3 = get_first_valid_starid(neigh[1]);
		s4 = get_first_valid_starid(neigh[2]);
		if ((s2 != -1) && (s3 != -1) && (s4 != -1)) {
			// see if we can create a valid quad...
			if (can_create_quad(s1, s2, s3, s4, code, starids)) {
				// set them to -1.
				set_first_valid_starid_invalid(i);
				set_first_valid_starid_invalid(neigh[0]);
				set_first_valid_starid_invalid(neigh[1]);
				set_first_valid_starid_invalid(neigh[2]);

				accept_quad(quadnum, starids[0], starids[1], starids[2], starids[3], code[0], code[1], code[2], code[3]);

				s1 = get_first_valid_starid(i);
				if (s1 == -1) continue;
			}
		}

		// neighbours 2, 3, 4.
		s2 = get_first_valid_starid(neigh[2]);
		s3 = get_first_valid_starid(neigh[3]);
		s4 = get_first_valid_starid(neigh[4]);
		if ((s2 != -1) && (s3 != -1) && (s4 != -1)) {
			// see if we can create a valid quad...
			if (can_create_quad(s1, s2, s3, s4, code, starids)) {
				// set them to -1.
				set_first_valid_starid_invalid(i);
				set_first_valid_starid_invalid(neigh[2]);
				set_first_valid_starid_invalid(neigh[3]);
				set_first_valid_starid_invalid(neigh[4]);

				accept_quad(quadnum, starids[0], starids[1], starids[2], starids[3], code[0], code[1], code[2], code[3]);

				s1 = get_first_valid_starid(i);
				if (s1 == -1) continue;
			}
		}

		if (nn < 7) continue;

		// neighbours 4, 5, 6.
		s2 = get_first_valid_starid(neigh[4]);
		s3 = get_first_valid_starid(neigh[5]);
		s4 = get_first_valid_starid(neigh[6]);
		if ((s2 != -1) && (s3 != -1) && (s4 != -1)) {
			// see if we can create a valid quad...
			if (can_create_quad(s1, s2, s3, s4, code, starids)) {
				// set them to -1.
				set_first_valid_starid_invalid(i);
				set_first_valid_starid_invalid(neigh[4]);
				set_first_valid_starid_invalid(neigh[5]);
				set_first_valid_starid_invalid(neigh[6]);

				accept_quad(quadnum, starids[0], starids[1], starids[2], starids[3], code[0], code[1], code[2], code[3]);

				s1 = get_first_valid_starid(i);
				if (s1 == -1) continue;
			}
		}

		// neighbours 6, 7, 0.
		s2 = get_first_valid_starid(neigh[6]);
		s3 = get_first_valid_starid(neigh[7]);
		s4 = get_first_valid_starid(neigh[0]);
		if ((s2 != -1) && (s3 != -1) && (s4 != -1)) {
			// see if we can create a valid quad...
			if (can_create_quad(s1, s2, s3, s4, code, starids)) {
				// set them to -1.
				set_first_valid_starid_invalid(i);
				set_first_valid_starid_invalid(neigh[6]);
				set_first_valid_starid_invalid(neigh[7]);
				set_first_valid_starid_invalid(neigh[0]);

				accept_quad(quadnum, starids[0], starids[1], starids[2], starids[3], code[0], code[1], code[2], code[3]);

				s1 = get_first_valid_starid(i);
				if (s1 == -1) continue;
			}
		}
	}

	if (!use_mmap) {
		free(starxyz);
	}


	free(starids);

	{
		int i, j;
		sidx numused = 0;
		magicval magic = MAGIC_VAL;

		// fix output file headers.
		fix_code_header(codefid, nquads, hdrlength);
		fix_quad_header(quadfid, nquads, hdrlength);

		// close .code and .quad files
		if (fclose(codefid)) {
			printf("Couldn't write code output file: %s\n", strerror(errno));
			exit(-1);
		}
		if (fclose(quadfid)) {
			printf("Couldn't write quad output file: %s\n", strerror(errno));
			exit(-1);
		}

		if (writeqidx) {
			fopenout(qidxfname, qidxfid);
			// write qidx (adapted from quadidx.c)

			printf("Writing qidx file...\n");
			fflush(stdout);

			// first count numused:
			// how many stars are members of quads.
			for (i=0; i<numstars; i++) {
				blocklist* list = quadlist[i];
				if (!list) continue;
				numused++;
			}
			printf("%li stars used\n", numused);
			if (fwrite(&magic, sizeof(magic), 1, qidxfid) != 1) {
				printf("Error writing magic: %s\n", strerror(errno));
				exit(-1);
			}
			if (fwrite(&numused, sizeof(numused), 1, qidxfid) != 1) {
				printf("Error writing numused: %s\n", strerror(errno));
				exit(-1);
			}
			for (i=0; i<numstars; i++) {
				qidx thisnumq;
				sidx thisstar;
				blocklist* list = quadlist[i];
				if (!list) continue;
				thisnumq = (qidx)blocklist_count(list);
				thisstar = i;
				if ((fwrite(&thisstar, sizeof(thisstar), 1, qidxfid) != 1) ||
					(fwrite(&thisnumq, sizeof(thisnumq), 1, qidxfid) != 1)) {
					printf("Error writing qidx entry for star %i: %s\n", i,
						   strerror(errno));
					exit(-1);
				}
				for (j=0; j<thisnumq; j++) {
					qidx kk;
					blocklist_get(list, j, &kk);
					if (fwrite(&kk, sizeof(kk), 1, qidxfid) != 1) {
						printf("Error writing qidx quads for star %i: %s\n",
							   i, strerror(errno));
						exit(-1);
					}
				}
				blocklist_free(list);
				quadlist[i] = NULL;
			}
		
			if (fclose(qidxfid)) {
				printf("Couldn't write quad index file: %s\n", strerror(errno));
				exit(-1);
			}
			free(quadlist);
		}
    }

	printf("Done.\n");
	fflush(stdout);


	if (use_mmap) {
		munmap(mmap_cat, mmap_cat_size);
	} else {
		fclose(catfid);
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

	return 0;

}


// from quadidx.c
void insertquad(qidx quadid,
                sidx iA, sidx iB, sidx iC, sidx iD) {
	blocklist* list;
	sidx inds[4];
	int i;

	inds[0] = iA;
	inds[1] = iB;
	inds[2] = iC;
	inds[3] = iD;

	// append this quad index to the lists of each of
	// its four stars.
	for (i=0; i<4; i++) {
		sidx starind = inds[i];
		list = quadlist[starind];
		// create the list if necessary
		if (!list) {
			list = blocklist_new(10, sizeof(qidx));
			quadlist[starind] = list;
		}
		blocklist_append(list, &quadid);
	}
}

void accept_quad(int quadnum, sidx iA, sidx iB, sidx iC, sidx iD,
                 double Cx, double Cy, double Dx, double Dy) {
    sidx itmp;
    double tmp;
    if (iC > iD) { // swap C and D if iC>iD, involves swapping Cxy/Dxy
		itmp = iC;
		iC = iD;
		iD = itmp;
		tmp = Cx;
		Cx = Dx;
		Dx = tmp;
		tmp = Cy;
		Cy = Dy;
		Dy = tmp;
    }
    if (iA > iB) { //swap A,B if iA>iB, involves C/Dxy->1-C/Dxy (??HOPE THIS IS OK)
		itmp = iA;
		iA = iB;
		iB = itmp;
		Cx = 1.0 - Cx;
		Cy = 1.0 - Cy;
		Dx = 1.0 - Dx;
		Dy = 1.0 - Dy;
    }

    writeonecode(codefid, Cx, Cy, Dx, Dy);
    writeonequad(quadfid, iA, iB, iC, iD);

	if (writeqidx)
		insertquad(quadnum, iA, iB, iC, iD);
}

/*
  void getstarcoord(star *sA, sidx iA) {
  if (iA >= maxstar) {
  fprintf(stderr, "iA %lu > maxstar %lu\n", iA, maxstar);
  }
  if (use_mmap) {
  memcpy(sA->farr, catalogue + iA * DIM_STARS, DIM_STARS * sizeof(double));
  } else {
  fseekocat(iA, cposmarker, catfid);
  freadstar(sA, catfid);
  }
  }
*/
