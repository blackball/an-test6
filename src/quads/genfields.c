#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include "starutil.h"

#define OPTIONS "hn:s:z:R:f:o:w:x:q:"
extern char *optarg;
extern int optind, opterr, optopt;

#define mk_starkdtree(s,r) mk_kdtree_from_points((dyv_array *)s,r)

qidx gen_pix(FILE *listfid,FILE *pix0fid,FILE *pixfid,
	     stararray *thestars,kdtree *kd,
	     double aspect,double noise, double distractors, double dropouts,
	     double ramin,double ramax,double decmin,double decmax,
	     double radscale,qidx maxPix);

char *catfname=NULL,*listfname=NULL,*pix0fname=NULL,*pixfname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  dimension Dim_Stars;
  qidx maxPix=0;
  int kd_Rmin=50;
  double radscale=1.0/10.0,aspect=1.0,distractors=0.0,dropouts=0.0,noise=0.0;
  double ramin,ramax,decmin,decmax;
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'R':
	kd_Rmin = (int)strtoul(optarg,NULL,0);
	break;
      case 'n':
	maxPix = strtoul(optarg,NULL,0);
	break;
      case 's':
	radscale = (double)strtoul(optarg,NULL,0);
	radscale = 1.0/radscale;
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
      case 'f':
	catfname = malloc(strlen(optarg)+6);
	sprintf(catfname,"%s.objs",optarg);
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
	fprintf(stderr, 
	"genfields  [-n numFields] [-z aspectRatio] [-s 1/scale] [-w noise] [-x distractors] [-q dropouts] [-f fname] [-o fieldname] [-R KD_RMIN]\n");
	return(1);
      default:
	return(2);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);

#define RANDSEED 777
  am_srand(RANDSEED);

  FILE *catfid=NULL,*listfid=NULL,*pix0fid=NULL,*pixfid=NULL;
  qidx numtries;
  sidx numstars;

  fprintf(stderr,"genfields: making %lu random fields from %s [RANDSEED=%d]\n",
	  maxPix,catfname,RANDSEED);

  fprintf(stderr,"  Reading star catalogue...");fflush(stderr);
  fopenin(catfname,catfid); fnfree(catfname);
  stararray *thestars = readcat(catfid,&numstars,&Dim_Stars,
				&ramin,&ramax,&decmin,&decmax);
  fclose(catfid);
  if(thestars==NULL) return(1);
  fprintf(stderr,"got %lu stars (dim %hu).\n",numstars,Dim_Stars);

  fprintf(stderr,"  Building star KD tree...");fflush(stderr);
  kdtree *starkd = mk_starkdtree(thestars,kd_Rmin);
  if(starkd==NULL) return(2);
  fprintf(stderr,"done (%d nodes, depth %d).\n",
	  starkd->num_nodes,starkd->max_depth);

  fprintf(stderr,"  Generating %lu fields at scale %g arcmin...\n",
	  maxPix,180.0*60.0*radscale/(double)PIl);
  fflush(stderr);
  fopenout(listfname,listfid); fnfree(listfname);
  fopenout(pix0fname,pix0fid); fnfree(pix0fname);
  fopenout(pixfname,pixfid); fnfree(pixfname);
  numtries=gen_pix(listfid,pix0fid,pixfid,thestars,starkd,aspect,
		   noise,distractors,dropouts,
		   ramin,ramax,decmin,decmax,radscale,maxPix);
  fclose(listfid); fclose(pix0fid); fclose(pixfid);
  fprintf(stderr,"  made %lu nonempty fields in %lu tries.\n",
	  maxPix,numtries);

  free_stararray(thestars); 
  free_kdtree(starkd); 

  //basic_am_malloc_report();
  return(0);
}



qidx gen_pix(FILE *listfid,FILE *pix0fid,FILE *pixfid,
	     stararray *thestars,kdtree *kd,
	     double aspect,double noise, double distractors, double dropouts,
	     double ramin,double ramax,double decmin,double decmax,
	     double radscale,qidx maxPix)
{
  sidx jj,numS,numX;
  qidx numtries=0,ii;
  double xx,yy;
  star *randstar;
  double scale=sqrt(2-2*cos(radscale/2));
  double pixxmin=0,pixymin=0,pixxmax=0,pixymax=0;
  kquery *kq = mk_kquery("rangesearch","",KD_UNDEF,scale,kd->rmin);
		
  fprintf(pix0fid,"NumFields=%lu\n",maxPix);
  fprintf(pixfid,"NumFields=%lu\n",maxPix);
  fprintf(listfid,"NumFields=%lu\n",maxPix);

  for(ii=0;ii<maxPix;ii++) {
    numS=0;
    while(!numS) {
      randstar=make_rand_star(ramin,ramax,decmin,decmax);
      kresult *krez = mk_kresult_from_kquery(kq,kd,randstar);

      numS=krez->count;
      //fprintf(stderr,"random location: %lu within scale.\n",numS);

      if(numS) {
fprintf(pix0fid,"centre %f,%f,%f\n",
star_ref(randstar,0),star_ref(randstar,1),star_ref(randstar,2));
	numX=floor(numS*distractors);
        fprintf(pixfid,"%lu",numS+numX); 
        fprintf(pix0fid,"%lu",numS); 
	fprintf(listfid,"%lu",numS);
        for(jj=0;jj<numS;jj++) {
	  fprintf(listfid,",%d",krez->pindexes->iarr[jj]);
	  star_coords(thestars->array[(krez->pindexes->iarr[jj])],
	  	    randstar,&xx,&yy);
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
