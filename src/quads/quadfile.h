#ifndef QUADFILE_H
#define QUADFILE_H

#ifndef uint
typedef unsigned int uint;
#endif

struct quadfile {
	uint numquads;
	uint numstars;
	double index_scale;
	void*  mmap_quad;
	size_t mmap_quad_size;
	uint*   quadarray;
};
typedef struct quadfile quadfile;

void quadfile_close(quadfile* qf);

quadfile* quadfile_open(char* quadfname);

double quadfile_get_index_scale_arcsec(quadfile* qf);

void quadfile_get_starids(quadfile* qf, uint quadid,
						  uint* starA, uint* starB,
						  uint* starC, uint* starD);

#endif
