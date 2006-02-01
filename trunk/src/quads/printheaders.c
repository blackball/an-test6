/**
   Prints the headers of a catalogue (.objs file)

   Input: .objs
*/

#include <errno.h>
#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:m"

extern char *optarg;
extern int optind, opterr, optopt;

char *catfname = NULL;
FILE *catfid = NULL;
off_t catposmarker = 0;

void get_star_radec(FILE *fid, off_t marker, 
					sidx i, double* pra, double* pdec);
/*
  void get_star(FILE *fid, off_t marker, int objsize,
  sidx i, star* s);
*/

void print_help(char* progname) {
    printf("\nUsage: %s -f <input-file> [-m]\n\n"
		   "  <input-file> should be "
		   "catalogue files, and should be specified without "
		   "its .objs suffix.\n"
		   "  -m: print Matlab literals describing the RA,DEC of each object.\n\n",
		   progname);
}

int main(int argc, char *argv[]) {
	dimension DimStars;
	int argchar;
	double ramin, ramax, decmin, decmax;
	sidx numstars;
	char readStatus;
	bool printout = FALSE;

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'm':
			printout = TRUE;
			break;
        case 'f':
            catfname = mk_catfn(optarg);
            break;
		case 'h':
			print_help(argv[0]);
			exit(0);
        default:
            return (OPT_ERR);
        }
    
	if (!catfname) {
		print_help(argv[0]);
		exit(-1);
    }

	fprintf(stderr, "printheaders: reading catalogue  %s\n", catfname);

	fopenin(catfname, catfid);
    free_fn(catfname);

    readStatus = read_objs_header(catfid, &numstars, &DimStars,
								  &ramin, &ramax, &decmin, &decmax);
    if (readStatus == READ_FAIL)
        return (1);

    fprintf(stderr, "    (%lu stars) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
            numstars, rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));

	if (printout) {
		int i;

		catposmarker = ftello(catfid);
		printf("radec=zeros([%i,2]);\n", (int)numstars);
		// read a star at a time...
		for (i=0; i<numstars; i++) {
			double ra, dec;
			get_star_radec(catfid, catposmarker, i, &ra, &dec);
			printf("radec(%i,:)=[%g,%g];\n", i+1, ra, dec);
		}
		printf("ra=radec(:,1);\n");
		printf("dec=radec(:,2);\n");
	}
 
	/*

	  ramin = 1e100;
	  ramax = -1e100;
	  decmin = 1e100;
	  decmax = -1e100;
	  // read a star at a time...
	  for (i=0; i<numstars; i++) {
	  double ra, dec;
	  get_star_radec(catfid, catposmarker, oneobjWidth, i, &ra, &dec);
	  if (ra > ramax) ramax = ra;
	  if (ra < ramin) ramin = ra;
	  if (dec > decmax) decmax = dec;
	  if (dec < decmin) decmin = dec;
	  //printf("%10i %20g %20g\n", i, ra, dec);
	  if (i % 100000 == 0) {
	  printf(".");
	  fflush(stdout);
	  }
	  }
	  printf("\n");

	  printf("RA  range %g, %g\n", rad2deg(ramin),  rad2deg(ramax));
	  printf("Dec range %g, %g\n", rad2deg(decmin), rad2deg(decmax));
	*/
	fclose(catfid);

    return 0;
}

/*
  void get_star(FILE *fid, off_t marker, int objsize,
  sidx i, star* s) {
  fseekocat(i, marker, fid);
  freadstar(s, fid);
  }
*/

/**
   Returns the RA,DEC (in radians) of the star with index 'i'.
*/
void get_star_radec(FILE *fid, off_t marker, 
					sidx i, double* pra, double* pdec) {
	star *tmps = NULL;
	double x, y, z, ra, dec;

	tmps = mk_star();
	if (tmps == NULL)
		return ;
	fseekocat(i, marker, fid);
	freadstar(tmps, fid);
	
	x = star_ref(tmps, 0);
	y = star_ref(tmps, 1);
	z = star_ref(tmps, 2);
	ra =  xy2ra(x, y);
	dec = z2dec(z);
	if (pra) *pra = ra;
	if (pdec) *pdec = dec;

	free_star(tmps);
}
