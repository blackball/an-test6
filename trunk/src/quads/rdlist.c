#include "rdlist.h"

Inline rdlist* rdlist_open(char* fn) {
	rdlist* rtn = xylist_open(fn);
	if (!rtn) return NULL;
	rtn->xname = "RA";
	rtn->yname = "DEC";
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
	rtn->xname = "RA";
	rtn->yname = "DEC";
	return rtn;
}

Inline int rdlist_write_header(rdlist* ls) {
	return xylist_write_header(ls);
}

Inline int rdlist_fix_header(rdlist* ls) {
	return xylist_fix_header(ls);
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
