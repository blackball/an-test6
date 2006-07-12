#ifndef XYLIST_H
#define XYLIST_H

#include <stdio.h>

#include "starutil.h"
#include "qfits.h"
#include "bl.h"

typedef dl xy;
#define mk_xy(n) dl_new((n)*2)
#define free_xy(x) dl_free(x)
#define xy_ref(x, i) dl_get(x, i)
#define xy_refx(x, i) dl_get(x, 2*(i))
#define xy_refy(x, i) dl_get(x, (2*(i))+1)
#define xy_size(x) (dl_size(x)/2)
#define xy_set(x,i,v) dl_set(x,i,v)
#define xy_setx(x,i,v) dl_set(x,2*(i),v)
#define xy_sety(x,i,v) dl_set(x,2*(i)+1,v)

#define AN_FILETYPE_XYLS "XYLS"

/*
  One table per field.
  One column.
  One row per entry.
*/
struct xylist {
	char* fn;
	int nfields;
	int parity;
	tfits_type xtype;
	tfits_type ytype;
	char* xname; // default "X"
	char* yname; // default "Y"

	char* antype; // Astrometry.net filetype string.

	// field we're currently reading/writing
	uint field;
	qfits_table* table;

	// reading
	int xcol;
	int ycol;

	// writing:
	qfits_header* header;
	FILE* fid;
	off_t data_offset;
	off_t table_offset;
};
typedef struct xylist xylist;

// you can change the parameters (ie, xname, yname) 
// after opening but before calling xylist_get_field.
xylist* xylist_open(const char* fn);

// it's your responsibility to free_xy() this.
xy* xylist_get_field(xylist* ls, uint field);

int xylist_n_entries(xylist* ls, uint field);

int xylist_read_entries(xylist* ls, uint field,
						uint offset, uint n,
						double* vals);

xylist* xylist_open_for_writing(char* fn);

int xylist_write_header(xylist* ls);

int xylist_fix_header(xylist* ls);

int xylist_write_new_field(xylist* ls);

int xylist_write_entries(xylist* ls, double* vals, uint nvals);

int xylist_fix_field(xylist* ls);

int xylist_close(xylist* ls);

#endif
