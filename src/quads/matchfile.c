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

	err |=
		read_u32_native(fid, &endian) ||
		read_u32_native(fid, &fieldnum) ||
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
		read_u32_native(fid, &quad) ||
		read_u32_native(fid, &iA) ||
		read_u32_native(fid, &iB) ||
		read_u32_native(fid, &iC) ||
		read_u32_native(fid, &iD) ||
		read_u32_native(fid, &fA) ||
		read_u32_native(fid, &fB) ||
		read_u32_native(fid, &fC) ||
		read_u32_native(fid, &fD) ||
		read_double(fid, &codeerr);

	mo = mk_MatchObj();
	mo->sMin = mk_star();
	mo->sMax = mk_star();

	for (i=0; i<MATCH_VECTOR_SIZE; i++) {
		if (!err) err |= read_double(fid, mo->vector + i);
	}
	for (i=0; i<DIM_STARS; i++) {
		double d;
		if (!err) err |= read_double(fid, &d);
		star_set(mo->sMin, i, d);
	}
	for (i=0; i<DIM_STARS; i++) {
		double d;
		if (!err) err |= read_double(fid, &d);
		star_set(mo->sMax, i, d);
	}

	if (err) {
		fprintf(stderr, "Error reading matchfile entry: %s\n", strerror(errno));
		free(indexpath);
		free(fieldpath);
		free_star(mo->sMin);
		free_star(mo->sMax);
		free_MatchObj(mo);
		return 1;
	}

	mo->quadno = quad;
	mo->iA = iA;
	mo->iB = iB;
	mo->iC = iC;
	mo->iD = iD;
	mo->fA = fA;
	mo->fB = fB;
	mo->fC = fC;
	mo->fD = fD;
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
