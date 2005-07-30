#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hpn:s:z:f:o:w:x:q:r:d:"
const char HelpString[]=
"genfields -f fname -o fieldname -s scale(arcmin) [-n num_rand_fields | -r RA -d DEC]  [-p] [-w noise] [-x distractors] [-q dropouts]\n";

extern char *optarg;
extern int optind, opterr, optopt;

qidx gen_pix(FILE *listfid,FILE *pix0fid,FILE *pixfid,
	     stararray *thestars,kdtree *kd,
	     double aspect,double noise, double distractors, double dropouts,
	     double ramin,double ramax,double decmin,double decmax,
	     double radscale,qidx numFields);

char *treefname=NULL,*listfname=NULL,*pix0fname=NULL,*pixfname=NULL;
char FlipParity=0;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  qidx numFields=0;
  double radscale=1.0/10.0,aspect=1.0,distractors=0.0,dropouts=0.0,noise=0.0;
  double centre_ra=0.0,centre_dec=0.0;

  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'n':
	numFields = strtoul(optarg,NULL,0);
	break;
      case 'p':
	FlipParity = 1;
	break;
      case 's':
	radscale = (double)strtod(optarg,NULL);
	if(radscale>=1.0) radscale = radscale*(double)PIl/(180.0*60);
	break;
      case 'z':
	aspect = strtod(optarg,NULL);
	break;
      case 'w':
	noise = strtod(optarg,NULL);
	break;
      case 'x':
	distractors = strtod(optarg,NULL);
	break;
      case 'q':
	dropouts = strtod(optarg,NULL);
	break;
      case 'r':
	centre_ra = deg2rad(strtod(optarg,NULL));
	break;
      case 'd':
	centre_dec = deg2rad(strtod(optarg,NULL));
	break;
      case 'f':
	treefname = malloc(strlen(optarg)+6);
	sprintf(treefname,"%s.skdt",optarg);
	break;
      case 'o':
	listfname = malloc(strlen(optarg)+6);
	pix0fname = malloc(strlen(optarg)+6);
	pixfname = malloc(strlen(optarg)+6);
	sprintf(listfname,"%s.ids0",optarg);
	sprintf(pix0fname,"%s.xyl0",optarg);
	sprintf(pixfname,"%s.xyls",optarg);
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
    return(OPT_ERR);
  }

#define RANDSEED 777
  am_srand(RANDSEED);

  FILE *treefid=NULL,*listfid=NULL,*pix0fid=NULL,*pixfid=NULL;
  qidx numtries;
  sidx numstars;
  double ramin,ramax,decmin,decmax;

  if(numFields)
  fprintf(stderr,"genfields: making %lu random fields from %s [RANDSEED=%d]\n",
	  numFields,treefname,RANDSEED);
  else {
  fprintf(stderr,"genfields: making fields from %s around ra=%lf,dec=%lf\n",
	  treefname,centre_ra,centre_dec);
  numFields=1;
  }

  fprintf(stderr,"  Reading star KD tree from ");
  if(treefname) fprintf(stderr,"%s...",treefname);
  else fprintf(stderr,"stdin...");  fflush(stderr);
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

  if(numFields>1)
  fprintf(stderr,"  Generating %lu fields at scale %g arcmin...\n",
	  numFields,180.0*60.0*radscale/(double)PIl);
  fflush(stderr);
  fopenout(listfname,listfid); fnfree(listfname);
  fopenout(pix0fname,pix0fid); fnfree(pix0fname);
  fopenout(pixfname,pixfid); fnfree(pixfname);
  if(numFields>1)
  numtries=gen_pix(listfid,pix0fid,pixfid,thestars,starkd,aspect,
		   noise,distractors,dropouts,
		   ramin,ramax,decmin,decmax,radscale,numFields);
  else
  numtries=gen_pix(listfid,pix0fid,pixfid,thestars,starkd,aspect,
		   noise,distractors,dropouts,
		   centre_ra,centre_ra,centre_dec,centre_dec,
		   radscale,numFields);
  fclose(listfid); fclose(pix0fid); fclose(pixfid);
  if(numFields>1)
  fprintf(stderr,"  made %lu nonempty fields in %lu tries.\n",
	  numFields,numtries);

  free_dyv_array_from_kdtree((dyv_array *)thestars);
  free_kdtree(starkd); 

  //basic_am_malloc_report();
  return(0);
}



qidx gen_pix(FILE *listfid,FILE *pix0fid,FILE *pixfid,
	     stararray *thestars,kdtree *kd,
	     double aspect,double noise, double distractors, double dropouts,
	     double ramin,double ramax,double decmin,double decmax,
	     double radscale,qidx numFields)
{
  sidx jj,numS,numX;
  qidx numtries=0,ii;
  double xx,yy;
  star *randstar;
#if PLANAR_GEOMETRY==1
  double scale=radscale;
#else
  double scale=sqrt(2-2*cos(radscale/2));
#endif
  double pixxmin=0,pixymin=0,pixxmax=0,pixymax=0;
  kquery *kq = mk_kquery("rangesearch","",KD_UNDEF,scale,kd->rmin);
		
  fprintf(pix0fid,"NumFields=%lu\n",numFields);
  fprintf(pixfid,"NumFields=%lu\n",numFields);
  fprintf(listfid,"NumFields=%lu\n",numFields);

  for(ii=0;ii<numFields;ii++) {
    numS=0;
    while(!numS) {
      randstar=make_rand_star(ramin,ramax,decmin,decmax);
      kresult *krez = mk_kresult_from_kquery(kq,kd,randstar);

      numS=krez->count;
      //fprintf(stderr,"random location: %lu within scale.\n",numS);

      if(numS) {
#if DIM_STARS==2
fprintf(pix0fid,"centre %f,%f\n",star_ref(randstar,0),star_ref(randstar,1));
#else
fprintf(pix0fid,"centre %f,%f,%f\n",
star_ref(randstar,0),star_ref(randstar,1),star_ref(randstar,2));
#endif
	numX=floor(numS*distractors);
        fprintf(pixfid,"%lu",numS+numX); 
        fprintf(pix0fid,"%lu",numS); 
	fprintf(listfid,"%lu",numS);
        for(jj=0;jj<numS;jj++) {
	  fprintf(listfid,",%d",krez->pindexes->iarr[jj]);
	  star_coords(thestars->array[(krez->pindexes->iarr[jj])],
	  	    randstar,&xx,&yy);
	  // should add random rotation here ???
	  if(FlipParity) xx=-xx;
	  if(jj==0) {pixxmin=pixxmax=xx; pixymin=pixymax=yy;}
	  if(xx>pixxmax) pixxmax=xx; if(xx<pixxmin) pixxmin=xx;
	  if(yy>pixymax) pixymax=yy; if(yy<pixymin) pixymin=yy;
	  fprintf(pix0fid,",%f,%f",xx,yy);
	  if(noise)
	    fprintf(pixfid,",%f,%f",
		    xx+noise*scale*gen_gauss(),
		    yy+noise*scale*gen_gauss() );
	  else
	    fprintf(pixfid,",%f,%f",xx,yy);
	}
	for(jj=0;jj<numX;jj++)
	  fprintf(pixfid,",%f,%f",
		  range_random(pixxmin,pixxmax),
		  range_random(pixymin,pixymax));
	  
	fprintf(pixfid,"\n"); fprintf(listfid,"\n"); fprintf(pix0fid,"\n");
      }
      free_star(randstar);
      free_kresult(krez);
      numtries++;
    }
  //if(is_power_of_two(ii))
  //fprintf(stderr,"  made %lu pix in %lu tries\r",ii,numtries);fflush(stdout);
  }
  
  free_kquery(kq);

  return numtries;
}
