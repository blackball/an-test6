#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hf:ix:y:z:r:d:t:k:"
const char HelpString[]=
"findstar -f fname [-i idx | -x x -y y -z z | -r RA -d DEC] [-t dist | -k kNN]\n";


extern char *optarg;
extern int optind, opterr, optopt;

void output_star(FILE *fid, sidx i, star *s);

char *treefname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  if(argc<=5) {fprintf(stderr,HelpString); return(OPT_ERR);}

  char whichset=0,xyzset=0,radecset=0,kset=0,dtolset=0;
  sidx whichstar=0;
  sidx K=1;
  double dtol=0.0,xx=0.0,yy=0.0,zz=0.0,ra=-10.0,dec=-10.0;
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'i':
	whichset=1;
	break;
      case 'k':
	K = strtoul(optarg,NULL,0);
	kset=1;
	break;
      case 't':
	dtol = strtod(optarg,NULL);
	dtolset=1;
	break;
      case 'x':
	xx = strtod(optarg,NULL);
	xyzset=1;
	break;
      case 'y':
	yy = strtod(optarg,NULL);
	xyzset=1;
	break;
      case 'z':
	zz = strtod(optarg,NULL);
	xyzset=1;
	break;
      case 'r':
	ra = strtod(optarg,NULL);
	radecset=1;
	break;
      case 'd':
	dec = strtod(optarg,NULL);
	radecset=1;
	break;
      case 'f':
	treefname = malloc(strlen(optarg)+6);
	sprintf(treefname,"%s.skdt",optarg);
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr,HelpString);
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  FILE *treefid=NULL;
  sidx numstars,ii;
  double ramin,ramax,decmin,decmax;

  fprintf(stderr,"findstar: getting stars from %s\n",treefname);
  fprintf(stderr,"  Reading star KD tree...");  fflush(stderr);
  fopenin(treefname,treefid); fnfree(treefname);
  kdtree *starkd = fread_kdtree(treefid);
  fread(&ramin,sizeof(double),1,treefid);
  fread(&ramax,sizeof(double),1,treefid);
  fread(&decmin,sizeof(double),1,treefid);
  fread(&decmax,sizeof(double),1,treefid);
  fclose(treefid);
  if(starkd==NULL) return(1);
  numstars=starkd->root->num_points;
  fprintf(stderr,"done\n    (%lu stars, %d nodes, depth %d).\n",
	  numstars,starkd->num_nodes,starkd->max_depth);
  fprintf(stderr,"    (dim %d) (limits %f<=ra<=%f;%f<=dec<=%f.)\n",
	  kdtree_num_dims(starkd),ramin,ramax,decmin,decmax);

  stararray *thestars = (stararray *)mk_dyv_array_from_kdtree(starkd);

  kquery *kq=NULL;
  kresult *krez=NULL;
  star *thequery=NULL;

  if(kset) 
    kq = mk_kquery("knn","",K,KD_UNDEF,starkd->rmin);
  else if(dtolset) 
    kq = mk_kquery("rangesearch","",KD_UNDEF,dtol,starkd->rmin);

  thequery = mk_star();
  if(!whichset && radecset) {
    star_set(thequery,0,radec2x(deg2rad(ra),deg2rad(dec)));
    star_set(thequery,1,radec2y(deg2rad(ra),deg2rad(dec)));
    star_set(thequery,2,radec2z(deg2rad(ra),deg2rad(dec)));
  }
  else if(xyzset) {
    star_set(thequery,0,xx); 
    star_set(thequery,1,yy); 
    star_set(thequery,2,zz);
  }

  if(whichset && !dtolset && !kset && argc<2)
    fprintf(stderr,"  getting star %lu\n",whichstar);
  else if(whichset && kset)
    fprintf(stderr,"  getting %lu stars nearest %lu\n",K,whichstar);
  else if(whichset && dtolset)
    fprintf(stderr,"  getting stars within %f of %lu\n",dtol,whichstar);
  else if(radecset && kset)
    fprintf(stderr,"  getting %lu stars closest to ra=%f,dec=%f\n",K,ra,dec);
  else if(radecset && dtolset)
    fprintf(stderr,"  getting stars within %f of ra=%f,dec=%f\n",dtol,ra,dec);
  else if(xyzset && kset)
  fprintf(stderr,"  getting %lu stars closest to x=%f,y=%f,z=%f\n",K,xx,yy,zz);
  else if(xyzset && dtolset)
    fprintf(stderr," stars within %f of x=%f,y=%f,z=%f\n",dtol,xx,yy,zz);
  else
    fprintf(stderr," --- error --- ");

  if(whichset && !dtolset && !kset) {
    for (argidx = optind; argidx < argc; argidx++) {
      whichstar = strtoul(argv[argidx],NULL,0);
      output_star(stdout,whichstar,thestars->array[whichstar]);
    }
  }
  else {
    whichstar = strtoul(argv[optind],NULL,0);
    thequery = thestars->array[whichstar];
    kresult *krez =  mk_kresult_from_kquery(kq,starkd,thequery);
    for(ii=0;ii<krez->count;ii++)
      output_star(stdout,krez->pindexes->iarr[ii],
		  thestars->array[(krez->pindexes->iarr[ii])]);
  }
  if(krez) free_kresult(krez);
  if(kq) free_kquery(kq);
  if(thequery && (radecset || xyzset)) free_star(thequery);

  free_dyv_array_from_kdtree((dyv_array *)thestars);
  free_kdtree(starkd); 

  //basic_am_malloc_report();
  return(0);
}


void output_star(FILE *fid, sidx i, star *s)
{
#if DIM_STARS==2
  fprintf(fid,"%lu: %f,%f\n",i,star_ref(s,0),star_ref(s,1));
#else
  fprintf(fid,"%lu: %f,%f,%f (%f,%f)\n",
	  i,star_ref(s,0),star_ref(s,1),star_ref(s,2),
	  rad2deg(xy2ra(star_ref(s,0),star_ref(s,1))),
	  rad2deg(z2dec(star_ref(s,2))));
#endif
  return;
}
