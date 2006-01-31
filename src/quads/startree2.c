/**
   \file  A replacement for \c startree using Keir's new
   kdtree.

   Input: star catalogue file (.objs)
   Output: star kd-tree v2 (.skdt2)
*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "kdtree/kdtree.h"
#include "kdtree/kdtree_io.h"

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hR:f:k:d:"
const char HelpString[] =
"startree2 -f fname [-R KD_RMIN] [-k keep] [-d radius]\n"
"  KD_RMIN: (default 50) is the max# points per leaf in KD tree\n"
"  keep: is the number of stars read from the catalogue\n"
"  radius: is the de-duplication radius: a star found within this radius "
"of another star will be discarded\n";

char *treefname = NULL;
char *catfname = NULL;

extern char *optarg;
extern int optind, opterr, optopt;

/**
   Reads a catalogue from the given FILE*.  The number of stars is placed in
   *numstars, the dimension of the stars is placed in *Dim_Stars, and the
   {ra,dec}{min,max} values are placed in the respective values.

   If \c nkeep is non-zero, it is an upper limit on the number of stars that
   will be read.  If non-null, the number of stars actually read will be placed
   in \c nread.

   If successful, a new array of doubles will be allocated and returned.
   Otherwise, NULL is returned.
*/
double *readcat2(FILE *fid, int *numstars, int *Dim_Stars,
		 double *ramin, double *ramax, double *decmin, double *decmax,
		 int nkeep, int* nread);

int main(int argc, char *argv[])
{
    int argidx, argchar; //  opterr = 0;
    int kd_Rmin = 50;
    FILE *catfid = NULL, *treefid = NULL;
    int numstars;
    int Dim_Stars;
    double ramin, ramax, decmin, decmax;
    int nkeep = 0;
    double duprad;
    double* stararray = NULL;
    kdtree_t *starkd = NULL;
    int levels;

    if (argc <= 2) {
	fprintf(stderr, HelpString);
	return 0;
    }

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
	switch (argchar) {
	case 'R':
	    kd_Rmin = (int)strtoul(optarg, NULL, 0);
	    break;
	case 'k':
	    nkeep = atoi(optarg);
	    if (nkeep == 0) {
		printf("Couldn't parse \'keep\': \"%s\"\n", optarg);
		exit(-1);
	    }
	    break;
	case 'd':
	    duprad = atof(optarg);
	    if (duprad < 0.0) {
		printf("Couldn't parse \'radius\': \"%s\"\n", optarg);
		exit(-1);
	    }
	    break;
	case 'f':
	    treefname = mk_stree2fn(optarg);
	    catfname = mk_catfn(optarg);
	    break;
	case '?':
	    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
	case 'h':
	    fprintf(stderr, HelpString);
	    return 0;
	default:
	    return -1;
	}

    if (optind < argc) {
	for (argidx = optind; argidx < argc; argidx++)
	    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
	fprintf(stderr, HelpString);
	return 0;
    }

    fprintf(stderr, "%s: building KD tree for %s\n", argv[0], catfname);

    if (nkeep) {
	fprintf(stderr, "keeping at most %i stars.\n", nkeep);
    }

    fprintf(stderr, "  Reading star catalogue...");
    fflush(stderr);
    fopenin(catfname, catfid);
    free_fn(catfname);

    stararray = readcat2(catfid, &numstars, &Dim_Stars,
			 &ramin, &ramax, &decmin, &decmax,
			 nkeep, NULL);
    fclose(catfid);
    if (!stararray) {
	fprintf(stderr, "Couldn't read catalogue.\n");
	exit(-1);
    }
    fprintf(stderr, "got %i stars.\n", numstars);
    /*
      fprintf(stderr, "    (dim %hu) (limits %f<=ra<=%f;%f<=dec<=%f.)\n",
      Dim_Stars, rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));
    */

    fprintf(stderr, "  Building star KD tree...");
    fflush(stderr);

    levels = (int)((log((double)numstars) - log((double)kd_Rmin))/log(2.0));
    if (levels < 1) {
	levels = 1;
    }
    fprintf(stderr, "Requesting %i levels.\n", levels);
    starkd = kdtree_build(stararray, numstars, Dim_Stars, levels);

    if (!starkd) {
	free(stararray);
	fprintf(stderr, "Couldn't build kdtree.\n");
	exit(-1);
    }
    fprintf(stderr, "done (%d nodes)\n", starkd->nnodes);

    fprintf(stderr, "  Writing star KD tree to %s...", treefname);
    fflush(stderr);

    fopenout(treefname, treefid);
    
    free_fn(treefname);

    if (kdtree_write(treefid, starkd)) {
	fclose(treefid);
	kdtree_free(starkd);
	free(stararray);
	fprintf(stderr, "Couldn't write star kdtree.\n");
	exit(-1);
    }

    fprintf(stderr, "done.\n");
    fclose(treefid);

    /*
      A bit of debugging to make sure reads and writes work...
      {
      FILE* fid = fopen("/tmp/tree1.dot", "wb");
      if (!fid) {
      printf("Couldn't write tree1");
      exit(-1);
      }
      kdtree_output_dot(fid, starkd);
      fclose(fid);
      }
    */

    kdtree_free(starkd);
    free(stararray);

    /*
      Debugging.
      {
      FILE* fid = fopen("/tmp/tree2.dot", "wb");
      FILE* fin = fopen(treefname, "rb");
      if (!fid) {
      printf("Couldn't write tree2");
      exit(-1);
      }
      if (!fin) {
      printf("Couldn't read tree");
      exit(-1);
      }
      starkd = kdtree_read(fin, 1, NULL, NULL);
      //starkd = kdtree_read(fin, 0);
      if (!starkd) {
      printf("Couldn't kdtree_read.\n");
      exit(-1);
      }
      fclose(fin);
      kdtree_output_dot(fid, starkd);
      fclose(fid);
      kdtree_free(starkd);
      }
    */

    return 0;
}


double *readcat2(FILE *fid, int *numstars, int *Dim_Stars,
		 double *ramin, double *ramax, double *decmin, double *decmax,
		 int nkeep, int* nread) {
    double* array;
    int nelems;
    char readStatus;
    sidx NSTARS;
    dimension DIMSTARS;

    readStatus = read_objs_header(fid, //numstars, Dim_Stars,
				  &NSTARS, &DIMSTARS,
				  ramin, ramax, decmin, decmax);
    if (readStatus == READ_FAIL)
	return NULL;

    *numstars = (int)NSTARS;
    *Dim_Stars = (int)DIMSTARS;

    if (nkeep && (nkeep < *numstars)) {
	// switcheroo!
	*numstars = nkeep;
    }

    nelems = (*numstars) * (*Dim_Stars);
    array = (double*)malloc(sizeof(double) * nelems);
    if (!array) {
	fprintf(stderr, "Couldn't allocate an array of %i doubles.\n", nelems);
	return NULL;
    }

    if (fread(array, sizeof(double), nelems, fid) != nelems) {
	fprintf(stderr, "Error reading from catalogue: %s\n", strerror(errno));
	return NULL;
    }

    if (nread)
	*nread = *numstars;

    return array;
}


