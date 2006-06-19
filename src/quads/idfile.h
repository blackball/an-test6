#ifndef idfile_H
#define idfile_H

#include <sys/types.h>
#include <stdint.h>
#include <assert.h>

#include "qfits.h"

struct idfile {
	uint numstars;
	int healpix;

	// when reading:
	void*  mmap_base;
	size_t mmap_size;

	// ok, we actually needed that after all
	uint64_t* anidarray;

	// when writing:
	FILE* fid;
	qfits_header* header;
	off_t header_end;
};
typedef struct idfile idfile;

idfile* idfile_open(char* fname, int modifiable);

idfile* idfile_open_for_writing(char* quadfname);

int idfile_close(idfile* qf);

uint64_t idfile_get_anid(idfile* qf, uint starid);

int idfile_write_anid(idfile* qf, uint64_t anid /* astrometry.net id */ );

int idfile_fix_header(idfile* qf);

int idfile_write_header(idfile* qf);


#endif
