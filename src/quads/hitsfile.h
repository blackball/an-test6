#ifndef HITSFILE_H
#define HITSFILE_H

#include <stdio.h>

#include "starutil.h"
#include "solver.h"
#include "matchfile.h"

struct hitsfileheader {
	float agreetol;
	unsigned int min_matches_to_agree;
	float overlap_needed;
	unsigned int field_objs_needed;
};
typedef struct hitsfileheader hits_header;

struct hitsfieldentry {
	bool failed;
	unsigned int field;
};
typedef struct hitsfieldentry hits_field;

void hits_header_init(hits_header* h);

void hits_write_header(FILE* fid, hits_header* h);

void hits_write_tailer(FILE* fid);


void hits_field_init(hits_field* h);

void hits_write_field_header(FILE* fid, hits_field* h);

void hits_start_hits_list(FILE* fid);

void hits_write_hit(FILE* fid, MatchObj* mo);

void hits_end_hits_list(FILE* fid);

void hits_write_field_tailer(FILE* fid);


void hits_write_correspondences(FILE* fid, uint* starids, uint* fieldids,
								int Nids, int ok);

#endif
