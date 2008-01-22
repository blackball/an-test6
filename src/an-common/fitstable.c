#include <string.h>
#include <errno.h>

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

// When reading: allow this column to match to any FITS type.
tfits_type fitscolumn_any_type() {
    return (tfits_type)-1;
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
    if (!tab)
        goto bailout;
    tab->fn = strdup(fn);
    tab->primheader = qfits_header_read(fn);
    if (!tab->primheader) {
        fprintf(stderr, "Failed to read primary FITS header from %s.\n", fn);
        goto bailout;
    }
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

void fitstable_close(fitstable_t* tab) {
    if (!tab) return;
    if (tab->fid) {
        if (fclose(tab->fid)) {
            fprintf(stderr, "Failed to close output file %s: %s\n", tab->fn, strerror(errno));
        }
    }
    if (tab->primheader) {
        qfits_header_destroy(tab->primheader);
    }
    free(tab->fn);
    bl_free(tab->cols);
    il_free(tab->extra_cols);
    free(tab);
}

void fitstable_add_columns(fitstable_t* tab, fitscol_t* cols, int Ncols) {
    int i;
    for (i=0; i<Ncols; i++)
        bl_append(tab->cols, cols + i);
}

void fitstable_add_column(fitstable_t* tab, fitscol_t* col) {
    fitstable_add_columns(tab, col, 1);
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
        col->csize = fits_get_atom_size(col->ctype) * col->arraysize;

        // Column found?
        col->col = fits_find_column(qtab, col->colname);
        if (col->col == -1)
            continue;
        qcol = qtab->col + col->col;
        // save params
        col->fitstype = qcol->atom_type;
        col->arraysize = qcol->atom_nb;
        col->fitssize = fits_get_atom_size(col->fitstype) * col->arraysize;

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
    return ok;
}

int fitstable_write_header(fitstable_t* t) {
	assert(t->fid);
	assert(t->primheader);
    // note, qfits_header_dump pads the file to the next FITS block.
	qfits_header_dump(t->primheader, t->fid);
	t->end_header_offset = ftello(t->fid);
	return 0;
}

int fitstable_fix_header(fitstable_t* t) {
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
    if (tab->table) {
        qfits_table_close(tab->table);
    }
    fitstable_create_table(tab);
    if (tab->header) {
        qfits_header_destroy(tab->header);
    }
    tab->header = qfits_table_ext_header_default(tab->table);
}

int fitstable_write_table_header(fitstable_t* t) {
	assert(t->fid);
	assert(t->header);
    // add padding.
    fits_pad_file(t->fid);
	t->table_offset = ftello(t->fid);
	qfits_header_dump(t->header, t->fid);
    t->end_table_offset = ftello(t->fid);
	return 0;
}

int fitstable_fix_table_header(fitstable_t* t) {
	off_t offset;
    off_t old_end;
    off_t new_end;
	assert(t->fid);
	assert(t->header);
	offset = ftello(t->fid);
	fseeko(t->fid, t->table_offset, SEEK_SET);
    old_end = t->end_table_offset;
    // update NAXIS2 to reflect the number of rows written.
    fits_header_mod_int(tab->header, "NAXIS2", t->table->nr, NULL);
    fitstable_write_table_header(t);
    new_end = t->end_table_offset;
	if (old_end != new_end) {
		fprintf(stderr, "Error: fitstable table header size changed: was %u, but is now %u.  Corruption is likely!\n",
				(uint)old_end, (uint)new_end);
		return -1;
	}
    // go back to where we were (ie, probably the end of the data array)...
	fseeko(t->fid, offset, SEEK_SET);
    // add padding.
    fits_pad_file(ls->fid);
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
                          const void* maindata, int mainstride,
                          FILE* fid) {
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
                fits_write_data(fid, src + fits_get_atom_size(col->fitstype) * k, col->fitstype);
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
        fits_add_column(qt, i, col->fitstype, col->arraysize, col->units, col->colname);
    }
}

