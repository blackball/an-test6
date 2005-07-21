#include <unistd.h>
#include <stdio.h>
#include "starutil.h"

#define OPTIONS "hpf:o:t:"
extern char *optarg;
extern int optind, opterr, optopt;

#define ABCD_ORDER 0
#define BACD_ORDER 1
#define ABDC_ORDER 2
#define BADC_ORDER 3

xyarray *readxy(FILE *fid,qidx *numpix,sizev **pixsizes, char ParityFlip);
void solve_pix(xyarray *thepix, sizev *pixsizes, 
	       kdtree *codekd, double codetol, FILE *fid);
qidx try_all_codes(double Cx, double Cy, double Dx, double Dy,
		  xy *ABCDpix, kquery *kq,kdtree *codekd, FILE *fid);
void output_match(FILE *fid,xy *ABCDpix, kresult *krez, char order);
void fill_ids(FILE *hitfid, FILE *quadfid);

char *pixfname=NULL;
char *treefname=NULL;
char *hitfname=NULL;
char *quadfname=NULL;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;
  double codetol=0.01;
  char ParityFlip=0;

  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'f':
	treefname = malloc(strlen(optarg)+6);
	quadfname = malloc(strlen(optarg)+6);
	sprintf(treefname,"%s.ckdt",optarg);
	sprintf(quadfname,"%s.quad",optarg);
	break;
      case 'o':
	pixfname = malloc(strlen(optarg)+6);
	hitfname = malloc(strlen(optarg)+6);
	sprintf(pixfname,"%s.xyls",optarg);
	sprintf(hitfname,"%s.hits",optarg);
	break;
      case 't':
	codetol=strtod(optarg,NULL);
	break;
      case 'p':
	ParityFlip=1;
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h':
	fprintf(stderr, 
	"solvexy [-f fname] [-o fieldname] [-p flip_parity] [-t tol]\n");
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  for (argidx = optind; argidx < argc; argidx++)
    fprintf (stderr, "Non-option argument %s\n", argv[argidx]);


  FILE *pixfid=NULL,*treefid=NULL,*hitfid=NULL,*quadfid=NULL;
  qidx numpix;
  double index_scale;
  sizev *pixsizes;

  fprintf(stderr,"solvexy: solving fields in %s using %s\n",
	  pixfname,treefname);

  if(ParityFlip) fprintf(stderr,"  Flipping parity.\n");
  fprintf(stderr,"  Reading fields...");fflush(stderr);
  fopenin(pixfname,pixfid); fnfree(pixfname);
  xyarray *thepix = readxy(pixfid,&numpix,&pixsizes,ParityFlip);
  fclose(pixfid);
  if(thepix==NULL) return(1);
  fprintf(stderr,"got %lu fields.\n",numpix);

  fprintf(stderr,"  Reading code KD tree from ");
  if(treefname) fprintf(stderr,"%s...",treefname);
  else fprintf(stderr,"stdin...");  fflush(stderr);
  fopenin(treefname,treefid); fnfree(treefname);
  kdtree *codekd = fread_kdtree(treefid);
  fread(&index_scale,sizeof(index_scale),1,treefid);
  fclose(treefid);
  if(codekd==NULL) return(2);
  fprintf(stderr,"done\n    (%d quads, %d nodes, depth %d).\n",
	  codekd->root->num_points,codekd->num_nodes,codekd->max_depth);
  fprintf(stderr,"    (index scale = %f\n",index_scale);

  fprintf(stderr,"  Solving %lu fields...",numpix); fflush(stderr);
  fopenoutplus(hitfname,hitfid); fnfree(hitfname);
  solve_pix(thepix,pixsizes,codekd,codetol,hitfid);
  fprintf(stderr,"done.\n");
  free_xyarray(thepix); 
  free_sizev(pixsizes);
  free_kdtree(codekd); 

  fprintf(stderr,"  Filling in star IDs..."); fflush(stderr);
  fopenin(quadfname,quadfid); fnfree(quadfname);
  fill_ids(hitfid,quadfid);
  fclose(quadfid);
  fclose(hitfid);


  //basic_am_malloc_report();
  return(0);
}




xyarray *readxy(FILE *fid,qidx *numpix,sizev **pixsizes, char ParityFlip)
{
  char ASCII = 0;
  qidx ii,jj,numxy;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mFields=%lu\n",numpix);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (readxy) -- bad magic value in %s\n",pixfname);
      return((xyarray *)NULL);
    }
    ASCII=0;
    fread(numpix,sizeof(*numpix),1,fid);
  }
  xyarray *thepix = mk_xyarray(*numpix);
  *pixsizes = mk_sizev(*numpix);
  for(ii=0;ii<*numpix;ii++) {
    if(ASCII)
      fscanf(fid,"%lu",&numxy);
    else
      fread(&numxy,sizeof(numxy),1,fid);
    sizev_set(*pixsizes,ii,numxy);
    thepix->array[ii] = mk_xy(numxy);
    if(thepix->array[ii]==NULL) {
      fprintf(stderr,"ERROR (readxy) - out of memory at field %lu\n",ii);
      free_xyarray(thepix);
      free_sizev(*pixsizes);
      return (xyarray *)NULL;
    }
    if(ASCII) {
      for(jj=0;jj<numxy;jj++)
	fscanf(fid,",%lf,%lf",(thepix->array[ii]->farr)+2*jj,
  	       (thepix->array[ii]->farr)+2*jj+1   );
      fscanf(fid,"\n");
    }
    else
      fread(thepix->array[ii]->farr,sizeof(double),DIM_XY*numxy,fid);

    if(ParityFlip) 
      for(jj=0;jj<numxy;jj++)
	*((thepix->array[ii]->farr)+2*jj)*=-1;

  }
  return thepix;
}



void solve_pix(xyarray *thepix, sizev *pixsizes, 
	       kdtree *codekd, double codetol, FILE *fid)
{
  xy *thispix;
  qidx ii,numxy,nummatches,iA,iB,iC,iD;
  double Ax,Ay,Bx,By,Cx,Cy,Dx,Dy,costheta,sintheta,scale,xxtmp;
  long posmarker;
  kquery *kq = mk_kquery("rangesearch","",KD_UNDEF,
			 codetol,codekd->rmin);
  xy *ABCDpix=mk_xy(DIM_QUADS);

  for(ii=0;ii<thepix->size;ii++) {
    nummatches=0;
    numxy=sizev_ref(pixsizes,ii);
    thispix=thepix->array[ii];
    //fprintf(fid,"field %lu has %lu stars: 4*%lu quads will be checked\n",
    //	    ii,numxy,choose(numxy,DIM_QUADS));

    // find min and max coordinates in field
    if(numxy<DIM_QUADS) fprintf(fid,"field %lu: no matches\n",ii); else {
    Cx=xy_refx(thepix->array[ii],0); Cy=xy_refy(thepix->array[ii],0);
    Dx=xy_refx(thepix->array[ii],0); Dy=xy_refy(thepix->array[ii],0);
    for(iA=0;iA<numxy;iA++) {
      Ax=xy_refx(thepix->array[ii],iA); Ay=xy_refy(thepix->array[ii],iA);
      if(Ax<Cx) Cx=Ax; if(Ax>Dx) Dx=Ax;
      if(Ay<Cy) Cy=Ay; if(Ay>Dy) Dy=Ay;
    }
    posmarker=ftell(fid);
    fprintf(fid,"field %lu: %f,%f,%f,%f\n",ii,Cx,Cy,Dx,Dy);

    for(iA=0;iA<(numxy-1);iA++) {
      Ax=xy_refx(thepix->array[ii],iA); Ay=xy_refy(thepix->array[ii],iA);
      xy_setx(ABCDpix,0,Ax); xy_sety(ABCDpix,0,Ay);
      for(iB=iA+1;iB<numxy;iB++) {
	Bx=xy_refx(thepix->array[ii],iB); By=xy_refy(thepix->array[ii],iB);
	xy_setx(ABCDpix,1,Bx); xy_sety(ABCDpix,1,By);
	Bx-=Ax; By-=Ay;
	scale = Bx*Bx+By*By;
	costheta=(Bx+By)/scale; sintheta=(By-Bx)/scale;
	for(iC=0;iC<(numxy-1);iC++) {
	  if(iC!=iA && iC!=iB) {
          Cx=xy_refx(thepix->array[ii],iC); Cy=xy_refy(thepix->array[ii],iC);
	  xy_setx(ABCDpix,2,Cx); xy_sety(ABCDpix,2,Cy);
	  Cx-=Ax; Cy-=Ay;
	  xxtmp=Cx;
	  Cx=Cx*costheta+Cy*sintheta;
	  Cy=-xxtmp*sintheta+Cy*costheta;
	  if((Cx<1.0)&&(Cx>0.0)&&(Cy<1.0)&&(Cy>0.0)) {
	  for(iD=iC+1;iD<numxy;iD++) {
	    if(iD!=iA && iD!=iB) {
            Dx=xy_refx(thepix->array[ii],iD); Dy=xy_refy(thepix->array[ii],iD);
    	    xy_setx(ABCDpix,3,Dx); xy_sety(ABCDpix,3,Dy);
	    Dx-=Ax; Dy-=Ay;
	    xxtmp=Dx;
	    Dx=Dx*costheta+Dy*sintheta;
	    Dy=-xxtmp*sintheta+Dy*costheta;
	    if((Dx<1.0)&&(Dx>0.0)&&(Dy<1.0)&&(Dy>0.0)) {
	      //fprintf(fid,"iA:%lu,iB:%lu,iC:%lu,iD:%lu\n",iA,iB,iC,iD);
	      nummatches+=try_all_codes(Cx,Cy,Dx,Dy,ABCDpix,kq,codekd,fid);
	    }
	    }}}
	  }}
      }
    }
    if(nummatches==0) {
      fseek(fid,posmarker,SEEK_SET); 
      fprintf(fid,"field %lu: no matches\n",ii);
    }
    }

    //if(is_power_of_two(ii))
    //  fprintf(stderr,"  solved %lu fields\r",ii);fflush(stdout);
  }
  
  free_kquery(kq);
  free_xy(ABCDpix);

  return;
}


qidx try_all_codes(double Cx, double Cy, double Dx, double Dy,
		   xy *ABCDpix, kquery *kq,kdtree *codekd, FILE *fid)
{
  kresult *krez;
  code *thequery = mk_code();
  qidx numrez=0;

  // ABCD
  code_set(thequery,0,Cx); code_set(thequery,1,Cy);
  code_set(thequery,2,Dx); code_set(thequery,3,Dy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count)
    {output_match(fid,ABCDpix,krez,ABCD_ORDER); numrez+=krez->count;}
  free_kresult(krez);
  
  // BACD
  code_set(thequery,0,1.0-Cx); code_set(thequery,1,1.0-Cy);
  code_set(thequery,2,1.0-Dx); code_set(thequery,3,1.0-Dy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count)
    {output_match(fid,ABCDpix,krez,BACD_ORDER); numrez+=krez->count;}
  free_kresult(krez);
  
  // ABDC
  code_set(thequery,0,Dx); code_set(thequery,1,Dy);
  code_set(thequery,2,Cx); code_set(thequery,3,Cy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count)
    {output_match(fid,ABCDpix,krez,ABDC_ORDER); numrez+=krez->count;}
  free_kresult(krez);

  // BADC
  code_set(thequery,0,1.0-Dx); code_set(thequery,1,1.0-Dy);
  code_set(thequery,2,1.0-Cx); code_set(thequery,3,1.0-Cy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count)
    {output_match(fid,ABCDpix,krez,BADC_ORDER); numrez+=krez->count;}
  free_kresult(krez);

  free_code(thequery);

  return numrez;
}


void output_match(FILE *fid,xy *ABCDpix, kresult *krez, char order)
{
  qidx jj;
  char oA=0,oB=1,oC=2,oD=3;
  if(order==ABCD_ORDER) {oA=0; oB=1; oC=2; oD=3;}
  if(order==BACD_ORDER) {oA=1; oB=0; oC=2; oD=3;}
  if(order==ABDC_ORDER) {oA=0; oB=1; oC=3; oD=2;}
  if(order==BADC_ORDER) {oA=1; oB=0; oC=3; oD=2;}

  for(jj=0;jj<krez->count;jj++)
    fprintf(fid,"%f,%f,%f,%f,%f,%f,%f,%f,%lu\n",
	    xy_refx(ABCDpix,oA),xy_refy(ABCDpix,oA),
	    xy_refx(ABCDpix,oB),xy_refy(ABCDpix,oB),
	    xy_refx(ABCDpix,oC),xy_refy(ABCDpix,oC),
	    xy_refx(ABCDpix,oD),xy_refy(ABCDpix,oD),
	    (unsigned long int)krez->pindexes->iarr[jj]);

  return;
}


void fill_ids(FILE *hitfid, FILE *quadfid)
{
  long qposmarker;
  char ASCII = 0;
  qidx ii=999,numquads,thismatch;
  sidx iA,iB,iC,iD;
  dimension Dim_Quads;
  double index_scale;
  magicval magic;
  char buff[100],maxstarWidth;
  double junk;

  rewind(hitfid); rewind(quadfid);

  fread(&magic,sizeof(magic),1,quadfid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(quadfid,"mQuads=%lu\n",&numquads);
    fscanf(quadfid,"DimQuads=%hu\n",&Dim_Quads);
    fscanf(quadfid,"IndexScale=%lf\n",&index_scale);

    //sprintf(buff,"%lu",numstars-1); maxstarWidth=strlen(buff);
    maxstarWidth=3;
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (solvexy) -- bad magic value in quad file.\n");
      return;
    }
    ASCII=0;
    fread(&numquads,sizeof(numquads),1,quadfid);
    fread(&Dim_Quads,sizeof(Dim_Quads),1,quadfid);
    fread(&index_scale,sizeof(index_scale),1,quadfid);
  }

  qposmarker=ftell(quadfid);
  while(!feof(hitfid)) {
    if(!fscanf(hitfid,"field %lu: ",&ii))
      {fscanf(hitfid,"%s\n",buff); fprintf(stderr,"***%s***\n",buff);}
    fprintf(stdout,"now on pic %lu\n",ii);
    if(!fscanf(hitfid,"no matches\n")) {
      fscanf(hitfid,"%lf,%lf,%lf,%lf\n",&junk,&junk,&junk,&junk);
      while(fscanf(hitfid,"%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,",
	    &junk,&junk,&junk,&junk,&junk,&junk,&junk,&junk)) {
	fscanf(hitfid,"%lu\n",&thismatch);
	if(ASCII) {
	  fseek(quadfid,qposmarker+thismatch*
		(DIM_QUADS*(maxstarWidth+1)*sizeof(char)),SEEK_SET); 
	  fscanf(quadfid,"%lu,%lu,%lu,%lu\n",&iA,&iB,&iC,&iD);
	}
	else {
	  fseek(quadfid,qposmarker+thismatch*
		(DIM_QUADS*sizeof(iA)),SEEK_SET);
	  fread(&iA,sizeof(iA),1,quadfid);
	  fread(&iB,sizeof(iB),1,quadfid);
	  fread(&iC,sizeof(iC),1,quadfid);
	  fread(&iD,sizeof(iD),1,quadfid);
	}
	fprintf(stdout,"found quad %lu --> (%lu,%lu,%lu,%lu)\n",
		thismatch,iA,iB,iC,iD);

      } // while more matches
    } // if not "no matches"
  } // while not feof

}

