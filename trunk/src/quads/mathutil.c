#include "mathutil.h"

unsigned long int choose(unsigned int nn,unsigned int mm)
{
  if(nn<=0) return 0;
  else if(mm<=0) return 0;
  else if(mm>nn) return 0;
  else if(mm==1) return nn;
  unsigned int rr=1;
  unsigned int qq;
  for(qq=nn;qq>(nn-mm);qq--) rr *= qq;
  for(qq=mm;qq>1;qq--) rr/=qq;
  return rr;
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
    fprintf(stderr,"ERROR (solvey) == memory error in fit_transform\n");
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
  if(det<0) fprintf(stderr,"WARNING (fit_transform) -- determinant<0\n");

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


