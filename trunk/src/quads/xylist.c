#include "xylist.h"
#include "fileutil.h"


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
