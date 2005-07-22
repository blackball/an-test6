#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "habs:q:f:"
extern char *optarg;
extern int optind, opterr, optopt;

qidx get_quads(FILE *quadfid,FILE *codefid,
	       stararray *thestars,kdtree *kd,double index_scale,
	       double ramin,double ramax,double decmin, double decmax,
	       qidx maxCodes,char ASCII);

void deduplicate_quads(FILE *quadfid, qidx maxQuads,double index_scale);

char *treefname=NULL;
char *quadfname=NULL;
char *codefname=NULL;
char ASCII=0;
qidx *thedata,*thesortorder;
char buff[100],maxstarWidth;



int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;

  qidx maxQuads=0;
  double index_scale=1.0/10.0;
  double ramin,ramax,decmin,decmax;
     
  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'a':
	ASCII = 1;
	break;
      case 'b':
	ASCII = 0;
	break;
      case 's':
	index_scale = (double)strtod(optarg,NULL);
	if(index_scale>=1.0) index_scale = index_scale*(double)PIl/(180.0*60);
	break;
      case 'q':
	maxQuads = strtoul(optarg,NULL,0);
	break;
      case 'f':
	treefname = malloc(strlen(optarg)+6);
	quadfname = malloc(strlen(optarg)+6);
	codefname = malloc(strlen(optarg)+6);
	sprintf(treefname,"%s.skdt",optarg);
	sprintf(quadfname,"%s.quad",optarg);
	sprintf(codefname,"%s.code",optarg);
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr, 
	"getquads [-s scale(arcmin)] [-q maxQuads] [-a|-b] [-f fname]\n");
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);

#define RANDSEED 888
  am_srand(RANDSEED); 

  FILE *treefid=NULL,*quadfid=NULL,*codefid=NULL;
  sidx numstars;
  qidx numtries;

  fprintf(stderr,"getquads: finding %lu quads in %s [RANDSEED=%d]\n",
	  maxQuads,treefname,RANDSEED);

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
  if(starkd==NULL) return(2);
  numstars=starkd->root->num_points;
  if(ASCII){sprintf(buff,"%lu",numstars-1);maxstarWidth=strlen(buff);}
  fprintf(stderr,"done\n    (%lu stars, %d nodes, depth %d).\n",
	  numstars,starkd->num_nodes,starkd->max_depth);
  fprintf(stderr,"    (dim %d) (limits %f<=ra<=%f;%f<=dec<=%f.)\n",
	  kdtree_num_dims(starkd),ramin,ramax,decmin,decmax);


  stararray *thestars = (stararray *)mk_dyv_array_from_kdtree(starkd);

  fprintf(stderr,"  Finding %lu quads at scale %g arcmin...\n",
	  maxQuads,180.0*60.0*index_scale/(double)PIl); fflush(stderr);
  fopenout(quadfname,quadfid); //fnfree(quadfname);
  fopenout(codefname,codefid); fnfree(codefname);
  numtries=get_quads(quadfid,codefid,thestars,starkd,index_scale,
		     ramin,ramax,decmin,decmax,maxQuads,ASCII);
  fclose(quadfid); fclose(codefid);
  fprintf(stderr,"  got %lu codes in %lu tries.\n", maxQuads,numtries);

  free_dyv_array_from_kdtree((dyv_array *)thestars);
  free_kdtree(starkd); 

  //fprintf(stderr,"  Deduplicating quads...\n"); fflush(stderr);
  //fopenin(quadfname,quadfid); 
  //deduplicate_quads(quadfid,maxQuads,index_scale);
  //fclose(quadfid); 
  fnfree(quadfname);
  


  //basic_am_malloc_report();
  return(0);
}


qidx get_quads(FILE *quadfid,FILE *codefid,
	       stararray *thestars,kdtree *kd,double index_scale,
	       double ramin,double ramax,double decmin, double decmax,
	       qidx maxCodes,char ASCII)
{
  char still_not_done;
  sidx count;
  qidx numtries=0,ii;
  sidx iA,iB,iC,iD,jj,kk,numS;
  double Ax,Ay,Bx,By,Cx,Cy,Dx,Dy;
  double scale,thisx,thisy,xxtmp,costheta,sintheta;
  star *midpoint=mk_star();
  kquery *kq = mk_kquery("rangesearch","",KD_UNDEF,index_scale,kd->rmin);

  write_code_header(codefid,ASCII,maxCodes,(sidx)thestars->size,
		    DIM_CODES,index_scale);
  write_quad_header(quadfid,ASCII,maxCodes,(sidx)thestars->size,
		    DIM_QUADS,index_scale);

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
	  scale = Bx*Bx+By*By;
	  if(sqrt(2.0*scale)>index_scale) { // ?? is this right?
          star_midpoint(midpoint,thestars->array[iA],thestars->array[iB]);
          star_coords(thestars->array[iA],midpoint,&Ax,&Ay);
          star_coords(thestars->array[iB],midpoint,&Bx,&By);
	  Bx-=Ax; By-=Ay;  
	  scale = Bx*Bx+By*By;
	  costheta=(Bx+By)/scale; sintheta=(By-Bx)/scale;
	  count=0;
	  for(kk=1;kk<numS;kk++) {
	    if(kk!=jj) {
	      star_coords(thestars->array[krez->pindexes->iarr[kk]],
			  midpoint,&thisx,&thisy);
	      thisx-=Ax; thisy-=Ay;
	      xxtmp=thisx;
	      thisx=thisx*costheta+thisy*sintheta;
	      thisy=-xxtmp*sintheta+thisy*costheta;
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
	    if(ivec_ref(candidates,i2)<ivec_ref(candidates,i1))
	      {sidx itmp=i1; i1=i2; i2=itmp;}
	    iC=(sidx)ivec_ref(candidates,i1);
	    iD=(sidx)ivec_ref(candidates,i2);
	    Cx=dyv_ref(candidatesX,i1); Cy=dyv_ref(candidatesY,i1); 
	    Dx=dyv_ref(candidatesX,i2); Dy=dyv_ref(candidatesY,i2);
	    
	    if(ASCII) {
	      fprintf(codefid,"%f,%f,%f,%f\n",Cx,Cy,Dx,Dy);
	      fprintf(quadfid,"%2$*1$lu,%3$*1$lu,%4$*1$lu,%5$*1$lu\n",
		      maxstarWidth,iA,iB,iC,iD);
	    }
	    else {
             fwrite(&Cx,sizeof(Cx),1,codefid);fwrite(&Cy,sizeof(Cy),1,codefid);
             fwrite(&Dx,sizeof(Dx),1,codefid);fwrite(&Dy,sizeof(Dy),1,codefid);
	     fwrite(&iA,sizeof(iA),1,quadfid);
	     fwrite(&iB,sizeof(iB),1,quadfid);
	     fwrite(&iC,sizeof(iC),1,quadfid);
	     fwrite(&iD,sizeof(iD),1,quadfid);
	    }

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



signed int compare_qidx(const void *a,const void *b) {
  unsigned long int aval,bval,adata,bdata;

  aval=*(unsigned long int *)a;
  bval=*(unsigned long int *)b;
  adata = thedata[aval];
  bdata = thedata[bval];

  if(adata>bdata) return(1);
  else if(adata<bdata) return(-1);
  else return(0);

}



void deduplicate_quads(FILE *quadfid, qidx maxQuads,double index_scale)
{
  sidx iA,iB,iC,iD,liA,liB,liC,liD,numstars;
  qidx ii,lastAB,lastii=0;
  dimension Dim_Quads;
  long posmarker;
  char ASCII;

  ASCII=read_quad_header(quadfid,&ii,&numstars,&Dim_Quads,&index_scale);
  if(ASCII==READ_FAIL)
    fprintf(stderr,"ERROR (getquads) -- read_quad_header failed\n");
  posmarker=ftell(quadfid);
    
  if(ii!=maxQuads)
    fprintf(stderr,"ERROR (getquads) -- NumQuads wrong when re-reading %s\n",
	    quadfname);

  thedata = (qidx *)malloc(maxQuads*sizeof(qidx));
  thesortorder = (qidx *)malloc(maxQuads*sizeof(qidx));
  for(ii=0;ii<maxQuads;ii++) {
    if(ASCII)
      fscanf(quadfid,"%lu,%lu,%lu,%lu\n",&iA,&iB,&iC,&iD);
    else {
      fread(&iA,sizeof(iA),1,quadfid);
      fread(&iB,sizeof(iB),1,quadfid);
      fread(&iC,sizeof(iC),1,quadfid);
      fread(&iD,sizeof(iD),1,quadfid);
    }
    thedata[ii]=iA+iB;
    thesortorder[ii]=ii;
  }
  qsort(thesortorder,maxQuads,sizeof(qidx),compare_qidx);
  if(thedata[thesortorder[0]]) lastAB=0; else lastAB=1;
  for(ii=0;ii<maxQuads;ii++) {
    fprintf(stderr,"idx: %lu \t iA+iB: %lu --",
	    thesortorder[ii],
	    thedata[thesortorder[ii]]);
    if(thedata[thesortorder[ii]]!=lastAB) {
      lastii=thesortorder[ii];
      lastAB=thedata[lastii];
      fprintf(stderr,"***\n");
    }
    else {
      if(ASCII) {
	fseek(quadfid,posmarker+lastii*
	   (DIM_QUADS*(maxstarWidth+1)*sizeof(char)),SEEK_SET); 
	fscanf(quadfid,"%lu,%lu,%lu,%lu\n",&liA,&liB,&liC,&liD);
	fseek(quadfid,posmarker+thesortorder[ii]*
	   (DIM_QUADS*(maxstarWidth+1)*sizeof(char)),SEEK_SET); 
	fscanf(quadfid,"%lu,%lu,%lu,%lu\n",&iA,&iB,&iC,&iD);
      }
      else {
	fseek(quadfid,posmarker+lastii*
	      (DIM_QUADS*sizeof(liA)),SEEK_SET);
	fread(&liA,sizeof(liA),1,quadfid);
	fread(&liB,sizeof(liB),1,quadfid);
	fread(&liC,sizeof(liC),1,quadfid);
	fread(&liD,sizeof(liD),1,quadfid);
	fseek(quadfid,posmarker+thesortorder[ii]*
	      (DIM_QUADS*sizeof(iA)),SEEK_SET);
	fread(&iA,sizeof(iA),1,quadfid);
	fread(&iB,sizeof(iB),1,quadfid);
	fread(&iC,sizeof(iC),1,quadfid);
	fread(&iD,sizeof(iD),1,quadfid);
      }
      if((iA==liA && iB==liB && iC==liC && iD==liD) ||
	 (iA==liB && iB==liA && iC==liC && iD==liD) ||
	 (iA==liA && iB==liB && iC==liD && iD==liC) ||
	 (iA==liB && iB==liA && iC==liD && iD==liC))
	fprintf(stderr,"DUPLICATE\n");
      else
	fprintf(stderr,"\n");
      //fprintf(stderr,"iA=%lu,iB=%lu,iC=%lu,iD=%lu --- ",iA,iB,iC,iD);
      //fprintf(stderr,"liA=%lu,liB=%lu,liC=%lu,liD=%lu\n",liA,liB,liC,liD);
    }
  }
  fprintf(stderr,"done.\n");
  free(thedata); free(thesortorder);
}

