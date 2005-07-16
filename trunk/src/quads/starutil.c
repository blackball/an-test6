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

/* Write kdtree to disk */
unsigned int fwrite_kdtree(kdtree *kdt, FILE *fid)
{
  int pointdim=kdtree_num_dims(kdt);
  fwrite(&(kdt->num_nodes),sizeof(kdt->num_nodes),1,fid);
  if(kdt->num_nodes==0) return(0);
  fwrite(&(kdt->max_depth),sizeof(kdt->max_depth),1,fid);
  fwrite(&(kdt->rmin),sizeof(kdt->rmin),1,fid);
  fwrite(&pointdim,sizeof(pointdim),1,fid);
  return fwrite_node(kdt->root,fid);
}

/* Read kdtree from disk */
kdtree *fread_kdtree(FILE *fid)
{
  int numnodes;
  int pointdim;
  fread(&numnodes,sizeof(numnodes),1,fid);
  if(numnodes==0) return((kdtree *)NULL);
  kdtree *kdt = AM_MALLOC(kdtree);
  kdt->num_nodes=numnodes;
  fread(&(kdt->max_depth),sizeof(kdt->max_depth),1,fid);
  fread(&(kdt->rmin),sizeof(kdt->rmin),1,fid);
  fread(&pointdim,sizeof(pointdim),1,fid);
  kdt->root = fread_node(pointdim,fid);
  return(kdt);
}


unsigned int fwrite_node(node *n,FILE *fid)
{
  if(n==NULL) {
    int nullsize=0;
    fwrite(&nullsize,sizeof(int),1,fid);
    return(0);
  }
  else {
    unsigned int c1=0,c2=0,ii;
    int numpoints=n->num_points;
    char isleaf=node_is_leaf(n);
    if(!isleaf) numpoints=-numpoints;
    fwrite(&numpoints,sizeof(numpoints),1,fid);
    fwrite_dyv(n->box->lo,fid);
    fwrite_dyv(n->box->hi,fid);
    if(isleaf) {
      fwrite_ivec(n->pindexes,fid);
      for(ii=0;ii<dyv_array_size(n->points);ii++)
	fwrite_dyv(dyv_array_ref(n->points,ii),fid);
    }
    else {
      c1=fwrite_node(n->child1,fid);
      c2=fwrite_node(n->child2,fid);
    }
    return(c1+c2+1);
  }
}

node *fread_node(int pointdim,FILE *fid)
{
  int numpoints;
  unsigned int ii;
  char isleaf=1;
  fread(&numpoints,sizeof(numpoints),1,fid);
  if(numpoints==0) return((node *)NULL);
  if(numpoints<0) {
    numpoints=-numpoints; 
    isleaf=0;
  }

  node *n=AM_MALLOC(node); //?? should check if it is null
  n->num_points=numpoints;
  dyv *boxlo = mk_dyv(pointdim); //?? should check if it is null
  dyv *boxhi = mk_dyv(pointdim); //?? should check if it is null
  fread_dyv(boxlo,fid);
  fread_dyv(boxhi,fid);
  n->box = mk_hrect(boxlo,boxhi); //?? should check if it is null
  free_dyv(boxlo);
  free_dyv(boxhi);
  if(isleaf) {
    n->pindexes=mk_ivec(n->num_points); //?? should check if it is null
    if(n->pindexes==NULL) 
      fprintf(stderr,"ERROR (fread_node) -- cannot alloc pindexes in new leaf node\n");
    fread_ivec(n->pindexes,fid);
    n->points=mk_dyv_array(n->num_points);  //?? should check if it is null
    for(ii=0;ii<(n->num_points);ii++) {
      dyv_array_set_no_copy(n->points,ii,mk_dyv(pointdim)); //?? check if null
      fread_dyv(dyv_array_ref(n->points,ii),fid);
    }
    n->child1=(node *)NULL;
    n->child2=(node *)NULL;
  }
  else {
    n->pindexes=(ivec *)NULL;
    n->points=(dyv_array *)NULL;
    n->child1=fread_node(pointdim,fid);
    n->child2=fread_node(pointdim,fid);
    if(n->child1==NULL) 
      fprintf(stderr,"ERROR (fread_node) -- read null child1\n");
    if(n->child2==NULL) 
      fprintf(stderr,"ERROR (fread_node) -- read null child2\n");
  }
  return(n);
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
  *x=star_ref(s,0); 
  *y=star_ref(s,1);
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

#endif

  return;


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



void free_kdtreedebug(kdtree *x)
{
  free_nodedebug(x->root);
  AM_FREE(x,kdtree);
}


void free_nodedebug(node *x)
{
  if ( node_is_leaf(x) )
  {
    fprintf(stderr,"freeing leaf node with %d points\n",x->num_points);
    fflush(stderr);
    my_assert(x->child1 == NULL);
    my_assert(x->child2 == NULL);
    fprintf(stderr,"  freeing ivec (size %d)\n",ivec_size(x->pindexes));
    fflush(stderr);
    free_ivec(x->pindexes);
    fprintf(stderr,"  freeing dyv_array (size %d)\n",dyv_array_size(x->points));
    fflush(stderr);
    free_dyv_array(x->points);
  }
  else
  {
    fprintf(stderr,"freeing nonleaf node with %d points\n",x->num_points);
    fflush(stderr);
    my_assert(x->pindexes == NULL);
    my_assert(x->points == NULL);
    fprintf(stderr,"  freeing child1 (%d points)\n",x->child1->num_points);
    fflush(stderr);
    free_nodedebug(x->child1);
    fprintf(stderr,"  freeing child2 (%d points)\n",x->child2->num_points);
    fflush(stderr);
    free_nodedebug(x->child2);
  }

  free_hrect(x->box);

  AM_FREE(x,node);
}


