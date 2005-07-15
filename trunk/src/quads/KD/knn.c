/*
   File:        knn.c
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:38:54 EST 2004
   Description: Classic kdtree knn implementation
   Copyright (c) Carnegie Mellon University
*/

#include "knn.h"
#include "neighbors.h"

/* To understand the implementation, make sure you read neighbors.h first. */

/* Recursively searches the tree starting at node n, and updating "nbs" with 
   any points that are "owned" by node n and who are among the k nearest
   neighbors of query out of those points ownded by n and those points already 
   recorded in "nbs" on entry.

   Formally:

      nbs_exit = KNN(query,POINTS(n) UNION POINTS(nbs_entry))

   Where we define

      KNN(q,S) = a set of pindexes, and a subset of S with

              i in KNN(q,S) if and only if there are k-1 or fewer points in
                            S that are closer to q than the point associated with 
                            pindex i

      POINTS(n) = the set of pindexes "owned" by node n

      POINTS(nbs) = the set of points represented by nbs

      nbs_entry = the contents of nbs on entry to this function

      nbs_exit =  the contents of nbs on exit from this function

   Annoying warning: The "k" in "k-nearest-neighbor" is different from the "k" in "kdtree"

   query - the query point
   nbs   - the set of candidate neighbors found so far in the search
   n     - the current node in the tree

   node_query_min_dsqd must, on entry, equal the distance between query and the
   closest point in the hyper-rectangle represented by n
*/
void knn_search(dyv *query,neighbors *nbs,node *n,double node_query_min_dsqd)
{
  my_assert(node_query_min_dsqd == node_dyv_min_dsqd(n,query));

  if ( neighbors_dqsd_or_below_is_interesting(nbs) > node_query_min_dsqd )
  {
    if ( node_is_leaf(n) )
      update_neighbors_from_points_and_pindexes(nbs,query,n->points,
						n->pindexes);
    else
    {
      double child1_min_dsqd = node_dyv_min_dsqd(n->child1,query);
      double child2_min_dsqd = node_dyv_min_dsqd(n->child2,query);

      if ( child1_min_dsqd < child2_min_dsqd )
      {
	knn_search(query,nbs,n->child1,child1_min_dsqd);
	knn_search(query,nbs,n->child2,child2_min_dsqd);
      }
      else
      {
	knn_search(query,nbs,n->child2,child2_min_dsqd);
	knn_search(query,nbs,n->child1,child1_min_dsqd);
      }
    }
  }
}


/* Finds the k-nearest neighbors of query among the points represented by kdtree.

   Annoying warning: The "k" in "k-nearest-neighbor" is different from the "k" in "kdtree"

   kd    - the kdtree
   query - the query point
   k     - the number of neighbors required

   The result is a vector of pindexes (see kdtree.h for an explanation of what is
   a pindex).

   The result is sorted in order of increasing distance from the query (thus
   ivec_ref(result,0) is the pindex that is closest to the query).

   For example, if k is 2 and if result is { 9 , 15 }, that tells us that
   pindex 9 is closest to the query and pindex 15 is second closest. 
*/
ivec *mk_knn_pindexes(kdtree *kd,dyv *query,int k)
{
  neighbors *nbs = mk_empty_neighbors(k);
  ivec *pindexes;

  knn_search(query,nbs,kd->root,node_dyv_min_dsqd(kd->root,query));

  pindexes = mk_copy_ivec(nbs->rank_to_pindex);

  free_neighbors(nbs);

  return pindexes;
}


