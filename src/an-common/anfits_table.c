#include "anfits_private.h"
#include "anfits.h"

anfits_table_t* anfits_table_open_for_writing(const char* fn) {
    return NULL;
}

void            anfits_table_add_column(anfits_table_t* table, const char* name, anfits_type_t type) {
}

int             anfits_table_write_row(anfits_table_t* table, ...) {
    return -1;
}

int             anfits_table_close(anfits_table_t* table) {
    return -1;
}

anfits_table_t* anfits_table_open(const char* fn) {
    return NULL;
}

int             anfits_table_nrows(const anfits_table_t* table) {
    return -1;
}

int             anfits_table_ncols(const anfits_table_t* table) {
    return -1;
}

void            anfits_table_read_column(anfits_table_t* table, const char* name, anfits_type_t, void **out) {
}

