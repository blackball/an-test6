/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "qfits.h"
#include "fitsioutils.h"
#include "starutil.h"
#include "ioutils.h"
#include "qidxfile.h"

static qidxfile* new_qidxfile() {
	qidxfile* qf = calloc(1, sizeof(qidxfile));
	if (!qf) {
		fprintf(stderr, "Couldn't malloc a qidxfile struct: %s\n", strerror(errno));
		return NULL;
	}
	return qf;
}

static int callback_read_header(qfits_header* primheader, qfits_header* header,
								size_t* expected, char** errstr,
								void* userdata) {
	qidxfile* qf = userdata;

	if (fits_check_endian(primheader)) {
		if (errstr) *errstr = "File was written with wrong endianness.";
		return -1;
	}
	qf->numstars = qfits_header_getint(primheader, "NSTARS", -1);
	qf->numquads = qfits_header_getint(primheader, "NQUADS", -1);
    qf->dimquads = qfits_header_getint(primheader, "DIMQUADS", 4);
	if ((qf->numstars == -1) || (qf->numquads == -1)) {
		if (errstr) *errstr = "Couldn't find NSTARS or NQUADS entries in FITS header.";
		return -1;
	}

    *expected = qf->numstars * 2 * sizeof(uint32_t) +
		qf->numquads * qf->dimquads * sizeof(uint32_t);
	return 0;
}

qidxfile* qidxfile_open(const char* fn) {
	qidxfile* qf = NULL;
	fitsbin_t* fb = NULL;
	char* errstr = NULL;

	qf = new_qidxfile();
	if (!qf)
		goto bailout;

	fb = fitsbin_open(fn, "qidx", &errstr, callback_read_header, qf);
	if (!fb) {
		fprintf(stderr, "%s\n", errstr);
		goto bailout;
	}
	qf->fb = fb;
	qf->index = (uint32_t*)fb->data;
	qf->heap  = qf->index + 2 * qf->numstars;
	return qf;

 bailout:
	if (qf)
		free(qf);
	return NULL;
}

int qidxfile_close(qidxfile* qf) {
    int rtn;
	if (!qf) return 0;
	rtn = fitsbin_close(qf->fb);
	free(qf);
    return rtn;
}

qidxfile* qidxfile_open_for_writing(const char* fn, uint nstars, uint nquads) {
	qidxfile* qf;
	fitsbin_t* fb = NULL;
	char* errstr = NULL;
	qfits_header* hdr;

	qf = new_qidxfile();
	if (!qf)
		goto bailout;
	qf->numstars = nstars;
	qf->numquads = nquads;

	fb = fitsbin_open_for_writing(fn, "quads", &errstr);
	if (!fb) {
		fprintf(stderr, "%s\n", errstr);
		goto bailout;
	}
	qf->fb = fb;

    // default
    qf->dimquads = 4;

	hdr = fb->primheader;
    fits_add_endian(fb->primheader);
	fits_header_add_int(hdr, "NSTARS", qf->numstars, "Number of stars used.");
	fits_header_add_int(hdr, "NQUADS", qf->numquads, "Number of quads used.");
	qfits_header_add(hdr, "AN_FILE", "QIDX", "This is a quad index file.", NULL);
	qfits_header_add(hdr, "COMMENT", "The data table of this file has two parts:", NULL, NULL);
	qfits_header_add(hdr, "COMMENT", " -the index", NULL, NULL);
	qfits_header_add(hdr, "COMMENT", " -the heap", NULL, NULL);
	fits_add_long_comment(hdr, "The index contains two uint32 values for each star: the offset and "
						  "length, in the heap, of the list of quads to which it belongs.  "
						  "The offset and length are in units of uint32s, not bytes.  "
						  "Offset 0 is the first uint32 in the heap.  "
						  "The heap is ordered and tightly packed.  "
						  "The heap is a flat list of quad indices (uint32s).");
	return qf;

bailout:
	if (qf)
		free(qf);
	return NULL;
}

int qidxfile_write_header(qidxfile* qf) {
	fitsbin_t* fb = qf->fb;
	fb->itemsize = sizeof(uint32_t);
	fb->nrows = 2 * qf->numstars + qf->dimquads * qf->numquads;
	if (fitsbin_write_primary_header(fb) ||
		fitsbin_write_header(fb)) {
		fprintf(stderr, "Failed to write qidxfile header.\n");
		return -1;
	}
	qf->cursor_index = 0;
	qf->cursor_heap  = 0;
	return 0;
}

int qidxfile_write_star(qidxfile* qf, uint* quads, uint nquads) {
	fitsbin_t* fb = qf->fb;
	FILE* fid = fb->fid;
	uint32_t nq;
	int i;

	// Write the offset & size:
	if (fseeko(fid, fb->header_end + qf->cursor_index * 2 * sizeof(uint32_t), SEEK_SET)) {
		fprintf(stderr, "qidxfile_write_star: failed to fseek: %s\n", strerror(errno));
		return -1;
	}
	nq = nquads;
	if ((fwrite(&qf->cursor_heap, sizeof(uint32_t), 1, fid) != 1) ||
		(fwrite(&nq, sizeof(uint32_t), 1, fid) != 1)) {
		fprintf(stderr, "qidxfile_write_star: failed to write a qidx offset/size.\n");
		return -1;
	}
	// Write the quads.
	if (fseeko(fid, fb->header_end + qf->numstars * 2 * sizeof(uint32_t) +
			   qf->cursor_heap * sizeof(uint32_t), SEEK_SET)) {
		fprintf(stderr, "qidxfile_write_star: failed to fseek: %s\n", strerror(errno));
		return -1;
	}
	for (i=0; i<nquads; i++) {
		uint32_t q = quads[i];
		if (fwrite(&q, sizeof(uint32_t), 1, fid) != 1) {
			fprintf(stderr, "qidxfile_write_star: failed to write quads.\n");
			return -1;
		}
	}
	qf->cursor_index++;
	qf->cursor_heap += nquads;
	return 0;
}

int qidxfile_get_quads(const qidxfile* qf, uint starid, uint32_t** quads, uint* nquads) {
	uint heapindex = qf->index[2*starid];
	*nquads = qf->index[2*starid + 1];
	*quads = qf->heap + heapindex;
	return 0;
}

qfits_header* qidxfile_get_header(const qidxfile* qf) {
	return qf->fb->primheader;
}
