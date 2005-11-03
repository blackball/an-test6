/**
  \file A program to fix the headers of catalogue files.

  It seems the NYU people just set the RA,DEC limits to be some constant value
  that isn't a tight bound on the actual stars in the catalogue - eg,
  [0, 360], [-90, 90].  This program reads a catalogue, finds the range of actual
  RA,DEC values, and writes the contents to a new file.  The body of the file is
  unchanged.

  If you use the same filename for input and output, it will fix the header in-place,
  which is much faster than copying the file, but of course has the potential to mess
  up your file.
*/
#include <byteswap.h>
#include <errno.h>
#include "starutil.h"
#include "fileutil.h"

#define OPTIONS "hf:o:b"

extern char *optarg;
extern int optind, opterr, optopt;

char *outfname = NULL, *catfname = NULL;
FILE *outfid = NULL, *catfid = NULL;
char cASCII = (char)READ_FAIL;
char buff[100], oneobjWidth;
bool byteswap;
off_t catposmarker = 0;

/*
  struct obj_header {
  magicval magic;
  sidx numstars;
  dimension dimstars;
  double ramin;
  double ramax;
  double decmin;
  double decmax;
  };
  typedef struct obj_header obj_header;

  #define swap_magic bswap_16
  #define swap_numstars bswap_32
  #define swap_sidx bswap_32
  #define swap_dimension bswap_16
*/
/**
   Caution, this changes the header in-place!
   
   Calling this twice should give you back the original header.
*/
/*
  void byteswap_header(obj_header* header) {
  header->magic = swap_magic(header->magic);
  header->numstars = swap_sidx(header->numstars);
  header->dimstars = swap_dimension(header->dimstars);
  }

  void read_header(FILE* fid, obj_header* header, bool swap) {
  magicval magic;
  fread(&magic, sizeof(magic), 1, fid);
  if (swap) magic = swap_magic(magic);
  if (magic != MAGIC_VAL) {
  fprintf(stderr, "ERROR (read_objs_header) -- bad magic value\n");
  return (READ_FAIL);
  }
  fread(numstars, sizeof(*numstars), 1, fid);
  fread(DimStars, sizeof(*DimStars), 1, fid);
  fread(ramin, sizeof(*ramin), 1, fid);
  fread(ramax, sizeof(*ramax), 1, fid);
  fread(decmin, sizeof(*decmin), 1, fid);
  fread(decmax, sizeof(*decmax), 1, fid);
  }
  if (swap) byteswap_header(header);
  return (ASCII);
  }
*/

void get_star_radec(FILE *fid, off_t marker, int objsize,
					sidx i, double* pra, double* pdec);
void get_star(FILE *fid, off_t marker, int objsize,
			  sidx i, star* s);

int main(int argc, char *argv[])
{
  dimension DimStars;
  int argchar; //  opterr = 0;
  double ramin, ramax, decmin, decmax;
  sidx numstars;
  int i;
  star* mystar;
  bool inplace;
  //obj_header header;
  //bool byteswap = false;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'f':
            catfname = mk_catfn(optarg);
            break;
        case 'o':
            outfname = mk_catfn(optarg);
            break;
        case 'b':
		  //byteswap = true;
            break;
        default:
            return (OPT_ERR);
        }
    
    fprintf(stderr, "fixheaders: reading catalogue  %s\n", catfname);
    fprintf(stderr, "            writing results to %s\n", outfname);
	if ((catfname == NULL) || (outfname == NULL)) {
	  fprintf(stderr, "\nYou must specify input and output file (-f <input> -o <output>), without the .objs suffix)\n\n");
	}
	inplace = (strcmp(catfname, outfname) == 0);

	if (inplace) {
	  catfid = fopen(catfname, "rw");
	  if (!catfid) {
		printf("Couldn't open catalogue file %s\n",
			   catfname);
		exit(-1);
	  }
	  outfid = catfid;
	} else {
	  fopenin(catfname, catfid);
	  fopenout(outfname, outfid);
	}
	free_fn(outfname);
    free_fn(catfname);

    cASCII = read_objs_header(catfid, &numstars, &DimStars,
                              &ramin, &ramax, &decmin, &decmax);
    //cASCII = read_header(catfid, &header, byteswap);

    if (cASCII == (char)READ_FAIL)
        return (1);

    fprintf(stderr, "    (%lu stars) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
            numstars, rad2deg(ramin), rad2deg(ramax), rad2deg(decmin), rad2deg(decmax));
    catposmarker = ftello(catfid);

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

	printf("\nWriting output...\n");
	if (inplace) {
	  fseek(outfid, 0, SEEK_SET);
	}
	write_objs_header(outfid, numstars, DimStars,
					  ramin, ramax, decmin, decmax);

	if (!inplace) {
	  // write the contents...
	  if (!cASCII) {
		char* block;
		int blocksize = 10000;
		int starsize = (DIM_STARS * sizeof(double));
		int nblocks = numstars/blocksize;
		int nleft;
		block = malloc(starsize * blocksize);
		fseek(catfid, catposmarker, SEEK_SET);
		for (i=0; i<nblocks; i++) {
		  fread(block, 1, starsize * blocksize, catfid);
		  fwrite(block, 1, starsize * blocksize, outfid);
		  if (i % 10 == 0) {
			printf(".");
			fflush(stdout);
		  }
		}
		nleft = numstars % blocksize;
		fread (block, 1, starsize * nleft, catfid);
		fwrite(block, 1, starsize * nleft, outfid);
		printf("\n");
	  } else {
		// one star at a time...
		mystar = mk_star();
		for (i=0; i<numstars; i++) {
		  get_star(catfid, catposmarker, oneobjWidth, i, mystar);
		  fwritestar(mystar, outfid);
		  if (i % 100000 == 0) {
			printf("."); fflush(stdout);
		  }
		}
		printf("\n");
		free_star(mystar);
	  }
	}

	if (fclose(outfid)) {
	  printf("Error closing output file.\n");
	  return 1;
	}

	if (!inplace) {
	  fclose(catfid);
	}

    return 0;
}

void get_star(FILE *fid, off_t marker, int objsize,
			  sidx i, star* s) {
  fseekocat(i, marker, fid);
  freadstar(s, fid);
}


/**
   Returns the RA,DEC (in radians) of the star with index 'i'.
 */
void get_star_radec(FILE *fid, off_t marker, int objsize,
			  sidx i, double* pra, double* pdec) {
	star *tmps = NULL;
	double tmpx, tmpy, tmpz;
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


