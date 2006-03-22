#ifndef QUADFILE_H
#define QUADFILE_H

#include <sys/types.h>

struct quadfile {
	uint numquads;
	uint numstars;
	double index_scale;
	void*  mmap_quad;
	size_t mmap_quad_size;
	uint*   quadarray;

	// when writing:
	FILE* fid;
};
typedef struct quadfile quadfile;

void quadfile_close(quadfile* qf);

quadfile* quadfile_open(char* quadfname);

quadfile* quadfile_fits_open(char* quadfname);

double quadfile_get_index_scale_arcsec(quadfile* qf);

void quadfile_get_starids(quadfile* qf, uint quadid,
						  uint* starA, uint* starB,
						  uint* starC, uint* starD);

quadfile* quadfile_fits_open_for_writing(char* quadfname);

int quadfile_fits_write_quad(quadfile* qf, uint iA, uint iB, uint iC, uint iD);

int quadfile_fits_fix_header(quadfile* qf, uint numQuads);

//int read_one_quad(FILE *fid, uint *iA, uint *iB, uint *iC, uint *iD);

int write_one_quad(FILE *fid, uint iA, uint iB, uint iC, uint iD);

int write_quad_header(FILE *quadfid, uint numQuads, uint numstars,
                      uint DimQuads, double index_scale);

int fix_quad_header(FILE *quadfid, uint numQuads);

#endif
