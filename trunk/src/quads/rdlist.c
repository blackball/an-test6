#include "rdlist.h"

inline rdlist* rdlist_open(char* fn) {
	rdlist* rtn = xylist_open(fn);
	if (!rtn) return NULL;
	rtn->xname = "RA";
	rtn->yname = "DEC";
	return rtn;
}

inline rd* rdlist_get_field(rdlist* ls, uint field) {
	return xylist_get_field(ls, field);
}

inline int rdlist_n_entries(rdlist* ls, uint field) {
	return xylist_n_entries(ls, field);
}

inline int rdlist_read_entries(rdlist* ls, uint field,
							   uint offset, uint n,
							   double* vals) {
	return xylist_read_entries(ls, field, offset, n, vals);
}

inline rdlist* rdlist_open_for_writing(char* fn) {
	rdlist* rtn = xylist_open_for_writing(fn);
	rtn->antype = AN_FILETYPE_RDLS;
	rtn->xname = "RA";
	rtn->yname = "DEC";
	return rtn;
}

inline int rdlist_write_header(rdlist* ls) {
	return xylist_write_header(ls);
}

inline int rdlist_fix_header(rdlist* ls) {
	return xylist_fix_header(ls);
}

inline int rdlist_write_new_field(rdlist* ls) {
	return xylist_write_new_field(ls);
}

inline int rdlist_write_entries(rdlist* ls, double* vals, uint nvals) {
	return xylist_write_entries(ls, vals, nvals);
}

inline int rdlist_fix_field(rdlist* ls) {
	return xylist_fix_field(ls);
}

inline void rdlist_close(rdlist* ls) {
	xylist_close(ls);
}
