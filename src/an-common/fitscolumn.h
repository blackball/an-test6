
#include <sys/types.h>

#include "qfits_table.h"

/**
 For quick-n-easy access to a column of data in a FITS BINTABLE.
 */

enum ctype_t {
    CTYPE_DOUBLE,
    CTYPE_FLOAT,
    CTYPE_INT,
};
typedef enum ctype_t ctype_t;

struct fitscol_t {
    const char* colname;
    tfits_type fitstype;
    ctype_t ctype;

    // Internals
    qfits_table* table;
    // column number of the FITS table.
    int col;
    off_t data_offset;
};
typedef struct fitscol_t fitscol_t;




