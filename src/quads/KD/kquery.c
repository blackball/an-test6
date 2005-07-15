/*
   File:        kquery.c
   Author(s):   Andrew Moore
   Created:     Thu Jan 22 18:24:53 EST 2004
   Description: Represents a single-tree query
   Copyright (c) Carnegie Mellon University
*/

#include "kquery.h"

/* NOTE TO PEOPLE TRYING TO UNDERSTAND HOW KD-TREES and KD-TREE SEARCH WORKS:

   This file is probably boring to you. It is concerned with the "glue" of
   representing, outputting and inputting questions and/or answers for general kdtree
   queries. It doesn't teach you anything about kd-trees themselves */

/* Makes a kquery. As always COPIES in its inputs (e.g. taskname is copied
   into the structure) */
kquery *mk_kquery(char *taskname,char *method,int k,double range,int rmin)
{
  kquery *x = AM_MALLOC(kquery);

  x -> taskname = mk_copy_string(taskname);
  x -> method = mk_copy_string(method);
  x -> k = k;
  x -> range = range;
  x -> rmin = rmin;

  return x;
}

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

kquery *mk_kquery_from_args(int argc,char *argv[])
{
  char *taskname = string_from_args("taskname",argc,argv,"knn");
  char *method = string_from_args("method",argc,argv,"singletree");
  int k = int_from_args("k",argc,argv,1);
  double range = double_from_args("range",argc,argv,10.0);
  int rmin = int_from_args("rmin",argc,argv,50);
  
  return mk_kquery(taskname,method,k,range,rmin);
}

void free_kquery(kquery *x)
{
  free_string(x->taskname);
  free_string(x->method);
  AM_FREE(x,kquery);
}

void fprintf_kquery(FILE *s,char *m1,kquery *x,char *m2)
{
  char *buff;

  buff = mk_printf("%s -> taskname",m1);
  fprintf_string(s,buff,x->taskname,m2);
  free_string(buff);

  buff = mk_printf("%s -> method",m1);
  fprintf_string(s,buff,x->method,m2);
  free_string(buff);

  buff = mk_printf("%s -> k",m1);
  fprintf_int(s,buff,x->k,m2);
  free_string(buff);

  buff = mk_printf("%s -> range",m1);
  fprintf_double(s,buff,x->range,m2);
  free_string(buff);

  buff = mk_printf("%s -> rmin",m1);
  fprintf_int(s,buff,x->rmin,m2);
  free_string(buff);
}

void pkquery(kquery *x)
{
  fprintf_kquery(stdout,"kquery",x,"\n");
}

kquery *mk_copy_kquery(kquery *old)
{
  kquery *nu = AM_MALLOC(kquery);

  nu -> taskname = mk_copy_string(old->taskname);
  nu -> method = mk_copy_string(old->method);
  nu -> k = old -> k;
  nu -> range = old -> range;
  nu -> rmin = old -> rmin;

  return nu;
}

/* Runs the kquery for the given querypoint. Result is stored in a kresult */
kresult *mk_kresult_from_kquery(kquery *kq,kdtree *kd,dyv *query)
{
  kresult *kr = NULL;

  if ( eq_string(kq->taskname,"knn") )
  {
    ivec *knn_pindexes = mk_knn_pindexes(kd,query,kq->k);
    kr = mk_kresult_from_pindexes(knn_pindexes);
    free_ivec(knn_pindexes);
  }
  else if ( eq_string(kq->taskname,"rangesearch") )
  {
    ivec *pindexes = mk_rangesearch_pindexes(kd,query,kq->range);
    kr = mk_kresult_from_pindexes(pindexes);
    free_ivec(pindexes);
  }
#ifdef NEVER
  else if ( eq_string(kq->taskname,"rangecount") )
  {
    int count = rangecount(kd,query,kq->range);
    kr = mk_kresult_from_count(count);
  }
#endif
  else 
    my_errorf("Unknown kquery_taskname: %s\n",kq->taskname);

  return kr;
}

/* Makes an array of kresults: one kresult for each query_point.

   kresults_ref(result,i) is the result corresponding to 
   query == dyv_array_ref(query_points,i) 

   Uses the singletree algorithm (repeated calls to individual queries)
*/
kresults *mk_kresults_from_kquery_singletree(kquery *kq,kdtree *kd,
					     dyv_array *query_points)
{
  int num_queries = dyv_array_size(query_points);
  kresults *krs = mk_empty_kresults(num_queries);
  int qnum;

  for ( qnum = 0 ; qnum < num_queries ; qnum++ )
  {
    dyv *query = dyv_array_ref(query_points,qnum);
    kresult *kr = mk_kresult_from_kquery(kq,kd,query);
    
    if ( is_power_of_two(qnum) )
      printf("processed %d out of %d queries\n",qnum,num_queries);
    kresults_set(krs,qnum,kr);

    free_kresult(kr);
  }
  printf("processed %d out of %d queries\n",num_queries,num_queries);

  return krs;
}

/* Makes an array of kresults: one kresult for each query_point.

   kresults_ref(result,i) is the result corresponding to 
   query == dyv_array_ref(query_points,i) 

   Uses the dualtree algorithm.

   NOT YET IMPLEMENTED SO IN FACT TERMINATES WITH ERROR MESSAGE
*/
kresults *mk_kresults_from_kquery_dualtree(kquery *kq,kdtree *kd,
					   dyv_array *query_points)
{
  my_error("mk_kresults_from_kquery_dualtree not implemented");
  return NULL;
}

/* Makes an array of kresults: one kresult for each query_point.

   kresults_ref(result,i) is the result corresponding to 
   query == dyv_array_ref(query_points,i) 

   Uses whatever algorithmic method is specified by kquery
*/
kresults *mk_kresults_from_kquery(kquery *kq,dyv_array *pindex_to_point,
				  dyv_array *query_points)
{
  kdtree *kd;
  kresults *krs = NULL;
  int start_secs = global_time();
  int middle_secs;
  int end_secs;

  printf("Will build kdtree...\n");
  kd = mk_kdtree_from_points(pindex_to_point,kq->rmin);
  printf("...have built kdtree\n");

  middle_secs = global_time();

  explain_kdtree(kd);
  if ( kdtree_num_nodes(kd) < 10 && kdtree_num_points(kd) < 100 )
  {
    fprintf_kdtree(stdout,"kd",kd,"\n");
    printf("The above is a printout of the kdtree\n");
  }

  if ( eq_string(kq->method,"singletree") )
    krs = mk_kresults_from_kquery_singletree(kq,kd,query_points);
  else if ( eq_string(kq->method,"dualtree") )
    krs = mk_kresults_from_kquery_dualtree(kq,kd,query_points);
  else
    my_errorf("Unknown kquery method: %s\n",kq->method);
    
  end_secs = global_time();


  free_kdtree(kd);

  printf("%40s = %6d secs\n","Time to build kdtree",middle_secs-start_secs);
  printf("%40s = %6d secs\n","Time to do all queries",end_secs-middle_secs);
  printf("%40s = %6d secs\n","Total time",end_secs-start_secs);

  return krs;
}

void kdtree_main(int argc,char *argv[])
{
  kquery *kq = mk_kquery_from_args(argc,argv);
  char *outname = string_from_args("out",argc,argv,"output.csv");
  dyv_array *pindex_to_point = 
    mk_dyv_array_from_args_basic("data",argc,argv,TRUE);
  dyv_array *query_points = 
    mk_dyv_array_from_args_basic("query",argc,argv,TRUE);
  kresults *krs = mk_kresults_from_kquery(kq,pindex_to_point,query_points);

  output_kresults(krs,outname);

  free_dyv_array(pindex_to_point);
  free_dyv_array(query_points);
  free_kquery(kq);
  free_kresults(krs);
}
