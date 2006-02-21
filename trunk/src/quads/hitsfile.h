#ifndef HITSFILE_H_
#define HITSFILE_H_

#include <stdio.h>

#include "starutil.h"
#include "solver2.h"
#include "matchfile.h"

struct hitsfileheader {
	char* field_file_name;
	char* tree_file_name;
	unsigned int nfields;
	unsigned int ncodes;
	unsigned int nstars;
	double codetol;
	// DEPRECATE: agreetol
	double agreetol;
	bool parity;
	unsigned int min_matches_to_agree;
	unsigned int max_matches_needed;
};
typedef struct hitsfileheader hits_header;

struct hitsfieldentry {
	bool user_quit;
	unsigned int field;
	unsigned int objects_in_field;
	unsigned int objects_examined;
	xy* field_corners;
	unsigned int ntries;
	unsigned int nmatches;
	unsigned int nagree;
	bool parity;
	char* fieldpath;
};
typedef struct hitsfieldentry hits_field;

void hits_header_init(hits_header* h);

void hits_write_header(FILE* fid, hits_header* h);

void hits_write_tailer(FILE* fid);


void hits_field_init(hits_field* h);

void hits_write_field_header(FILE* fid, hits_field* h);

void hits_start_hits_list(FILE* fid);

void hits_write_hit(FILE* fid, MatchObj* mo, matchfile_entry* me);

void hits_end_hits_list(FILE* fid);

void hits_write_field_tailer(FILE* fid);


void hits_write_correspondences(FILE* fid, sidx* starids, sidx* fieldids,
								int Nids, int ok);

#endif
