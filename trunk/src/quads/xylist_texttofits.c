#include "xylist.h"
#include "bl.h"

xyarray *readxy(FILE *fid, char ParityFlip);

typedef pl xyarray;
#define mk_xyarray(n)         pl_new(n)
#define free_xyarray(l)       pl_free(l)
#define xya_ref(l, i)    (xy*)pl_get((l), (i))
#define xya_set(l, i, v)      pl_set((l), (i), (v))
#define xya_size(l)           pl_size(l)

xyarray *readxy(FILE *fid, char ParityFlip)
{
	uint ii, jj, numxy, numfields;
	magicval magic;
	xyarray *thepix = NULL;
	int tmpchar;

	if (fread(&magic, sizeof(magic), 1, fid) != 1) {
		fprintf(stderr, "ERROR (readxy) -- bad magic value in field file.\n");
		return NULL;
	}
	if (magic != ASCII_VAL) {
		fprintf(stderr, "Error - bad magic value.\n");
		return NULL;
	}
	if (fscanf(fid, "mFields=%u\n", &numfields) != 1) {
		fprintf(stderr, "ERROR (readxy) -- bad first line in field file.\n");
		return NULL;
	}
	thepix = mk_xyarray(numfields);
	for (ii = 0;ii < numfields;ii++) {
		tmpchar = fgetc(fid);
		while (tmpchar == COMMENT_CHAR) {
			fscanf(fid, "%*[^\n]");
			fgetc(fid);
			tmpchar = fgetc(fid);
		}
		ungetc(tmpchar, fid);
		fscanf(fid, "%u", &numxy); // CHECK THE RETURN VALUE MORON!

		xya_set(thepix, ii, mk_xy(numxy) );

		if (xya_ref(thepix, ii) == NULL) {
			fprintf(stderr, "ERROR (readxy) - out of memory at field %u\n", ii);
			free_xyarray(thepix);
			return NULL;
		}
		for (jj = 0;jj < numxy;jj++) {
			double tmp1, tmp2;
			fscanf(fid, ",%lf,%lf", &tmp1, &tmp2);
			xy_setx(xya_ref(thepix, ii), jj, tmp1);
			xy_sety(xya_ref(thepix, ii), jj, tmp2);
		}
		fscanf(fid, "\n");
		
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
