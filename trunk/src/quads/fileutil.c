#include <stdlib.h>
#include <string.h>
#include "fileutil.h"

uint read_quadidx(FILE *fid, uint **starlist, uint **starnumq,
                 uint ***starquads)
{
	magicval magic;
	uint numStars, thisstar, jj;
	uint thisnumq, ii;

	fread(&magic, sizeof(magic), 1, fid);
	{
		if (magic != MAGIC_VAL) {
			fprintf(stderr, "ERROR (fieldquads) -- bad magic value in quad index\n");
			return (0);
		}
		fread(&numStars, sizeof(numStars), 1, fid);
	}
	*starlist = malloc(numStars * sizeof(uint));
	if (*starlist == NULL)
		return (0);
	*starnumq = malloc(numStars * sizeof(uint));
	if (*starnumq == NULL) {
		free(*starlist);
		return (0);
	}
	*starquads = malloc(numStars * sizeof(uint *));
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
		(*starquads)[jj] = malloc(thisnumq * sizeof(uint));
		if ((*starquads)[jj] == NULL)
			return (0);
		for (ii = 0;ii < thisnumq;ii++) {
		  fread(((*starquads)[jj]) + ii, sizeof(uint), 1, fid);
		}
	}

	return (numStars);
}

char *mk_filename(const char *basename, const char *extension)
{
	char *fname;
	fname = malloc(strlen(basename) + strlen(extension) + 1);
	sprintf(fname, "%s%s", basename, extension);
	return fname;
}


