#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"

#define OPTIONS "hpf:o:t:k:"
const char HelpString[]=
"solvexy -f fname -o fieldname [-t tol] [-p]\n"
"  tol == code_tolerance, -p flips parity\n";

extern char *optarg;
extern int optind, opterr, optopt;

#define ABCD_ORDER 0
#define BACD_ORDER 1
#define ABDC_ORDER 2
#define BADC_ORDER 3

qidx solve_pix(xyarray *thepix, sizev *pixsizes, kdtree *codekd, 
			    double codetol, FILE *hitfid, 
		    FILE *quadfid, char qASCII, FILE *catfid, char cASCII);
qidx try_all_codes(double Cx, double Cy, double Dx, double Dy,
		   double xxmin, double xxmax, double yymin, double yymax,
		   xy *ABCDpix, kquery *kq,kdtree *codekd,  FILE *hitfid, 
		   FILE *quadfid, char qASCII, FILE *catfid, char cASCII);
void output_match(double xxmin, double xxmax, double yymin, double yymax,
		  FILE *hitfid,FILE *quadfid, char qASCII,
		  FILE *catfid, char cASCII, 
		  kresult *krez, xy *ABCDpix, char order);
void image_to_xyz(double uu, double vv, star *s, double *transform);
double *fit_transform(xy *ABCDpix,char order,star *A,star *B,star *C,star *D);
double inverse_3by3(double *matrix);



char *pixfname=NULL,*treefname=NULL,*hitfname=NULL;
char *quadfname=NULL,*catfname=NULL;
off_t qposmarker,cposmarker;
char buff[100],maxstarWidth,oneobjWidth;

kdtree *hitkd=NULL;
dyv *hitdyv=NULL;
double dsq=1e20;
int whichmatch;
ivec *hitquads;

int main(int argc,char *argv[])
{
  int argidx,argchar;//  opterr = 0;
  double codetol=-1.0;
  char ParityFlip=0;

  if(argc<=3) {fprintf(stderr,HelpString); return(OPT_ERR);}

  while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
    switch (argchar)
      {
      case 'f':
	treefname = malloc(strlen(optarg)+6);
	quadfname = malloc(strlen(optarg)+6);
	catfname = malloc(strlen(optarg)+6);
	sprintf(treefname,"%s.ckdt",optarg);
	sprintf(quadfname,"%s.quad",optarg);
	sprintf(catfname,"%s.objs",optarg);
	break;
      case 'o':
	pixfname = malloc(strlen(optarg)+6);
	hitfname = malloc(strlen(optarg)+6);
	sprintf(pixfname,"%s.xyls",optarg);
	sprintf(hitfname,"%s.hits",optarg);
	break;
      case 'k':
	codetol=(double)strtoul(optarg,NULL,0);
	break;
      case 't':
	codetol=strtod(optarg,NULL);
	if(codetol>=1) codetol=1.0;
	break;
      case 'p':
	ParityFlip=1;
	break;
      case '?':
	fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      case 'h': 
      	fprintf(stderr,HelpString);
	return(HELP_ERR);
      default:
	return(OPT_ERR);
      }

  if(treefname==NULL || pixfname==NULL || codetol<0)
    {fprintf(stderr,HelpString);return(OPT_ERR);}

  if(optind<argc) {
    for (argidx = optind; argidx < argc; argidx++)
      fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
    fprintf(stderr,HelpString);
    return(OPT_ERR);
  }


  FILE *pixfid=NULL,*treefid=NULL,*hitfid=NULL,*quadfid=NULL,*catfid=NULL;
  qidx numpix,numquads,numtries;
  sidx numstars;
  double index_scale,ramin,ramax,decmin,decmax;
  char qASCII,cASCII;
  dimension Dim_Quads,Dim_Stars;
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
  kdtree *codekd = read_codekd(treefid,&index_scale);
  fclose(treefid);
  if(codekd==NULL) return(2);
  fprintf(stderr,"done\n    (%d quads, %d nodes, depth %d).\n",
	  kdtree_num_points(codekd),kdtree_num_nodes(codekd),
	  kdtree_max_depth(codekd));
  fprintf(stderr,"    (index scale = %lf)\n",rad2arcmin(index_scale));

  fopenin(quadfname,quadfid); fnfree(quadfname);
  qASCII=read_quad_header(quadfid,&numquads,&numstars,&Dim_Quads,&index_scale);
  if(qASCII==READ_FAIL) return(3);
  if(qASCII) {sprintf(buff,"%lu",numstars-1); maxstarWidth=strlen(buff);}
  qposmarker=ftello(quadfid);

  fopenin(catfname,catfid); fnfree(catfname);
  cASCII=read_objs_header(catfid,&numstars,&Dim_Stars,
			 &ramin,&ramax,&decmin,&decmax);
  if(cASCII==READ_FAIL) return(4);
  if(cASCII) {sprintf(buff,"%lf,%lf,%lf\n",0.0,0.0,0.0);
              oneobjWidth=strlen(buff);}
  cposmarker=ftello(catfid);

  fprintf(stderr,"  Solving %lu fields...",numpix); fflush(stderr);
  fopenout(hitfname,hitfid); fnfree(hitfname);
  numtries=solve_pix(thepix,pixsizes,codekd,codetol,
		     hitfid,quadfid,qASCII,catfid,cASCII);
  fclose(hitfid); fclose(quadfid); fclose(catfid);
  fprintf(stderr,"done (tried %lu quads).\n",numtries);

  free_xyarray(thepix); 
  free_sizev(pixsizes);
  free_kdtree(codekd); 

  //basic_am_malloc_report();
  return(0);
}





qidx solve_pix(xyarray *thepix, sizev *pixsizes, kdtree *codekd, 
			    double codetol, FILE *hitfid, 
		      FILE *quadfid, char qASCII, FILE *catfid, char cASCII)
{
  qidx numtries=0;
  xy *thispix;
  qidx ii,jj,numxy,nummatches,iA,iB,iC,iD;
  double Ax,Ay,Bx,By,Cx,Cy,Dx,Dy;
  double costheta,sintheta,scale,xxtmp,yytmp;
  double xxmin,xxmax,yymin,yymax;
  off_t posmarker;
  hitquads = mk_ivec(0);

  kquery *kq;
  if(codetol<1.0)
    kq = mk_kquery("rangesearch","",KD_UNDEF,codetol,kdtree_rmin(codekd));
  else
    kq = mk_kquery("knn","",(int)codetol,KD_UNDEF,kdtree_rmin(codekd));

  xy *ABCDpix=mk_xy(DIM_QUADS);

  for(ii=0;ii<dyv_array_size(thepix);ii++) {
    nummatches=0;
    numxy=sizev_ref(pixsizes,ii);
    thispix=xya_ref(thepix,ii);
    //fprintf(hitfid,"field %lu has %lu stars: 4*%lu quads will be checked\n",
    //	    ii,numxy,choose(numxy,DIM_QUADS));

    // find min and max coordinates in field
    if(numxy<DIM_QUADS) fprintf(hitfid,"field %lu: no matches\n",ii); else {
    xxmin=xy_refx(xya_ref(thepix,ii),0); yymin=xy_refy(xya_ref(thepix,ii),0);
    xxmax=xy_refx(xya_ref(thepix,ii),0); yymax=xy_refy(xya_ref(thepix,ii),0);
    for(jj=0;jj<numxy;jj++) {
     xxtmp=xy_refx(xya_ref(thepix,ii),jj);yytmp=xy_refy(xya_ref(thepix,ii),jj);
      if(xxtmp<xxmin) xxmin=xxtmp; if(xxtmp>xxmax) xxmax=xxtmp;
      if(yytmp<yymin) yymin=yytmp; if(yytmp>yymax) yymax=yytmp;
    }
    posmarker=ftello(hitfid);
    fprintf(hitfid,"field %lu: %lf,%lf,%lf,%lf\n",ii,xxmin,yymin,xxmax,yymax);


    for(iA=0;iA<(numxy-1);iA++) {
      Ax=xy_refx(xya_ref(thepix,ii),iA); Ay=xy_refy(xya_ref(thepix,ii),iA);
      xy_setx(ABCDpix,0,Ax); xy_sety(ABCDpix,0,Ay);
      for(iB=iA+1;iB<numxy;iB++) {
	Bx=xy_refx(xya_ref(thepix,ii),iB); By=xy_refy(xya_ref(thepix,ii),iB);
	xy_setx(ABCDpix,1,Bx); xy_sety(ABCDpix,1,By);
	Bx-=Ax; By-=Ay;
	scale = Bx*Bx+By*By;
	costheta=(Bx+By)/scale; sintheta=(By-Bx)/scale;
	for(iC=0;iC<(numxy-1);iC++) {
	  if(iC!=iA && iC!=iB) {
          Cx=xy_refx(xya_ref(thepix,ii),iC); Cy=xy_refy(xya_ref(thepix,ii),iC);
	  xy_setx(ABCDpix,2,Cx); xy_sety(ABCDpix,2,Cy);
	  Cx-=Ax; Cy-=Ay;
	  xxtmp=Cx;
	  Cx=Cx*costheta+Cy*sintheta;
	  Cy=-xxtmp*sintheta+Cy*costheta;
	  if((Cx<1.0)&&(Cx>0.0)&&(Cy<1.0)&&(Cy>0.0)) {
	  for(iD=iC+1;iD<numxy;iD++) {
	    if(iD!=iA && iD!=iB) {
          Dx=xy_refx(xya_ref(thepix,ii),iD); Dy=xy_refy(xya_ref(thepix,ii),iD);
    	    xy_setx(ABCDpix,3,Dx); xy_sety(ABCDpix,3,Dy);
	    Dx-=Ax; Dy-=Ay;
	    xxtmp=Dx;
	    Dx=Dx*costheta+Dy*sintheta;
	    Dy=-xxtmp*sintheta+Dy*costheta;
	    if((Dx<1.0)&&(Dx>0.0)&&(Dy<1.0)&&(Dy>0.0)) {
	      //fprintf(hitfid,"iA:%lu,iB:%lu,iC:%lu,iD:%lu\n",iA,iB,iC,iD);
	      numtries++;
	      nummatches+=try_all_codes(Cx,Cy,Dx,Dy,xxmin,xxmax,yymin,yymax,
                        ABCDpix,kq,codekd,hitfid,quadfid,qASCII,catfid,cASCII);
	    }
	    }}}
	  }}
      }
    }
    if(nummatches==0) {
      fseeko(hitfid,posmarker,SEEK_SET); 
      fprintf(hitfid,"field %lu: no matches\n",ii);
    }
    }

    //if(is_power_of_two(ii))
    //  fprintf(stderr,"  solved %lu fields\r",ii);fflush(stdout);
  }
  
  free_kquery(kq);
  free_xy(ABCDpix);

  return numtries;
}


qidx try_all_codes(double Cx, double Cy, double Dx, double Dy,
		   double xxmin, double xxmax, double yymin, double yymax,
		   xy *ABCDpix, kquery *kq,kdtree *codekd,  FILE *hitfid, 
		   FILE *quadfid, char qASCII, FILE *catfid, char cASCII)
{
  kresult *krez;
  code *thequery = mk_code();
  qidx numrez=0;

  // ABCD
  code_set(thequery,0,Cx); code_set(thequery,1,Cy);
  code_set(thequery,2,Dx); code_set(thequery,3,Dy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    //fprintf(hitfid,"  abcd code:%lf,%lf,%lf,%lf\n",Cx,Cy,Dx,Dy);
    output_match(xxmin,xxmax,yymin,yymax,
                 hitfid,quadfid,qASCII,catfid,cASCII,krez,ABCDpix,ABCD_ORDER); 
    numrez+=krez->count;
  }
  free_kresult(krez);
  
  // BACD
  code_set(thequery,0,1.0-Cx); code_set(thequery,1,1.0-Cy);
  code_set(thequery,2,1.0-Dx); code_set(thequery,3,1.0-Dy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
 //fprintf(hitfid,"  bacd code:%lf,%lf,%lf,%lf\n",1.0-Cx,1.0-Cy,1.0-Dx,1.0-Dy);
    output_match(xxmin,xxmax,yymin,yymax,
                 hitfid,quadfid,qASCII,catfid,cASCII,krez,ABCDpix,BACD_ORDER); 
    numrez+=krez->count;
  }
  free_kresult(krez);
  
  // ABDC
  code_set(thequery,0,Dx); code_set(thequery,1,Dy);
  code_set(thequery,2,Cx); code_set(thequery,3,Cy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    //fprintf(hitfid,"  abdc code:%lf,%lf,%lf,%lf\n",Dx,Dy,Cx,Cy);
    output_match(xxmin,xxmax,yymin,yymax,
                 hitfid,quadfid,qASCII,catfid,cASCII,krez,ABCDpix,ABDC_ORDER); 
    numrez+=krez->count;
  }
  free_kresult(krez);

  // BADC
  code_set(thequery,0,1.0-Dx); code_set(thequery,1,1.0-Dy);
  code_set(thequery,2,1.0-Cx); code_set(thequery,3,1.0-Cy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
 //fprintf(hitfid,"  badc code:%lf,%lf,%lf,%lf\n",1.0-Dx,1.0-Dy,1.0-Cx,1.0-Cy);
    output_match(xxmin,xxmax,yymin,yymax,
                 hitfid,quadfid,qASCII,catfid,cASCII,krez,ABCDpix,BADC_ORDER); 
    numrez+=krez->count;
  }
  free_kresult(krez);

  free_code(thequery);

  return numrez;
}


void output_match(double xxmin, double xxmax, double yymin, double yymax,
                  FILE *hitfid,FILE *quadfid, char qASCII,
		  FILE *catfid, char cASCII, 
		  kresult *krez, xy *ABCDpix, char order)
{
  int numq;
  qidx jj,thisquad;
  sidx iA,iB,iC,iD;
  double tmpx,tmpy,tmpz,*transform;
  star *sA,*sB,*sC,*sD,*sMin,*sMax;
  sA=mk_star(); if(sA==NULL) return;  sB=mk_star(); if(sB==NULL) return;
  sC=mk_star(); if(sC==NULL) return;  sD=mk_star(); if(sD==NULL) return;
  sMin=mk_star();if(sMin==NULL) return; sMax=mk_star();if(sMax==NULL) return;
  for(jj=0;jj<krez->count;jj++) {
  thisquad = (qidx)krez->pindexes->iarr[jj];
  numq=hitquads->size;
  if(add_to_ivec_unique2(hitquads,thisquad) >= numq) {

    if(qASCII) {
      fseeko(quadfid,qposmarker+thisquad*
	     (DIM_QUADS*(maxstarWidth+1)*sizeof(char)),SEEK_SET); 
      fscanf(quadfid,"%lu,%lu,%lu,%lu\n",&iA,&iB,&iC,&iD);
    }
    else {
      fseeko(quadfid,qposmarker+thisquad*
	     (DIM_QUADS*sizeof(iA)),SEEK_SET);
      readonequad(quadfid,&iA,&iB,&iC,&iD);
    }

    if(cASCII) {
      fseeko(catfid,cposmarker+iA*oneobjWidth*sizeof(char),SEEK_SET); 
      fscanf(catfid,"%lf,%lf,%lf\n",&tmpx,&tmpy,&tmpz);
      star_set(sA,0,tmpx); star_set(sA,1,tmpy); star_set(sA,2,tmpz);
      fseeko(catfid,cposmarker+iB*oneobjWidth*sizeof(char),SEEK_SET); 
      fscanf(catfid,"%lf,%lf,%lf\n",&tmpx,&tmpy,&tmpz);
      star_set(sB,0,tmpx); star_set(sB,1,tmpy); star_set(sB,2,tmpz);
      fseeko(catfid,cposmarker+iC*oneobjWidth*sizeof(char),SEEK_SET); 
      fscanf(catfid,"%lf,%lf,%lf\n",&tmpx,&tmpy,&tmpz);
      star_set(sC,0,tmpx); star_set(sC,1,tmpy); star_set(sC,2,tmpz);
      fseeko(catfid,cposmarker+iD*oneobjWidth*sizeof(char),SEEK_SET); 
      fscanf(catfid,"%lf,%lf,%lf\n",&tmpx,&tmpy,&tmpz);
      star_set(sD,0,tmpx); star_set(sD,1,tmpy); star_set(sD,2,tmpz);
    }
    else {
      fseeko(catfid,cposmarker+iA*(DIM_STARS*sizeof(double)),SEEK_SET);
      fread(sA->farr,sizeof(double),DIM_STARS,catfid);
      fseeko(catfid,cposmarker+iB*(DIM_STARS*sizeof(double)),SEEK_SET);
      fread(sB->farr,sizeof(double),DIM_STARS,catfid);
      fseeko(catfid,cposmarker+iC*(DIM_STARS*sizeof(double)),SEEK_SET);
      fread(sC->farr,sizeof(double),DIM_STARS,catfid);
      fseeko(catfid,cposmarker+iD*(DIM_STARS*sizeof(double)),SEEK_SET);
      fread(sD->farr,sizeof(double),DIM_STARS,catfid);
    }

    transform=fit_transform(ABCDpix,order,sA,sB,sC,sD);
    if(transform==NULL) {fprintf(stderr,"ERROR (solvey) == transform NULL\n");}
    image_to_xyz(xxmin,yymin,sMin,transform);
    image_to_xyz(xxmax,yymax,sMax,transform);

    if(hitdyv==NULL) hitdyv=mk_dyv(6);

    dyv_set(hitdyv,0,star_ref(sMin,0));
    dyv_set(hitdyv,1,star_ref(sMin,1));
    dyv_set(hitdyv,2,star_ref(sMin,2));
    dyv_set(hitdyv,3,star_ref(sMax,0));
    dyv_set(hitdyv,4,star_ref(sMax,1));
    dyv_set(hitdyv,5,star_ref(sMax,2));

    if(hitkd==NULL) {
      dyv_array *tmp=mk_dyv_array(1);
      dyv_array_set(tmp,0,hitdyv);
      hitkd=mk_kdtree_from_points(tmp,50);
      free_dyv_array(tmp);
    }
    else
      dsq=add_point_to_kdtree_dsq(hitkd,hitdyv,&whichmatch);

    if(dsq<1.0e-9) {
      fprintf(hitfid,"quad=%lu, starids(ABCD)=%lu,%lu,%lu,%lu\n",
    	       thisquad,iA,iB,iC,iD);
      fprintf(hitfid,"  dist=%.10g match=%d\n",
	      dsq,ivec_ref(hitquads,whichmatch));
      fprintf(hitfid," min xyz=(%lf,%lf,%lf) radec=(%lf,%lf)\n",
	      star_ref(sMin,0),star_ref(sMin,1),star_ref(sMin,2),
	      xy2ra(star_ref(sMin,0),star_ref(sMin,1)),
	      z2dec(star_ref(sMin,2)));
      fprintf(hitfid," max xyz=(%lf,%lf,%lf) radec=(%lf,%lf)\n",
	      star_ref(sMax,0),star_ref(sMax,1),star_ref(sMax,2),
	      xy2ra(star_ref(sMax,0),star_ref(sMax,1)),
	      z2dec(star_ref(sMax,2)));
    }

    free(transform); 
    
  }
  }

  free_star(sA);free_star(sB);free_star(sC);free_star(sD);
  free_star(sMin); free_star(sMax);
  
  return;
}





void image_to_xyz(double uu, double vv, star *s, double *transform)
{
  double length;
  if(s==NULL || transform==NULL) return;
  star_set(s,0,uu*(*(transform+0)) + 
                  vv*(*(transform+1)) + *(transform+2));
  star_set(s,1,uu*(*(transform+3)) + 
                  vv*(*(transform+4)) + *(transform+5));
  star_set(s,2,uu*(*(transform+6)) + 
                  vv*(*(transform+7)) + *(transform+8));
  length=sqrt(star_ref(s,0)*star_ref(s,0)+star_ref(s,1)*star_ref(s,1)+
	      star_ref(s,2)*star_ref(s,2));
  star_set(s,0,star_ref(s,0)/length);
  star_set(s,1,star_ref(s,1)/length);
  star_set(s,2,star_ref(s,2)/length);
  return;
}


double *fit_transform(xy *ABCDpix,char order,star *A,star *B,star *C,star *D)
{
  double det,uu,uv,vv,sumu,sumv;
  char oA=0,oB=1,oC=2,oD=3;
  double Au,Av,Bu,Bv,Cu,Cv,Du,Dv;
  double *matQ = (double *)malloc(9*sizeof(double));
  double *matR = (double *)malloc(12*sizeof(double));
  if(matQ==NULL || matR==NULL) {
    if(matQ) free(matQ);
    if(matR) free(matR);
    return(NULL);
  }

  if(order==ABCD_ORDER) {oA=0; oB=1; oC=2; oD=3;}
  if(order==BACD_ORDER) {oA=1; oB=0; oC=2; oD=3;}
  if(order==ABDC_ORDER) {oA=0; oB=1; oC=3; oD=2;}
  if(order==BADC_ORDER) {oA=1; oB=0; oC=3; oD=2;}

  // image plane coordinates of A,B,C,D
  Au=xy_refx(ABCDpix,oA); Av=xy_refy(ABCDpix,oA);
  Bu=xy_refx(ABCDpix,oB); Bv=xy_refy(ABCDpix,oB);
  Cu=xy_refx(ABCDpix,oC); Cv=xy_refy(ABCDpix,oC);
  Du=xy_refx(ABCDpix,oD); Dv=xy_refy(ABCDpix,oD);

  //fprintf(stderr,"Image ABCD = (%lf,%lf) (%lf,%lf) (%lf,%lf) (%lf,%lf)\n",
  //  	    Au,Av,Bu,Bv,Cu,Cv,Du,Dv);

  // define M to be the 3x4 matrix [Au,Bu,Cu,Du;ones(1,4)]
  // define X to be the 3x4 matrix [Ax,Bx,Cx,Dx;Ay,By,Cy,Dy;Az,Bz,Cz,Dz]

  // set Q to be the 3x3 matrix  M*M'
  uu = Au*Au + Bu*Bu + Cu*Cu + Du*Du;
  uv = Au*Av + Bu*Bv + Cu*Cv + Du*Dv;
  vv = Av*Av + Bv*Bv + Cv*Cv + Dv*Dv;
  sumu = Au+Bu+Cu+Du;  sumv = Av+Bv+Cv+Dv;
  *(matQ+0)=uu;   *(matQ+1)=uv;   *(matQ+2)=sumu;
  *(matQ+3)=uv;   *(matQ+4)=vv;   *(matQ+5)=sumv;
  *(matQ+6)=sumu; *(matQ+7)=sumv; *(matQ+8)=4.0;

  // take the inverse of Q in-place, so Q=inv(M*M')
  det = inverse_3by3(matQ);

  //fprintf(stderr,"det=%.12g\n",det);

  if(det==0.0) {
    fprintf(stderr,"ERROR (fit_transform) -- determinant zero\n");
    if(matQ) free(matQ); if(matR) free(matR); return(NULL); }

  // set R to be the 4x3 matrix M'*inv(M*M')=M'*Q
  *(matR+0) = *(matQ+0)*Au + *(matQ+3)*Av + *(matQ+6);
  *(matR+1) = *(matQ+1)*Au + *(matQ+4)*Av + *(matQ+7);
  *(matR+2) = *(matQ+2)*Au + *(matQ+5)*Av + *(matQ+8);
  *(matR+3) = *(matQ+0)*Bu + *(matQ+3)*Bv + *(matQ+6);
  *(matR+4) = *(matQ+1)*Bu + *(matQ+4)*Bv + *(matQ+7);
  *(matR+5) = *(matQ+2)*Bu + *(matQ+5)*Bv + *(matQ+8);
  *(matR+6) = *(matQ+0)*Cu + *(matQ+3)*Cv + *(matQ+6);
  *(matR+7) = *(matQ+1)*Cu + *(matQ+4)*Cv + *(matQ+7);
  *(matR+8) = *(matQ+2)*Cu + *(matQ+5)*Cv + *(matQ+8);
  *(matR+9) = *(matQ+0)*Du + *(matQ+3)*Dv + *(matQ+6);
  *(matR+10)= *(matQ+1)*Du + *(matQ+4)*Dv + *(matQ+7);
  *(matR+11)= *(matQ+2)*Du + *(matQ+5)*Dv + *(matQ+8);

  // set Q to be the 3x3 matrix X*R

  *(matQ+0) = star_ref(A,0)*(*(matR+0)) + star_ref(B,0)*(*(matR+3)) +
              star_ref(C,0)*(*(matR+6)) + star_ref(D,0)*(*(matR+9));
  *(matQ+1) = star_ref(A,0)*(*(matR+1)) + star_ref(B,0)*(*(matR+4)) +
              star_ref(C,0)*(*(matR+7)) + star_ref(D,0)*(*(matR+10));
  *(matQ+2) = star_ref(A,0)*(*(matR+2)) + star_ref(B,0)*(*(matR+5)) +
              star_ref(C,0)*(*(matR+8)) + star_ref(D,0)*(*(matR+11));

  *(matQ+3) = star_ref(A,1)*(*(matR+0)) + star_ref(B,1)*(*(matR+3)) +
              star_ref(C,1)*(*(matR+6)) + star_ref(D,1)*(*(matR+9));
  *(matQ+4) = star_ref(A,1)*(*(matR+1)) + star_ref(B,1)*(*(matR+4)) +
              star_ref(C,1)*(*(matR+7)) + star_ref(D,1)*(*(matR+10));
  *(matQ+5) = star_ref(A,1)*(*(matR+2)) + star_ref(B,1)*(*(matR+5)) +
              star_ref(C,1)*(*(matR+8)) + star_ref(D,1)*(*(matR+11));

  *(matQ+6) = star_ref(A,2)*(*(matR+0)) + star_ref(B,2)*(*(matR+3)) +
              star_ref(C,2)*(*(matR+6)) + star_ref(D,2)*(*(matR+9));
  *(matQ+7) = star_ref(A,2)*(*(matR+1)) + star_ref(B,2)*(*(matR+4)) +
              star_ref(C,2)*(*(matR+7)) + star_ref(D,2)*(*(matR+10));
  *(matQ+8) = star_ref(A,2)*(*(matR+2)) + star_ref(B,2)*(*(matR+5)) +
              star_ref(C,2)*(*(matR+8)) + star_ref(D,2)*(*(matR+11));

  free(matR);

  return(matQ);
  
}

double inverse_3by3(double *matrix)
{
  double det;
  double a11,a12,a13,a21,a22,a23,a31,a32,a33;
  double b11,b12,b13,b21,b22,b23,b31,b32,b33;

  a11=*(matrix+0); a12=*(matrix+1); a13=*(matrix+2);
  a21=*(matrix+3); a22=*(matrix+4); a23=*(matrix+5);
  a31=*(matrix+6); a32=*(matrix+7); a33=*(matrix+8);

  det = a11 * ( a22 * a33 - a23 * a32 ) +
        a12 * ( a23 * a31 - a21 * a33 ) +
        a13 * ( a21 * a32 - a22 * a31 );

  if(det!=0) {

    b11 = + ( a22 * a33 - a23 * a32 ) / det;
    b12 = - ( a12 * a33 - a13 * a32 ) / det;
    b13 = + ( a12 * a23 - a13 * a22 ) / det;

    b21 = - ( a21 * a33 - a23 * a31 ) / det;
    b22 = + ( a11 * a33 - a13 * a31 ) / det;
    b23 = - ( a11 * a23 - a13 * a21 ) / det;

    b31 = + ( a21 * a32 - a22 * a31 ) / det;
    b32 = - ( a11 * a32 - a12 * a31 ) / det;
    b33 = + ( a11 * a22 - a12 * a21 ) / det;

    *(matrix+0)=b11; *(matrix+1)=b12; *(matrix+2)=b13;
    *(matrix+3)=b21; *(matrix+4)=b22; *(matrix+5)=b23;
    *(matrix+6)=b31; *(matrix+7)=b32; *(matrix+8)=b33;

  }

  //fprintf(stderr,"transform determinant = %g\n",det);

  return(det);

}


