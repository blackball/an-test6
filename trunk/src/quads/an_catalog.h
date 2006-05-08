#ifndef AN_CATALOG_H
#define AN_CATALOG_H

#include <stdio.h>
#include <stdint.h>

typedef uint64_t uint64;

#include "qfits.h"

enum an_sources {
	AN_SOURCE_UNKNOWN,
	AN_SOURCE_USNOB,
	AN_SOURCE_TYCHO2
};

struct an_observation {
	unsigned char catalog;
	unsigned char band;
	uint id;
	float mag;
	float sigma_mag;
};
typedef struct an_observation an_observation;

#define AN_N_OBSERVATIONS 5

struct an_entry {
	uint64 id;
	double ra;
	double dec;
	float motion_ra;
	float motion_dec;
	float sigma_ra;
	float sigma_dec;
	float sigma_motion_ra;
	float sigma_motion_dec;
	int nobs;
	an_observation obs[AN_N_OBSERVATIONS];
};
typedef struct an_entry an_entry;

#define AN_FITS_COLUMNS 35

struct an_catalog {
	qfits_table* table;
	int columns[AN_FITS_COLUMNS];
	// when writing:
	qfits_header* header;
	FILE* fid;
	off_t data_offset;
	uint nentries;
};
typedef struct an_catalog an_catalog;

an_catalog* an_catalog_open(char* fn);

an_catalog* an_catalog_open_for_writing(char* fn);

int an_catalog_write_headers(an_catalog* cat);

int an_catalog_fix_headers(an_catalog* cat);

int an_catalog_read_entries(an_catalog* cat, uint offset,
							uint count, an_entry* entries);

int an_catalog_count_entries(an_catalog* cat);

int an_catalog_close(an_catalog* cat);

int an_catalog_write_entry(an_catalog* cat, an_entry* entry);

int64_t an_catalog_get_id(int catversion, int64_t starid);

#endif
