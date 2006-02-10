#include <errno.h>
#include <string.h>

#include "suspend.h"
#include "solver2.h"
#include "ioutils.h"

uint SUSPEND_MAGIC = 0x33445566;

void suspend_write_header(FILE* fid, double index_scale, char* fieldfname,
						  char* treefname, unsigned int nfields) {
	int err = 0;
	int len;
	err =
		write_u32(fid, SUSPEND_MAGIC) ||
		write_u32_native(fid, ENDIAN_DETECTOR) ||
		write_double(fid, index_scale);

	len = strlen(fieldfname);
	if (len > 255) {
		fprintf(stderr, "Warning: in suspend_write_header: field filename is too long (%i > 255); truncating.\n", len);
	}
	if (!err) err |= write_fixed_length_string(fid, fieldfname, 256);

	len = strlen(treefname);
	if (len > 255) {
		fprintf(stderr, "Warning: in suspend_write_header: tree filename is too long (%i > 255); truncating.\n", len);
	}
	if (!err) err |= (write_fixed_length_string(fid, treefname, 256) ||
					  write_u32_native(fid, nfields));
	if (err) {
		fprintf(stderr, "Error writing suspend file header.\n");
	}
}

int suspend_read_header(FILE* fid, double* index_scale, char* fieldfname,
						char* treefname, unsigned int* nfields) {
	int err = 0;
	unsigned int magic;
	unsigned int endian;
	char fn[256];
	err =
		read_u32(fid, &magic) ||
		read_u32_native(fid, &endian);
	if (err) goto readerr;
	if (magic != SUSPEND_MAGIC) {
		fprintf(stderr, "Suspend file does not contain the correct magic number (0x%x)\n",
				SUSPEND_MAGIC);
		return 1;
	}
	if (endian != ENDIAN_DETECTOR) {
		fprintf(stderr, "Suspend file: endianness looks wrong: 0x%x vs 0x%x\n",
				endian, ENDIAN_DETECTOR);
		return 1;
	}
	err |=
		read_double(fid, index_scale) ||
		read_fixed_length_string(fid, fn, 256);
	if (err) goto readerr;
	fn[255] = '\0';
	strcpy(fieldfname, fn);
	err |= read_fixed_length_string(fid, fn, 256);
	if (err) goto readerr;
	fn[255] = '\0';
	strcpy(treefname, fn);
	err |= read_u32_native(fid, nfields);

 readerr:
	if (err) {
		fprintf(stderr, "Couldn't read suspend file header.\n");
	}
	return err;
}

int write_match_object(FILE* fid, MatchObj* mo) {
	int err = 0;
	int i;
	err |=
		write_u32_native(fid, mo->quadno) ||
		write_u32_native(fid, mo->iA) ||
		write_u32_native(fid, mo->iB) ||
		write_u32_native(fid, mo->iC) ||
		write_u32_native(fid, mo->iD) ||
		write_u32_native(fid, mo->fA) ||
		write_u32_native(fid, mo->fB) ||
		write_u32_native(fid, mo->fC) ||
		write_u32_native(fid, mo->fD) ||
		write_double(fid, mo->code_err);

	for (i=0; i<MATCH_VECTOR_SIZE; i++) {
		if (!err) err |= write_double(fid, mo->vector[i]);
	}
	for (i=0; i<DIM_STARS; i++) {
		if (!err) err |= write_double(fid, star_ref(mo->sMin, i));
	}
	for (i=0; i<DIM_STARS; i++) {
		if (!err) err |= write_double(fid, star_ref(mo->sMax, i));
	}
	return err;
}

MatchObj* read_match_object(FILE* fid) {
	MatchObj mo;
	int err = 0;
	unsigned int i;
	MatchObj* rtn;
	double smin[DIM_STARS];
	double smax[DIM_STARS];
	if (!err) err |= read_u32_native(fid, &i);
	mo.quadno = i;
	if (!err) err |= read_u32_native(fid, &i);
	mo.iA = i;
	if (!err) err |= read_u32_native(fid, &i);
	mo.iB = i;
	if (!err) err |= read_u32_native(fid, &i);
	mo.iC = i;
	if (!err) err |= read_u32_native(fid, &i);
	mo.iD = i;
	if (!err) err |= read_u32_native(fid, &i);
	mo.fA = i;
	if (!err) err |= read_u32_native(fid, &i);
	mo.fB = i;
	if (!err) err |= read_u32_native(fid, &i);
	mo.fC = i;
	if (!err) err |= read_u32_native(fid, &i);
	mo.fD = i;
	if (!err) err |= read_double(fid, &mo.code_err);
	for (i=0; i<MATCH_VECTOR_SIZE; i++) {
		if (!err) err |= read_double(fid, mo.vector + i);
	}
	for (i=0; i<DIM_STARS; i++) {
		if (!err) err |= read_double(fid, smin + i);
	}
	for (i=0; i<DIM_STARS; i++) {
		if (!err) err |= read_double(fid, smax + i);
	}
	if (err) {
		fprintf(stderr, "Couldn't read match object: %s\n", strerror(errno));
		return NULL;
	}
	rtn = (MatchObj*)malloc(sizeof(MatchObj));
	memcpy(rtn, &mo, sizeof(MatchObj));
	rtn->sMin = mk_star();
	rtn->sMax = mk_star();
	for (i=0; i<DIM_STARS; i++) {
		star_set(rtn->sMin, i, smin[i]);
		star_set(rtn->sMax, i, smax[i]);
	}
	return rtn;
}

void suspend_write_field(FILE* fid, unsigned int fieldnum,
						 uint objs_used, uint ntried, blocklist* hits) {
	int err = 0;
	int nhits, i;
	if (!hits) nhits = 0;
	else nhits = blocklist_count(hits);
	err |= 
		write_u32_native(fid, fieldnum) ||
		write_u32_native(fid, objs_used) ||
		write_u32_native(fid, ntried) ||
		write_u32_native(fid, nhits);
	for (i=0; i<nhits; i++) {
		MatchObj* mo = (MatchObj*)blocklist_pointer_access(hits, i);
		if (!err) err |= write_match_object(fid, mo);
	}
	if (err) {
		fprintf(stderr, "Error writing suspend file: field %i\n", fieldnum);
	}
}

int suspend_read_field(FILE* fid, uint* fieldnum, uint* objs_used,
					   uint* ntried, blocklist* hits) {
	int err = 0;
	unsigned int nhits, i;

	err |= 
		read_u32_native(fid, fieldnum) ||
		read_u32_native(fid, objs_used) ||
		read_u32_native(fid, ntried) ||
		read_u32_native(fid, &nhits);
	for (i=0; i<nhits; i++) {
		MatchObj* mo = read_match_object(fid);
		if (!mo) {
			err = 1;
			break;
		}
		blocklist_pointer_append(hits, mo);
	}
	if (err) {
		fprintf(stderr, "Error reading a field from suspend file: %s\n",
				strerror(errno));
	}
	return err;
}
