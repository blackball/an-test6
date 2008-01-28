#include "qfits_table.h"

enum anfits_type_e {
	ANTYPE_DOUBLE = TFITS_BIN_TYPE_D
};
typedef enum anfits_type_e anfits_type_t;

/* Table handling */

struct anfits_table_t;
typedef struct anfits_table_t anfits_table_t;

anfits_table_t* anfits_table_open_for_writing(const char* fn);
void            anfits_table_add_column(anfits_table_t* table, const char* name, anfits_type_t type);
int             anfits_table_write_row(anfits_table_t* table, ...);
int             anfits_table_close(anfits_table_t* table);

anfits_table_t* anfits_table_open(const char* fn);
int             anfits_table_nrows(const anfits_table_t* table);
int             anfits_table_ncols(const anfits_table_t* table);
void            anfits_table_read_column(anfits_table_t* table, const char* name, anfits_type_t, void **out);


