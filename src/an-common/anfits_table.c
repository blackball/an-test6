#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <assert.h>
#include <sys/param.h>

#include "anfits_private.h"
#include "anfits.h"
#include "fitsioutils.h"

anfits_table_t* anfits_table_open_for_writing(const char* fn) {
	anfits_table_t* table = malloc(sizeof(anfits_table_t));
	table->fid = fopen(fn, "wb");
	if (!table->fid) {
		fprintf(stderr, "Failed to open FITS table %s for writing: %s\n", table->filename, strerror(errno));
		free(table);
		return NULL;
	}
	table->columns = bl_new(10, sizeof(column_t));
	table->filename = strdup(fn);
	table->qtable = NULL;
	table->written_rows = 0;
    return table;
}

void anfits_table_add_column(anfits_table_t* table, anfits_type_t type, const char* name, const char* units) {
	column_t* col = bl_extend(table->columns);
	col->name = strdup(name);
	col->type = type;
	col->units = strdup(units);
}

int anfits_table_write_header(anfits_table_t* table) {
	int ncols = anfits_table_ncols(table);
	table->qtable = qfits_table_new(table->filename,
			QFITS_BINTABLE, 0, ncols, table->written_rows);

	int i;
	for (i=0; i<ncols; i++) {
		column_t *col = bl_access(table->columns, i);
		fits_add_column(table->qtable, i,
				col->type, 1, col->units, col->name);
	}
	table->qtable->tab_w = qfits_compute_table_width(table->qtable);

	qfits_header* primary_hdr = qfits_table_prim_header_default();
	int ret = qfits_header_dump(primary_hdr, table->fid);
	qfits_header_destroy(primary_hdr);
	if (ret == -1)
		return -1;

	qfits_header* ext_hdr = qfits_table_ext_header_default(table->qtable);
	ret = qfits_header_dump(ext_hdr, table->fid);
	qfits_header_destroy(ext_hdr);
    return ret;
}

int anfits_table_write_row(anfits_table_t* table, ...) {
	va_list ap;
	int ncols = anfits_table_ncols(table);
	int i;

	int ret = 0;
	va_start(ap, table);
	for (i=0; i<ncols; i++) {
		column_t *col = bl_access(table->columns, i);
		void *columndata = va_arg(ap, void *);
		ret = fits_write_data(table->fid, columndata, col->type);
		if (ret)
			break;
	}
	va_end(ap);
	table->written_rows++;
    return ret;
}

int anfits_table_close(anfits_table_t* table) {
	fits_pad_file(table->fid);
	rewind(table->fid);
	anfits_table_write_header(table);
	int ncols = anfits_table_ncols(table);
	int i;
	for (i=0; i<ncols; i++) {
		column_t *col = bl_access(table->columns, i);
		free(col->name);
		free(col->units);
	}
	bl_free(table->columns);
	free(table->filename);
	int ret = fclose(table->fid);
	if (ret) {
		fprintf(stderr, "Failed to close FITS table %s: %s\n", table->filename, strerror(errno));
	}
	free(table);
    return ret;
}

anfits_table_t* anfits_table_open(const char* fn) {
	anfits_table_t* table = malloc(sizeof(anfits_table_t));
    /*
     table->fid = fopen(fn, "rb");
     if (!table->fid) {
     fprintf(stderr, "Failed to open file %s as FITS table: %s\n", fn, strerror(errno));
     free(table);
     return NULL;
     }
     */
    table->fid = NULL;
	table->qtable = qfits_table_open(fn, 1);
    if (!table->qtable) {
        fprintf(stderr, "Failed to read FITS table header from file %s.\n", fn);
        free(table);
        return NULL;
    }
	table->filename = strdup(fn);
	table->columns = NULL; //bl_new(10, sizeof(column_t));
	table->written_rows = 0;
    return table;
}

int anfits_table_nrows(const anfits_table_t* table) {
    assert(table->qtable);
    return table->qtable->nr;
}

int anfits_table_ncols(const anfits_table_t* table) {
    return bl_size(table->columns);
}

void* anfits_table_read_column(anfits_table_t* table,
                               const char* name,
                               anfits_type_t ctype) {
    int colnum;
    int fitssize;
    int csize;
    int fitstype;
    qfits_col* col;
    void* data;
    int N;

    colnum = fits_find_column(table->qtable, name);
    if (colnum == -1) {
        fprintf(stderr, "Column \"%s\" not found in FITS table %s.\n", name, table->filename);
        return NULL;
    }
    col = table->qtable->col + colnum;
    if (col->atom_nb != 1) {
        fprintf(stderr, "Column \"%s\" in FITS table %s is an array of size %i, not a scalar.\n",
                name, table->filename, col->atom_nb);
        return NULL;
    }
    fitstype = col->atom_type;
    fitssize = fits_get_atom_size(fitstype);
    csize = fits_get_atom_size(ctype);
    N = table->qtable->nr;
    data = calloc(MAX(csize, fitssize), N);

    qfits_query_column_seq_to_array(table->qtable, colnum, 0, N, data, fitssize);

    if (fitstype != ctype) {
        fits_convert_data(data, csize, ctype,
                          data, fitssize, fitstype, N);
        data = realloc(data, csize * N);
    }

	return data;
}

