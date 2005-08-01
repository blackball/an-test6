#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hf:ixrt:k:"
const char HelpString[]=
//"findstar -f fname [-t dist | -k kNN] {-i idx | -x x y z | -r RA DEC}  OR\n"
"findstar -f fname [-t dist | -k kNN] {-i | -x | -r} then read stdin\n";

extern char *optarg;
extern int optind, opterr, optopt;

void output_star(FILE *fid, sidx i, stararray *sa, 
		 char cASCII, off_t catposmarker);

char *treefname=NULL,*catfname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  if(argc<=3) {fprintf(stderr,HelpString); return(OPT_ERR);}

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
	fprintf(stderr,HelpString);
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  if(!xyzset && !radecset && !whichset) {
    fprintf(stderr,HelpString); return(OPT_ERR);}

  FILE *treefid=NULL,*catfid=NULL;
  sidx numstars,whichstar,ii;
  double xx,yy,zz,ra,dec,ramin,ramax,decmin,decmax;
  kquery *kq=NULL;
  kresult *krez=NULL;
  star *thequery=NULL;
  kdtree *starkd=NULL;
  stararray *thestars;
  off_t catposmarker;
  char cASCII=READ_FAIL;


  if(whichset && kset && (K==0)) {
    dimension DimStars;
    fprintf(stderr,"findstar: getting stars from %s\n",catfname);
    fopenin(catfname,catfid); fnfree(catfname);
    cASCII = read_objs_header(catfid,&numstars,&DimStars,
			      &ramin,&ramax,&decmin,&decmax);
    if(cASCII==READ_FAIL) return(1);
    catposmarker=ftello(catfid);
  }
  else {
    fprintf(stderr,"findstar: getting stars from %s\n",treefname);
    fprintf(stderr,"  Reading star KD tree...");  fflush(stderr);
    fopenin(treefname,treefid); fnfree(treefname);
    starkd=read_starkd(treefid,&ramin,&ramax,&decmin,&decmax);
    fclose(treefid);
    if(starkd==NULL) return(2);
    numstars=starkd->root->num_points;
    fprintf(stderr,"done\n    (%lu stars, %d nodes, depth %d).\n",
	    numstars,starkd->num_nodes,starkd->max_depth);
    fprintf(stderr,"    (dim %d) (limits %lf<=ra<=%lf;%lf<=dec<=%lf.)\n",
	    kdtree_num_dims(starkd),ramin,ramax,decmin,decmax);

    thestars = (stararray *)mk_dyv_array_from_kdtree(starkd);

    thequery = mk_star();
  }

  if(!whichset && kset && (K<=0)) K=1;
  if(kset && K>0) {
    if(whichset && kset)
      kq = mk_kquery("knn","",K+1,KD_UNDEF,starkd->rmin);
    else
      kq = mk_kquery("knn","",K,KD_UNDEF,starkd->rmin);
  }
  else if(dtolset)
    kq = mk_kquery("rangesearch","",KD_UNDEF,dtol,starkd->rmin);

  //fprintf(stderr,"optind=%d,argc=%d\n",optind,argc);
  //for (argidx = optind; argidx < argc; argidx++)
  //  fprintf(stderr,"argv[%d]=%s\n",argidx,argv[argidx]);

  if(optind<argc) {
    //for (argidx = optind; argidx < argc; argidx++) {
    //whichstar = strtoul(argv[argidx],NULL,0);
    //if(kset && K==0)
    //output_star(stdout,whichstar);
    //else {
    //}
    //}
  }
  else {
    char scanrez=1;
    while(!feof(stdin) && scanrez) {
      if(whichset) {
	scanrez=fscanf(stdin,"%lu",&whichstar);
	if(scanrez==1) {
	  //fprintf(stderr,"read idx:%lu\n",whichstar);
	  if(kset && K==0) {
	    fprintf(stderr,"  getting star %lu\n",whichstar);
	    output_star(stdout,whichstar,NULL,cASCII,catposmarker);
	  }
	  else {
	    if(kset)
	      fprintf(stderr,"  getting %lu stars nearest %lu\n",K,whichstar);
	    else if(dtolset)
          fprintf(stderr,"  getting stars within %lf of %lu\n",dtol,whichstar);
	    thequery = thestars->array[whichstar];
	    krez =  mk_kresult_from_kquery(kq,starkd,thequery);
	    if(krez!=NULL) {
	      for(ii=0;ii<krez->count;ii++)
		output_star(stdout,krez->pindexes->iarr[ii],thestars,0,0);
	      free_kresult(krez);
	    }
	  }
	}
      }
      else if(radecset) {
	scanrez=fscanf(stdin,"%lf %lf",&ra,&dec);
	if(scanrez==2) {
	  //fprintf(stderr,"read ra=%lf,dec=%lf\n",ra,dec);
	  star_set(thequery,0,radec2x(deg2rad(ra),deg2rad(dec)));
	  star_set(thequery,1,radec2y(deg2rad(ra),deg2rad(dec)));
	  star_set(thequery,2,radec2z(deg2rad(ra),deg2rad(dec)));
	  if(kset)
    fprintf(stderr,"  getting %lu stars closest to ra=%lf,dec=%lf\n",K,ra,dec);
	  else if(dtolset)
  fprintf(stderr,"  getting stars within %lf of ra=%lf,dec=%lf\n",dtol,ra,dec);
	  krez =  mk_kresult_from_kquery(kq,starkd,thequery);
	  if(krez!=NULL) {
	    for(ii=0;ii<krez->count;ii++)
	      output_star(stdout,krez->pindexes->iarr[ii],thestars,0,0);
	    free_kresult(krez);
	  }
	}
      }
      else { //xyzset 
	scanrez=fscanf(stdin,"%lf %lf %lf",&xx,&yy,&zz);
	if(scanrez==3) {
	  //fprintf(stderr,"read x=%lf,y=%lf,z=%lf\n",xx,yy,zz);
	  star_set(thequery,0,xx); 
	  star_set(thequery,1,yy); 
	  star_set(thequery,2,zz);
	  if(kset)
fprintf(stderr,"  getting %lu stars closest to x=%lf,y=%lf,z=%lf\n",K,xx,yy,zz);
	  else if(dtolset)
  fprintf(stderr," stars within %lf of x=%lf,y=%lf,z=%lf\n",dtol,xx,yy,zz);

	  krez =  mk_kresult_from_kquery(kq,starkd,thequery);
	  if(krez!=NULL) {
	    for(ii=0;ii<krez->count;ii++)
	      output_star(stdout,krez->pindexes->iarr[ii],thestars,0,0);
	    free_kresult(krez);
	  }
	}

      }
    }
  }

  if(kq) free_kquery(kq);
  if(thequery) free_star(thequery);
  if(!whichset) {
    free_dyv_array_from_kdtree((dyv_array *)thestars);
    free_kdtree(starkd); 
  }
  if(catfid) fclose(catfid);

  //basic_am_malloc_report();
  return(0);
}


void output_star(FILE *fid, sidx i, stararray *sa, 
		 char cASCII, off_t catposmarker)
{
  if(sa==NULL) {
  }
  else {
  star *s=sa->array[i];
#if DIM_STARS==2
  fprintf(fid,"%lu: %lf,%lf\n",i,star_ref(s,0),star_ref(s,1));
#else
  fprintf(fid,"%lu: %lf,%lf,%lf (%lf,%lf)\n",
	  i,star_ref(s,0),star_ref(s,1),star_ref(s,2),
	  rad2deg(xy2ra(star_ref(s,0),star_ref(s,1))),
	  rad2deg(z2dec(star_ref(s,2))));
#endif
  }
  return;
}


