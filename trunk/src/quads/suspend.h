#ifndef SUSPEND_H_
#define SUSPEND_H_

#include <stdio.h>

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "blocklist.h"

void suspend_write_header(FILE* fid, double index_scale, char* fieldfname,
						  char* treefname, uint nfields);

void suspend_write_field(FILE* fid, uint fieldnum,
						 uint objs_used, uint ntried, blocklist* hits);

int suspend_read_header(FILE* fid, double* index_scale, char* fieldfname,
						char* treefname, uint* nfields);

int suspend_read_field(FILE* fid, uint* fieldnum, uint* objs_used, uint* ntried,
					   blocklist* hits);

#endif
