#include <unistd.h>
#include <stdio.h>
#include "starutil.h"

#define OPTIONS "habn:f:r:R:d:D:"
extern char *optarg;
extern int optind, opterr, optopt;
void output_star(star *thestar, char ASCII, FILE *fid);


int main(int argc,char *argv[])
{
  int argidx,argchar; //  opterr = 0;

  sidx numstars=10;
  char ASCII = 1;
  char *fname=NULL;
#if PLANAR_GEOMETRY==1
  double ramin=0.0,ramax=1.0,decmin=0.0,decmax=1.0;
#else
  double ramin=0.0,ramax=2*PIl,decmin=-PIl/2.0,decmax=PIl/2.0;
#endif
  
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'a':
	ASCII = 1;
	break;
      case 'b':
	ASCII = 0;
	break;
      case 'n':
	numstars = strtoul(optarg,NULL,0);
	break;
      case 'r':
	ramin = strtod(optarg,NULL);
	break;
      case 'R':
	ramax = strtod(optarg,NULL);
	break;
      case 'd':
	decmin = strtod(optarg,NULL);
	break;
      case 'D':
	decmax = strtod(optarg,NULL);
	break;
      case 'f':
	fname = malloc(strlen(optarg)+6);
	sprintf(fname,"%s.objs",optarg);
	break;
      case '?':
	fprintf (stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr, 
	"randcat [-a|-b] [-n numstars] [-f catfile] [-r/R RAmin/max] [-d/R DECmin/max]\n");
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);

#define RANDSEED 999
  am_srand(RANDSEED);

  fprintf(stderr, "randcat: Making %lu random stars",numstars);
#if PLANAR_GEOMETRY==1
  fprintf(stderr," in the unit square ");
  fprintf(stderr,"[RANDSEED=%d]\n",RANDSEED);
  if(ramin>0.0 || ramax< 1.0 || decmin>0.0 || decmax<1.0)
    fprintf(stderr,"  using limits %f<=x<=%f ; %f<=y<=%f.\n",
	    ramin,ramax,decmin,decmax);
#else
  fprintf(stderr," on the unit sphere ");
  fprintf(stderr,"[RANDSEED=%d]\n",RANDSEED);
  if(ramin>0.0 || ramax< 2*PIl || decmin>-PIl/2.0 || decmax<PIl/2.0)
    fprintf(stderr,"  using limits %f<=RA<=%f ; %f<=DEC<=%f radians.\n",
	    ramin,ramax,decmin,decmax);
#endif

  sidx ii;
  star *thestar;
  FILE *fid=NULL;

  fopenout(fname,fid); fnfree(fname);

  write_objs_header(fid,ASCII,numstars,DIM_STARS,ramin,ramax,decmin,decmax);
  
  for(ii=0;ii<numstars;ii++) {
    thestar = make_rand_star(ramin,ramax,decmin,decmax);
    if(thestar==NULL) {
      fprintf(stderr,"ERROR (randcat) -- out of memory at star %lu\n",ii);
      return(1);
    }
    else {
      output_star(thestar,ASCII,fid);
      free_star(thestar);
    }
  }
  fclose(fid);

  return(0);

}



void output_star(star *thestar, char ASCII, FILE *fid)
{
  if(ASCII)
#if DIM_STARS==2
    fprintf(fid,"%lf,%lf\n",
	    star_ref(thestar,0),star_ref(thestar,1));
#elif DIM_STARS==3
    fprintf(fid,"%lf,%lf,%lf\n",
	    star_ref(thestar,0),star_ref(thestar,1),star_ref(thestar,2));
#endif
  else if(!ASCII)
    fwrite((double *)thestar->farr,sizeof(double),DIM_STARS,fid);
}
