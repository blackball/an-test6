#include <unistd.h>
#include <stdio.h>
#include "starutil.h"
#include "kdutil.h"

#define OPTIONS "hR:f:"
#define MEM_LOAD 1000000000
extern char *optarg;
extern int optind, opterr, optopt;

#define mk_starkdtree(s,r) mk_kdtree_from_points((dyv_array *)s,r)

char *treefname=NULL;
char *catfname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;
  int kd_Rmin=50;
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'R':
	kd_Rmin = (int)strtoul(optarg,NULL,0);
	break;
      case 'f':
	treefname = malloc(strlen(optarg)+6);
	catfname = malloc(strlen(optarg)+6);
	sprintf(treefname,"%s.skdt",optarg);
	sprintf(catfname,"%s.objs",optarg);
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr, 
	"startree [-f fname] [-R KD_RMIN]\n");
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);

  FILE *catfid=NULL,*treefid=NULL;
  sidx numstars;
  dimension Dim_Stars;
  double ramin,ramax,decmin,decmax;

  fprintf(stderr,"startree: building KD tree for %s\n",catfname);


  fprintf(stderr,"  Reading star catalogue...");fflush(stderr);
  fopenin(catfname,catfid); fnfree(catfname);
  stararray *thestars = readcat(catfid,&numstars,&Dim_Stars,
				&ramin,&ramax,&decmin,&decmax);
  fclose(catfid);
  if(thestars==NULL) return(1);
  fprintf(stderr,"got %lu stars.\n",numstars);
  fprintf(stderr,"    (dim %hu) (limits %f<=ra<=%f;%f<=dec<=%f.)\n",
	  Dim_Stars,ramin,ramax,decmin,decmax);

  fprintf(stderr,"  Building star KD tree...");fflush(stderr);
  kdtree *starkd = mk_starkdtree(thestars,kd_Rmin);
  free_stararray(thestars);
  if(starkd==NULL) return(2);
  fprintf(stderr,"done (%d nodes, depth %d).\n",
	  starkd->num_nodes,starkd->max_depth);

  fprintf(stderr,"  Writing star KD tree to ");
  if(treefname) fprintf(stderr,"%s...",treefname); 
  else fprintf(stderr,"stdout..."); 
  fflush(stderr);
  fopenout(treefname,treefid); fnfree(treefname);
  fwrite_kdtree(starkd,treefid);
  fwrite(&ramin,sizeof(double),1,treefid);
  fwrite(&ramax,sizeof(double),1,treefid);
  fwrite(&decmin,sizeof(double),1,treefid);
  fwrite(&decmax,sizeof(double),1,treefid);
  fprintf(stderr,"done.\n");
  fclose(treefid);

  free_kdtree(starkd);

  //basic_am_malloc_report();
  return(0);

}


