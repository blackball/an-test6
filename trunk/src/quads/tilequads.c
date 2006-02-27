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

int main(int argc, char *argv[]) {
    char* progname;
    int argchar; //  opterr = 0;
    double ramin, ramax, decmin, decmax;

    // max radius of the search (in radians)
    double radius;

    // lower bound of quad scale (fraction of radius)
    double lower = 0.0;

    char *catfname  = NULL;
    char *quadfname = NULL;
    char *codefname = NULL;
    char *qidxfname = NULL;

    FILE *qidxfid = NULL;
	char readStatus;
	dimension Dim_Stars;
	off_t endoffset;
    int hdrlength = 0;

	int nquads = 0;
	time_t starttime, endtime;

	double* starxyz;

	// how many stars per healpix?
	int PASSES = 2;

	// how many healpixes?
	int Nside = 512;
	int HEALPIXES = 12*Nside*Nside;

	// the star ids in each healpix.  size HEALPIXES * NSTARS.
	// healpix i, pass j is in starids[j*HEALPIXES + i].
	int* starids;

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
		if (use_mmap) {
			starxyz = catalogue + i * DIM_STARS;
		} else {
			fseekocat(i, cposmarker, catfid);
			

			//#define freadstar(s,f) fread(s->farr,sizeof(double),DIM_STARS,f)

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
