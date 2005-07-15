/*
   File:        kresult.c
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:53:15 EST 2004
   Description: General result from a kdtree query
   Copyright (c) Carnegie Mellon University
*/

/* NOTE TO PEOPLE TRYING TO UNDERSTAND HOW KD-TREES and KD-TREE SEARCH WORKS:

   This file is probably boring to you. It is concerned with the "glue" of
   representing, outputting and inputting questions and/or answers for general kdtree
   queries. It doesn't teach you anything about kd-trees themselves */

#include "kresult.h"

kresult *mk_kresult_from_pindexes(ivec *pindexes)
{
  kresult *x = AM_MALLOC(kresult);

  x -> count = ivec_size(pindexes);
  x -> pindexes = mk_copy_ivec(pindexes);

  return x;
}

kresult *mk_kresult_from_count(int count)
{
  kresult *x = AM_MALLOC(kresult);

  x -> count = count;
  x -> pindexes = NULL;

  return x;
}

void free_kresult(kresult *x)
{
  if ( x->pindexes != NULL ) free_ivec(x->pindexes);

  AM_FREE(x,kresult);
}

void fprintf_kresult(FILE *s,char *m1,kresult *x,char *m2)
{
  char *buff;

  buff = mk_printf("%s -> count",m1);
  fprintf_int(s,buff,x->count,m2);
  free_string(buff);

  if ( x->pindexes != NULL )
  {
    buff = mk_printf("%s -> pindexes",m1);
    fprintf_ivec(s,buff,x->pindexes,m2);
    free_string(buff);
  }
}

void pkresult(kresult *x)
{
  fprintf_kresult(stdout,"kresult",x,"\n");
}

kresult *mk_copy_kresult(kresult *old)
{
  kresult *nu = AM_MALLOC(kresult);

  nu -> count = old -> count;
  nu -> pindexes = (old->pindexes==NULL) ? NULL : mk_copy_ivec(old->pindexes);

  return nu;
}

