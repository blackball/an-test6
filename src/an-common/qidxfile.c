/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, Dustin Lang, Keir Mierle and Sam Roweis.

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
#include "fileutil.h"
#include "ioutils.h"
#include "qidxfile.h"

static qidxfile* new_qidxfile()
{
	qidxfile* qf = malloc(sizeof(qidxfile));
	if (!qf) {
		fprintf(stderr, "Couldn't malloc a qidxfile struct: %s\n", strerror(errno));
		return NULL;
	}
	memset(qf, 0, sizeof(qidxfile));
	return qf;
}

qidxfile* qidxfile_open(char* fn, int modifiable)
{
	FILE* fid = NULL;
	qfits_header* header = NULL;
	qidxfile* qf = NULL;
	int off, datasize;
	int size;
	void* map;
	int mode, flags;

	if (!qfits_is_fits(fn)) {
		fprintf(stderr, "File %s doesn't look like a FITS file.\n", fn);
		goto bailout;
	}
	fid = fopen(fn, "rb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read qidx file: %s\n", fn, strerror(errno));
		goto bailout;
	}
	header = qfits_header_read(fn);
	if (!header) {
		fprintf(stderr, "Couldn't read FITS header from %s.\n", fn);
		goto bailout;
	}

	if (fits_check_endian(header) ||
		fits_check_uint_size(header)) {
		fprintf(stderr, "File %s was written with wrong endianness or uint size.\n", fn);
		goto bailout;
	}

	qf = new_qidxfile();
	if (!qf)
		goto bailout;

	qf->numstars = qfits_header_getint(header, "NSTARS", -1);
	qf->numquads = qfits_header_getint(header, "NQUADS", -1);
	qfits_header_destroy(header);

	if ((qf->numstars == -1) || (qf->numquads == -1)) {
		fprintf(stderr, "Couldn't find NSTARS or NQUADS entries in FITS header.");
		goto bailout;
	}

	if (fits_find_table_column(fn, "qidx", &off, &datasize)) {
		fprintf(stderr, "Couldn't find \"qidx\" column in FITS file.");
		goto bailout;
	}

	if (fits_blocks_needed(qf->numstars * 2 * sizeof(uint) +
						   qf->numquads * 4 * sizeof(uint)) != datasize) {
		fprintf(stderr, "Number of qidx entries promised does jive with the table size: %u vs %u.\n",
		        fits_blocks_needed(qf->numstars * 2 * sizeof(uint) +
								   qf->numquads * 4 * sizeof(uint)),
				datasize);
		goto bailout;
	}

	if (modifiable) {
		mode = PROT_READ | PROT_WRITE;
		flags = MAP_PRIVATE;
	} else {
		mode = PROT_READ;
		flags = MAP_SHARED;
	}
	size = off + datasize;
	map = mmap(0, size, mode, flags, fileno(fid), 0);
	fclose(fid);
	fid = NULL;
	if (map == MAP_FAILED) {
		fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
		goto bailout;
	}

	qf->index = (uint*)(map + off);
	qf->heap  = qf->index + 2 * qf->numstars;
	qf->mmap_base = map;
	qf->mmap_size = size;
	return qf;

bailout:
	if (qf)
		free(qf);
	if (fid)
		fclose(fid);
	return NULL;
}

int qidxfile_close(qidxfile* qf)
{
	int rtn = 0;
	if (qf->mmap_base)
		if (munmap(qf->mmap_base, qf->mmap_size)) {
			fprintf(stderr, "Error munmapping qidxfile: %s\n", strerror(errno));
			rtn = -1;
		}
	if (qf->fid) {
		fits_pad_file(qf->fid);
		if (fclose(qf->fid)) {
			fprintf(stderr, "Error closing qidxfile: %s\n", strerror(errno));
			rtn = -1;
		}
	}
	if (qf->header)
		qfits_header_destroy(qf->header);

	free(qf);
	return rtn;
}

qidxfile* qidxfile_open_for_writing(char* fn, uint nstars, uint nquads)
{
	qidxfile* qf;
	char val[256];

	qf = new_qidxfile();
	if (!qf)
		goto bailout;
	qf->numstars = nstars;
	qf->numquads = nquads;
	qf->fid = fopen(fn, "wb");
	if (!qf->fid) {
		fprintf(stderr, "Couldn't open file %s for FITS output: %s\n", fn, strerror(errno));
		goto bailout;
	}

	// the header
	qf->header = qfits_table_prim_header_default();
	fits_add_endian(qf->header);
	fits_add_uint_size(qf->header);

	sprintf(val, "%u", qf->numstars);
	qfits_header_add(qf->header, "NSTARS", val, "Number of stars used.", NULL);
	sprintf(val, "%u", qf->numquads);
	qfits_header_add(qf->header, "NQUADS", val, "Number of quads.", NULL);
	qfits_header_add(qf->header, "AN_FILE", "QIDX", "This is a quad index file.", NULL);
	qfits_header_add(qf->header, "COMMENT", "The data table of this file has two parts:", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", " -the index", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", " -the heap", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", "The index contains two uints for each star: the offset and", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", "  length, in the heap, of the list of quads to which it belongs.", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", "  (Offset and length are in units of uints, not bytes)", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", "  (Offset 0 is the first uint in the heap)", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", "  (The heap is ordered and tightly packed)", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", "The heap is a flat list of quad indices (uints).", NULL, NULL);
	qfits_header_add(qf->header, "COMMENT", "All uints are in native endian and size.", NULL, NULL);

	return qf;

bailout:
	if (qf) {
		if (qf->fid)
			fclose(qf->fid);
		free(qf);
	}
	return NULL;
}

int qidxfile_write_header(qidxfile* qf)
{
	// first table: the quads.
	int datasize = 1;
	int ncols = 1;
	int nrows = 2 * sizeof(uint) * qf->numstars + 4 * sizeof(uint) * qf->numquads;
	int tablesize = datasize * nrows * ncols;
	qfits_table* table = qfits_table_new("", QFITS_BINTABLE, tablesize, ncols, nrows);
	qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
	               "qidx", "", "", "", 0, 0, 0, 0, 0);
	qfits_header_dump(qf->header, qf->fid);
	qfits_header* tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, qf->fid);
	qfits_table_close(table);
	qfits_header_destroy(tablehdr);
	qf->header_end = ftello(qf->fid);
	qf->cursor_index = 0;
	qf->cursor_heap  = 0;
	return 0;
}

int qidxfile_write_star(qidxfile* qf, uint* quads, uint nquads)
{
	if (!qf->fid) {
		fprintf(stderr, "qidxfile_write_star: fid is null.\n");
		return -1;
	}
	if (fseeko(qf->fid, qf->header_end + qf->cursor_index * 2 * sizeof(uint),
			   SEEK_SET)) {
		fprintf(stderr, "qidxfile_write_star: failed to fseek: %s\n", strerror(errno));
		return -1;
	}
	if ((fwrite(&qf->cursor_heap, sizeof(uint), 1, qf->fid) != 1) ||
		(fwrite(&nquads,      sizeof(uint), 1, qf->fid) != 1)) {
		fprintf(stderr, "qidxfile_write_star: failed to write a qidx offset/size.\n");
		return -1;
	}
	if (fseeko(qf->fid, qf->header_end + qf->numstars * 2 * sizeof(uint) +
			   qf->cursor_heap * sizeof(uint), SEEK_SET)) {
		fprintf(stderr, "qidxfile_write_star: failed to fseek: %s\n", strerror(errno));
		return -1;
	}
	if (fwrite(quads, sizeof(uint), nquads, qf->fid) != nquads) {
		fprintf(stderr, "qidxfile_write_star: failed to write quads.\n");
		return -1;
	}

	qf->cursor_index++;
	qf->cursor_heap += nquads;
	return 0;
}

int qidxfile_get_quads(qidxfile* qf, uint starid, uint** quads, uint* nquads) {
	uint heapindex = qf->index[2*starid];
	*nquads = qf->index[2*starid + 1];
	*quads = qf->heap + heapindex;
	return 0;
}

