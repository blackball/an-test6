#ifndef PERMUTED_SORT_H
#define PERMUTED_SORT_H

/*
  This uses global variables and is NOT thread-safe.

  It's really an ugly hack, but it's one that was repeated in several places
  so needed to be extracted.  At least it's just ugly in one place now...
*/

void permuted_sort_set_params(void* realarray, int array_stride,
							  int (*compare)(const void*, const void*));

void permuted_sort(int* perm, int Nperm);

#endif
