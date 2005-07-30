#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hf:ixrt:k:"
const char HelpString[]=
"findstar -f fname [-t dist | -k kNN] {-i idx | -x x y z | -r RA DEC}  OR\n"
"findstar -f fname [-t dist | -k kNN] {-i | -x | -r} then read stdin\n";

extern char *optarg;
extern int optind, opterr, optopt;

void output_star(FILE *fid, sidx i, stararray *sa);

char *treefname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  if(argc<=5) {fprintf(stderr,HelpString); return(OPT_ERR);}

  char whichset=0,xyzset=0,radecset=0,kset=1,dtolset=0;
  sidx K=0;
  double dtol=0.0;
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'k':
	K = strtoul(optarg,NULL,0);
	kset=1;	dtolset=0;
	break;
      case 't':
	dtol = strtod(optarg,NULL);
	dtolset=1; kset=0;
	break;
      case 'i':
	whichset=1; xyzset=0; radecset=0;
	break;
      case 'x':
	xyzset=1; whichset=0; radecset=0;
	break;
      case 'r':
	radecset=1; whichset=0; xyzset=0;
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

  if(!xyzset && !radecset && !whichset) {
    fprintf(stderr,HelpString); return(OPT_ERR);}

  FILE *treefid=NULL;
  sidx numstars,ii,whichstar;
  double xx,yy,zz,ra,dec,ramin,ramax,decmin,decmax;
  kquery *kq=NULL;
  kresult *krez=NULL;
  star *thequery=NULL;
  kdtree *starkd=NULL;
  stararray *thestars;

  fprintf(stderr,"findstar: getting stars from %s\n",treefname);

  if(whichset && kset && (K==0)) {
    //open_raw_objs;
  }
  else {
    fprintf(stderr,"  Reading star KD tree...");  fflush(stderr);
    fopenin(treefname,treefid); fnfree(treefname);
    starkd=read_starkd(treefid,&ramin,&ramax,&decmin,&decmax);
    fclose(treefid);
    if(starkd==NULL) return(1);
    numstars=starkd->root->num_points;
    fprintf(stderr,"done\n    (%lu stars, %d nodes, depth %d).\n",
	    numstars,starkd->num_nodes,starkd->max_depth);
    fprintf(stderr,"    (dim %d) (limits %f<=ra<=%f;%f<=dec<=%f.)\n",
	    kdtree_num_dims(starkd),ramin,ramax,decmin,decmax);

    thestars = (stararray *)mk_dyv_array_from_kdtree(starkd);

    thequery = mk_star();
  }

  if(kset && K>0) 
    kq = mk_kquery("knn","",K,KD_UNDEF,starkd->rmin);
  else if(dtolset)
    kq = mk_kquery("rangesearch","",KD_UNDEF,dtol,starkd->rmin);

  if(optind<argc) {
    //for (argidx = optind; argidx < argc; argidx++) {
    //whichstar = strtoul(argv[argidx],NULL,0);
    //if(kset && K==0)
    //output_star(stdout,whichstar);
    //else {
    //thequery = thestars->array[whichstar];
    //kresult *krez =  mk_kresult_from_kquery(kq,starkd,thequery);
    //for(ii=0;ii<krez->count;ii++)
    //output_star(stdout,krez->pindexes->iarr[ii],thestars)
    //}
    //}
  }
  else {
    //read from stdin;
    //call outputter
  }


  if(krez) free_kresult(krez);
  if(kq) free_kquery(kq);
  if(thequery) free_star(thequery);
  if(!whichset) {
    free_dyv_array_from_kdtree((dyv_array *)thestars);
    free_kdtree(starkd); 
  }

  //basic_am_malloc_report();
  return(0);
}


void output_star(FILE *fid, sidx i, stararray *sa)
{
  star *s=sa->array[i];
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



/*
  if(whichset) {
  }
  else if(radecset) {
    star_set(thequery,0,radec2x(deg2rad(ra),deg2rad(dec)));
    star_set(thequery,1,radec2y(deg2rad(ra),deg2rad(dec)));
    star_set(thequery,2,radec2z(deg2rad(ra),deg2rad(dec)));
  }
  else // xyzset
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
*/
