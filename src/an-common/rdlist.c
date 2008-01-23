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

#include "rdlist.h"

void rdlist_set_rname(rdlist* ls, const char* name) {
    xylist_set_xname(ls, name);
}
void rdlist_set_dname(rdlist* ls, const char* name) {
    xylist_set_yname(ls, name);
}
void rdlist_set_rtype(rdlist* ls, tfits_type type) {
    xylist_set_xtype(ls, type);
}
void rdlist_set_dtype(rdlist* ls, tfits_type type) {
    xylist_set_ytype(ls, type);
}
void rdlist_set_runits(rdlist* ls, const char* units) {
    xylist_set_xunits(ls, units);
}
void rdlist_set_dunits(rdlist* ls, const char* units) {
    xylist_set_yunits(ls, units);
}

qfits_header* rdlist_get_header(rdlist* ls) {
    return xylist_get_header(ls);
}

qfits_header* rdlist_get_field_header(rdlist* ls) {
    return xylist_get_field_header(ls, ls->field);
}

Inline rdlist* rdlist_open(char* fn) {
	rdlist* rtn = xylist_open(fn);
	if (!rtn) return NULL;
    rdlist_set_rname(rtn, "RA");
    rdlist_set_dname(rtn, "DEC");
	return rtn;
}

Inline rd* rdlist_get_field(rdlist* ls, uint field) {
	return xylist_get_field(ls, field);
}

Inline int rdlist_n_entries(rdlist* ls, uint field) {
	return xylist_n_entries(ls, field);
}

Inline int rdlist_read_entries(rdlist* ls, uint field,
							   uint offset, uint n,
							   double* vals) {
	return xylist_read_entries(ls, field, offset, n, vals);
}

Inline rdlist* rdlist_open_for_writing(char* fn) {
	rdlist* rtn = xylist_open_for_writing(fn);
	rtn->antype = AN_FILETYPE_RDLS;
    rdlist_set_rname(rtn, "RA");
    rdlist_set_dname(rtn, "DEC");
    rdlist_set_runits(rtn, "degrees");
    rdlist_set_dunits(rtn, "degrees");
    rdlist_set_rtype(rtn, TFITS_BIN_TYPE_D);
    rdlist_set_dtype(rtn, TFITS_BIN_TYPE_D);
	return rtn;
}

Inline int rdlist_n_fields(rdlist* ls) {
	return ls->nfields;
}

Inline int rdlist_write_header(rdlist* ls) {
	return xylist_write_header(ls);
}

Inline int rdlist_fix_header(rdlist* ls) {
	return xylist_fix_header(ls);
}

Inline int rdlist_new_field(rdlist* ls) {
  return xylist_new_field(ls);
}

Inline int rdlist_write_field_header(rdlist* ls) {
  return xylist_write_field_header(ls);
}

Inline int rdlist_write_new_field(rdlist* ls) {
	return xylist_write_new_field(ls);
}

Inline int rdlist_write_entries(rdlist* ls, double* vals, uint nvals) {
	return xylist_write_entries(ls, vals, nvals);
}

Inline int rdlist_fix_field(rdlist* ls) {
	return xylist_fix_field(ls);
}

Inline int rdlist_close(rdlist* ls) {
	return xylist_close(ls);
}
