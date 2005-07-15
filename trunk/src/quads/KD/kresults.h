/*
   File:        kresults.h
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:42:23 EST 2004
   Description: Array of results from batched kdtree queries
   Copyright (c) Carnegie Mellon University
*/

#ifndef kresults_H
#define kresults_H

/* NOTE TO PEOPLE TRYING TO UNDERSTAND HOW KD-TREES and KD-TREE SEARCH WORKS:

   This file is probably boring to you. It is concerned with the "glue" of
   representing, outputting and inputting questions and/or answers for general
   kdtree queries. It doesn't teach you anything about kd-trees themselves */


#include "kresult.h"
#include "amdyv_array.h"

typedef struct kresults
{
  int size;
  kresult **array;
} kresults;

kresults *mk_empty_kresults(int size);
void free_kresults(kresults *x);
void fprintf_kresults(FILE *s,char *m1,kresults *x,char *m2);
void pkresults(kresults *x);
kresults *mk_copy_kresults(kresults *old);
int kresults_size(kresults *krs);
kresult *kresults_ref(kresults *krs,int index);
void kresults_set(kresults *krs,int index,kresult *kr);

string_array *mk_kresults_names(kresults *krs);
dyv_array *mk_dyv_array_from_kresults(kresults *krs);
void output_kresults(kresults *krs,char *filename);

#endif /* ifndef kresults_H */
