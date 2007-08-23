#ifndef QSORT_REENTRANT_H__
#define QSORT_REENTRANT_H__

typedef int cmp_t(void *, const void *, const void *);
void qsort_r(void *a, size_t n, size_t es, void *thunk, cmp_t *cmp);

#endif  // QSORT_REENTRANT_H__
