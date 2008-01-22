
#include <sys/types.h>
#include <stdio.h>

#include "qfits_table.h"
#include "an-bool.h"
#include "bl.h"

/**
 For quick-n-easy access to a column of data in a FITS BINTABLE.
 */

struct fitscol_t;
typedef struct fitscol_t fitscol_t;

struct fitscol_t {
    const char* colname;

    // ??? ARGH
    tfits_type target_fitstype;
    tfits_type target_arraysize;

    tfits_type fitstype;
    tfits_type ctype;
    const char* units;
    int arraysize;

    bool required;

    // size of one data item (or the whole array if it's an array type)
    // computed: fits_sizeof({fits,c}type) * arraysize
    int fitssize;
    int csize;

    // When being used to write to a C struct, the offset in the struct.
    // ???
    bool in_struct;
    int coffset;

    // ??? Called to retrieve data to be written to the output file.
    //void (*get_data_callback)(void* data, int offset, int N, fitscol_t* col, void* user);
    //void* get_data_user;
    // ??? Called when data has been read from the input file.
    //void (*put_data_callback)(void* data, int offset, int N, fitscol_t* col, void* user);
    //void* put_data_user;

    // Where to read/write data from/to.
    void* cdata;
    int cdata_stride;

    // Internals
    //qfits_table* table;

    // column number of the FITS table.
    int col;
    // ??? offset in the file of the first data element.
    //off_t fileoffset;
};


// ????
struct fitstable_t {
    qfits_table* table;
    // header for this extension's table
    qfits_header* header;

    // primary header
    qfits_header* primheader;

    //pl* cols;
    bl* cols;
    // ???
    il* extra_cols;

    // When writing:
	const char* fn;
    FILE* fid;
    // the end of the primary header (including FITS padding)
    off_t end_header_offset;
    // beginning of the current table
    off_t table_offset;
    // end of the current table (including FITS padding)
    off_t end_table_offset;
};
typedef struct fitstable_t fitstable_t;


// Returns the FITS type of "int" on this machine.
tfits_type fitscolumn_int_type();
tfits_type fitscolumn_double_type();

// When reading: allow this column to match to any FITS type.
tfits_type fitscolumn_any_type();

fitstable_t* fitstable_new();

fitstable_t* fitstable_open_for_writing(const char* fn);

void fitstable_free(fitstable_t*);

void fitstable_add_columns(fitstable_t* tab, fitscol_t* cols, int Ncols);

void fitstable_add_column(fitstable_t* tab, fitscol_t* col);

int fitstable_read(fitstable_t* tab, qfits_table* qtab);

// = new() + add_columns() + read().
//int fitstable_find(const qfits_table* tab, fitscol_t* cols, int Ncols);

// When reading: close the current table and reset all fields that refer to it.
void fitstable_close_table(fitstable_t* tab);

// When writing: close the existing table, reset everything, and create a
// new table.
void fitstable_reset_table(fitstable_t* tab);

int fitstable_nrows(fitstable_t* t);

void fitstable_print_missing(fitstable_t* tab, FILE* f);

fitscol_t* fitstable_get_column(fitstable_t* table, int col);

int fitstable_read_array(const fitstable_t* tab,
                         //const fitscol_t* cols, int Ncols,
                         int offset, int N,
                         void* data, int stride);

int fitstable_write_array(const fitstable_t* tab,
                          int offset, int N,
                          const void* data, int stride,
                          FILE* fid);

//int fitscolumn_find_extra(fitstable_t* tab, fitscol_t* col, bool claim);

// for writing...
void fitstable_create_table(fitstable_t* tab);

