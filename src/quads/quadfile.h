#ifndef QUADFILE_H
#define QUADFILE_H

#include <sys/types.h>

#include "qfits.h"

struct quadfile {
	uint numquads;
	uint numstars;
	// upper bound
	double index_scale;
	// lower bound
	double index_scale_lower;

	// when reading:
	void*  mmap_quad;
	size_t mmap_quad_size;
	uint*   quadarray;

	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;
};
typedef struct quadfile quadfile;

int quadfile_close(quadfile* qf);

void quadfile_get_starids(quadfile* qf, uint quadid,
						  uint* starA, uint* starB,
						  uint* starC, uint* starD);

int quadfile_write_quad(quadfile* qf, uint iA, uint iB, uint iC, uint iD);

int quadfile_fix_header(quadfile* qf);

int quadfile_write_header(quadfile* qf);

double quadfile_get_index_scale_arcsec(quadfile* qf);

double quadfile_get_index_scale_lower_arcsec(quadfile* qf);

quadfile* quadfile_open(char* fname, int modifiable);

quadfile* quadfile_open_for_writing(char* quadfname);

#endif
