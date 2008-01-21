
#include <sys/types.h>

#include "qfits_table.h"
#include "an-bool.h"
#include "bl.h"

/**
 For quick-n-easy access to a column of data in a FITS BINTABLE.
 */

struct fitscol_t {
    const char* colname;
    tfits_type fitstype;
    tfits_type ctype;
    char* units;
    int arraysize;

    bool required;

    // size of one data item (or the whole array if it's an array type)
    // computed: fits_sizeof({fits,c}type) * arraysize
    int fitssize;
    int csize;

    // When being used to write to a C struct, the offset in the struct.
    int coffset;

    // Internals
    qfits_table* table;
    // column number of the FITS table.
    int col;
    // offset in the file of the first data element.
    off_t fileoffset;
};
typedef struct fitscol_t fitscol_t;


// ????
struct fitstable_t {
    qfits_table* table;
    pl* cols;
    il* extra_cols;
};
typedef struct fitstable_t fitstable_t;


// Returns the FITS type of "int" on this machine.
tfits_type fitscolumn_int_type();

int fitscolumn_find(const qfits_table* tab, fitscolumn_t* cols, int Ncols);

int fitscolumn_read_array(const fitscolumn_t* cols, int Ncols,
                          int offset, int N,
                          void* data, int stride);

int fitscolumn_find_extra(fitstable_t* tab, fitscol_t* col, bool claim);
