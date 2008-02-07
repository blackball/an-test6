/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#ifndef RDLIST_H
#define RDLIST_H
#include "xylist.h"

#define AN_FILETYPE_RDLS "RDLS"

typedef xylist rdlist;

/*
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
 */
/*
void rdlist_set_rname(rdlist* ls, const char* name);
void rdlist_set_dname(rdlist* ls, const char* name);
void rdlist_set_rtype(rdlist* ls, tfits_type type);
void rdlist_set_dtype(rdlist* ls, tfits_type type);
void rdlist_set_runits(rdlist* ls, const char* units);
void rdlist_set_dunits(rdlist* ls, const char* units);

qfits_header* rdlist_get_header(rdlist* ls);

qfits_header* rdlist_get_field_header(rdlist* ls);

inline rdlist* rdlist_open(char* fn);

inline int rdlist_n_fields(rdlist* ls);

inline rd* rdlist_get_field(rdlist* ls, uint field);

inline int rdlist_n_entries(rdlist* ls, uint field);

inline int rdlist_read_entries(rdlist* ls, uint field,
							   uint offset, uint n,
							   double* vals);

inline rdlist* rdlist_open_for_writing(char* fn);

inline int rdlist_write_header(rdlist* ls);

inline int rdlist_fix_header(rdlist* ls);

inline int rdlist_write_new_field(rdlist* ls);

int rdlist_new_field(rdlist* ls);

int rdlist_write_field_header(rdlist* ls);

inline int rdlist_write_entries(rdlist* ls, double* vals, uint nvals);

inline int rdlist_fix_field(rdlist* ls);

inline int rdlist_close(rdlist* ls);
 */
#endif
