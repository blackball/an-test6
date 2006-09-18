#ifndef QIDXFILE_H
#define QIDXFILE_H

#include <sys/types.h>

#include "qfits.h"

struct qidxfile {
	uint numstars;
	uint numquads;

	// when reading:
	void*  mmap_base;
	size_t mmap_size;
	uint* index;
	uint* heap;

	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;

	uint cursor_index;
	uint cursor_heap;
};
typedef struct qidxfile qidxfile;

int qidxfile_close(qidxfile* qf);

int qidxfile_get_quads(qidxfile* qf, uint starid, uint** quads, uint* nquads);

int qidxfile_write_star(qidxfile* qf, uint* quads, uint nquads);

int qidxfile_write_header(qidxfile* qf);

qidxfile* qidxfile_open(char* fname, int modifiable);

qidxfile* qidxfile_open_for_writing(char* qidxfname,
									uint nstars, uint nquads);

#endif
