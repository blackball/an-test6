#include <stdio.h>
#include <stdlib.h>

#include "qfits.h"
#include "fitsioutils.h"

struct badfield {
	int run;
	int firstfield;
	int lastfield;
	char* problem;
	char* comments;
};
typedef struct badfield badfield;

static badfield baddies[] = {
#include "badfields.inc"
};

int main(int argc, char** args) {
	int i;
	int NB = sizeof(baddies) / sizeof(badfield);

	printf("NB %i.\n", NB);
	for (i=0; i<NB; i++) {
		printf("run %i, fields %i-%i.\n", baddies[i].run, baddies[i].firstfield, baddies[i].lastfield);
	}

	if (argc == 3) {
		//char* template = args[1];
		char* fn;
		//for (i=1; i<=35; i++) {
		//char fn[256];
		int filenum;
		int irun, ifield;
		int ext, Next;
		int run, field;
		qfits_table* table;
		printf("# filenum fieldnum run field\n");
		fn = args[1];
		filenum = atoi(args[2]);
		//sprintf(fn, template, i);
		Next = qfits_query_n_ext(fn);
		for (ext=1; ext<Next; ext++) {
			table  = qfits_table_open(fn, ext);
			if (!table) {
				fprintf(stderr, "Table %i in file %s not found.\n", ext, fn);
				continue;
			}
			irun   = fits_find_column(table, "RUN");
			ifield = fits_find_column(table, "FIELD");
			if (irun == -1 || ifield == -1) {
				fprintf(stderr, "Columns RUN or FIELD not found.\n");
				continue;
			}
			qfits_query_column_seq_to_array(table, irun, 0, 1, &run, sizeof(int));
			qfits_query_column_seq_to_array(table, ifield, 0, 1, &field, sizeof(int));
			//printf("Table %i: run %i, field %i.\n", ext, run, field);
			printf("%i %i %i %i\n", filenum, ext-1, run, field);
			fflush(stdout);
			qfits_table_close(table);
		}
		//}
	}

	return 0;
}
