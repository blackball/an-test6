#include "anfits.h"
#include "bl.h"
#include "qfits.h"
#include <stdio.h>

// PRIVATE data structure
struct column_t {
	char* name;
	char* units; // is NULL for reading
	anfits_type_t type;
};
typedef struct column_t column_t;

struct anfits_table_t {
	char* filename;
	bl* columns;
	qfits_table* qtable;
	int written_rows;
	FILE *fid;
};
