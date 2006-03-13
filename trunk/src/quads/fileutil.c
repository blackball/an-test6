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

xyarray *readxy(FILE *fid, char ParityFlip)
{
	char ASCII = 0;
	qidx ii, jj, numxy, numfields;
	magicval magic;
	xyarray *thepix = NULL;
	int tmpchar;

	if (fread(&magic, sizeof(magic), 1, fid) != 1) {
		fprintf(stderr, "ERROR (readxy) -- bad magic value in field file.\n");
		return ((xyarray *)NULL);
	}
	if (magic == ASCII_VAL) {
		ASCII = 1;
		if (fscanf(fid, "mFields=%lu\n", &numfields) != 1) {
			fprintf(stderr, "ERROR (readxy) -- bad first line in field file.\n");
			return ((xyarray *)NULL);
		}
	} else {
		if (magic != MAGIC_VAL) {
			fprintf(stderr, "ERROR (readxy) -- bad magic value in field file.\n");
			return ((xyarray *)NULL);
		}
		ASCII = 0;
		if (fread(&numfields, sizeof(numfields), 1, fid) != 1) {
			fprintf(stderr, "ERROR (readxy) -- bad numfields fread in field file.\n");
			return ((xyarray *)NULL);
		}
	}
	thepix = mk_xyarray(numfields);
	for (ii = 0;ii < numfields;ii++) {
		if (ASCII) {
			tmpchar = fgetc(fid);
			while (tmpchar == COMMENT_CHAR) {
				fscanf(fid, "%*[^\n]");
				fgetc(fid);
				tmpchar = fgetc(fid);
			}
			ungetc(tmpchar, fid);
			fscanf(fid, "%lu", &numxy); // CHECK THE RETURN VALUE MORON!
		} else
			fread(&numxy, sizeof(numxy), 1, fid); // CHECK THE RETURN VALUE MORON!

		xya_set(thepix, ii, mk_xy(numxy) );

		if (xya_ref(thepix, ii) == NULL) {
			fprintf(stderr, "ERROR (readxy) - out of memory at field %lu\n", ii);
			free_xyarray(thepix);
			return (xyarray *)NULL;
		}
		if (ASCII) {
			for (jj = 0;jj < numxy;jj++) {
				double tmp1, tmp2;
				fscanf(fid, ",%lf,%lf", &tmp1, &tmp2);
				xy_setx(xya_ref(thepix, ii), jj, tmp1);
				xy_sety(xya_ref(thepix, ii), jj, tmp2);
			}
			fscanf(fid, "\n");
		} else {
			double tmp[DIM_XY*numxy];
			fread(tmp, sizeof(double), DIM_XY*numxy, fid);
			for (jj = 0; jj<numxy; jj++) {
				double tmp1, tmp2;
				tmp1 = tmp[2*jj];
				tmp2 = tmp[2*jj+1];
				xy_setx(xya_ref(thepix, ii), jj, tmp1);
				xy_sety(xya_ref(thepix, ii), jj, tmp2);
			}
		}

		if (ParityFlip) {
			xy* xya = xya_ref(thepix, ii);
			double swaptmp;
			for (jj = 0;jj < numxy;jj++) {
				swaptmp = xy_refx(xya, jj);
				xy_setx(xya, jj, xy_refy(xya, jj));
				xy_sety(xya, jj, swaptmp);
			}
		}
	}

	return thepix;
}
