/*
   File:        kresult.h
   Author(s):   Andrew Moore
   Created:     Wed Jan 21 16:53:14 EST 2004
   Description: General result from a kdtree query
   Copyright (c) Carnegie Mellon University
*/

#ifndef kresult_H
#define kresult_H

/* NOTE TO PEOPLE TRYING TO UNDERSTAND HOW KD-TREES and KD-TREE SEARCH WORKS:

   This file is probably boring to you. It is concerned with the "glue" of
   representing, outputting and inputting questions and/or answers for general kdtree
   queries. It doesn't teach you anything about kd-trees themselves */

#include "am_string.h"
#include "amiv.h"

typedef struct kresult
{
  int count;
  ivec *pindexes;
} kresult;

kresult *mk_kresult_from_pindexes(ivec *pindexes);
kresult *mk_kresult_from_count(int count);

void free_kresult(kresult *x);
void fprintf_kresult(FILE *s,char *m1,kresult *x,char *m2);
void pkresult(kresult *x);
kresult *mk_copy_kresult(kresult *old);


#endif /* ifndef kresult_H */
