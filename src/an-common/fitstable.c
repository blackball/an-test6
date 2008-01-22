#include "fitstable.h"
#include "fitsioutils.h"

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

fitstable_t* fitstable_new() {
    fitstable_t* tab;
    tab = calloc(1, sizeof(fitstable_t));
    if (!tab)
        return tab;
    tab->cols = bl_new(8, sizeof(fitscol_t));
    tab->extra_cols = il_new(8);
    return tab;
}

void fitstable_free(fitstable_t* tab) {
    if (!tab) return;
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

int fitstable_read(fitstable_t* tab, qfits_table* qtab) {
    int i;
    int ok = 1;
    tab->table = qtab;
    for (i=0; i<ncols(tab); i++) {
        fitscol_t* col = getcol(tab, i);
        qfits_col* qcol;

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

void fitstable_create_table(fitstable_t* tab) {
    qfits_table* qt;
    int i;

    qt = qfits_table_new("", QFITS_BINTABLE, 0, ncols(tab), 0);
    tab->table = qt;

    for (i=0; i<ncols(tab); i++) {
        fitscol_t* col = getcol(tab, i);
        fits_add_column(qt, i, col->fitstype, col->arraysize, col->units, col->colname);
    }
}

