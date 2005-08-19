#include "kdutil.h"

void set_array_ptrs_below_node(node *n,dyv_array *da);

/* Write kdtree to disk */
unsigned int fwrite_kdtree(kdtree *kdt, FILE *fid)
{
  int numnodes,pointdim;
  if(kdt==NULL) numnodes=0; else numnodes=kdt->num_nodes;
  fwrite(&numnodes,sizeof(numnodes),1,fid);
  if(numnodes==0) return(0);
  fwrite(&(kdt->max_depth),sizeof(kdt->max_depth),1,fid);
  fwrite(&(kdt->rmin),sizeof(kdt->rmin),1,fid);
  pointdim=kdtree_num_dims(kdt);
  fwrite(&pointdim,sizeof(pointdim),1,fid);
  return fwrite_node(kdt->root,fid);
}

/* Read kdtree from disk */
kdtree *fread_kdtree(FILE *fid)
{
  int numnodes,pointdim;
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

/* write a node AND ALL THE NODES BELOW IT to disk*/
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

/* read a node AND ALL NODES BELOW IT from disk */
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


/* make a flat array of points by traversing a KD tree and taking
   the points field out of each leaf node struct 
   just makes an array object and points to the same dyvs as the
   KD tree is pointing to; DOES NOT copy the dyv's                */
dyv_array *mk_dyv_array_from_kdtree(kdtree *kd)
{
  if(kd->root->num_points==0) return (dyv_array *)NULL;
  dyv_array *da=AM_MALLOC( dyv_array);
  if(da==NULL) return (dyv_array *)NULL;
  da->size = kd->root->num_points;
  da->array_size = da->size;
  da->array = AM_MALLOC_ARRAY( dyv_ptr, da->size);
  if(da->array==NULL) {AM_FREE(da,dyv_array); return (dyv_array *)NULL;}
  set_array_ptrs_below_node(kd->root,da);
  return da;
}

void set_array_ptrs_below_node(node *n,dyv_array *da)
{
  if(n==NULL) return;
  unsigned int ii;
  if(node_is_leaf(n)) {
    for(ii=0;ii<dyv_array_size(n->points);ii++)
      da->array[ivec_ref(n->pindexes,ii)]=dyv_array_ref(n->points,ii);
  }
  else {
    set_array_ptrs_below_node(n->child1,da);
    set_array_ptrs_below_node(n->child2,da);
  }
  return;
}


/* free a flat array of points originally made by traversing a KD tree 
   just frees the array object but none of the dyv's it points to */
void free_dyv_array_from_kdtree(dyv_array *da)
{
  AM_FREE_ARRAY(da->array,dyv_ptr,da->array_size);
  AM_FREE(da,dyv_array);
  return;
}



/*

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


*/
