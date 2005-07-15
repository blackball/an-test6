/*
   File:        neighbors.h
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 18:05:53 EST 2004
   Description: Queue of neighbors represented as (pindex,dsqd) pairs
   Copyright (c) Carnegie Mellon University
*/

#ifndef neighbors_H
#define neighbors_H

/* Includes the "neighbors" structure that keeps track of the
   list of best-candidates-so-far of the k-nearest neighbors to a
   query point within a kd-tree.  The "neighbors" structure is
   updated with an ever-improving list of candidates during
   knn_search execution, until ultimately the actual k-nearest
   neighbors are found.  (Refer to knn.h and knn.c for details
   of the k-nearest neighbor search algorithm.)
*/

#include "distances.h"

/* In the neighbors structure template:

       The so-far-best nearest neighbors are ranked with 0 being
       the nearest to the query point, 1 being the next nearest,
       and so forth, up to a maximum rank of k - 1, for a maximum
       of k ranked nearest neighbors.  (It is possible for fewer than
       k points to be ranked.  For example, at the beginning of the
       knn search, no points are yet ranked; also, there may be
       fewer than k points in the entire kd-tree.)

   rank_to_pindex		  an ivec mapping from each point's "nearness
                                  rank" to to its pindex (see the discussion
                                  of pindex in rdtree.h)

   rank_to_dsqd			  a dyv mapping from "nearness rank" to the
				  (square of the) distance from the ranked
                                  point to the query point.  (dsqd of point
				  ranked 0 is <= dsqd of point ranked 1, etc.)

   k				  the number of nearest neighbors to search
				  for (this is the "k" parameter specified
				  at program startup, default value 1)

   dqsd_or_below_is_interesting	  the (squared) distance from the current
				  highest-ranked neighbor to the query point.
				  Any kd-tree leaf closer to the query point
				  than this is interesting; any leaf further
				  away does not need to be investigated.
				  Initially "plus infinity"

*/

typedef struct neighbors
{
  ivec *rank_to_pindex;
  dyv *rank_to_dsqd;
  int k;
  double dqsd_or_below_is_interesting;
} neighbors;

/* create the initially empty neighbors structure */
neighbors *mk_empty_neighbors(int k);

/* free the neighbors structure */
void free_neighbors(neighbors *x);

/* print the neighbors structure to a file */
void fprintf_neighbors(FILE *s,char *m1,neighbors *x,char *m2);

/* print to standard output */
void pneighbors(neighbors *x);

/* make a new, separate copy of a neighbors structure */
neighbors *mk_copy_neighbors(neighbors *old);

/* return the current number of ranked neighbors */
int neighbors_size(neighbors *nbs);

/* return the current dqsd_or_below_is_interesting value */
#ifdef AMFAST
#define neighbors_dqsd_or_below_is_interesting(nbs) \
((nbs)->dqsd_or_below_is_interesting)
#else
double slow_neighbors_dqsd_or_below_is_interesting(neighbors *nbs);
#define neighbors_dqsd_or_below_is_interesting(nbs) \
slow_neighbors_dqsd_or_below_is_interesting(nbs)
#endif

/* add one point to the neighbors structure, IF its dsqd value
   is "interesting", and knocking out the highest-ranked-and-
   no-longer-interesting point, if there are already k neighbor points.
   Also resets dqsd_or_below_is_interesting, if necessary */
void add_to_neighbors(neighbors *nbs,int pindex,double dsqd);

/* searches through the points in an "interesting" leaf node, and
   updates the neighbors structure with its "interesting" point(s) */
void update_neighbors_from_points_and_pindexes(neighbors *nbs,dyv *query,
					       dyv_array *points,
					       ivec *pindexes);


#endif /* ifndef neighbors_H */
