
#include <sys/types.h>

#include "qfits_table.h"

/**
 For quick-n-easy access to a column of data in a FITS BINTABLE.
 */

struct fitscol_t {
    const char* colname;
    tfits_type fitstype;
    tfits_type ctype;

    // Internals
    qfits_table* table;
    // column number of the FITS table.
    int col;
    off_t data_offset;
};
typedef struct fitscol_t fitscol_t;




