/*
   File:        knn.h
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:38:54 EST 2004
   Description: Classic kdtree knn implementation
   Copyright (c) Carnegie Mellon University
*/

#ifndef knn_H
#define knn_H

/* This header contains just one function which does nearest neighbor search
   using a kdtree. Read kdtree.h first, and especially make sure you understand
   what's a pindex before you read on */

#include "kdtree.h"

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
ivec *mk_knn_pindexes(kdtree *kd,dyv *query,int k);

#endif /* ifndef knn_H */
