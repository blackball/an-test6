/*
   File:        neighbors.c
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 18:05:53 EST 2004
   Description: Queue of neighbors represented as (pindex,dsqd) pairs
   Copyright (c) Carnegie Mellon University
*/

#include "neighbors.h"

#define LARGEST_POSSIBLE_DSQD (1e100)

/* Consult neighbors.h for details concerning the neighbors structure
   and its components.
*/

/* create the initially empty neighbors structure */
neighbors *mk_empty_neighbors(int k)
{
  neighbors *x = AM_MALLOC(neighbors);

  my_assert(k >= 1);

  x -> rank_to_pindex = mk_ivec(0);
  x -> rank_to_dsqd = mk_dyv(0);
  x -> k = k;
  x -> dqsd_or_below_is_interesting = LARGEST_POSSIBLE_DSQD;

  return x;
}

/* free the neighbors structure */
void free_neighbors(neighbors *x)
{
  free_ivec(x->rank_to_pindex);
  free_dyv(x->rank_to_dsqd);

  AM_FREE(x,neighbors);
}

/* print the neighbors structure to a file */
void fprintf_neighbors(FILE *s,char *m1,neighbors *x,char *m2)
{
  char *buff;

  buff = mk_printf("%s -> rank_to_pindex",m1);
  fprintf_ivec(s,buff,x->rank_to_pindex,m2);
  free_string(buff);

  buff = mk_printf("%s -> rank_to_dsqd",m1);
  fprintf_dyv(s,buff,x->rank_to_dsqd,m2);
  free_string(buff);

  buff = mk_printf("%s -> k",m1);
  fprintf_int(s,buff,x->k,m2);
  free_string(buff);

  buff = mk_printf("%s -> dqsd_or_below_is_interesting",m1);
  fprintf_double(s,buff,x->dqsd_or_below_is_interesting,m2);
  free_string(buff);
}

/* print to standard output */
void pneighbors(neighbors *x)
{
  fprintf_neighbors(stdout,"neighbors",x,"\n");
}

/* make a new, separate copy of a neighbors structure */
neighbors *mk_copy_neighbors(neighbors *old)
{
  neighbors *nu = AM_MALLOC(neighbors);

  nu -> rank_to_pindex = mk_copy_ivec(old->rank_to_pindex);
  nu -> rank_to_dsqd = mk_copy_dyv(old->rank_to_dsqd);
  nu -> k = old -> k;
  nu -> dqsd_or_below_is_interesting = old -> dqsd_or_below_is_interesting;

  return nu;
}

/* return the current number of ranked neighbors */
int neighbors_size(neighbors *nbs)
{
  return ivec_size(nbs->rank_to_pindex);
}

/* return the current dqsd_or_below_is_interesting value
   (only if not in AM_FAST mode: in that case, replaced by a macro) */
double slow_neighbors_dqsd_or_below_is_interesting(neighbors *nbs)
{
#ifdef DOING_ASSERTS
  int size = neighbors_size(nbs);
  if ( size < nbs->k )
    my_assert(nbs->dqsd_or_below_is_interesting == LARGEST_POSSIBLE_DSQD);
  else
  {
    double dqsd_or_below_is_interesting = dyv_ref(nbs->rank_to_dsqd,size-1);
    if ( nbs->dqsd_or_below_is_interesting != dqsd_or_below_is_interesting )
    {
      pneighbors(nbs);
      printf("dqsd_or_below_is_interesting = %g\n",
	     dqsd_or_below_is_interesting);
      my_errorf("dqsd_or_below_is_interesting should equal\n"
		"rightmost dsqd value");
    }
  }
#endif

  return nbs->dqsd_or_below_is_interesting;
}

/* add one point to the neighbors structure, IF its dsqd value
   is "interesting", and knocking out the highest-ranked-and-
   no-longer-interesting point, if there are already k neighbor points.
   Also resets dqsd_or_below_is_interesting, if necessary */
void add_to_neighbors(neighbors *nbs,int pindex,double dsqd)
{
  int i;
  int insert_index = -1;
  int size = neighbors_size(nbs);

  my_assert(dsqd < LARGEST_POSSIBLE_DSQD);
  my_assert(dsqd >= 0.0);
  my_assert(neighbors_dqsd_or_below_is_interesting(nbs) > dsqd);

  if ( size == 0 )
    insert_index = 0;
  else
  {
    for ( i = neighbors_size(nbs)-1 ; insert_index < 0 && i >= 0 ; i-- )
    {
      if ( dyv_ref(nbs->rank_to_dsqd,i) <= dsqd )
	insert_index = i+1;
    } 

    if ( insert_index < 0 ) insert_index = 0;
  }

  /* 
     forall i in [0,insert_index-1]    rank_to_dsqd[i] <= dsqd
     forall i in [insert_index,size-1] rank_to_dsqd[i] >  dsqd
  */

#ifdef DOING_ASSERTS

  for ( i = 0 ; i < neighbors_size(nbs) ; i++ )
  {
    my_assert( (i < insert_index) ? (dyv_ref(nbs->rank_to_dsqd,i) <= dsqd) :
                                    (dyv_ref(nbs->rank_to_dsqd,i) > dsqd) );
  }
  my_assert(insert_index >= 0);
  my_assert(insert_index <= neighbors_size(nbs));

#endif

  if ( size < nbs->k )
  {
    add_to_ivec(nbs->rank_to_pindex,-777);
    add_to_dyv(nbs->rank_to_dsqd,-7.7777e77);
    size += 1;
  }
  else
    my_assert(size == nbs->k);

  for ( i = size-1 ; i > insert_index ; i-- )
  {
    ivec_set(nbs->rank_to_pindex,i,ivec_ref(nbs->rank_to_pindex,i-1));
    dyv_set(nbs->rank_to_dsqd,i,dyv_ref(nbs->rank_to_dsqd,i-1));
  }

  ivec_set(nbs->rank_to_pindex,insert_index,pindex);
  dyv_set(nbs->rank_to_dsqd,insert_index,dsqd);

  my_assert(dyv_size(nbs->rank_to_dsqd) == size);
  my_assert(dyv_size(nbs->rank_to_dsqd) <= nbs->k);

  if ( size == nbs->k )
  {
    nbs->dqsd_or_below_is_interesting = 
      dyv_ref(nbs->rank_to_dsqd,dyv_size(nbs->rank_to_dsqd)-1);
  }
}


/* searches through the points in an "interesting" leaf node, and
   updates the neighbors structure with its "interesting" point(s) */
void update_neighbors_from_points_and_pindexes(neighbors *nbs,dyv *query,
					       dyv_array *points,
					       ivec *pindexes)
{
  int i;

  for ( i = ivec_size(pindexes)-1 ; i >= 0 ; i-- )
  {
    dyv *point = dyv_array_ref(points,i);
    double dsqd = dyv_dyv_dsqd(query,point);

    if ( dsqd < neighbors_dqsd_or_below_is_interesting(nbs) )
    {
      int pindex = ivec_ref(pindexes,i);
      add_to_neighbors(nbs,pindex,dsqd);
    }
  }
}


