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


void write_objs_header(FILE *fid, uint numstars,
                       uint DimStars, double ramin, double ramax, 
							  double decmin, double decmax)
{
	magicval magic = MAGIC_VAL;
  fwrite(&magic, sizeof(magic), 1, fid);
  fwrite(&numstars, sizeof(numstars), 1, fid);
  fwrite(&DimStars, sizeof(DimStars), 1, fid);
  fwrite(&ramin, sizeof(ramin), 1, fid);
  fwrite(&ramax, sizeof(ramin), 1, fid);
  fwrite(&decmin, sizeof(ramin), 1, fid);
  fwrite(&decmax, sizeof(ramin), 1, fid);
}

void write_code_header(FILE *codefid, uint numCodes,
                       uint numstars, uint DimCodes, double index_scale)
{
  magicval magic = MAGIC_VAL;
  fwrite(&magic, sizeof(magic), 1, codefid);
  fwrite(&numCodes, sizeof(numCodes), 1, codefid);
  fwrite(&DimCodes, sizeof(DimCodes), 1, codefid);
  fwrite(&index_scale, sizeof(index_scale), 1, codefid);
  fwrite(&numstars, sizeof(numstars), 1, codefid);
}

void write_quad_header(FILE *quadfid, uint numQuads, uint numstars,
                       uint DimQuads, double index_scale)
{
  magicval magic = MAGIC_VAL;

  fwrite(&magic, sizeof(magic), 1, quadfid);
  fwrite(&numQuads, sizeof(numQuads), 1, quadfid);
  fwrite(&DimQuads, sizeof(DimQuads), 1, quadfid);
  fwrite(&index_scale, sizeof(index_scale), 1, quadfid);
  fwrite(&numstars, sizeof(numstars), 1, quadfid);
}

void fix_code_header(FILE *codefid, uint numCodes)
{
	magicval magic = MAGIC_VAL;
	rewind(codefid);
	fwrite(&magic, sizeof(magic), 1, codefid);
	fwrite(&numCodes, sizeof(numCodes), 1, codefid);
}

void fix_quad_header(FILE *quadfid, uint numQuads)
{
	magicval magic = MAGIC_VAL;
	rewind(quadfid);
	fwrite(&magic, sizeof(magic), 1, quadfid);
	fwrite(&numQuads, sizeof(numQuads), 1, quadfid);
}


char read_objs_header(FILE *fid, uint *numstars, uint *DimStars,
                      double *ramin, double *ramax, 
							 double *decmin, double *decmax)
{
	magicval magic;
	fread(&magic, sizeof(magic), 1, fid);
	{
		if (magic != MAGIC_VAL) {
			fprintf(stderr, "ERROR (read_objs_header) -- bad magic value\n");
			return (READ_FAIL);
		}
		fread(numstars, sizeof(*numstars), 1, fid);
		fread(DimStars, sizeof(*DimStars), 1, fid);
		fread(ramin, sizeof(*ramin), 1, fid);
		fread(ramax, sizeof(*ramax), 1, fid);
		fread(decmin, sizeof(*decmin), 1, fid);
		fread(decmax, sizeof(*decmax), 1, fid);
	}

	return (0);
}



char read_code_header(FILE *fid, uint *numcodes, uint *numstars,
                      uint *DimCodes, double *index_scale)
{
	magicval magic;
	fread(&magic, sizeof(magic), 1, fid);
	{
		if (magic != MAGIC_VAL) {
			fprintf(stderr, "ERROR (read_code_header) -- bad magic value\n");
			return (READ_FAIL);
		}
		fread(numcodes, sizeof(*numcodes), 1, fid);
		fread(DimCodes, sizeof(*DimCodes), 1, fid);
		fread(index_scale, sizeof(*index_scale), 1, fid);
		fread(numstars, sizeof(*numstars), 1, fid);
	}
	return (0);

}

char read_quad_header(FILE *fid, uint *numquads, uint *numstars,
                      uint *DimQuads, double *index_scale)
{
	magicval magic;
	fread(&magic, sizeof(magic), 1, fid);
	{
		if (magic != MAGIC_VAL) {
			fprintf(stderr, "ERROR (read_quad_header) -- bad magic value\n");
			return (READ_FAIL);
		}
		fread(numquads, sizeof(*numquads), 1, fid);
		fread(DimQuads, sizeof(*DimQuads), 1, fid);
		fread(index_scale, sizeof(*index_scale), 1, fid);
		fread(numstars, sizeof(*numstars), 1, fid);
	}
	return (0);

}

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

void readonequad(FILE *fid, uint *iA, uint *iB, uint *iC, uint *iD)
{
	fread(iA, sizeof(*iA), 1, fid);
	fread(iB, sizeof(*iB), 1, fid);
	fread(iC, sizeof(*iC), 1, fid);
	fread(iD, sizeof(*iD), 1, fid);
	return ;
}

void writeonequad(FILE *fid, uint iA, uint iB, uint iC, uint iD)
{
	fwrite(&iA, sizeof(iA), 1, fid);
	fwrite(&iB, sizeof(iB), 1, fid);
	fwrite(&iC, sizeof(iC), 1, fid);
	fwrite(&iD, sizeof(iD), 1, fid);
	return ;
}

void readonecode(FILE *fid, double *Cx, double *Cy, double *Dx, double *Dy)
{
	fread(Cx, sizeof(*Cx), 1, fid);
	fread(Cy, sizeof(*Cy), 1, fid);
	fread(Dx, sizeof(*Dx), 1, fid);
	fread(Dy, sizeof(*Dy), 1, fid);
	return ;
}

void writeonecode(FILE *fid, double Cx, double Cy, double Dx, double Dy)
{
	fwrite(&Cx, sizeof(Cx), 1, fid);
	fwrite(&Cy, sizeof(Cy), 1, fid);
	fwrite(&Dx, sizeof(Dx), 1, fid);
	fwrite(&Dy, sizeof(Dy), 1, fid);
	return ;
}

