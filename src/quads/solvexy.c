#include <unistd.h>
#include <stdio.h>
#include "starutil.h"

#define OPTIONS "hpf:o:t:"
extern char *optarg;
extern int optind, opterr, optopt;

xyarray *readxy(FILE *fid,qidx *numpix,sizev **pixsizes, char ParityFlip);
void solve_pix(xyarray *thepix, sizev *pixsizes, 
	       kdtree *codekd, double codetol, FILE *fid);
void try_all_codes(double Cx, double Cy, double Dx, double Dy,
		   xy *ABCDpix, kquery *kq,kdtree *codekd, FILE *fid);

char *pixfname=NULL;
char *treefname=NULL;
char *hitfname=NULL;

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
	sprintf(treefname,"%s.ckdt",optarg);
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


  FILE *pixfid=NULL,*treefid=NULL,*hitfid=NULL;
  qidx numpix;
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
  fclose(treefid);
  if(codekd==NULL) return(2);
  fprintf(stderr,"done (%d quads, %d nodes, depth %d).\n",
	  codekd->root->num_points,codekd->num_nodes,codekd->max_depth);
  // ?? should have scale also

  fprintf(stderr,"  Solving %lu fields...",numpix); fflush(stderr);
  fopenout(hitfname,hitfid); fnfree(hitfname);
  solve_pix(thepix,pixsizes,codekd,codetol,hitfid);
  fprintf(stderr,"done.\n");
  fclose(hitfid);

  free_xyarray(thepix); 
  free_sizev(pixsizes);
  free_kdtree(codekd); 

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
    // ?? read in how many xy points in this pic
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
  qidx ii,numxy,iA,iB,iC,iD;
  double Ax,Ay,Bx,By,Cx,Cy,Dx,Dy,costheta,sintheta,scale,xxtmp;
  kquery *kq = mk_kquery("rangesearch","",KD_UNDEF,
			 codetol,codekd->rmin);
  xy *ABCDpix=mk_xy(DIM_QUADS);

  for(ii=0;ii<thepix->size;ii++) {
    numxy=sizev_ref(pixsizes,ii);
    thispix=thepix->array[ii];
    //fprintf(fid,"field %lu has %lu stars: 4*%lu quads will be checked\n",
    //	    ii,numxy,choose(numxy,DIM_QUADS));
    fprintf(fid,"%lu:\n",ii);
    for(iA=0;iA<(numxy-1);iA++) {
      Ax=xy_refx(thepix->array[ii],iA); Ay=xy_refy(thepix->array[ii],iA);
      xy_setx(ABCDpix,0,Ax); xy_sety(ABCDpix,0,Ay);
      for(iB=iA+1;iB<numxy;iB++) {
	Bx=xy_refx(thepix->array[ii],iB); By=xy_refy(thepix->array[ii],iB);
	xy_setx(ABCDpix,1,Bx); xy_sety(ABCDpix,1,By);
	Bx-=Ax; By-=Ay;
	scale = sqrt(2*(Bx*Bx+By*By));
	costheta=(Bx+By)/scale; sintheta=(By-Bx)/scale;
	for(iC=0;iC<(numxy-1);iC++) {
	  if(iC!=iA && iC!=iB) {
          Cx=xy_refx(thepix->array[ii],iC); Cy=xy_refy(thepix->array[ii],iC);
	  xy_setx(ABCDpix,2,Cx); xy_sety(ABCDpix,2,Cy);
	  Cx-=Ax; Cy-=Ay;
	  xxtmp=Cx;
	  Cx=2*(Cx*costheta+Cy*sintheta)/scale; 
	  Cy=2*(-xxtmp*sintheta+Cy*costheta)/scale;
	  if((Cx<1.0)&&(Cx>0.0)&&(Cy<1.0)&&(Cy>0.0)) {
	    //if(1) {
	  for(iD=iC+1;iD<numxy;iD++) {
	    if(iD!=iA && iD!=iB) {
            Dx=xy_refx(thepix->array[ii],iD); Dy=xy_refy(thepix->array[ii],iD);
    	    xy_setx(ABCDpix,3,Dx); xy_sety(ABCDpix,3,Dy);
	    Dx-=Ax; Dy-=Ay;
	    xxtmp=Dx;
	    Dx=2*(Dx*costheta+Dy*sintheta)/scale; 
	    Dy=2*(-xxtmp*sintheta+Dy*costheta)/scale;
	    if((Dx<1.0)&&(Dx>0.0)&&(Dy<1.0)&&(Dy>0.0)) {
	    //if(1) {
	      //fprintf(fid,"iA:%lu,iB:%lu,iC:%lu,iD:%lu\n",iA,iB,iC,iD);
	      try_all_codes(Cx,Cy,Dx,Dy,ABCDpix,kq,codekd,fid);
	    }
	    }}}
	  }}
      }
    }

    //if(is_power_of_two(ii))
    //  fprintf(stderr,"  solved %lu fields\r",ii);fflush(stdout);
  }
  
  free_kquery(kq);
  free_xy(ABCDpix);

  return;
}


void try_all_codes(double Cx, double Cy, double Dx, double Dy,
		   xy *ABCDpix, kquery *kq,kdtree *codekd, FILE *fid)
{
  kresult *krez;
  code *thequery = mk_code();
  qidx jj;

  code_set(thequery,0,Cx); code_set(thequery,1,Cy);
  code_set(thequery,2,Dx); code_set(thequery,3,Dy);
  //fprintf(fid,"code:%f,%f,%f,%f\n",Cx,Cy,Dx,Dy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    //fprintf(fid,"ABCD gives %d matches:",krez->count);
    fprintf(fid,"%f,%f,%f,%f,%f,%f,%f,%f",
	    xy_refx(ABCDpix,0),xy_refy(ABCDpix,0),
	    xy_refx(ABCDpix,1),xy_refy(ABCDpix,1),
	    xy_refx(ABCDpix,2),xy_refy(ABCDpix,2),
	    xy_refx(ABCDpix,3),xy_refy(ABCDpix,3));
    for(jj=0;jj<krez->count;jj++)
      fprintf(fid,",%lu",(unsigned long int)krez->pindexes->iarr[jj]);
    fprintf(fid,"\n");
  }
  free_kresult(krez);
  
  code_set(thequery,0,-Cx); code_set(thequery,1,-Cy);
  code_set(thequery,2,-Dx); code_set(thequery,3,-Dy);
  //fprintf(fid,"code:%f,%f,%f,%f\n",-Cx,-Cy,-Dx,-Dy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    //fprintf(fid,"BACD gives %d matches:",krez->count);
    fprintf(fid,"%f,%f,%f,%f,%f,%f,%f,%f",
	    xy_refx(ABCDpix,1),xy_refy(ABCDpix,1),
	    xy_refx(ABCDpix,0),xy_refy(ABCDpix,0),
	    xy_refx(ABCDpix,2),xy_refy(ABCDpix,2),
	    xy_refx(ABCDpix,3),xy_refy(ABCDpix,3));
    for(jj=0;jj<krez->count;jj++)
      fprintf(fid,",%lu",(unsigned long int)krez->pindexes->iarr[jj]);
    fprintf(fid,"\n");
  }
  free_kresult(krez);
  
  code_set(thequery,0,Dx); code_set(thequery,1,Dy);
  code_set(thequery,2,Cx); code_set(thequery,3,Cy);
  //fprintf(fid,"code:%f,%f,%f,%f\n",Dx,Dy,Cx,Cy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    //fprintf(fid,"ABDC gives %d matches:",krez->count);
    fprintf(fid,"%f,%f,%f,%f,%f,%f,%f,%f",
	    xy_refx(ABCDpix,0),xy_refy(ABCDpix,0),
	    xy_refx(ABCDpix,1),xy_refy(ABCDpix,1),
	    xy_refx(ABCDpix,3),xy_refy(ABCDpix,3),
	    xy_refx(ABCDpix,2),xy_refy(ABCDpix,2));
    for(jj=0;jj<krez->count;jj++)
      fprintf(fid,",%lu",(unsigned long int)krez->pindexes->iarr[jj]);
    fprintf(fid,"\n");
  }
  free_kresult(krez);
  
  code_set(thequery,0,-Dx); code_set(thequery,1,-Dy);
  code_set(thequery,2,-Cx); code_set(thequery,3,-Cy);
  //fprintf(fid,"code:%f,%f,%f,%f\n",-Dx,-Dy,-Cx,-Cy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    //fprintf(fid,"BADC gives %d matches:",krez->count);
    fprintf(fid,"%f,%f,%f,%f,%f,%f,%f,%f",
	    xy_refx(ABCDpix,1),xy_refy(ABCDpix,1),
	    xy_refx(ABCDpix,0),xy_refy(ABCDpix,0),
	    xy_refx(ABCDpix,3),xy_refy(ABCDpix,3),
	    xy_refx(ABCDpix,2),xy_refy(ABCDpix,2));
    for(jj=0;jj<krez->count;jj++)
      fprintf(fid,",%lu",(unsigned long int)krez->pindexes->iarr[jj]);
    fprintf(fid,"\n");
  }

  free_kresult(krez);
  free_code(thequery);

  return;
}
