#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "catalog.h"
#include "fileutil.h"

int catalog_write_to_file(catalog* cat, char* fn) {
	FILE *catfid  = NULL;
	catfid = fopen(fn, "wb");
	if (!catfid) {
		fprintf(stderr, "Couldn't open catalog output file %s: %s\n",
				fn, strerror(errno));
		return -1;
	}
	write_objs_header(catfid, cat->nstars, DIM_STARS,
					  cat->ramin, cat->ramax, cat->decmin, cat->decmax);

	if (fwrite(cat->stars, sizeof(double), cat->nstars*DIM_STARS, catfid) !=
		cat->nstars * DIM_STARS) {
		fprintf(stderr, "Failed to write catalog data to file %s: %s.\n",
				fn, strerror(errno));
		fclose(catfid);
		return -1;
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

double* catalog_get_base(catalog* cat) {
	return cat->stars;
}

catalog* catalog_open(char* basefn) {
	char* tempfn;
	catalog* cat;
	/*
	  int len;
	  len = strlen(basefn);
	  tempfn = malloc(len + 1 + 5);
	  sprintf(tempfn, "%s.objs", basefn);
	*/
	tempfn = mk_catfn(basefn);
	cat = catalog_open_file(tempfn);
	//free(tempfn);
	free_fn(tempfn);
	return cat;
}

catalog* catalog_open_file(char* catfn) {
	FILE *catfid  = NULL;
	char readStatus;
	dimension Dim_Stars;
	off_t endoffset;
	off_t headeroffset;
	uint maxstar;
	catalog* cat;
	sidx nstars_tmp;

	cat = malloc(sizeof(catalog));
	if (!cat) {
		fprintf(stderr, "catalog_open: malloc failed.\n");
		return cat;
	}

	cat->catfname = strdup(catfn);

	// Read .objs file...
	readStatus = read_objs_header(catfid, &nstars_tmp, &Dim_Stars,
								  &cat->ramin, &cat->ramax,
								  &cat->decmin, &cat->decmax);
	cat->nstars = nstars_tmp;
	if (readStatus == READ_FAIL) {
		fprintf(stderr, "Failed to read catalog header.\n");
		goto bail;
	}
	headeroffset = ftello(catfid);
	// check that the catalogue file is the right size.
	fseeko(catfid, 0, SEEK_END);
	endoffset = ftello(catfid);
	maxstar = (endoffset - headeroffset) / (3 * sizeof(double));
	if (maxstar != cat->nstars) {
		fprintf(stderr, "Error: numstars=%i (specified in .objs file header) does "
				"not match maxstars=%i (computed from size of .objs file).\n",
				cat->nstars, maxstar);
		goto bail;
	}

	cat->mmap_cat_size = endoffset;
	cat->mmap_cat = mmap(0, cat->mmap_cat_size, PROT_READ, MAP_SHARED, fileno(catfid), 0);
	if (cat->mmap_cat == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap catalogue file: %s\n", strerror(errno));
		goto bail;
	}
	fclose(catfid);
	cat->stars = (double*)(((char*)cat->mmap_cat) + headeroffset);

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

void catalog_close(catalog* cat) {
	munmap(cat->mmap_cat, cat->mmap_cat_size);
	free(cat->catfname);
	free(cat);
}

