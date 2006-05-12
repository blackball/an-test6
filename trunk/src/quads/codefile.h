#ifndef CODEFILE_H_
#define CODEFILE_H_

#include <sys/types.h>
#include <stdio.h>

#include "starutil.h"
#include "qfits.h"

struct codefile {
	uint numcodes;
	uint numstars;
	// upper bound
	double index_scale;
	// lower bound
	double index_scale_lower;

	// when reading:
	void*  mmap_code;
	size_t mmap_code_size;
	double* codearray;

	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;
};
typedef struct codefile codefile;

int codefile_close(codefile* qf);

void codefile_get_code(codefile* qf, uint codeid,
                       double* cx, double* cy, double* dx, double* dy);

codefile* codefile_open(char* codefname, int modifiable);

codefile* codefile_open_for_writing(char* codefname);

int codefile_write_header(codefile* qf);

int codefile_write_code(codefile* qf, double cx, double cy, double dx, double dy);

int codefile_fix_header(codefile* qf);

#endif
