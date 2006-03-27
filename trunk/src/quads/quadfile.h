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
	void*  mmap_quad;
	size_t mmap_quad_size;
	uint*   quadarray;

	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;
};
typedef struct quadfile quadfile;

void quadfile_close(quadfile* qf);

quadfile* quadfile_open(char* quadfname);

quadfile* quadfile_fits_open(char* quadfname);

double quadfile_get_index_scale_arcsec(quadfile* qf);

double quadfile_get_index_scale_lower_arcsec(quadfile* qf);

void quadfile_get_starids(quadfile* qf, uint quadid,
						  uint* starA, uint* starB,
						  uint* starC, uint* starD);

quadfile* quadfile_fits_open_for_writing(char* quadfname);

int quadfile_fits_write_quad(quadfile* qf, uint iA, uint iB, uint iC, uint iD);

// updates the "NQUADS" entry and the "quads" table descriptor.
int quadfile_fits_fix_header(quadfile* qf);

//int read_one_quad(FILE *fid, uint *iA, uint *iB, uint *iC, uint *iD);

int write_one_quad(FILE *fid, uint iA, uint iB, uint iC, uint iD);

int write_quad_header(FILE *quadfid, uint numQuads, uint numstars,
                      uint DimQuads, double index_scale);

int fix_quad_header(FILE *quadfid, uint numQuads);

#endif
