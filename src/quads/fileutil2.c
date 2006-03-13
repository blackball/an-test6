#include <stdlib.h>
#include <string.h>
#include "fileutil2.h"

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


void write_objs_header(FILE *fid, sidx numstars,
                       dimension DimStars, double ramin, double ramax, 
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

void write_code_header(FILE *codefid, qidx numCodes,
                       sidx numstars, dimension DimCodes, double index_scale)
{
  magicval magic = MAGIC_VAL;
  fwrite(&magic, sizeof(magic), 1, codefid);
  fwrite(&numCodes, sizeof(numCodes), 1, codefid);
  fwrite(&DimCodes, sizeof(DimCodes), 1, codefid);
  fwrite(&index_scale, sizeof(index_scale), 1, codefid);
  fwrite(&numstars, sizeof(numstars), 1, codefid);
}

void write_quad_header(FILE *quadfid, qidx numQuads, sidx numstars,
                       dimension DimQuads, double index_scale)
{
  magicval magic = MAGIC_VAL;

  fwrite(&magic, sizeof(magic), 1, quadfid);
  fwrite(&numQuads, sizeof(numQuads), 1, quadfid);
  fwrite(&DimQuads, sizeof(DimQuads), 1, quadfid);
  fwrite(&index_scale, sizeof(index_scale), 1, quadfid);
  fwrite(&numstars, sizeof(numstars), 1, quadfid);
}

void fix_code_header(FILE *codefid, qidx numCodes)
{
	magicval magic = MAGIC_VAL;
	rewind(codefid);
	fwrite(&magic, sizeof(magic), 1, codefid);
	fwrite(&numCodes, sizeof(numCodes), 1, codefid);
}

void fix_quad_header(FILE *quadfid, qidx numQuads)
{
	magicval magic = MAGIC_VAL;
	rewind(quadfid);
	fwrite(&magic, sizeof(magic), 1, quadfid);
	fwrite(&numQuads, sizeof(numQuads), 1, quadfid);
}


char read_objs_header(FILE *fid, sidx *numstars, dimension *DimStars,
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



char read_code_header(FILE *fid, qidx *numcodes, sidx *numstars,
                      dimension *DimCodes, double *index_scale)
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

char read_quad_header(FILE *fid, qidx *numquads, sidx *numstars,
                      dimension *DimQuads, double *index_scale)
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


char *mk_filename(const char *basename, const char *extension)
{
	char *fname;
	fname = (char *)malloc(strlen(basename) + strlen(extension) + 1);
	sprintf(fname, "%s%s", basename, extension);
	return fname;
}

/*
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
*/

void readonequad(FILE *fid, qidx *iA, qidx *iB, qidx *iC, qidx *iD)
{
	fread(iA, sizeof(*iA), 1, fid);
	fread(iB, sizeof(*iB), 1, fid);
	fread(iC, sizeof(*iC), 1, fid);
	fread(iD, sizeof(*iD), 1, fid);
	return ;
}

void writeonequad(FILE *fid, qidx iA, qidx iB, qidx iC, qidx iD)
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


/*
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
*/


