/*
   File:        kquery.h
   Author(s):   Andrew Moore
   Created:     Thu Jan 22 18:24:53 EST 2004
   Description: Represents a single-tree query
   Copyright (c) Carnegie Mellon University
*/

#ifndef kquery_H
#define kquery_H

/* NOTE TO PEOPLE TRYING TO UNDERSTAND HOW KD-TREES and KD-TREE SEARCH WORKS:

   This file is probably boring to you. It is concerned with the "glue" of
   representing, outputting and inputting questions and/or answers for general kdtree
   queries. It doesn't teach you anything about kd-trees themselves */

#include "kresults.h"
#include "knn.h"
#include "rangesearch.h"

typedef struct kquery
{
  char *taskname;
  /*
                        Can be one of: 
                        knn rangesearch rangecount
                        ...is the operation you want
   */
  char *method;
  /*
                        Can only by singletree right now..
                        refers to the algorithm for 
                        doing the task. Later we hope
                        to include dualtree 
   */
  int k;
  /*
                        Only relevant if taskname==knn
                        The number of neighbors needed
   */
  double range;
  /*
                        Only relevant if taskname != knn
                        The radius of the range
   */
  int rmin;
  /*
                        The rmin parameter for building
                        the tree (max num. points in a
                        leaf node).
   */
} kquery;

/* Makes a kquery. As always COPIES in its inputs (e.g. taskname is copied
   into the structure) */
kquery *mk_kquery(char *taskname,char *method,int k,double range,int rmin);

/* Parses the command-line arguments to make a kquery. Looks for key/value pairs
   in argc/argv and uses sensible defaults if doesn't find them. The arguments it
   looks for are:

ARGV              taskname <string> default      knn    Can be one of: 
                                                        knn rangesearch rangecount
                                                        ...is the operation you want

ARGV                method <string> default singletree  Can only by singletree right now..
                                                        refers to the algorithm for 
                                                        doing the task. Later we hope
                                                        to include dualtree 

ARGV                     k    <int> default        1    Only relevant if taskname==knn
                                                        The number of neighbors needed

ARGV                 range <double> default       10    Only relevant if taskname != knn
                                                        The radius of the range

ARGV                  rmin    <int> default       50    The rmin parameter for building
                                                        the tree (max num. points in a
                                                        leaf node).
*/

kquery *mk_kquery_from_args(int argc,char *argv[]);

void free_kquery(kquery *x);

void fprintf_kquery(FILE *s,char *m1,kquery *x,char *m2);

void pkquery(kquery *x);

kquery *mk_copy_kquery(kquery *old);

/* Makes an array of kresults: one kresult for each query_point.

   kresults_ref(result,i) is the result corresponding to 
   query == dyv_array_ref(query_points,i) 

   Uses whatever algorithmic method is specified by kquery
*/
kresults *mk_kresults_from_kquery(kquery *kq,dyv_array *pindex_to_point,
				  dyv_array *query_points);

/* Topmost-level call. See readme.txt to see what parameters are expected (either
   optionally or compulsory-ally) on the command line */
void kdtree_main(int argc,char *argv[]);

#endif /* ifndef kquery_H */
