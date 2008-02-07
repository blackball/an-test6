#include <stdio.h>

#include "anfits.h"
#include "bl.h"
#include "qfits.h"
#include "an-bool.h"

// PRIVATE data structure
struct column_t {
	char* name;
	char* units; // is NULL for reading
	anfits_type_t type;
};
typedef struct column_t column_t;

struct anfits_table_t {
	char* filename;
	qfits_table* qtable;
    //
    bool writing;
    // 
    qfits_header* primheader;
    qfits_header* header;

    // only used when writing:
	off_t primheader_end;
	off_t header_start;
	off_t header_end;

	int written_rows;
	FILE *fid;
	bl* columns;
};
