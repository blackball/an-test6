/*
   File:        kdtree.c
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:35:27 EST 2004
   Description: Simple kdtree implementation
   Copyright (c) Carnegie Mellon University
*/

#include "kdtree.h"

/* Returns TRUE iff n is a leaf (i.e. has no children) */
bool slow_node_is_leaf(node *n)
{
  bool result = n->child1 == NULL;

#ifdef DOING_ASSERTS
  if ( result )
  {
    my_assert(n->child2 == NULL);
    my_assert(n->pindexes != NULL);
    my_assert(n->points != NULL);
  }
  else
  {
    my_assert(n->child2 != NULL);
    my_assert(n->pindexes == NULL);
    my_assert(n->points == NULL);
  }
#endif

  return result;
}

/* Frees x and all its subfields (including, recursively all its descendent
   nodes if it has any) */
void free_node(node *x)
{
  if ( node_is_leaf(x) )
  {
    my_assert(x->child1 == NULL);
    my_assert(x->child2 == NULL);
    free_ivec(x->pindexes);
    free_dyv_array(x->points);
  }
  else
  {
    my_assert(x->pindexes == NULL);
    my_assert(x->points == NULL);
    free_node(x->child1);
    free_node(x->child2);
  }

  free_hrect(x->box);

  AM_FREE(x,node);
}

/* Prints node x in standard Auton style */
void fprintf_node(FILE *s,char *m1,node *x,char *m2)
{
  char *buff;

  buff = mk_printf("%s -> box",m1);
  fprintf_hrect(s,buff,x->box,m2);
  free_string(buff);

  buff = mk_printf("%s -> num_points",m1);
  fprintf_int(s,buff,x->num_points,m2);
  free_string(buff);

  if ( node_is_leaf(x) )
  {
    buff = mk_printf("%s -> pindexes",m1);
    fprintf_ivec(s,buff,x->pindexes,m2);
    free_string(buff);

    buff = mk_printf("%s -> points",m1);
    fprintf_dyv_array(s,buff,x->points,m2);
    free_string(buff);
  }
  else
  {
    buff = mk_printf("%s -> child1",m1);
    fprintf_node(s,buff,x->child1,m2);
    free_string(buff);

    buff = mk_printf("%s -> child2",m1);
    fprintf_node(s,buff,x->child2,m2);
    free_string(buff);
  }
}

/* Prints x to stdout */
void pnode(node *x)
{
  fprintf_node(stdout,"node",x,"\n");
}

/* Makes a copy of "old" (and all subcomponents are copied recursively) */
node *mk_copy_node(node *old)
{
  node *nu = AM_MALLOC(node);

  nu -> box = mk_copy_hrect(old->box);
  nu -> pindexes = (old->pindexes==NULL) ? NULL : mk_copy_ivec(old->pindexes);
  nu -> points = (old->points == NULL) ? NULL : mk_copy_dyv_array(old->points);
  nu -> child1 = (old->child1 == NULL) ? NULL : mk_copy_node(old->child1);
  nu -> child2 = (old->child2 == NULL) ? NULL : mk_copy_node(old->child2);
  nu -> num_points = old -> num_points;

  return nu;
}

/* Returns the maximum depth below this node. If this node is
   a leaf the result is 1 */
int max_depth_below_node(node *n)
{
  return (n==NULL) ? 0 : (1 + int_max(max_depth_below_node(n->child1),
				      max_depth_below_node(n->child2)));
}

/* Returns 1 + number of descendents of n. Returns 1 if n is a leaf node. */
int num_nodes_at_or_below_node(node *n)
{
  int result = 1;

  if ( !node_is_leaf(n) )
  {
    result += num_nodes_at_or_below_node(n->child1);
    result += num_nodes_at_or_below_node(n->child2);
  }

  return result;
}
    
/* Assert: on entry split_dim is widest dim of box */
/*
   Make sure you have read about pindexes in kdtree.h

   dyv_array_ref(pindex_to_point,pi) is a dyv of size k representing the pi'th
                                     datapoint in our input dataset.

   pindexes is the set of point indexes for the "current node" in a tree

   "box" is the bounding box for the set of points denoted by "pindexes"

   split_dim is the dimension (the coordinate-axis-number in k-dimensional space)
   of the widest dimension of "box". Thus

     0 <= split_dim < k (where the points are in k-dimensional space)
     split_dim == argmax_i hrect_width(box,i)

     *r_child1_pindexes and *r_child2_pindexes are both call by reference return
     values. This function allocates their memory and returns them. This is just
     a standard C mechanism for returning multiple values. See the call to
     make_child_pindexes to see how its used.

   On exit

     pindexes = r_child1_pindexes UNION     r_child2_pindexes
     empty    = r_child1_pindexes INTERSECT r_child2_pindexes

     i is in r_child1_pindexes if and only if pindex "i" has been award to child 1
     i is in r_child1_pindexes if and only if pindex "i" has been award to child 2

  PRECONDITION: This function may not be called if all dimensions of the box have
                width zero (i.e. if all points referred to in "pindexes" are on top
                of each other)
*/
void make_child_pindexes(dyv_array *pindex_to_point,ivec *pindexes,hrect *box,
			 int split_dim,
			 ivec **r_child1_pindexes,ivec **r_child2_pindexes)
{
  int i;
  double split_val = hrect_middle_ref(box,split_dim);

  my_assert(split_dim == hrect_widest_dim(box));

  *r_child1_pindexes = mk_ivec(0);
  *r_child2_pindexes = mk_ivec(0);

  for ( i = 0 ; i < ivec_size(pindexes) ; i++ )
  {
    int pindex = ivec_ref(pindexes,i);
    dyv *point = dyv_array_ref(pindex_to_point,pindex);
    double val = dyv_ref(point,split_dim);

    if ( val <= split_val )
      add_to_ivec(*r_child1_pindexes,pindex);
    else
      add_to_ivec(*r_child2_pindexes,pindex);
  }

  my_assert(ivec_size(*r_child1_pindexes) > 0);
  my_assert(ivec_size(*r_child2_pindexes) > 0);
}

/* Recursively makes a node and all its children (if it deserves to have any children) */
/* 
   dyv_array_ref(pindex_to_point,pi) is a dyv of size k representing the pi'th
                                     datapoint in our input dataset.

   pindexes is the set of point indexes for the "current node" in a tree

   rmin is the leaf-decision parameter. A node will be a leaf if either
    (a) It has "rmin" or fewer points
    (b) All its points are in the same location.

    *r_nodes_so_far is a call by reference integer recording how many nodes
    have been created so far in the creation of the tree. 

    *r_nodes_so_far on exit == *r_nodes_so_far on entry  +  number created by this call
*/
node *mk_node_from_pindexes(dyv_array *pindex_to_point,ivec *pindexes,int rmin,
			    int *r_nodes_so_far)
{
  node *n = AM_MALLOC(node);
  int split_dim = -777;
  int num_points = ivec_size(pindexes);
  bool leaf = num_points <= rmin;

  n -> box = mk_hrect_bounding_dyv_array_rows(pindex_to_point,pindexes);

  if ( !leaf )
  {
    split_dim = hrect_widest_dim(n->box);

    my_assert(hrect_width_ref(n->box,split_dim) >= 0.0);

    if ( hrect_width_ref(n->box,split_dim) == 0.0 )
      leaf = TRUE;
  }

  /*
  printf("num_points = %d, widest_width = %g, rmin = %d, leaf = %d\n",
	 num_points,hrect_width_ref(n->box,split_dim),rmin,leaf);
  */

  n -> num_points = num_points;

  my_assert(n->num_points > 0);
  
  n -> pindexes = NULL;
  n -> points = NULL;
  n -> child1 = NULL;
  n -> child2 = NULL;

  *r_nodes_so_far += 1;

  if ( leaf )
  {
    n -> pindexes = mk_copy_ivec(pindexes);
    n -> points = mk_dyv_array_subset(pindex_to_point,pindexes);
  }
  else
  {
    ivec *child1_pindexes;
    ivec *child2_pindexes;

    make_child_pindexes(pindex_to_point,pindexes,n->box,split_dim,
			&child1_pindexes,&child2_pindexes);

/*     if ( is_power_of_two(*r_nodes_so_far) ) */
/*       printf("Made %d kdtree nodes\n",*r_nodes_so_far); */

    n -> child1 = mk_node_from_pindexes(pindex_to_point,child1_pindexes,rmin,
					r_nodes_so_far);
    n -> child2 = mk_node_from_pindexes(pindex_to_point,child2_pindexes,rmin,
					r_nodes_so_far);

    free_ivec(child1_pindexes);
    free_ivec(child2_pindexes);
  }

  return n;
}

/* Frees kdtree and all its components (including all its nodes and their contents) */
void free_kdtree(kdtree *x)
{
  free_node(x->root);
  AM_FREE(x,kdtree);
}

/* Prints the structure using the standard Auton printing convention */
void fprintf_kdtree(FILE *s,char *m1,kdtree *x,char *m2)
{
  char *buff;

  buff = mk_printf("%s -> root",m1);
  fprintf_node(s,buff,x->root,m2);
  free_string(buff);

  buff = mk_printf("%s -> num_nodes",m1);
  fprintf_int(s,buff,x->num_nodes,m2);
  free_string(buff);

  buff = mk_printf("%s -> max_depth",m1);
  fprintf_int(s,buff,x->max_depth,m2);
  free_string(buff);

  buff = mk_printf("%s -> rmin",m1);
  fprintf_int(s,buff,x->rmin,m2);
  free_string(buff);
}

/* Simple print to stdout. Useful to call in the debugger to see pretty-printed
   dump of tree */
void pkdtree(kdtree *x)
{
  fprintf_kdtree(stdout,"kdtree",x,"\n");
}

/* Makes a complete copy of old and all its subcomponents. */
kdtree *mk_copy_kdtree(kdtree *old)
{
  kdtree *nu = AM_MALLOC(kdtree);

  nu -> root = mk_copy_node(old->root);
  nu -> num_nodes = old -> num_nodes;
  nu -> max_depth = old -> max_depth;
  nu -> rmin = old -> rmin;

  return nu;
}

/* Builds and returns a kdtree.

   dyv_array_ref(pindex_to_point,pi) is a dyv of size k representing the pi'th
                                     datapoint in our input dataset.

   rmin is the leaf-decision parameter. A node will be a leaf if either
    (a) It has "rmin" or fewer points
    (b) All its points are in the same location.
*/
kdtree *mk_kdtree_from_points(dyv_array *pindex_to_point,int rmin)
{
  kdtree *kd = AM_MALLOC(kdtree);
  int num_points = dyv_array_size(pindex_to_point);
  ivec *pindexes = mk_identity_ivec(num_points);
  int num_nodes = 0;

  kd -> root = mk_node_from_pindexes(pindex_to_point,pindexes,rmin,&num_nodes);

  //printf("Finished. Made a total of %d kdtree nodes\n",num_nodes);

  kd -> num_nodes = num_nodes_at_or_below_node(kd->root);

  my_assert(num_nodes == kd->num_nodes);

  kd -> max_depth = max_depth_below_node(kd->root);
  kd -> rmin = rmin;

  free_ivec(pindexes);
  
  return kd;
}

/* Returns the depth of the tree. If the tree is just a root node, returns 1 */
int kdtree_max_depth(kdtree *kd)
{
  return kd->max_depth;
}

/* Returns "k", the number of dimensions of the datapoints in kd */
int kdtree_num_dims(kdtree *kd)
{
  return hrect_size(kd->root->box);
}

/* Returns the rmin parameter that had been used to build the tree. */
int kdtree_rmin(kdtree *kd)
{
  return kd -> rmin;
}

/* Returns the number of points "R" in the dataset represented by the tree */
int kdtree_num_points(kdtree *kd)
{
  return kd -> root -> num_points;
}

/* Returns the number of nodes the tree. If rmin == 1 and no points are exactly
   on top of each other, this is 2R-1 (where R = number of points in dataset) */
int kdtree_num_nodes(kdtree *kd)
{
  return kd->num_nodes;
}

/* The minimum squared distance from x to any point in n */
double node_dyv_min_dsqd(node *n,dyv *x)
{
  return hrect_dyv_min_dsqd(n->box,x);
}

/* The maximum squared distance from x to any point in n */
double node_dyv_max_dsqd(node *n,dyv *x)
{
  return hrect_dyv_max_dsqd(n->box,x);
}

/* Prints (to stdout) the vital statistics about the tree */
void explain_kdtree(kdtree *kd)
{
  printf("%15s = %6d\n","max_depth",kdtree_max_depth(kd));
  printf("%15s = %6d\n","num_dims",kdtree_num_dims(kd));
  printf("%15s = %6d\n","rmin",kdtree_rmin(kd));
  printf("%15s = %6d\n","num_points",kdtree_num_points(kd));
  printf("%15s = %6d\n","num_nodes",kdtree_num_nodes(kd));
}

/* All the points in the tree rooted at node n have pindexes from
   0 to n->num_points-1. Replace these pindexes with indexes into
   a larger list. That is, point->pindex = p implies that now
   point->pindex = new_pindex_list[p].

   This function makes it possible to create a subtree and then
   incorporate it with a larger subtree with different pindexes.

   Implemented by Nidhi Kalra. For info/bugs, email nidhi@ri.cmu.edu
*/
void replace_pindexes(node *subtree, ivec *new_pindexes){

  int i, old_pindex, new_pindex;
  my_assert(subtree->num_points <= ivec_size(new_pindexes));
 
  /* If we've reached a node, replace indices and return */
  if ( node_is_leaf(subtree) ){
	for(i=0; i<subtree->num_points; i++){
	  old_pindex = ivec_ref(subtree->pindexes, i);
	  new_pindex = ivec_ref(new_pindexes, old_pindex);
	  ivec_set(subtree->pindexes, i, new_pindex);
	}
	return;
  }

  /* Otherwise, recurse on children */
  replace_pindexes(subtree->child1, new_pindexes);
  replace_pindexes(subtree->child2, new_pindexes);
}


/* Add a new point to the KD tree. This method does not check for but may
   cause a tree imbalance. It is suitable for adding random points into the
   tree. To rebuild a badly balanced tree, delete the tree and build it from
   scratch:

  dyv_array *pindex_to_point = mk_pindex_to_point_from_kdtree(kd);
  ivec *pindexes = mk_identity_ivec(kdtree_num_points(kd));
  free_node(kd->root);
  kd->root = mk_node_from_pindexes(pindex_to_point, pindexes, rmin,
			    int *r_nodes_so_far);
   
   Implemented by Nidhi Kalra. For info/bugs email nidhi@ri.cmu.edu */
void add_point_to_kdtree(kdtree *kd, dyv *x){
  
   node *closest_to_x = kd->root, **ptr_loc_to_closest=&kd->root, *closest_to_x_split;
   double child1_min_dsqd, child2_min_dsqd;
   int num_nodes;
   ivec *tmp_pindexes;

   /* Find the nearest node n to the query x. Increase the number
	  of points owned by all the nodes and the size of the hrect
	  on the way to n. Keep track of the location of the pointer
	  to the leaf (&parent->child) use in replacing that child
	  with a new child.
   */
  while ( !node_is_leaf(closest_to_x) ){
	closest_to_x->num_points++;
	maybe_expand_hrect(closest_to_x->box,x);

	child1_min_dsqd = node_dyv_min_dsqd(closest_to_x->child1, x);
	child2_min_dsqd = node_dyv_min_dsqd(closest_to_x->child2, x);
	
	if ( child1_min_dsqd < child2_min_dsqd ){
	  ptr_loc_to_closest = &closest_to_x->child1;
	  closest_to_x = closest_to_x->child1;
	}else{
	  ptr_loc_to_closest = &closest_to_x->child2;
	  closest_to_x = closest_to_x->child2;
	}
  }

   /* Now, add the query to this node by
	  1) increasing the number of nodes
	  2) listing it in the ivec
	  3) and adding it to the list of points for this node
	  4) adjusting the hrect to include this point
   */
  closest_to_x->num_points++;
  add_to_ivec(closest_to_x->pindexes, kd->root->num_points-1);

  add_to_dyv_array(closest_to_x->points, x);
  maybe_expand_hrect(closest_to_x->box, x);

  /* If we've exceeded the number of points allowed per leaf,
	 split the current leaf into two. Free the node closest_to_x
	 and replace the pointer to it by a pointer to the new node.
  */
  if (closest_to_x->num_points > kd->rmin){
	num_nodes = 0;
	tmp_pindexes = mk_identity_ivec(closest_to_x->num_points);
	closest_to_x_split = mk_node_from_pindexes(closest_to_x->points, 
					   tmp_pindexes, kd->rmin, &num_nodes);
	replace_pindexes(closest_to_x_split, closest_to_x->pindexes);
	free_node(closest_to_x);
	free_ivec(tmp_pindexes);
	*ptr_loc_to_closest = closest_to_x_split;

	kd->num_nodes+=(num_nodes-1);
	kd->max_depth=max_depth_below_node(kd->root);

  }
}



double add_point_to_kdtree_dsq(kdtree *kd, dyv *x, int *nearestpindex)
{

  // modified by STR to return distance of point to nearest neighbour
  
  node *closest_to_x = kd->root, **ptr_loc_to_closest=&kd->root, *closest_to_x_split;
  double child1_min_dsqd, child2_min_dsqd;
  int num_nodes;
  ivec *tmp_pindexes;
  double insertdist=-1.0,tmp,tmp2;
  dyv *thispoint;
  int ii,jj;

   /* Find the nearest node n to the query x. Increase the number
	  of points owned by all the nodes and the size of the hrect
	  on the way to n. Keep track of the location of the pointer
	  to the leaf (&parent->child) use in replacing that child
	  with a new child.
   */
  while ( !node_is_leaf(closest_to_x) ){
	closest_to_x->num_points++;
	maybe_expand_hrect(closest_to_x->box,x);

	child1_min_dsqd = node_dyv_min_dsqd(closest_to_x->child1, x);
	child2_min_dsqd = node_dyv_min_dsqd(closest_to_x->child2, x);
	
	if ( child1_min_dsqd < child2_min_dsqd ){
	  ptr_loc_to_closest = &closest_to_x->child1;
	  closest_to_x = closest_to_x->child1;
	}else{
	  ptr_loc_to_closest = &closest_to_x->child2;
	  closest_to_x = closest_to_x->child2;
	}
  }

   /* Now, add the query to this node by
          0) finding its NN
	  1) increasing the number of nodes
	  2) listing it in the ivec
	  3) and adding it to the list of points for this node
	  4) adjusting the hrect to include this point
   */

  for(ii=0;ii<closest_to_x->num_points;ii++) {
    tmp=0.0;
    thispoint = dyv_array_ref(closest_to_x->points,ii);
    for(jj=0;jj<x->size;jj++) {
      tmp2=dyv_ref(thispoint,jj)-dyv_ref(x,jj);
      tmp2=tmp2*tmp2;
      tmp+=tmp2;
    }
    if(ii==0) {insertdist=tmp; *nearestpindex=0;}
    else if(tmp<insertdist) {insertdist=tmp; *nearestpindex=ii;}
  }

  closest_to_x->num_points++;
  add_to_ivec(closest_to_x->pindexes, kd->root->num_points-1);

  add_to_dyv_array(closest_to_x->points, x);
  maybe_expand_hrect(closest_to_x->box, x);

  /* If we've exceeded the number of points allowed per leaf,
	 split the current leaf into two. Free the node closest_to_x
	 and replace the pointer to it by a pointer to the new node.
  */
  if (closest_to_x->num_points > kd->rmin){
	num_nodes = 0;
	tmp_pindexes = mk_identity_ivec(closest_to_x->num_points);
	closest_to_x_split = mk_node_from_pindexes(closest_to_x->points, 
					   tmp_pindexes, kd->rmin, &num_nodes);
	replace_pindexes(closest_to_x_split, closest_to_x->pindexes);
	free_node(closest_to_x);
	free_ivec(tmp_pindexes);
	*ptr_loc_to_closest = closest_to_x_split;

	kd->num_nodes+=(num_nodes-1);
	kd->max_depth=max_depth_below_node(kd->root);

  }

  return insertdist;
}

