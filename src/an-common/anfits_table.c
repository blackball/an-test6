#include "anfits_private.h"
#include "anfits.h"
#include "fitsioutils.h"
#include <stdarg.h>
#include <errno.h>

anfits_table_t* anfits_table_open_for_writing(const char* fn) {
	anfits_table_t* table = malloc(sizeof(anfits_table_t));
	table->fid = fopen(fn, "wb");
	if (!table->fid) {
		printf("error in anfits_table_open_for_writing with %s: ", fn);
		perror(NULL);
		free(table);
		return NULL;
	}
	table->columns = bl_new(10, sizeof(column_t));
	table->filename = strdup(fn);
	table->qtable = NULL;
	table->written_rows = 0;
    return table;
}

void anfits_table_add_column(anfits_table_t* table, anfits_type_t type, const char* name, const char* units) {
	column_t* col = bl_extend(table->columns);
	col->name = strdup(name);
	col->type = type;
	col->units = strdup(units);
}

int anfits_table_write_header(anfits_table_t* table) {
	int ncols = anfits_table_ncols(table);
	table->qtable = qfits_table_new(table->filename,
			QFITS_BINTABLE, 0, ncols, table->written_rows);

	int i;
	for (i=0; i<ncols; i++) {
		column_t *col = bl_access(table->columns, i);
		fits_add_column(table->qtable, i,
				col->type, 1, col->units, col->name);
	}
	table->qtable->tab_w = qfits_compute_table_width(table->qtable);

	qfits_header* primary_hdr = qfits_table_prim_header_default();
	int ret = qfits_header_dump(primary_hdr, table->fid);
	qfits_header_destroy(primary_hdr);
	if (ret == -1)
		return -1;

	qfits_header* ext_hdr = qfits_table_ext_header_default(table->qtable);
	ret = qfits_header_dump(ext_hdr, table->fid);
	qfits_header_destroy(ext_hdr);
    return ret;
}

int anfits_table_write_row(anfits_table_t* table, ...) {
	va_list ap;
	int ncols = anfits_table_ncols(table);
	int i;

	int ret = 0;
	va_start(ap, table);
	for (i=0; i<ncols; i++) {
		column_t *col = bl_access(table->columns, i);
		void *columndata = va_arg(ap, void *);
		ret = fits_write_data(table->fid, columndata, col->type);
		if (ret)
			break;
	}
	va_end(ap);
	table->written_rows++;
    return ret;
}

int anfits_table_close(anfits_table_t* table) {
	fits_pad_file(table->fid);
	rewind(table->fid);
	anfits_table_write_header(table);
	int ncols = anfits_table_ncols(table);
	int i;
	for (i=0; i<ncols; i++) {
		column_t *col = bl_access(table->columns, i);
		free(col->name);
		free(col->units);
	}
	bl_free(table->columns);
	free(table->filename);
	int ret = fclose(table->fid);
	if (ret) {
		perror(NULL);
	}
	free(table);
    return ret;
}

anfits_table_t* anfits_table_open(const char* fn) {
    return NULL;
}

int anfits_table_nrows(const anfits_table_t* table) {
    return -1;
}

int anfits_table_ncols(const anfits_table_t* table) {
    return bl_size(table->columns);
}

void* anfits_table_read_column(
		anfits_table_t* table,
		const char* name, anfits_type_t type) {
	return NULL;
}

