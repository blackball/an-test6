#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hR:f:"
const char HelpString[]=
"startree -f fname [-R KD_RMIN]\n"
"  KD_RMIN (default 50) is the max# points per leaf in KD tree\n";

char *treefname=NULL;
char *catfname=NULL;


extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;
  int kd_Rmin=DEFAULT_KDRMIN;

  if(argc<=2) {fprintf(stderr,HelpString);return(HELP_ERR);}
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'R':
	kd_Rmin = (int)strtoul(optarg,NULL,0);
	break;
      case 'f':
	treefname = mk_streefn(optarg);
	catfname = mk_catfn(optarg);
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr,HelpString);
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  if(optind<argc) {
    for (argidx = optind; argidx < argc; argidx++)
      fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
    fprintf(stderr,HelpString);
    return(HELP_ERR);
  }

  FILE *catfid=NULL,*treefid=NULL;
  sidx numstars;
  dimension Dim_Stars;
  double ramin,ramax,decmin,decmax;

  fprintf(stderr,"startree: building KD tree for %s\n",catfname);


  fprintf(stderr,"  Reading star catalogue...");fflush(stderr);
  fopenin(catfname,catfid); free_fn(catfname);
  stararray *thestars = readcat(catfid,&numstars,&Dim_Stars,
				&ramin,&ramax,&decmin,&decmax);
  fclose(catfid);
  if(thestars==NULL) return(1);
  fprintf(stderr,"got %lu stars.\n",numstars);
  fprintf(stderr,"    (dim %hu) (limits %f<=ra<=%f;%f<=dec<=%f.)\n",
      Dim_Stars,rad2deg(ramin),rad2deg(ramax),rad2deg(decmin),rad2deg(decmax));

  fprintf(stderr,"  Building star KD tree...");fflush(stderr);
  kdtree *starkd = mk_starkdtree(thestars,kd_Rmin);
  free_stararray(thestars);
  if(starkd==NULL) return(2);
  fprintf(stderr,"done (%d nodes, depth %d).\n",
	  starkd->num_nodes,starkd->max_depth);

  fprintf(stderr,"  Writing star KD tree to %s...",treefname); fflush(stderr);
  fopenout(treefname,treefid); free_fn(treefname);
  write_starkd(treefid,starkd,ramin,ramax,decmin,decmax);
  fprintf(stderr,"done.\n");
  fclose(treefid);

  free_kdtree(starkd);

  //basic_am_malloc_report();
  return(0);

}


