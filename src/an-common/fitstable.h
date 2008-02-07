
#ifndef FITSTABLE_H
#define FITSTABLE_H

#include <sys/types.h>
#include <stdio.h>

#include "qfits_table.h"
#include "an-bool.h"
#include "bl.h"

/**
 For quick-n-easy(-ish) access to a column of data in a FITS BINTABLE.

 Some example usage scenarios:

 char* filename = "/tmp/xxx";
 fitstable_t* tab = fitstable_open_for_writing(filename);
 // Add column "X", a scalar double (FITS type D)
 fitstable_add_write_column(tab, fitscolumn_double_type(), "X", "xunits");
 // Add column "Y", a 2-D float (FITS type E), but the data to be
 // written is in C doubles.
 fitstable_add_write_column_array_convert(tab, fitscolumn_float_type(), fitscolumn_double_type(), 2, "Y", "yunits");
 // Add some stuff to the primary header...
 qfits_header* hdr = fitstable_get_primary_header(tab);
 fits_header_add_int(hdr, "KEYVAL", 42, "Comment");
 // Add some stuff to the extension header...
 hdr = fitstable_get_header(tab);
 fits_header_add_int(hdr, "KEYVAL2", 43, "Comment");
 // Write...
 if (fitstable_write_primary_header(tab) ||
     fitstable_write_header(tab)) {
   // error...
 }
 // Write data...
 double x[] = { 1,2,3 };
 double y[] = { 3,4, 5,6, 7,8 };
 int i, N = 3;
 for (i=0; i<N; i++)
   fitstable_write_row(tab, x + i, y + 2*i);
 if (fitstable_fix_header(tab)) {
   // error...
 }
 // Write some data to another extension.
 fitstable_next_extension(tab);
 fitstable_clear_table(tab);
 // Add column "Z", a scalar double
 fitstable_add_write_column(tab, fitscolumn_double_type(), "Z", "zunits");
 // Add some stuff to the extension header...
 hdr = fitstable_get_header(tab);
 fits_header_add_int(hdr, "KEYVAL3", 44, "Comment");
 if (fitstable_write_header(tab)) {
   // error...
 }
 // Write data...
 double z[] = { 9, 10, 11 };
 N = 3;
 for (i=0; i<N; i++)
   fitstable_write_row(tab, z + i);
 if (fitstable_fix_header(tab) ||
     fitstable_close(tab)) {
   // error...
 }


 char* filename = "/tmp/xxx";
 fitstable_t* tab = fitstable_open(filename);
 // Read the primary header.
 qfits_header* hdr = fitstable_get_primary_header(tab);
 int val = qfits_header_getint(hdr, "KEYVAL", -1);
 // Read a value from the extension header.
 hdr = fitstable_get_header(tab);
 int val2 = qfits_header_getint(hdr, "KEYVAL2", -1);
 // Read the arrays.
 int N = fitstable_nrows(tab);
 // Read a column in the first extension table as a scalar double.
 tfits_type dubl = fitscolumn_double_type();
 double* x = fitstable_read_column(tab, "X", dubl);
 // Read a column into a double array.
 int D = fitstable_get_array_size(tab, "Y");
 double* y = fitstable_read_column_array(tab, "Y", dubl);
 // Read the second extension...
 fitstable_open_next_extension(tab);
 // Read a value from the extension header...
 hdr = fitstable_get_header(tab);
 int val3 = qfits_header_getint(hdr, "KEYVAL3", -1);
 // Read the arrays.
 N = fitstable_nrows(tab);
 double* z = fitstable_read_column(tab, "Z", dubl);
 // Done.
 fitstable_close(tab);


 */

struct fitstable_t {
    qfits_table* table;
    // header for this extension's table
    qfits_header* header;

    // primary header
    qfits_header* primheader;

    bl* cols;

    int extension;

    //bool writing;

    // When writing:
	char* fn;
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
tfits_type fitscolumn_float_type();

// When reading: allow this column to match to any FITS type.
tfits_type fitscolumn_any_type();

fitstable_t* fitstable_open(const char* fn);

fitstable_t* fitstable_open_for_writing(const char* fn);

// reading...
int fitstable_open_extension(fitstable_t* tab, int ext);

// reading...
int fitstable_open_next_extension(fitstable_t* tab);

int  fitstable_close(fitstable_t*);

int fitstable_ncols(fitstable_t* t);

int fitstable_nrows(fitstable_t* t);

// when writing...
void fitstable_next_extension(fitstable_t* tab);

// when writing: remove all existing columns from the table.
void fitstable_clear_table(fitstable_t* tab);

// Called just before starting to write a new table (extension).
int fitstable_new_table(fitstable_t* t);

// when reading...
int  fitstable_read_extension(fitstable_t* tab, int ext);

int fitstable_get_array_size(fitstable_t* tab, const char* name);

int fitstable_get_type(fitstable_t* tab, const char* name);

void fitstable_add_read_column_struct(fitstable_t* tab,
                                      tfits_type c_type,
                                      int arraysize,
                                      int structoffset,
                                      tfits_type fits_type,
                                      const char* name);

void fitstable_add_write_column_struct(fitstable_t* tab,
                                       tfits_type c_type,
                                       int arraysize,
                                       int structoffset,
                                       tfits_type fits_type,
                                       const char* name,
                                       const char* units);

void fitstable_add_column_struct(fitstable_t* tab,
                                 tfits_type c_type,
                                 int arraysize,
                                 int structoffset,
                                 tfits_type fits_type,
                                 const char* name,
                                 const char* units);

void fitstable_add_write_column(fitstable_t* tab, tfits_type t,
                                const char* name, const char* units);

void fitstable_add_write_column_array(fitstable_t* tab, tfits_type t,
                                      int arraysize,
                                      const char* name,
                                      const char* units);

void fitstable_add_write_column_convert(fitstable_t* tab,
                                        tfits_type fits_type,
                                        tfits_type c_type,
                                        const char* name,
                                        const char* units);

void fitstable_add_write_column_array_convert(fitstable_t* tab,
                                              tfits_type fits_type,
                                              tfits_type c_type,
                                              int arraysize,
                                              const char* name,
                                              const char* units);

void* fitstable_read_column(const fitstable_t* tab,
                            const char* colname, tfits_type t);

void* fitstable_read_column_array(const fitstable_t* tab,
                                  const char* colname, tfits_type t);

int fitstable_write_row(fitstable_t* table, ...);

int fitstable_write_struct(fitstable_t* table, const void* struc);

int fitstable_read_struct(fitstable_t* table, int index, void* struc);

int fitstable_read_structs(fitstable_t* table, void* struc,
                           int stride, int offset, int N);

qfits_header* fitstable_get_primary_header(fitstable_t* t);

// Write primary header.
int fitstable_write_primary_header(fitstable_t* t);

// Rewrite (fix) primary header.
int fitstable_fix_primary_header(fitstable_t* t);

qfits_header* fitstable_get_header(fitstable_t* t);

// Write the table header.
int fitstable_write_header(fitstable_t* t);

// Rewrite (fix) the table header.
int fitstable_fix_header(fitstable_t* t);

// When reading: close the current table and reset all fields that refer to it.
void fitstable_close_table(fitstable_t* tab);


#endif
