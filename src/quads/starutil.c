#include "starutil.h"

stararray *readcat(FILE *fid,sidx *numstars, dimension *Dim_Stars,
	   double *ramin, double *ramax, double *decmin, double *decmax)
{
  char ASCII = 0;
  sidx ii;
  magicval magic;
  fread(&magic,sizeof(magic),1,fid);
  if(magic==ASCII_VAL) {
    ASCII=1;
    fscanf(fid,"mStars=%lu\n",numstars);
    fscanf(fid,"DimStars=%hu\n",Dim_Stars);
    fscanf(fid,"Limits=%lf,%lf,%lf,%lf\n",ramin,ramax,decmin,decmax);
  }
  else {
    if(magic!=MAGIC_VAL) {
      fprintf(stderr,"ERROR (readcat) -- bad magic value in objs file.\n");
      return((stararray *)NULL);
    }
    ASCII=0;
    fread(numstars,sizeof(*numstars),1,fid);
    fread(Dim_Stars,sizeof(*Dim_Stars),1,fid);
    fread(ramin,sizeof(*ramin),1,fid);
    fread(ramax,sizeof(*ramax),1,fid);
    fread(decmin,sizeof(*decmin),1,fid);
    fread(decmax,sizeof(*decmax),1,fid);
  }
  stararray *thestars = mk_stararray(*numstars);
  for(ii=0;ii<*numstars;ii++) {
    thestars->array[ii] = mk_stard(*Dim_Stars);
    if(thestars->array[ii]==NULL) {
      fprintf(stderr,"ERROR (readcat) -- out of memory at star %lu\n",ii);
      free_stararray(thestars);
      return (stararray *)NULL;
    }
    if(ASCII) {
      if(*Dim_Stars==2)
	fscanf(fid,"%lf,%lf\n",thestars->array[ii]->farr,
  	       thestars->array[ii]->farr+1   );
      else if(*Dim_Stars==3)
	fscanf(fid,"%lf,%lf,%lf\n",thestars->array[ii]->farr,
  	       thestars->array[ii]->farr+1,
  	       thestars->array[ii]->farr+2   );
    }
    else
      fread(thestars->array[ii]->farr,sizeof(double),*Dim_Stars,fid);
  }
  return thestars;
}

star *make_rand_star(double ramin, double ramax, 
		     double decmin, double decmax)
{
#if PLANAR_GEOMETRY==1
  if(ramin<0.0) ramin=0.0;
  if(ramax>1.0) ramax=1.0;
  if(decmin<0.0) decmin=0.0;
  if(decmax>1.0) decmax=1.0;
  star *thestar = mk_star();
  if(thestar!=NULL) {
    star_set(thestar, 0, range_random(ramin,ramax));
    star_set(thestar, 1, range_random(ramin,ramax));
  }
  return thestar;
#else
  double decval,raval;
  if(ramin<0.0) ramin=0.0;
  if(ramax>(2*PIl)) ramax=2*PIl;
  if(decmin<-PIl/2.0) decmin=-PIl/2.0;
  if(decmax>PIl/2.0) decmax=PIl/2.0;
  star *thestar = mk_star();
  if(thestar!=NULL) {
    decval=asin(range_random(sin(decmin),sin(decmax)));
    raval=range_random(ramin,ramax);
    star_set(thestar, 0, cos(decval)*cos(raval));
    star_set(thestar, 1, cos(decval)*sin(raval));
    star_set(thestar, 2, sin(decval));
  }
  return thestar;
#endif
}




void star_coords(star *s,star *r,double *x,double *y)
{
#ifndef AMFAST
  if(s==NULL) {fprintf(stderr,"ERROR (star_coords) -- s NULL\n"); return;}
  if(r==NULL) {fprintf(stderr,"ERROR (star_coords) -- r NULL\n"); return;}
#endif

#if FALSE
  double chklen;
  chklen=star_ref(s,0)*star_ref(s,0)+star_ref(s,1)*star_ref(s,1)+
         star_ref(s,2)*star_ref(s,2);
  if(fabs(chklen-1.0)>0.0001) 
    fprintf(stderr,"ERROR (star_coords) -- s has length %f\n",chklen);
  chklen=star_ref(r,0)*star_ref(r,0)+star_ref(r,1)*star_ref(r,1)+
         star_ref(r,2)*star_ref(r,2);
  if(fabs(chklen-1.0)>0.0001) 
    fprintf(stderr,"ERROR (star_coords) -- r has length %f\n",chklen);
#endif

#if PLANAR_GEOMETRY==1
  *x=star_ref(s,0)-star_ref(r,0); 
  *y=star_ref(s,1)-star_ref(r,1);
  return;
#else
  double sdotr = star_ref(s,0)*star_ref(r,0) + 
                 star_ref(s,1)*star_ref(r,1) +
                 star_ref(s,2)*star_ref(r,2);
  if(sdotr<=0.0) {
 fprintf(stderr,"ERROR (star_coords) -- s dot r <=0; undefined projection.\n");
    return;
  }

  if(star_ref(r,2)==1.0) {
    *x = star_ref(s,0)/star_ref(s,2);
    *y = star_ref(s,1)/star_ref(s,2);
  }
  else if (star_ref(r,2)==-1.0) { //???
    *x = star_ref(s,0)/star_ref(s,2);
    *y = -star_ref(s,1)/star_ref(s,2);
  }
  else {
    double etax,etay,etaz,xix,xiy,xiz,eta_norm;

    // eta is a vector perpendicular to r
    etax=-star_ref(r,1); etay=+star_ref(r,0); etaz=0.0;
    eta_norm = sqrt(etax*etax+etay*etay);
    etax/=eta_norm; etay/=eta_norm;

    // xi =  r cross eta

    //xix = star_ref(r,1)*etaz-star_ref(r,2)*etay;
    xix = -star_ref(r,2)*etay;
    //xiy = star_ref(r,2)*etax-star_ref(r,0)*etaz;
    xiy = star_ref(r,2)*etax;
    xiz = star_ref(r,0)*etay-star_ref(r,1)*etax;

    *x = star_ref(s,0)*xix/sdotr + 
         star_ref(s,1)*xiy/sdotr +
         star_ref(s,2)*xiz/sdotr;
    *y = star_ref(s,0)*etax/sdotr + 
         star_ref(s,1)*etay/sdotr;
         //+star_ref(s,2)*etaz/sdotr;
  }

  return;
#endif

}



void star_midpoint(star *M,star *A,star *B)
{
#if PLANAR_GEOMETRY==1
  star_set(M,0,(star_ref(A,0)+star_ref(B,0))/2);
  star_set(M,1,(star_ref(A,1)+star_ref(B,1))/2);
  return;
#else
  double len;
  star_set(M,0,(star_ref(A,0)+star_ref(B,0))/2);
  star_set(M,1,(star_ref(A,1)+star_ref(B,1))/2);
  star_set(M,2,(star_ref(A,2)+star_ref(B,2))/2);
  len=sqrt(star_ref(M,0)*star_ref(M,0)+star_ref(M,1)*star_ref(M,1)+
	   star_ref(M,2)*star_ref(M,2));
  star_set(M,0,star_ref(M,0)/len);
  star_set(M,1,star_ref(M,1)/len);
  star_set(M,2,star_ref(M,2)/len);
  return;
#endif
}


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


