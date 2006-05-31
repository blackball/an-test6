#ifndef RDLIST_H
#define RDLIST_H
#include "xylist.h"

#define AN_FILETYPE_RDLS "RDLS"

typedef xylist rdlist;

typedef xy rd;
#define mk_rd(n) dl_new((n)*2)
#define free_rd(x) dl_free(x)
#define rd_ref(x, i) dl_get(x, i)
#define rd_refra(x, i) dl_get(x, 2*(i))
#define rd_refdec(x, i) dl_get(x, (2*(i))+1)
#define rd_size(x) (dl_size(x)/2)
#define rd_set(x,i,v) dl_set(x,i,v)
#define rd_setra(x,i,v) dl_set(x,2*(i),v)
#define rd_setdec(x,i,v) dl_set(x,2*(i)+1,v)

inline rdlist* rdlist_open(char* fn);

inline rd* rdlist_get_field(rdlist* ls, uint field);

inline int rdlist_n_entries(rdlist* ls, uint field);

inline int rdlist_read_entries(rdlist* ls, uint field,
							   uint offset, uint n,
							   double* vals);

inline rdlist* rdlist_open_for_writing(char* fn);

inline int rdlist_write_header(rdlist* ls);

inline int rdlist_fix_header(rdlist* ls);

inline int rdlist_write_new_field(rdlist* ls);

inline int rdlist_write_entries(rdlist* ls, double* vals, uint nvals);

inline int rdlist_fix_field(rdlist* ls);

inline void rdlist_close(rdlist* ls);

#endif
