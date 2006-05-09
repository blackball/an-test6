#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "catalog.h"
#include "fitsioutils.h"
#include "fileutil.h"
#include "ioutils.h"
#include "mathutil.h"

int write_objs_header(FILE *fid, uint numstars,
					  uint DimStars, double ramin, double ramax, 
					  double decmin, double decmax)
{
	magicval magic = MAGIC_VAL;
	if (write_u16(fid, magic) ||
		write_u32(fid, numstars) ||
		write_u16(fid, DimStars) ||
		write_double(fid, ramin) ||
		write_double(fid, ramax) ||
		write_double(fid, decmin) ||
		write_double(fid, decmax)) {
		fprintf(stderr, "Couldn't write objs header.\n");
		return 1;
	}
	return 0;
}

int read_objs_header(FILE *fid, uint *numstars, uint *DimStars,
					 double *ramin, double *ramax, 
					 double *decmin, double *decmax)
{
	uint magic;

	if (read_u16(fid, &magic)) {
		fprintf(stderr, "Couldn't read objs header.\n");
		return 1;
	}
	if (magic != MAGIC_VAL) {
		fprintf(stderr, "Error reading objs header: bad magic value.\n");
		return 1;
	}
	if (read_u32(fid, numstars) ||
		read_u16(fid, DimStars) ||
		read_double(fid, ramin) ||
		read_double(fid, ramax) ||
		read_double(fid, decmin) ||
		read_double(fid, decmax)) {
		fprintf(stderr, "Couldn't read objs header.\n");
		return 1;
	}
	return 0;
}


int catalog_write_to_file(catalog* cat, char* fn, int fits) {
	FILE *catfid  = NULL;
	catfid = fopen(fn, "wb");
	if (!catfid) {
		fprintf(stderr, "Couldn't open catalog output file %s: %s\n",
				fn, strerror(errno));
		return -1;
	}

	if (fits) {
		char val[256];
		qfits_table* table;
		qfits_header* tablehdr;
		qfits_header* hdr;
		uint datasize;
		uint ncols, nrows, tablesize;
		// the header
	    hdr = qfits_table_prim_header_default();
		fits_add_endian(hdr);
		fits_add_double_size(hdr);
		sprintf(val, "%u", cat->nstars);
		qfits_header_add(hdr, "NSTARS", val, "Number of stars used.", NULL);
		qfits_header_dump(hdr, catfid);
		qfits_header_destroy(hdr);

		// first table: the star locations.
		datasize = DIM_STARS * sizeof(double);
		ncols = 1;
		// may be dummy
		nrows = cat->nstars;
		tablesize = datasize * nrows * ncols;
		table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
		qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
					   "xyz", "", "", "", 0, 0, 0, 0, 0);
		tablehdr = qfits_table_ext_header_default(table);
		qfits_header_dump(tablehdr, catfid);
		qfits_table_close(table);
		qfits_header_destroy(tablehdr);
		
	} else {
		write_objs_header(catfid, cat->nstars, DIM_STARS,
						  cat->ramin, cat->ramax, cat->decmin, cat->decmax);
	}

	if (fwrite(cat->stars, sizeof(double), cat->nstars*DIM_STARS, catfid) !=
		cat->nstars * DIM_STARS) {
		fprintf(stderr, "Failed to write catalog data to file %s: %s.\n",
				fn, strerror(errno));
		fclose(catfid);
		return -1;
	}

	if (fits && cat->ids) {
		qfits_table* table;
		qfits_header* tablehdr;
		int datasize, ncols, nrows, tablesize;

		// second table: the star ids.
		datasize = sizeof(int64_t);
		ncols = 1;
		// may be dummy
		nrows = cat->nstars;
		tablesize = datasize * nrows * ncols;
		table = qfits_table_new(fn, QFITS_BINTABLE, tablesize, ncols, nrows);
		qfits_col_fill(table->col, datasize, 0, 1, TFITS_BIN_TYPE_A,
					   "ids", "", "", "", 0, 0, 0, 0, 0);
		tablehdr = qfits_table_ext_header_default(table);
		qfits_header_dump(tablehdr, catfid);
		qfits_table_close(table);
		qfits_header_destroy(tablehdr);

		if (fwrite(cat->ids, sizeof(int64_t), cat->nstars, catfid) !=
			cat->nstars) {
			fprintf(stderr, "Failed to write catalog data to file %s: %s.\n",
					fn, strerror(errno));
			fclose(catfid);
			return -1;
		}
	}

	if (fits) {
		fits_pad_file(catfid);
	}

	if (fclose(catfid)) {
		fprintf(stderr, "Couldn't close catalog file %s: %s\n",
				cat->catfname, strerror(errno));
		return -1;
	}
	return 0;
}

int catalog_rewrite_header(catalog* cat) {
	FILE *catfid  = NULL;
	catfid = fopen(cat->catfname, "ab");
	if (!catfid) {
		fprintf(stderr, "Couldn't open catalog file %s: %s\n",
				cat->catfname, strerror(errno));
		return -1;
	}
	fseeko(catfid, 0, SEEK_SET);
	write_objs_header(catfid, cat->nstars, DIM_STARS,
					  cat->ramin, cat->ramax, cat->decmin, cat->decmax);
	fseeko(catfid, 0, SEEK_END);
	if (fclose(catfid)) {
		fprintf(stderr, "Couldn't close catalog file %s after rewriting "
				"the header: %s\n", cat->catfname, strerror(errno));
		return -1;
	}
	return 0;
}

void catalog_compute_radecminmax(catalog* cat) {
	double ramin, ramax, decmin, decmax;
	int i;
	ramin = 1e100;
	ramax = -1e100;
	decmin = 1e100;
	decmax = -1e100;
	for (i=0; i<cat->nstars; i++) {
		double* xyz;
		double ra, dec;
		xyz = catalog_get_star(cat, i);
		ra = xy2ra(xyz[0], xyz[1]);
		dec = z2dec(xyz[2]);
		if (ra > ramax) ramax = ra;
		if (ra < ramin) ramin = ra;
		if (dec > decmax) decmax = dec;
		if (dec < decmin) decmin = dec;
	}
	cat->ramin = ramin;
	cat->ramax = ramax;
	cat->decmin = decmin;
	cat->decmax = decmax;
}

double* catalog_get_base(catalog* cat) {
	return cat->stars;
}

/*
  catalog* catalog_open(char* basefn, int modifiable) {
  char* tempfn;
  catalog* cat;
  tempfn = mk_catfn(basefn);
  cat = catalog_open_file(tempfn, modifiable);
  free_fn(tempfn);
  return cat;
  }
*/

catalog* catalog_open(char* catfn, int fits, int modifiable) {
	FILE *catfid  = NULL;
	uint Dim_Stars;
	off_t endoffset;
	off_t headeroffset;
	uint maxstar;
	catalog* cat;
	uint nstars_tmp;
	int mode, flags;
	int offxyz = -1, offids = -1;

	cat = malloc(sizeof(catalog));
	if (!cat) {
		fprintf(stderr, "catalog_open: malloc failed.\n");
		return cat;
	}

	cat->catfname = strdup(catfn);

	if (fits) {
		int sizexyz, sizeids;
		qfits_header* header;

		if (!is_fits_file(cat->catfname)) {
			fprintf(stderr, "File %s doesn't look like a FITS file.\n", cat->catfname);
			goto bail;
		}
		header = qfits_header_read(cat->catfname);
		if (!header) {
			fprintf(stderr, "Couldn't read FITS header from %s.\n", cat->catfname);
			goto bail;
		}
		cat->nstars = qfits_header_getint(header, "NSTARS", -1);
		if (fits_check_endian(header) ||
			fits_check_double_size(header)) {
			fprintf(stderr, "File %s was written with wrong endianness or double size.\n", cat->catfname);
			qfits_header_destroy(header);
			goto bail;
		}
		qfits_header_destroy(header);
		if (cat->nstars == -1) {
			fprintf(stderr, "Couldn't find NSTARS header in file %s\n", cat->catfname);
			goto bail;
		}

		if (fits_find_table_column(cat->catfname, "xyz", &offxyz, &sizexyz) ||
			fits_find_table_column(cat->catfname, "ids", &offids, &sizeids)) {
			fprintf(stderr, "Couldn't find \"xyz\" or \"ids\" columns in "
					"FITS file.");
			goto bail;
		}

		if (fits_blocks_needed(cat->nstars * sizeof(double) * DIM_STARS) != sizexyz) {
			fprintf(stderr, "Number of stars promised does jive with the xyz table size: %u vs %u.\n",
					fits_blocks_needed(cat->nstars * sizeof(double) * DIM_STARS), sizexyz);
			goto bail;
		}
		if (fits_blocks_needed(cat->nstars * sizeof(int64_t)) != sizeids) {
			fprintf(stderr, "Number of stars promised does jive with the \"ids\" table size: %u vs %u.\n",
					fits_blocks_needed(cat->nstars * sizeof(int64_t)), sizeids);
			goto bail;
		}

		cat->mmap_cat_size = imax(offxyz + sizexyz, offids + sizeids);

	} else {

		catfid = fopen(cat->catfname, "rb");
		if (!catfid) {
			fprintf(stderr, "catalog_open: couldn't open file %s: %s.\n", cat->catfname,
					strerror(errno));
			free(cat->catfname);
			free(cat);
			return NULL;
		}

		// Read .objs file...
		if (read_objs_header(catfid, &nstars_tmp, &Dim_Stars,
							 &cat->ramin, &cat->ramax,
							 &cat->decmin, &cat->decmax)) {
			fprintf(stderr, "Failed to read catalog header.\n");
			goto bail;
		}
		cat->nstars = nstars_tmp;
		headeroffset = ftello(catfid);
		offxyz = headeroffset;

		// check that the catalogue file is the right size.
		fseeko(catfid, 0, SEEK_END);
		endoffset = ftello(catfid);
		cat->mmap_cat_size = endoffset;

		maxstar = (endoffset - headeroffset) / (3 * sizeof(double));
		if (maxstar != cat->nstars) {
			fprintf(stderr, "Error: nstars=%i (specified in .objs file header) does "
					"not match maxstars=%i (computed from size of .objs file).\n",
					cat->nstars, maxstar);
			goto bail;
		}

	}
	if (modifiable) {
		mode = PROT_READ | PROT_WRITE;
		flags = MAP_PRIVATE;
	} else {
		mode = PROT_READ;
		flags = MAP_SHARED;
	}
	cat->mmap_cat = mmap(0, cat->mmap_cat_size, mode, flags, fileno(catfid), 0);
	if (cat->mmap_cat == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap catalogue file: %s\n", strerror(errno));
		goto bail;
	}
	fclose(catfid);

	cat->stars = (double*)(((char*)cat->mmap_cat) + offxyz);
	if (offids == -1)
		cat->ids = NULL;
	else
		cat->ids = (int64_t*)(((char*)cat->mmap_cat) + offids);

	return cat;

 bail:
		free_fn(cat->catfname);
		free(cat);
		return NULL;
}

double* catalog_get_star(catalog* cat, uint sid) {
	if (sid >= cat->nstars) {
		fprintf(stderr, "catalog: asked for star %u, but catalog size is only %u.\n",
				sid, cat->nstars);
		return NULL;
	}
	return cat->stars + sid * 3;
}

int64_t catalog_get_star_id(catalog* cat, uint sid) {
	if (cat->ids)
		return cat->ids[sid];
	return -1;
}

void catalog_close(catalog* cat) {
	munmap(cat->mmap_cat, cat->mmap_cat_size);
	free(cat->catfname);
	free(cat);
}

