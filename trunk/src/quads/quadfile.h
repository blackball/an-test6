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

double quadfile_get_index_scale_arcsec(quadfile* qf);

double quadfile_get_index_scale_lower_arcsec(quadfile* qf);

void quadfile_get_starids(quadfile* qf, uint quadid,
						  uint* starA, uint* starB,
						  uint* starC, uint* starD);

//// Traditional binary-format specific:

quadfile* quadfile_open(char* quadfname);

quadfile* quadfile_open_for_writing(char* quadfname);

int quadfile_write_quad(quadfile* qf, uint iA, uint iB, uint iC, uint iD);

/*
  int quadfile_write_header(FILE *quadfid, uint numQuads, uint numstars,
  uint DimQuads, double index_scale);
*/

int quadfile_fix_header(quadfile* qf);


//// FITS-specific

quadfile* quadfile_fits_open(char* quadfname);

// updates the "NQUADS" entry and the "quads" table descriptor.
int quadfile_fits_fix_header(quadfile* qf);

quadfile* quadfile_fits_open_for_writing(char* quadfname);

int quadfile_fits_write_quad(quadfile* qf, uint iA, uint iB, uint iC, uint iD);


#endif
