#include <unistd.h>
#include <stdio.h>
#include "starutil.h"

#define OPTIONS "hs:q:R:f:"
extern char *optarg;
extern int optind, opterr, optopt;

#define mk_starkdtree(s,r) mk_kdtree_from_points((dyv_array *)s,r)

qidx get_quads(FILE *quadfid,FILE *codefid,
	       stararray *thestars,kdtree *kd,double index_scale,
	       double ramin,double ramax,double decmin, double decmax,
	       qidx maxCodes);

char *catfname=NULL;
char *quadfname=NULL;
char *codefname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;
  dimension Dim_Stars;
  qidx maxQuads=0;
  int kd_Rmin=50;
  double index_scale=1.0/10.0;
  double ramin,ramax,decmin,decmax;
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'R':
	kd_Rmin = (int)strtoul(optarg,NULL,0);
	break;
      case 's':
	index_scale = (double)strtoul(optarg,NULL,0);
	index_scale = 1.0/index_scale;
	break;
      case 'q':
	maxQuads = strtoul(optarg,NULL,0);
	break;
      case 'f':
	catfname = malloc(strlen(optarg)+6);
	quadfname = malloc(strlen(optarg)+6);
	codefname = malloc(strlen(optarg)+6);
	sprintf(catfname,"%s.objs",optarg);
	sprintf(quadfname,"%s.quad",optarg);
	sprintf(codefname,"%s.code",optarg);
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr, 
	"getquads [-s 1/scale] [-q maxQuads] [-f fname] [-R KD_RMIN]\n");
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);

#define RANDSEED 888
  am_srand(RANDSEED); 

  FILE *catfid=NULL,*quadfid=NULL,*codefid=NULL;
  sidx numstars;
  qidx numtries;

  fprintf(stderr,"getquads: finding %lu quads in %s [RANDSEED=%d]\n",
	  maxQuads,catfname,RANDSEED);

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
  if(starkd==NULL) return(2);
  fprintf(stderr,"done (%d nodes, depth %d).\n",
	  starkd->num_nodes,starkd->max_depth);

  fprintf(stderr,"  Finding %lu quads at scale %g arcmin...\n",
	  maxQuads,180.0*60.0*index_scale/(double)PIl); fflush(stderr);
  fopenout(quadfname,quadfid); fnfree(quadfname);
  fopenout(codefname,codefid); fnfree(codefname);
  numtries=get_quads(quadfid,codefid,thestars,starkd,index_scale,
		     ramin,ramax,decmin,decmax,maxQuads);
  fclose(quadfid); fclose(codefid);
  fprintf(stderr,"  got %lu codes in %lu tries.\n", maxQuads,numtries);

  free_stararray(thestars); 
  free_kdtree(starkd); 

  /*
  DONT FORGET THAT WE FREED quadfname above
  fprintf(stderr,"  Deduplicating quads..."); fflush(stderr);
  sidx iA,iB,iC,iD,ii;
  dimension Dim_Quads;
  qidx *thequads;
  fopenin(quadfname,quadfid); fnfree(quadfname);
  fscanf(quadfid,"NumQuads=%lu\n",&ii);
  if(ii!=maxQuads) 
  fprintf(stderr,"ERROR (getquads) -- numquads!=ii re-reading %s\n",quadfname);
  fscanf(quadfid,"DimQuads=%hu\n",&Dim_Quads);
  thequads = (qidx *)malloc(Dim_Quads*maxQuads*sizeof(qidx));
  for(ii=0;ii<maxQuads;ii++)
    fscanf(quadfid,"%lu,%lu,%lu,%lu\n",
	   thequads+ii*(Dim_Quads*sizeof(qidx))   ,
	   thequads+ii*(Dim_Quads*sizeof(qidx))+sizeof(qidx),
	   thequads+ii*(Dim_Quads*sizeof(qidx))+2*sizeof(qidx),
	   thequads+ii*(Dim_Quads*sizeof(qidx))+3*sizeof(qidx));
  //qsort(thequads,maxQuads,DimQuads*sizeof(qidx),???);
  fprintf(stderr,"done.\n");
  free(thequads);
asdsd
  fclose(quadfid);*/
  
  //basic_am_malloc_report();
  return(0);
}


qidx get_quads(FILE *quadfid,FILE *codefid,
	       stararray *thestars,kdtree *kd,double index_scale,
	       double ramin,double ramax,double decmin, double decmax,
	       qidx maxCodes)
{
  char still_not_done;
  int count;
  qidx numtries=0,ii;
  sidx iA,iB,iC,iD,jj,kk,numS;
  double Ax,Ay,Bx,By,Cx,Cy,Dx,Dy,xbar,ybar,zbar;
  double scale,thisx,thisy,xxtmp,costheta,sintheta;
  star *midpoint=mk_star();
  kquery *kq = mk_kquery("rangesearch","",KD_UNDEF,index_scale,kd->rmin);
  fprintf(codefid,"NumCodes=%lu\n",maxCodes);
  fprintf(codefid,"DimCodes=%hu\n",DIM_CODES);
  fprintf(quadfid,"NumQuads=%lu\n",maxCodes);
  fprintf(quadfid,"DimQuads=%hu\n",DIM_QUADS);

  for(ii=0;ii<maxCodes;ii++) {
    still_not_done=1;
    while(still_not_done) { // could be waiting a long time...
      star *randstar=make_rand_star(ramin,ramax,decmin,decmax); 
                        // better to actively select...
      kresult *krez = mk_kresult_from_kquery(kq,kd,randstar);
      numS=(sidx)krez->count;
      //fprintf(stderr,"random location: %d within index_scale.\n",numS);
      if(numS>=DIM_QUADS) {
	ivec *candidates=mk_ivec(numS-2);
	dyv *candidatesX=mk_dyv(numS-2);
	dyv *candidatesY=mk_dyv(numS-2);
	iA = (sidx)krez->pindexes->iarr[0];
	star_coords(thestars->array[iA],thestars->array[iA],&Ax,&Ay);
	// Ax,Ay will be almost exactly zero for now
	for(jj=(numS-1);jj>0 && still_not_done;jj--) {
	  iB=(sidx)krez->pindexes->iarr[jj];
	  star_coords(thestars->array[iB],thestars->array[iA],&Bx,&By);
          Bx-=Ax; By-=Ay; // probably don't need this
	  scale = sqrt(2*(Bx*Bx+By*By));
	  if(scale>index_scale) {
        xbar = star_ref(thestars->array[iA],0)+star_ref(thestars->array[iB],0);
        ybar = star_ref(thestars->array[iA],1)+star_ref(thestars->array[iB],1);
        zbar = star_ref(thestars->array[iA],2)+star_ref(thestars->array[iB],2);
	  xxtmp = sqrt(xbar*xbar+ybar*ybar+zbar*zbar);
          star_set(midpoint,0,xbar/xxtmp);
          star_set(midpoint,1,ybar/xxtmp);
          star_set(midpoint,2,zbar/xxtmp);
          star_coords(thestars->array[iA],midpoint,&Ax,&Ay);
          star_coords(thestars->array[iB],midpoint,&Bx,&By);
	  Bx-=Ax; By-=Ay;  
	  costheta=(Bx+By)/scale; sintheta=(By-Bx)/scale;
	  count=0;
	  for(kk=1;kk<numS;kk++) {
	    if(kk!=jj) {
	      star_coords(thestars->array[krez->pindexes->iarr[kk]],
			  midpoint,&thisx,&thisy);
	      thisx-=Ax; thisy-=Ay;
	      xxtmp=thisx;
	      thisx=2*(thisx*costheta+thisy*sintheta)/scale; 
	      thisy=2*(-xxtmp*sintheta+thisy*costheta)/scale;
	      if((thisx<1.0)&&(thisx>0.0)&&(thisy<1.0)&&(thisy>0.0)) {
		ivec_set(candidates,count,krez->pindexes->iarr[kk]);
		dyv_set(candidatesX,count,thisx);
		dyv_set(candidatesY,count,thisy);
		count++;
	      }
	    }
	  }
	  if(count>=2) {
	    sidx i1=(sidx)int_random(count); 
	    sidx i2=(sidx)int_random(count-1); 
	    if(i2>=i1) i2++;
	    iC=(sidx)ivec_ref(candidates,i1); 
	    iD=(sidx)ivec_ref(candidates,i2);
	    Cx=dyv_ref(candidatesX,i1); Dx=dyv_ref(candidatesX,i2);
	    Cy=dyv_ref(candidatesY,i1); Dy=dyv_ref(candidatesY,i2);

	    fprintf(codefid,"%f,%f,%f,%f\n",Cx,Cy,Dx,Dy);
	    fprintf(quadfid,"%lu,%lu,%lu,%lu\n",iA,iB,iC,iD);

	    //if(iA>*maxID) *maxID=iA;
	    //if(iB>*maxID) *maxID=iB;
	    //if(iC>*maxID) *maxID=iC;
	    //if(iD>*maxID) *maxID=iD;
 
	    still_not_done=0;
	  } // if count>=2
	}} // if scale and for jj
        //if(still_not_done) fprintf(stderr,"failed to get ABCD\n");
	free_ivec(candidates);
	free_dyv(candidatesX);
	free_dyv(candidatesY);
      } //if numS>=DIM_QUADS
      free_star(randstar);
      free_kresult(krez);
      numtries++;
    } // while(still_not_done)
   if(is_power_of_two(ii+1))
 fprintf(stderr,"  got %lu codes in %lu tries\r",ii+1,numtries);fflush(stdout);
  }
  
  free_kquery(kq);
  free_star(midpoint);

  return numtries;
}








