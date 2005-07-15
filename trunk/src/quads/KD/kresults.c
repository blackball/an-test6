/*
   File:        kresults.c
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:42:23 EST 2004
   Description: Array of results from batched kdtree queries
   Copyright (c) Carnegie Mellon University
*/

#include "kresults.h"

/* NOTE TO PEOPLE TRYING TO UNDERSTAND HOW KD-TREES and KD-TREE SEARCH WORKS:

   This file is probably boring to you. It is concerned with the "glue" of
   representing, outputting and inputting questions and/or answers for general kdtree
   queries. It doesn't teach you anything about kd-trees themselves */

kresults *mk_empty_kresults(int size)
{
  kresults *x = AM_MALLOC(kresults);
  int i;

  x -> size = size;
  x -> array = AM_MALLOC_ARRAY(kresult *,size);

  for ( i = 0 ; i < size ; i++ )
    x -> array[i] = NULL;

  return x;
}

void free_kresults(kresults *x)
{
  int i;

  for ( i = 0 ; i < x->size ; i++ )
    if ( x->array[i] != NULL ) free_kresult(x->array[i]);

  AM_FREE_ARRAY(x->array,kresult *,x->size);
  AM_FREE(x,kresults);
}

void fprintf_kresults(FILE *s,char *m1,kresults *x,char *m2)
{
  char *buff;
  int i;

  buff = mk_printf("%s -> size",m1);
  fprintf_int(s,buff,x->size,m2);
  free_string(buff);

  for ( i = 0 ; i < x->size ; i++ )
  {
    if ( x->array[i] != NULL )
    {
      buff = mk_printf("%s -> array[%d]",m1,i);
      fprintf_kresult(s,buff,x->array[i],m2);
      free_string(buff);
    }
  }
}

void pkresults(kresults *x)
{
  fprintf_kresults(stdout,"kresults",x,"\n");
}

kresults *mk_copy_kresults(kresults *old)
{
  kresults *nu = AM_MALLOC(kresults);
  int i;

  nu -> size = old -> size;
  nu -> array = AM_MALLOC_ARRAY(kresult *,old->size);

  for ( i = 0 ; i < old->size ; i++ )
    nu -> array[i] = (old->array[i]==NULL) ? NULL : 
                     mk_copy_kresult(old->array[i]);

  return nu;
}

int kresults_size(kresults *krs)
{
  return krs->size;
}

kresult *kresults_ref(kresults *krs,int index)
{
  my_assert(index >= 0);
  my_assert(index < kresults_size(krs));
  return krs->array[index];
}

void kresults_set(kresults *krs,int index,kresult *kr)
{
  my_assert(index >= 0);
  my_assert(index < kresults_size(krs));
  my_assert(kr != NULL);

  if ( krs->array[index] != NULL ) free_kresult(krs->array[index]);

  krs -> array[index] = mk_copy_kresult(kr);
}

string_array *mk_kresults_names(kresults *krs)
{
  bool short_names;
  string_array *result;

  if ( kresults_size(krs) == 0 )
    short_names = TRUE;
  else
  {
    kresult *first = kresults_ref(krs,0);
    short_names = first->pindexes == NULL;
  }

  if ( short_names )
    result = mk_broken_string("query_record count");
  else
    result = mk_broken_string("query_record rank data_record");

  return result;
}

dyv_array *mk_dyv_array_from_kresults(kresults *krs)
{
  dyv_array *result = mk_empty_dyv_array();
  int i;

  for ( i = 0 ; i < kresults_size(krs) ; i++ )
  {
    kresult *kr = kresults_ref(krs,i);
    if ( kr->pindexes == NULL )
    {
      dyv *d = mk_dyv_2((double)i,(double)kr->count);
      add_to_dyv_array(result,d);
      free_dyv(d);
    }
    else
    {
      int j;

      for ( j = 0 ; j < ivec_size(kr->pindexes) ; j++ )
      {
	int pindex = ivec_ref(kr->pindexes,j);
	dyv *d = mk_dyv_3((double)i,(double)j,(double) pindex);
	add_to_dyv_array(result,d);
	free_dyv(d);
      }
    }
  }

  return result;
}

void output_kresults(kresults *krs,char *filename)
{
  string_array *krnames = mk_kresults_names(krs);
  int i;
  FILE *s;

  printf("Saving results to %s\n",filename);
  s = safe_fopen(filename,"w");

  fprint_string_array_csv(s,krnames);

  for ( i = 0 ; i < kresults_size(krs) ; i++ )
  {
    kresult *kr = kresults_ref(krs,i);

    if ( kr->pindexes == NULL )
    {
      ivec *iv = mk_ivec_2(i,kr->count);
      fprintf_ivec_csv(s,iv);
      free_ivec(iv);
    }
    else
    {
      int j;

      for ( j = 0 ; j < ivec_size(kr->pindexes) ; j++ )
      {
	int pindex = ivec_ref(kr->pindexes,j);
	ivec *iv = mk_ivec_3(i,j,pindex);
	fprintf_ivec_csv(s,iv);
	free_ivec(iv);
      }
    }
  }
  fclose(s);
  printf("Finished saving results to %s\n",filename);

  free_string_array(krnames);
}
