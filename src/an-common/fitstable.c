#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/param.h>

#include "fitstable.h"
#include "fitsioutils.h"

static void fitstable_create_table(fitstable_t* tab);

static int ncols(const fitstable_t* t) {
    return bl_size(t->cols);
}
static fitscol_t* getcol(const fitstable_t* t, int i) {
    return bl_access(t->cols, i);
}

tfits_type fitscolumn_int_type() {
    switch (sizeof(int)) {
    case 2:
        return TFITS_BIN_TYPE_I;
    case 4:
        return TFITS_BIN_TYPE_J;
    case 8:
        return TFITS_BIN_TYPE_K;
    }
    return -1;
}

tfits_type fitscolumn_double_type() {
    return TFITS_BIN_TYPE_D;
}

tfits_type fitscolumn_float_type() {
    return TFITS_BIN_TYPE_E;
}

// When reading: allow this column to match to any FITS type.
tfits_type fitscolumn_any_type() {
    return (tfits_type)-1;
}

int fitstable_ncols(fitstable_t* t) {
    return ncols(t);
}

void fitstable_add_write_column(fitstable_t* tab, tfits_type t,
                                const char* name, const char* units) {
    fitstable_add_write_column_array_convert(tab, t, t, 1, name, units);
}

void fitstable_add_write_column_convert(fitstable_t* tab,
                                        tfits_type fits_type,
                                        tfits_type c_type,
                                        const char* name,
                                        const char* units) {
    fitstable_add_write_column_array_convert(tab, fits_type, c_type, 1, name, units);
}

void fitstable_add_write_column_array(fitstable_t* tab, tfits_type t,
                                      int arraysize,
                                      const char* name,
                                      const char* units) {
    fitstable_add_write_column_array_convert(tab, t, t, arraysize, name, units);
}

void fitstable_add_write_column_array_convert(fitstable_t* tab,
                                              tfits_type fits_type,
                                              tfits_type c_type,
                                              int arraysize,
                                              const char* name,
                                              const char* units) {
    fitscol_t col;
    memset(&col, 0, sizeof(fitscol_t));
    col.colname = name;
    col.units = units;
    col.fitstype = fits_type;
    col.ctype = c_type;
    col.arraysize = arraysize;
    col.in_struct = FALSE;
    fitstable_add_column(tab, &col);
}

void fitstable_add_write_column_struct(fitstable_t* tab,
                                       tfits_type c_type,
                                       int arraysize,
                                       int structoffset,
                                       tfits_type fits_type,
                                       const char* name,
                                       const char* units) {
    fitstable_add_column_struct(tab, c_type, arraysize, structoffset,
                                fits_type, name, units);
}

void fitstable_add_read_column_struct(fitstable_t* tab,
                                      tfits_type c_type,
                                      int arraysize,
                                      int structoffset,
                                      tfits_type fits_type,
                                      const char* name) {
    fitstable_add_column_struct(tab, c_type, arraysize, structoffset,
                                fits_type, name, NULL);
}

void fitstable_add_column_struct(fitstable_t* tab,
                                 tfits_type c_type,
                                 int arraysize,
                                 int structoffset,
                                 tfits_type fits_type,
                                 const char* name,
                                 const char* units) {
    fitscol_t col;
    memset(&col, 0, sizeof(fitscol_t));
    col.colname = name;
    col.units = units;
    col.fitstype = fits_type;
    col.target_fitstype = fits_type;
    col.target_arraysize = arraysize;
    col.ctype = c_type;
    col.arraysize = arraysize;
    col.in_struct = TRUE;
    col.coffset = structoffset;
    fitstable_add_column(tab, &col);
}

int fitstable_read_structs(fitstable_t* tab, void* struc,
                           int strucstride, int offset, int N) {
    int i;
    void* tempdata = NULL;
    int highwater = 0;

    for (i=0; i<ncols(tab); i++) {
        void* dest;
        int stride;
        void* finaldest;
        int finalstride;
        fitscol_t* col = getcol(tab, i);
        if (col->col == -1)
            continue;
        if (!col->in_struct)
            continue;
        finaldest = ((char*)struc) + col->coffset;
        finalstride = strucstride;

        if (col->fitstype != col->ctype) {
            int NB = col->fitssize * col->arraysize * N;
            if (NB > highwater) {
                free(tempdata);
                tempdata = malloc(NB);
                highwater = NB;
            }
            dest = tempdata;
            stride = col->fitssize * col->arraysize;
        } else {
            dest = finaldest;
            stride = finalstride;
        }

        // Read from FITS file...
        qfits_query_column_seq_to_array(tab->table, col->col, offset, N, dest, stride);

        if (col->fitstype != col->ctype) {
            int j;
            for (j=0; j<col->arraysize; j++)
                fits_convert_data(((char*)finaldest) + j * col->csize, finalstride, col->ctype,
                                  ((char*)dest) + j * col->fitssize, stride, col->fitstype,
                                  N);
        }
    }
    free(tempdata);
    return 0;
}

int fitstable_read_struct(fitstable_t* tab, int offset, void* struc) {
    return fitstable_read_structs(tab, struc, 0, offset, 1);
}

int fitstable_write_struct(fitstable_t* table, const void* struc) {
    int i;
    char* buf = NULL;
    int Nbuf = 0;
	int ret = 0;

    for (i=0; i<ncols(table); i++) {
        fitscol_t* col = getcol(table, i);
        void* columndata = ((char*)struc) + col->coffset;
        if (col->fitstype != col->ctype) {
            int sz = MAX(256, MAX(col->csize, col->fitssize) * col->arraysize);
            if (sz > Nbuf) {
                free(buf);
                buf = malloc(sz);
            }
            fits_convert_data(buf, col->fitssize, col->fitstype,
                              columndata, col->csize, col->ctype,
                              col->arraysize);
            columndata = buf;
        }
        ret = fits_write_data_array(table->fid, columndata,
                                    col->fitstype, col->arraysize);
        if (ret)
            break;
    }
    free(buf);
    table->table->nr++;
    return ret;
}

int fitstable_write_row(fitstable_t* table, ...) {
	va_list ap;
	int ncols = fitstable_ncols(table);
	int i;
    char* buf = NULL;
    int Nbuf = 0;
	int ret = 0;

	va_start(ap, table);
	for (i=0; i<ncols; i++) {
		fitscol_t *col = bl_access(table->cols, i);
		void *columndata = va_arg(ap, void *);
        if (col->fitstype != col->ctype) {
            int sz = MAX(256, MAX(col->csize, col->fitssize) * col->arraysize);
            if (sz > Nbuf) {
                free(buf);
                buf = malloc(sz);
            }
            fits_convert_data(buf, col->fitssize, col->fitstype,
                              columndata, col->csize, col->ctype,
                              col->arraysize);
            columndata = buf;
        }
        ret = fits_write_data_array(table->fid, columndata,
                                    col->fitstype, col->arraysize);
        if (ret)
            break;
    }
	va_end(ap);
    free(buf);
	//table->written_rows++;
    table->table->nr++;
    return ret;
}

void fitstable_clear_table(fitstable_t* tab) {
    bl_remove_all(tab->cols);
}

static void* read_array(const fitstable_t* tab,
                        const char* colname, tfits_type ctype,
                        bool array_ok) {
    int colnum;
    qfits_col* col;
    int fitssize;
    int csize;
    int fitstype;
    int arraysize;
    void* data;
    int N;

    colnum = fits_find_column(tab->table, colname);
    if (colnum == -1) {
        fprintf(stderr, "Column \"%s\" not found in FITS table %s.\n", colname, tab->fn);
        return NULL;
    }
    col = tab->table->col + colnum;
    if (!array_ok && (col->atom_nb != 1)) {
        fprintf(stderr, "Column \"%s\" in FITS table %s is an array of size %i, not a scalar.\n",
                colname, tab->fn, col->atom_nb);
        return NULL;
    }

    arraysize = col->atom_nb;
    fitstype = col->atom_type;
    fitssize = fits_get_atom_size(fitstype);
    csize = fits_get_atom_size(ctype);
    N = tab->table->nr;
    data = calloc(MAX(csize, fitssize), N * arraysize);

    qfits_query_column_seq_to_array(tab->table, colnum, 0, N, data, 
                                    fitssize * arraysize);

    if (fitstype != ctype) {
        if (csize <= fitssize) {
            fits_convert_data(data, csize, ctype,
                              data, fitssize, fitstype,
                              N * arraysize);
            if (csize < fitssize)
                data = realloc(data, csize * N * arraysize);
        } else if (csize > fitssize) {
            // HACK - stride backwards from the end of the array
            fits_convert_data(((char*)data) + ((N*arraysize)-1) * csize,
                              -csize, ctype,
                              ((char*)data) + ((N*arraysize)-1) * fitssize,
                              -fitssize, fitstype,
                              N * arraysize);
        }
    }
	return data;
}

void* fitstable_read_column(const fitstable_t* tab,
                            const char* colname, tfits_type ctype) {
    return read_array(tab, colname, ctype, FALSE);
}

void* fitstable_read_column_array(const fitstable_t* tab,
                                  const char* colname, tfits_type t) {
    return read_array(tab, colname, t, TRUE);
}

qfits_header* fitstable_get_primary_header(fitstable_t* t) {
    return t->primheader;
}

qfits_header* fitstable_get_header(fitstable_t* t) {
    if (!t->header) {
        fitstable_new_table(t);
    }
    return t->header;
}

void fitstable_next_extension(fitstable_t* tab) {
    fits_pad_file(tab->fid);
    qfits_table_close(tab->table);
    qfits_header_destroy(tab->header);
    tab->extension++;
    tab->table = NULL;
    tab->header = NULL;
}

static fitstable_t* fitstable_new() {
    fitstable_t* tab;
    tab = calloc(1, sizeof(fitstable_t));
    if (!tab)
        return tab;
    tab->cols = bl_new(8, sizeof(fitscol_t));
    tab->extra_cols = il_new(8);
    return tab;
}

fitstable_t* fitstable_open(const char* fn) {
    fitstable_t* tab;
    tab = fitstable_new();
    if (!tab) {
		fprintf(stderr, "Failed to allocate new FITS table structure.\n");
        goto bailout;
	}
    tab->extension = 1;
    tab->fn = strdup(fn);
    tab->primheader = qfits_header_read(fn);
    if (!tab->primheader) {
        fprintf(stderr, "Failed to read primary FITS header from %s.\n", fn);
        goto bailout;
    }
    if (fitstable_open_extension(tab, tab->extension)) {
        fprintf(stderr, "Failed to open extension %i in file %s.\n", tab->extension, fn);
        goto bailout;
    }
	return tab;
 bailout:
    if (tab) {
        fitstable_close(tab);
    }
    return NULL;
}

fitstable_t* fitstable_open_for_writing(const char* fn) {
    fitstable_t* tab;
    tab = fitstable_new();
    if (!tab)
        goto bailout;
    tab->fn = strdup(fn);
    tab->fid = fopen(fn, "wb");
	if (!tab->fid) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		goto bailout;
	}
	tab->primheader = qfits_table_prim_header_default();
    return tab;

 bailout:
    if (tab) {
        fitstable_close(tab);
    }
    return NULL;
}

int fitstable_close(fitstable_t* tab) {
    int rtn = 0;
    if (!tab) return 0;
    if (tab->fid) {
        if (fclose(tab->fid)) {
            fprintf(stderr, "Failed to close output file %s: %s\n", tab->fn, strerror(errno));
            rtn = -1;
        }
    }
    if (tab->primheader) {
        qfits_header_destroy(tab->primheader);
    }
    free(tab->fn);
    bl_free(tab->cols);
    il_free(tab->extra_cols);
    free(tab);
    return rtn;
}

void fitstable_add_columns(fitstable_t* tab, fitscol_t* cols, int Ncols) {
    int i;
    for (i=0; i<Ncols; i++) {
        fitscol_t* col = bl_append(tab->cols, cols + i);
        col->csize = fits_get_atom_size(col->ctype);
        col->fitssize = fits_get_atom_size(col->fitstype);
    }
}

void fitstable_add_column(fitstable_t* tab, fitscol_t* col) {
    fitstable_add_columns(tab, col, 1);
}

int fitstable_get_array_size(fitstable_t* tab, const char* name) {
    qfits_col* qcol;
    int colnum;
    colnum = fits_find_column(tab->table, name);
    if (colnum == -1)
        return -1;
    qcol = tab->table->col + colnum;
    return qcol->atom_nb;
}

int fitstable_get_type(fitstable_t* tab, const char* name) {
    qfits_col* qcol;
    int colnum;
    colnum = fits_find_column(tab->table, name);
    if (colnum == -1)
        return -1;
    qcol = tab->table->col + colnum;
    return qcol->atom_type;
}

int fitstable_open_next_extension(fitstable_t* tab) {
    tab->extension++;
    return fitstable_open_extension(tab, tab->extension);
}

int fitstable_open_extension(fitstable_t* tab, int ext) {
    if (tab->table) {
        qfits_table_close(tab->table);
    }
	tab->table = qfits_table_open(tab->fn, ext);
	if (!tab->table) {
		fprintf(stderr, "FITS extension %i in file %s is not a table (or there was an error opening the file).\n", ext, tab->fn);
		return -1;
	}
    if (tab->header) {
        qfits_header_destroy(tab->header);
    }
    tab->header = qfits_header_readext(tab->fn, ext);
	if (!tab->header) {
		fprintf(stderr, "Couldn't get header for FITS extension %i in file %s.\n", ext, tab->fn);
		return -1;
	}
    //tab->reading_row = 0;
    return 0;
}

int fitstable_read_extension(fitstable_t* tab, int ext) {
    int i;
    int ok = 1;

    if (tab->table) {
        qfits_table_close(tab->table);
    }
	tab->table = qfits_table_open(tab->fn, ext);
	if (!tab->table) {
		fprintf(stderr, "FITS extension %i in file %s is not a table (or there was an error opening the file).\n", ext, tab->fn);
		return -1;
	}
    if (tab->header) {
        qfits_header_destroy(tab->header);
    }
    tab->header = qfits_header_readext(tab->fn, ext);
	if (!tab->header) {
		fprintf(stderr, "Couldn't get header for FITS extension %i in file %s.\n", ext, tab->fn);
		return -1;
	}
    for (i=0; i<ncols(tab); i++) {
        fitscol_t* col = getcol(tab, i);
        qfits_col* qcol;

        // FIXME - target?

        // ? set this here?
        col->csize = fits_get_atom_size(col->ctype);

        // Column found?
        col->col = fits_find_column(tab->table, col->colname);
        if (col->col == -1)
            continue;
        qcol = tab->table->col + col->col;
        // save params
        col->fitstype = qcol->atom_type;
        col->arraysize = qcol->atom_nb;
        col->fitssize = fits_get_atom_size(col->fitstype);

        // Type correct?
        if (col->target_fitstype != fitscolumn_any_type() &&
            col->target_fitstype != col->fitstype) {
            col->col = -1;
            continue;
        }
        // Array size correct?
        if (col->target_arraysize &&
            col->arraysize != col->target_arraysize) {
            col->col = -1;
            continue;
        }
    }

    for (i=0; i<ncols(tab); i++) {
        fitscol_t* col = getcol(tab, i);
        if (col->col == -1 && col->required) {
            ok = 0;
            break;
        }
    }
	if (ok) return 0;
	return -1;
}

int fitstable_write_primary_header(fitstable_t* t) {
	assert(t->fid);
	assert(t->primheader);
    // note, qfits_header_dump pads the file to the next FITS block.
	qfits_header_dump(t->primheader, t->fid);
	t->end_header_offset = ftello(t->fid);
	return 0;
}

int fitstable_fix_primary_header(fitstable_t* t) {
	off_t offset;
    off_t old_end;
    off_t new_end;
	assert(t->fid);
	assert(t->primheader);
	offset = ftello(t->fid);
	fseeko(t->fid, 0, SEEK_SET);
    old_end = t->end_header_offset;
    fitstable_write_header(t);
    new_end = t->end_header_offset;
	if (old_end != new_end) {
		fprintf(stderr, "Error: fitstable header size changed: was %u, but is now %u.  Corruption is likely!\n",
				(uint)old_end, (uint)new_end);
		return -1;
	}
	fseeko(t->fid, offset, SEEK_SET);
	return 0;
}

// Called just before starting to write a new field.
int fitstable_new_table(fitstable_t* t) {
    if (t->table) {
        qfits_table_close(t->table);
    }
    fitstable_create_table(t);
    if (t->header) {
        qfits_header_destroy(t->header);
    }
    t->header = qfits_table_ext_header_default(t->table);
    return 0;
}

int fitstable_write_header(fitstable_t* t) {
	assert(t->fid);
    if (!t->header) {
        if (fitstable_new_table(t)) {
            return -1;
        }
    }
	assert(t->header);
    // add padding.
    fits_pad_file(t->fid);
	t->table_offset = ftello(t->fid);
	qfits_header_dump(t->header, t->fid);
    t->end_table_offset = ftello(t->fid);
	return 0;
}

int fitstable_fix_header(fitstable_t* t) {
	off_t offset;
    off_t old_end;
    off_t new_end;
	assert(t->fid);
	assert(t->header);
	offset = ftello(t->fid);
	fseeko(t->fid, t->table_offset, SEEK_SET);
    old_end = t->end_table_offset;
    // update NAXIS2 to reflect the number of rows written.
    fits_header_mod_int(t->header, "NAXIS2", t->table->nr, NULL);
    fitstable_write_header(t);
    new_end = t->end_table_offset;
	if (old_end != new_end) {
		fprintf(stderr, "Error: fitstable table header size changed: was %u, but is now %u.  Corruption is likely!\n",
				(uint)old_end, (uint)new_end);
		return -1;
	}
    // go back to where we were (ie, probably the end of the data array)...
	fseeko(t->fid, offset, SEEK_SET);
    // add padding.
    fits_pad_file(t->fid);
	return 0;
}

/*
 void fitstable_reset_table(fitstable_t* tab) {
 if (tab->table) {
 qfits_table_close(tab->table);
 }
 fitstable_create_table(tab);
 }
 */

void fitstable_close_table(fitstable_t* tab) {
    int i;
    if (tab->table) {
        qfits_table_close(tab->table);
        tab->table = NULL;
    }
    for (i=0; i<ncols(tab); i++) {
        fitscol_t* col = getcol(tab, i);
        col->col = -1;
        col->fitssize = 0;
        col->arraysize = 0;
        col->fitstype = fitscolumn_any_type();
    }
}

int fitstable_nrows(fitstable_t* t) {
    return t->table->nr;
}

void fitstable_print_missing(fitstable_t* tab, FILE* f) {
    int i;
    fprintf(f, "Missing required rows: ");
    for (i=0; i<ncols(tab); i++) {
        fitscol_t* col = getcol(tab, i);
        if (col->col == -1 && col->required) {
            fprintf(f, "%s ", col->colname);
        }
    }
    fprintf(f, "\n");
}

fitscol_t* fitstable_get_column(fitstable_t* table, int col) {
    return getcol(table, col);
}

int fitstable_read_array(const fitstable_t* tab,
                         int offset, int N,
                         void* maindata, int mainstride) {
    int i;
    void* tempdata = NULL;
    int highwater = 0;

    // We read in column-major order.

    for (i=0; i<ncols(tab); i++) {
        void* dest;
        int stride;
        void* finaldest;
        int finalstride;
        fitscol_t* col = getcol(tab, i);

        if (col->col == -1)
            continue;

        if (col->in_struct) {
            finaldest = ((char*)maindata) + col->coffset;
            finalstride = mainstride;
        } else if (col->cdata) {
            finaldest = col->cdata;
            finalstride = col->cdata_stride;
        } else
            continue;

        if (col->fitstype != col->ctype) {
            int NB = col->fitssize * N;
            if (NB > highwater) {
                free(tempdata);
                tempdata = malloc(NB);
                highwater = NB;
            }
            dest = tempdata;
            stride = col->fitssize;
        } else {
            dest = finaldest;
            stride = finalstride;
        }

        // Read from FITS file...
        qfits_query_column_seq_to_array
            (tab->table, col->col, offset, N, dest, stride);

        if (col->fitstype != col->ctype) {
            int j;
            for (j=0; j<col->arraysize; j++)
                fits_convert_data(((char*)finaldest) + j * fits_get_atom_size(col->ctype), finalstride, col->ctype,
                                  ((char*)dest) + j * fits_get_atom_size(col->fitstype), stride, col->fitstype, N);
        }
    }
    free(tempdata);
    return 0;
}

int fitstable_write_array(const fitstable_t* tab,
                          int offset, int N,
                          const void* maindata, int mainstride) {
    int i, j;
    void* tempdata = NULL;
    int highwater = 0;

    // We write in row-major order (to avoid fseeking around)

    for (j=0; j<N; j++) {
        for (i=0; i<ncols(tab); i++) {
            fitscol_t* col = getcol(tab, i);
            char* src;
            int k;

            // FIXME - should we pay attention to the col->col entry?
            // Or do we assume that when writing the orders match?
            // Ugh!!

            if (col->in_struct) {
                src = ((char*)maindata) + col->coffset
                    + mainstride * (offset + j);
            } else if (col->cdata) {
                src = col->cdata +
                    + col->cdata_stride * (offset + j);
            } else
                continue;

            if (col->fitstype != col->ctype) {
                int NB = col->fitssize;
                if (NB > highwater) {
                    free(tempdata);
                    tempdata = malloc(NB);
                    highwater = NB;
                }
                fits_convert_data(tempdata, fits_get_atom_size(col->fitstype), col->fitstype,
                                  src, fits_get_atom_size(col->ctype), col->ctype,
                                  col->arraysize);
                src = tempdata;
            }

            for (k=0; k<col->arraysize; k++)
                fits_write_data(tab->fid, src + fits_get_atom_size(col->fitstype) * k, col->fitstype);
        }
    }
    free(tempdata);

    // Increment row counter...
    tab->table->nr += N;
    return 0;
}

static void fitstable_create_table(fitstable_t* tab) {
    qfits_table* qt;
    int i;

    qt = qfits_table_new("", QFITS_BINTABLE, 0, ncols(tab), 0);
    tab->table = qt;

    for (i=0; i<ncols(tab); i++) {
        fitscol_t* col = getcol(tab, i);
		char* nil = "";
		assert(col->colname);
 fits_add_column(qt, i, col->fitstype, col->arraysize,
						col->units ? col->units : nil, col->colname);
    }
}

