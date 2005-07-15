/*
   File:        rangesearch.c
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:38:54 EST 2004
   Description: Classic kdtree rangesearch implementation
   Copyright (c) Carnegie Mellon University
*/

#include "rangesearch.h"

void rangesearch_node(dyv *query,node *n,double range_dsqd,
		      ivec *result_pindexes,dyv *result_dsqd_array)
{
  double min_poss_dsqd = node_dyv_min_dsqd(n,query);

  if ( min_poss_dsqd < range_dsqd )
  {
    if ( node_is_leaf(n) )
    {
      int i;
      for ( i = 0 ; i < n->num_points ; i++ )
      {
	dyv *point = dyv_array_ref(n->points,i);
	double dsqd = dyv_dyv_dsqd(query,point);
	if ( dsqd < range_dsqd )
	{
	  int pindex = ivec_ref(n->pindexes,i);
	  add_to_ivec(result_pindexes,pindex);
	  add_to_dyv(result_dsqd_array,dsqd);
	}
      }
    }
    else
    {
      rangesearch_node(query,n->child1,range_dsqd,result_pindexes,
		       result_dsqd_array);
      rangesearch_node(query,n->child2,range_dsqd,result_pindexes,
		       result_dsqd_array);
    }
  }
}

ivec *mk_rangesearch_pindexes(kdtree *kd,dyv *query,double range)
{
  ivec *unsorted_pindexes = mk_ivec(0);
  dyv *dsqd_array = mk_dyv(0);
  double range_dsqd = range * range;
  ivec *indexes;
  ivec *sorted_pindexes;

  rangesearch_node(query,kd->root,range_dsqd,unsorted_pindexes,dsqd_array);

  indexes = mk_indices_of_sorted_dyv(dsqd_array);
  sorted_pindexes = mk_ivec_subset(unsorted_pindexes,indexes);

  free_ivec(unsorted_pindexes);
  free_dyv(dsqd_array);
  free_ivec(indexes);

  return sorted_pindexes;
}


