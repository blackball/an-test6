#include "fileutil_am.h"
#include "kdutil.h"

stararray *readcat(FILE *fid, sidx *numstars, dimension *Dim_Stars,
                   double *ramin, double *ramax, double *decmin, double *decmax,
				   int nkeep)
{
	char readStatus;
	sidx ii;
	stararray *thestars = NULL;

	readStatus = read_objs_header(fid, numstars, 
				      Dim_Stars, ramin, ramax, decmin, decmax);
	if (readStatus == READ_FAIL)
		return ((stararray *)NULL);

	if (nkeep && (nkeep < *numstars)) {
	  // switcheroo!
	  *numstars = nkeep;
	}

	thestars = mk_stararray(*numstars);
	for (ii = 0;ii < *numstars;ii++) {
		thestars->array[ii] = mk_stard(*Dim_Stars);
		if (thestars->array[ii] == NULL) {
			fprintf(stderr, "ERROR (readcat) -- out of memory at star %lu\n", ii);
			free_stararray(thestars);
			return (stararray *)NULL;
		}
		fread(thestars->array[ii]->farr, sizeof(double), *Dim_Stars, fid);
	}

	// if nkeep is set, should we read and discard the rest?

	return thestars;
}



/* This function reads an id list, which is used mostly
   for debugging purposes, ie with fieldquads */
quadarray *readidlist(FILE *fid, qidx *numfields)
{
	qidx ii, numS;
	magicval magic;
	quadarray *thepids = NULL;
	fread(&magic, sizeof(magic), 1, fid);
	{
		if (magic != MAGIC_VAL) {
			fprintf(stderr, "ERROR (readidlist) -- bad magic value id list\n");
			return ((quadarray *)NULL);
		}
		fread(numfields, sizeof(*numfields), 1, fid);
	}
	thepids = mk_quadarray(*numfields);
	for (ii = 0;ii < *numfields;ii++) {
		// read in how many stars in this pic
	  fread(&numS, sizeof(numS), 1, fid);
		thepids->array[ii] = mk_quadd(numS);
		if (thepids->array[ii] == NULL) {
			fprintf(stderr, "ERROR (fieldquads) -- out of memory at field %lu\n", ii);
			free_quadarray(thepids);
			return (quadarray *)NULL;
		}
		fread(thepids->array[ii]->iarr, sizeof(int), numS, fid);
	}
	return thepids;
}


sidx readquadidx(FILE *fid, sidx **starlist, qidx **starnumq,
                 qidx ***starquads)
{
	magicval magic;
	sidx numStars, thisstar, jj;
	qidx thisnumq, ii;

	fread(&magic, sizeof(magic), 1, fid);
	{
		if (magic != MAGIC_VAL) {
			fprintf(stderr, "ERROR (fieldquads) -- bad magic value in quad index\n");
			return (0);
		}
		fread(&numStars, sizeof(numStars), 1, fid);
	}
	*starlist = malloc(numStars * sizeof(sidx));
	if (*starlist == NULL)
		return (0);
	*starnumq = malloc(numStars * sizeof(qidx));
	if (*starnumq == NULL) {
		free(*starlist);
		return (0);
	}
	*starquads = malloc(numStars * sizeof(qidx *));
	if (*starquads == NULL) {
		free(*starlist);
		free(*starnumq);
		return (0);
	}

	for (jj = 0;jj < numStars;jj++) {
	  fread(&thisstar, sizeof(thisstar), 1, fid);
	  fread(&thisnumq, sizeof(thisnumq), 1, fid);
		(*starlist)[jj] = thisstar;
		(*starnumq)[jj] = thisnumq;
		(*starquads)[jj] = malloc(thisnumq * sizeof(qidx));
		if ((*starquads)[jj] == NULL)
			return (0);
		for (ii = 0;ii < thisnumq;ii++) {
		  fread(((*starquads)[jj]) + ii, sizeof(qidx), 1, fid);
		}
	}

	return (numStars);
}

signed int compare_sidx(const void *x, const void *y)
{
	sidx xval, yval;
	xval = *(sidx *)x;
	yval = *(sidx *)y;
	if (xval > yval)
		return (1);
	else if (xval < yval)
		return ( -1);
	else
		return (0);
}

