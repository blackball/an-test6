#include "matchfile.h"
#include "ioutils.h"

int matchfile_write_match(FILE* fid, MatchObj* mo, matchfile_entry* me) {
	int err = 0;
	int i;
	err |=
		write_u32(fid, ENDIAN_DETECTOR) ||
		write_u32(fid, me->fieldnum) ||
		write_double(fid, me->codetol) ||
		write_u8(fid, me->parity) ||
		write_string(fid, me->indexpath) ||
		write_string(fid, me->fieldpath) ||
		write_u32(fid, mo->quadno) ||
		write_u32(fid, mo->star[0]) ||
		write_u32(fid, mo->star[1]) ||
		write_u32(fid, mo->star[2]) ||
		write_u32(fid, mo->star[3]) ||
		write_u32(fid, mo->field[0]) ||
		write_u32(fid, mo->field[1]) ||
		write_u32(fid, mo->field[2]) ||
		write_u32(fid, mo->field[3]) ||
		write_double(fid, mo->code_err);
	/* ticket #111
	  write_double(fid, mo->fieldunits_lower) ||
	  write_double(fid, mo->fieldunits_upper);
	*/

	for (i=0; i<MATCH_VECTOR_SIZE; i++) {
		if (!err) err |= write_double(fid, mo->vector[i]);
	}
	for (i=0; i<DIM_STARS; i++) {
		if (!err) err |= write_double(fid, mo->sMin[i]);
	}
	for (i=0; i<DIM_STARS; i++) {
		if (!err) err |= write_double(fid, mo->sMax[i]);
	}
	if (err) {
		fprintf(stderr, "Error writing match file entry: %s\n", strerror(errno));
	}
	return err;
}

int matchfile_read_match(FILE* fid, MatchObj** pmo, matchfile_entry* me) {
	int err = 0, i;
	uint endian;
	uint fieldnum;
	double codetol;
	unsigned char parity;
	char* indexpath;
	char* fieldpath;
	uint quad, iA, iB, iC, iD, fA, fB, fC, fD;
	double codeerr;
	MatchObj* mo;
	/* ticket #111
	   double funits_lower;
	   double funits_upper;
	*/
	err |=
		read_u32(fid, &endian) ||
		read_u32(fid, &fieldnum) ||
		read_double(fid, &codetol) ||
		read_u8(fid, &parity);
	if (err) {
		fprintf(stderr, "Error reading matchfile entry: %s\n", strerror(errno));
		return 1;
	}
	if (endian != ENDIAN_DETECTOR) {
		fprintf(stderr, "Matchfile has incorrect endianness: 0x%x vs 0x%x\n",
				endian, ENDIAN_DETECTOR);
		return 1;
	}
	indexpath = read_string(fid);
	fieldpath = read_string(fid);
	if (!indexpath || !fieldpath) {
		fprintf(stderr, "Error reading matchfile entry: %s\n", strerror(errno));
		free(indexpath);
		free(fieldpath);
		return 1;
	}

	err |=
		read_u32(fid, &quad) ||
		read_u32(fid, &iA) ||
		read_u32(fid, &iB) ||
		read_u32(fid, &iC) ||
		read_u32(fid, &iD) ||
		read_u32(fid, &fA) ||
		read_u32(fid, &fB) ||
		read_u32(fid, &fC) ||
		read_u32(fid, &fD) ||
		read_double(fid, &codeerr);
	/*
	  ticket #111
	  read_double(fid, &funits_lower) ||
	  read_double(fid, &funits_upper);
	*/

	mo = mk_MatchObj();

	for (i=0; i<MATCH_VECTOR_SIZE; i++) {
		if (!err) err |= read_double(fid, mo->vector + i);
	}
	for (i=0; i<DIM_STARS; i++) {
		double d;
		if (!err) err |= read_double(fid, &d);
		mo->sMin[i] = d;
	}
	for (i=0; i<DIM_STARS; i++) {
		double d;
		if (!err) err |= read_double(fid, &d);
		mo->sMax[i] = d;
	}

	if (err) {
		fprintf(stderr, "Error reading matchfile entry: %s\n", strerror(errno));
		free(indexpath);
		free(fieldpath);
		free_MatchObj(mo);
		return 1;
	}

	mo->quadno = quad;
	mo->star[0] = iA;
	mo->star[1] = iB;
	mo->star[2] = iC;
	mo->star[3] = iD;
	mo->field[0] = fA;
	mo->field[1] = fB;
	mo->field[2] = fC;
	mo->field[3] = fD;
	mo->code_err = codeerr;
	mo->transform = NULL;

	me->fieldnum = fieldnum;
	me->parity = parity;
	me->indexpath = indexpath;
	me->fieldpath = fieldpath;
	me->codetol = codetol;

	*pmo = mo;
	return 0;
}
