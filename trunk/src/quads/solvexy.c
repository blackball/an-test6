#include "starutil.h"
#include "kdutil.h"
#include "fileutil.h"
#include "mathutil.h"

#define OPTIONS "hpf:o:t:m:"
const char HelpString[]=
"solvexy -f fname -o fieldname -m mtol -t tol [-p]\n"
"  tol == code_tolerance, -p flips parity\n";


extern char *optarg;
extern int optind, opterr, optopt;

#define MATCH_TOL 1.0e-9
#define MIN_NEARBY 2

qidx solve_fields(xyarray *thefields, kdtree *codekd, double codetol);
qidx try_all_codes(double Cx, double Cy, double Dx, double Dy, xy *cornerpix,
		   xy *ABCDpix, sidx iA, sidx iB, sidx iC, sidx iD,
		   kquery *kq,kdtree *codekd, qidx *numgood);
qidx resolve_matches(xy *cornerpix, kresult *krez, code *query,
	     xy *ABCDpix, char order, sidx fA, sidx fB, sidx fC, sidx fD);
void output_match(MatchObj *mo);
void output_good_matches(MatchObj *first, ivec *glist);
ivec *add_transformed_corners(star *sMin, star *sMax, 
			     qidx thisquad, kdtree **kdt);
void free_matchlist(MatchObj *first);
void find_corners(xy *thisfield, xy *cornerpix);

void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD);
void getstarcoords(star *sA, star *sB, star *sC, star *sD,
		   sidx iA, sidx iB, sidx iC, sidx iD);



char *fieldfname=NULL,*treefname=NULL,*hitfname=NULL;
char *quadfname=NULL,*catfname=NULL;
FILE *hitfid=NULL,*quadfid=NULL,*catfid=NULL;
char qASCII,cASCII;
off_t qposmarker,cposmarker;
char buff[100],maxstarWidth,oneobjWidth;

kdtree *hitkd=NULL;
kquery *matchquery;
ivec *qlist=NULL,*goodlist=NULL;
double MatchTol=MATCH_TOL;
MatchObj *lastMatch=NULL;
MatchObj *firstMatch=NULL;

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
	fieldfname = malloc(strlen(optarg)+6);
	hitfname = malloc(strlen(optarg)+6);
	sprintf(fieldfname,"%s.xyls",optarg);
	sprintf(hitfname,"%s.hits",optarg);
	break;
      case 't':
	codetol=strtod(optarg,NULL);
	break;
      case 'm':
	MatchTol=strtod(optarg,NULL);
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

  if(treefname==NULL || fieldfname==NULL || codetol<0)
    {fprintf(stderr,HelpString);return(OPT_ERR);}

  if(optind<argc) {
    for (argidx = optind; argidx < argc; argidx++)
      fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
    fprintf(stderr,HelpString);
    return(OPT_ERR);
  }


  FILE *fieldfid=NULL,*treefid=NULL;
  qidx numfields,numquads,numsolved;
  sidx numstars;
  double index_scale,ramin,ramax,decmin,decmax;
  dimension Dim_Quads,Dim_Stars;

  fprintf(stderr,"solvexy: solving fields in %s using %s\n",
	  fieldfname,treefname);

  if(ParityFlip) fprintf(stderr,"  Flipping parity.\n");
  fprintf(stderr,"  Reading fields...");fflush(stderr);
  fopenin(fieldfname,fieldfid); fnfree(fieldfname);
  xyarray *thefields = readxy(fieldfid,ParityFlip);
  fclose(fieldfid);
  if(thefields==NULL) return(1);
  numfields=(qidx)thefields->size;
  fprintf(stderr,"got %lu fields.\n",numfields);

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
  fprintf(stderr,"    (index scale = %lf arcmin)\n",rad2arcmin(index_scale));

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

  fprintf(stderr,"  Solving %lu fields (codetol=%lg,matchtol=%lg)...\n",
	  numfields,codetol,MatchTol);
  fopenout(hitfname,hitfid); fnfree(hitfname);
  numsolved=solve_fields(thefields,codekd,codetol);
		     
  fclose(hitfid); fclose(quadfid); fclose(catfid);
  fprintf(stderr,"done (solved %lu).                                  \n",
	  numsolved);

  free_xyarray(thefields); 
  free_kdtree(codekd); 

  //basic_am_malloc_report();
  return(0);
}





qidx solve_fields(xyarray *thefields, kdtree *codekd, double codetol)
{
  qidx numtries,nummatches,numgood,numsolved,numAB,ii,numxy,iA,iB,iC,iD;
  double Ax,Ay,Bx,By,Cx,Cy,Dx,Dy;
  double costheta,sintheta,scale,xxtmp;
  xy *thisfield;
  xy *ABCDpix=mk_xy(DIM_QUADS);
  xy *cornerpix=mk_xy(2);

  kquery *kq=mk_kquery("rangesearch","",KD_UNDEF,codetol,kdtree_rmin(codekd));
 matchquery=mk_kquery("rangesearch","",KD_UNDEF,sqrt(MatchTol),DEFAULT_KDRMIN);
  numsolved=dyv_array_size(thefields);

  for(ii=0;ii<dyv_array_size(thefields);ii++) {
    numtries=0; nummatches=0; numgood=0;
    thisfield=xya_ref(thefields,ii);
    numxy=xy_size(thisfield);

    fprintf(hitfid,"--------------------\n");
    fprintf(hitfid,"field %lu\n",ii); fflush(hitfid);
    find_corners(thisfield,cornerpix);
    fprintf(hitfid,"  image corners: %lf,%lf,%lf,%lf\n",
	    xy_refx(cornerpix,0),xy_refy(cornerpix,0),
	    xy_refx(cornerpix,1),xy_refy(cornerpix,1));


    if(numxy>=DIM_QUADS) { //if there are<4 objects in field, forget it

    // try ALL POSSIBLE pairs AB with all possible pairs CD
    numAB=0;
    for(iA=0;iA<(numxy-1);iA++) {
      Ax=xy_refx(thisfield,iA); Ay=xy_refy(thisfield,iA);
      xy_setx(ABCDpix,0,Ax); xy_sety(ABCDpix,0,Ay);
      for(iB=iA+1;iB<numxy;iB++) {
	Bx=xy_refx(thisfield,iB); By=xy_refy(thisfield,iB);
	xy_setx(ABCDpix,1,Bx); xy_sety(ABCDpix,1,By);
	Bx-=Ax; By-=Ay;
	scale = Bx*Bx+By*By;
	costheta=(Bx+By)/scale; sintheta=(By-Bx)/scale;
	for(iC=0;iC<(numxy-1);iC++) {
	  if(iC!=iA && iC!=iB) {
	    Cx=xy_refx(thisfield,iC); Cy=xy_refy(thisfield,iC);
	    xy_setx(ABCDpix,2,Cx); xy_sety(ABCDpix,2,Cy);
	    Cx-=Ax; Cy-=Ay;
	    xxtmp=Cx;
	    Cx=Cx*costheta+Cy*sintheta;
	    Cy=-xxtmp*sintheta+Cy*costheta;
	    if((Cx<1.0)&&(Cx>0.0)&&(Cy<1.0)&&(Cy>0.0)) { //C inside AB box?
	      for(iD=iC+1;iD<numxy;iD++) {
		if(iD!=iA && iD!=iB) {
		  Dx=xy_refx(thisfield,iD); Dy=xy_refy(thisfield,iD);
		  xy_setx(ABCDpix,3,Dx); xy_sety(ABCDpix,3,Dy);
		  Dx-=Ax; Dy-=Ay;
		  xxtmp=Dx;
		  Dx=Dx*costheta+Dy*sintheta;
		  Dy=-xxtmp*sintheta+Dy*costheta;
		  if((Dx<1.0)&&(Dx>0.0)&&(Dy<1.0)&&(Dy>0.0)) { //D inside box?
		    numtries++;                   // let's try it!
		    nummatches+=try_all_codes(Cx,Cy,Dx,Dy,cornerpix,ABCDpix,
					      iA,iB,iC,iD,kq,codekd,&numgood);
		  }}}}}}
fprintf(stderr,"    field %lu: done %lu of %lu AB pairs                \r",
		ii,++numAB,choose(numxy,2));
      }}

    if(numgood==0) {
      fprintf(hitfid,"No matches.\n");
      numsolved--;
    }
    else
      output_good_matches(firstMatch,goodlist);
    }

    fprintf(stderr,"    field %lu: tried %lu, codematch %lu, match=%lu\n",
	    ii,numtries,nummatches,numgood);

    if(hitkd!=NULL) {free_kdtree(hitkd); hitkd=NULL;}
    if(qlist!=NULL) {free_ivec(qlist); qlist=NULL;}
    if(goodlist!=NULL) {free_ivec(goodlist); goodlist=NULL;}
    free_matchlist(firstMatch); firstMatch=NULL; lastMatch=NULL;

  }
  
  free_kquery(kq);
  free_kquery(matchquery);
  free_xy(ABCDpix);
  free_xy(cornerpix);

  return numsolved;
}


qidx try_all_codes(double Cx, double Cy, double Dx, double Dy, xy *cornerpix,
		   xy *ABCDpix, sidx iA, sidx iB, sidx iC, sidx iD,
		   kquery *kq,kdtree *codekd, qidx *numgood)
{
  kresult *krez;
  code *thequery = mk_code();
  qidx nummatch=0;

  // ABCD
  code_set(thequery,0,Cx); code_set(thequery,1,Cy);
  code_set(thequery,2,Dx); code_set(thequery,3,Dy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    nummatch+=krez->count;
    *numgood+=resolve_matches(cornerpix,krez,thequery,ABCDpix,ABCD_ORDER,
			      iA,iB,iC,iD); 
  }
  free_kresult(krez);
  
  // BACD
  code_set(thequery,0,1.0-Cx); code_set(thequery,1,1.0-Cy);
  code_set(thequery,2,1.0-Dx); code_set(thequery,3,1.0-Dy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    nummatch+=krez->count;
    *numgood+=resolve_matches(cornerpix,krez,thequery,ABCDpix,BACD_ORDER,
			      iB,iA,iC,iD); 
  }
  free_kresult(krez);
  
  // ABDC
  code_set(thequery,0,Dx); code_set(thequery,1,Dy);
  code_set(thequery,2,Cx); code_set(thequery,3,Cy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    nummatch+=krez->count;
    *numgood+=resolve_matches(cornerpix,krez,thequery,ABCDpix,ABDC_ORDER,
			      iA,iB,iD,iC);
  }
  free_kresult(krez);

  // BADC
  code_set(thequery,0,1.0-Dx); code_set(thequery,1,1.0-Dy);
  code_set(thequery,2,1.0-Cx); code_set(thequery,3,1.0-Cy);
  krez = mk_kresult_from_kquery(kq,codekd,thequery);
  if(krez->count) {
    nummatch+=krez->count;
    *numgood+=resolve_matches(cornerpix,krez,thequery,ABCDpix,BADC_ORDER,
			      iB,iA,iD,iC); 
  }
  free_kresult(krez);

  free_code(thequery);

  return nummatch;
}


qidx resolve_matches(xy *cornerpix, kresult *krez, code *query,
	     xy *ABCDpix, char order, sidx fA, sidx fB, sidx fC, sidx fD)
{
  qidx ii,jj,thisquadno,numgood=0;
  sidx iA,iB,iC,iD;
  double *transform;
  star *sA,*sB,*sC,*sD,*sMin,*sMax;
  MatchObj *mo;

  sA=mk_star(); sB=mk_star(); sC=mk_star(); sD=mk_star(); 

  for(jj=0;jj<krez->count;jj++) {
    mo = mk_MatchObj();
    mo->next=NULL;
    if(firstMatch==NULL) {mo->idx=0; firstMatch=mo;}
    else {mo->idx=(lastMatch->idx)+1; lastMatch->next=mo;}
    lastMatch=mo;

    thisquadno = (qidx)krez->pindexes->iarr[jj];
    getquadids(thisquadno,&iA,&iB,&iC,&iD);
    getstarcoords(sA,sB,sC,sD,iA,iB,iC,iD);
    transform=fit_transform(ABCDpix,order,sA,sB,sC,sD);
    sMin=mk_star(); sMax=mk_star();
    image_to_xyz(xy_refx(cornerpix,0),xy_refy(cornerpix,0),sMin,transform);
    image_to_xyz(xy_refx(cornerpix,1),xy_refy(cornerpix,1),sMax,transform);

    mo->quadno=thisquadno;
    mo->iA=iA; mo->iB=iB; mo->iC=iC; mo->iD=iD;
    mo->nearlist=NULL;
    mo->sMin=sMin; mo->sMax=sMax;
    mo->fA=fA; mo->fB=fB; mo->fC=fC; mo->fD=fD;
    //mo->code_err=0.0;
    // should be |query->farr - point(krez->pindexes->iarr[jj])|^2
    mo->nearlist = add_transformed_corners(sMin,sMax,thisquadno,&hitkd);
    if(mo->nearlist!=NULL && mo->nearlist->size > MIN_NEARBY) {
      if(goodlist==NULL)
	goodlist=mk_copy_ivec(mo->nearlist);
      else 
	for(ii=0;ii<mo->nearlist->size;ii++)
	  add_to_ivec_unique(goodlist,ivec_ref(mo->nearlist,ii));
      numgood++;
    }

    free(transform); 
    //free_star(sMin); free_star(sMax); // will be freed with MatchObj
  
    
  }

  free_star(sA);free_star(sB);free_star(sC);free_star(sD);
  return(numgood);
}

ivec *add_transformed_corners(star *sMin, star *sMax, 
			     qidx thisquad, kdtree **kdt)
{
  double dist_sq;
  dyv *hitdyv;
  int tmpmatch;
  kresult *kr;
  ivec *nearlist=NULL;

  hitdyv=mk_dyv(2*DIM_STARS);
  dyv_set(hitdyv,0,star_ref(sMin,0));
  dyv_set(hitdyv,1,star_ref(sMin,1));
  dyv_set(hitdyv,2,star_ref(sMin,2));
  dyv_set(hitdyv,3,star_ref(sMax,0));
  dyv_set(hitdyv,4,star_ref(sMax,1));
  dyv_set(hitdyv,5,star_ref(sMax,2));

  if(*kdt==NULL) {
    dyv_array *tmp=mk_dyv_array(1);
    dyv_array_set(tmp,0,hitdyv);
    *kdt=mk_kdtree_from_points(tmp,DEFAULT_KDRMIN);
    free_dyv_array(tmp);
    qlist=mk_ivec_1((int)thisquad);
  }
  else {
    dist_sq=add_point_to_kdtree_dsq(*kdt,hitdyv,&tmpmatch);
    add_to_ivec(qlist,(int)thisquad);
    if((dist_sq<MatchTol) || ((qidx)ivec_ref(qlist,tmpmatch)!=thisquad)) {
        kr=mk_kresult_from_kquery(matchquery,hitkd,hitdyv);
	nearlist=mk_copy_ivec(kr->pindexes);
	free_kresult(kr);
    }
  }

  free_dyv(hitdyv);
  return(nearlist);
}


void output_match(MatchObj *mo)
{
  fprintf(hitfid,"quad=%lu\n",mo->quadno);
  fprintf(hitfid,"  starids(ABCD)=%lu,%lu,%lu,%lu\n",
	  mo->iA,mo->iB,mo->iC,mo->iD);
  fprintf(hitfid,"  field_objects(ABDC)=%lu,%lu,%lu,%lu\n",
	  mo->fA,mo->fB,mo->fC,mo->fD);
  fprintf(hitfid,"  min xyz=(%lf,%lf,%lf) radec=(%lf,%lf)\n",
	  star_ref(mo->sMin,0),star_ref(mo->sMin,1),star_ref(mo->sMin,2),
	  rad2deg(xy2ra(star_ref(mo->sMin,0),star_ref(mo->sMin,1))),
	  rad2deg(z2dec(star_ref(mo->sMin,2))));
  fprintf(hitfid,"  max xyz=(%lf,%lf,%lf) radec=(%lf,%lf)\n",
	  star_ref(mo->sMax,0),star_ref(mo->sMax,1),star_ref(mo->sMax,2),
	  rad2deg(xy2ra(star_ref(mo->sMax,0),star_ref(mo->sMax,1))),
	  rad2deg(z2dec(star_ref(mo->sMax,2))));
  /*
  int jj;
  fprintf(hitfid,"  matches");
  if(mo->nearlist!=NULL && mo->nearlist->size>MIN_NEARBY)
  for(jj=0;jj<mo->nearlist->size;jj++)
    fprintf(hitfid," %d",ivec_ref(qlist,ivec_ref(mo->nearlist,jj)));
  fprintf(hitfid,"\n");
  */
  return;

}

void output_good_matches(MatchObj *first, ivec *glist)
{
  while(first!=NULL) {
    if(is_in_ivec(glist,first->idx))
      output_match(first);
    first=first->next;
  }
  return;
}


void free_matchlist(MatchObj *first)
{
  MatchObj *mo,*tmp;
  mo=first;
  while(mo!=NULL) {
    free_star(mo->sMin);
    free_star(mo->sMax);
    if(mo->nearlist!=NULL) free_ivec(mo->nearlist);
    tmp=mo->next;
    free_MatchObj(mo);
    mo=tmp;
  }
  return;
}



void find_corners(xy *thisfield, xy *cornerpix)
{
  double xxtmp,yytmp;
  qidx jj;

  // find min and max coordinates in this field
  
  xxtmp=xy_refx(thisfield,0); yytmp=xy_refy(thisfield,0);
  xy_setx(cornerpix,0,xxtmp); xy_sety(cornerpix,0,yytmp);
  xy_setx(cornerpix,1,xxtmp); xy_sety(cornerpix,1,yytmp);

  for(jj=0;jj<xy_size(thisfield);jj++) {
    xxtmp=xy_refx(thisfield,jj);yytmp=xy_refy(thisfield,jj);
    if(xxtmp<xy_refx(cornerpix,0)) xy_setx(cornerpix,0,xxtmp);
    if(yytmp<xy_refy(cornerpix,0)) xy_sety(cornerpix,0,yytmp);
    if(xxtmp>xy_refx(cornerpix,1)) xy_setx(cornerpix,1,xxtmp);
    if(yytmp>xy_refy(cornerpix,1)) xy_sety(cornerpix,1,yytmp);
  }

  return;

}



void getquadids(qidx thisquad, sidx *iA, sidx *iB, sidx *iC, sidx *iD)
{
  if(qASCII) {
    fseeko(quadfid,qposmarker+thisquad*
	   (DIM_QUADS*(maxstarWidth+1)*sizeof(char)),SEEK_SET); 
    fscanfonequad(quadfid,iA,iB,iC,iD);
  }
  else {
    fseeko(quadfid,qposmarker+thisquad*
	   (DIM_QUADS*sizeof(iA)),SEEK_SET);
    readonequad(quadfid,iA,iB,iC,iD);
  }
  return;
}


void getstarcoords(star *sA, star *sB, star *sC, star *sD,
		   sidx iA, sidx iB, sidx iC, sidx iD)
{
  double tmpx,tmpy,tmpz;

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
      freadstar(sA,catfid);
      fseeko(catfid,cposmarker+iB*(DIM_STARS*sizeof(double)),SEEK_SET);
      freadstar(sB,catfid);
      fseeko(catfid,cposmarker+iC*(DIM_STARS*sizeof(double)),SEEK_SET);
      freadstar(sC,catfid);
      fseeko(catfid,cposmarker+iD*(DIM_STARS*sizeof(double)),SEEK_SET);
      freadstar(sD,catfid);
    }
    return;
}
