/*
   File:        rangesearch.h
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:38:54 EST 2004
   Description: Classic kdtree rangesearch implementation
   Copyright (c) Carnegie Mellon University
*/

#ifndef rangesearch_H
#define rangesearch_H

#include "kdtree.h"

ivec *mk_rangesearch_pindexes(kdtree *kd,dyv *query,double range);

#endif /* ifndef rangesearch_H */
