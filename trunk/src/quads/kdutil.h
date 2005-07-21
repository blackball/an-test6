#ifndef kdutil_H
#define kdutil_H
#include "KD/kdtree.h"
#include "KD/kquery.h"

#define KD_UNDEF 0

#define fread_dyv(d,f) fread(d->farr,sizeof(double),dyv_size(d),f)
#define fread_ivec(i,f) fread(i->iarr,sizeof(int),ivec_size(i),f)
#define fwrite_dyv(d,f) fwrite(d->farr,sizeof(double),dyv_size(d),f)
#define fwrite_ivec(i,f) fwrite(i->iarr,sizeof(int),ivec_size(i),f)

extern kresult *mk_kresult_from_kquery(kquery *kq,kdtree *kd,dyv *query);

unsigned int fwrite_kdtree(kdtree *kdt, FILE *fid);
kdtree *fread_kdtree(FILE *fid);
unsigned int fwrite_node(node *n,FILE *fid);
node *fread_node(int pointdim,FILE *fid);
void free_nodedebug(node *x);
void free_kdtreedebug(kdtree *x);

dyv_array *mk_dyv_array_from_kdtree(kdtree *kd);
void free_dyv_array_from_kdtree(dyv_array *da);


#endif
