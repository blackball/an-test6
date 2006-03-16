#include <stdlib.h>
#include <string.h>
#include "fileutil.h"

/* The binary files all start with the unsigned short int MAGIC_VAL
   which helps identify them as binary and also sort out endian issues.
*/

/* The binary catalogue format (.objs) is as follows:
   unsigned short int = MAGIC_VAL
   unsigned long int = number_of_stars_in_catalogue
   unsigned short int = dimension_of_space (almost always==3)
   double ramin = minimum RA value of all stars
   double ramax = maximum RA value of all stars
   double decmin = minimum DEC value of all stars
   double decmax = maximum DEC value of all stars
   double x0, double y0, double z0
   double x1, double y1, double z1
   ...
*/

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
	fname = (char *)malloc(strlen(basename) + strlen(extension) + 1);
	sprintf(fname, "%s%s", basename, extension);
	return fname;
}


