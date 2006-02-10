#include "matchfile.h"
#include "ioutils.h"

int matchfile_write_match(FILE* fid, MatchObj* mo, matchfile_entry* me) {
	int err = 0;
	int i;
	err |=
		write_u32_native(fid, ENDIAN_DETECTOR) ||
		write_u32_native(fid, me->fieldnum) ||
		write_double(fid, me->codetol) ||
		write_u8(fid, me->parity) ||
		write_string(fid, me->indexpath) ||
		write_string(fid, me->fieldpath) ||
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
	if (err) {
		fprintf(stderr, "Error writing match file entry: %s\n", strerror(errno));
	}
	return err;
}

