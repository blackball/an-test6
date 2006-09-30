
#include "ezfits.h"

int ezwriteimage(char* fn, int datatype, void* data, int w, int h)
{

	int status = 0;
	fitsfile *fptr;        /* FITS file pointer to output file */
	if (fits_create_file(&fptr, fn, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}

	long naxis[2] = {w, h};
	long fpixel[2] = {1, 1};
	if (fits_create_img(fptr, 32, 2, naxis, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}
	if (fits_write_pix(fptr, datatype, fpixel, w*h, data, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}

	if (fits_close_file(fptr, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}
	return 1;
}

int ezwritescatter(char* fn, int datatype, double* x, double *y, int n)
{

	int status = 0;
	fitsfile *fptr;        /* FITS file pointer to output file */
	if (fits_create_file(&fptr, fn, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}

	long naxis[2] = {w, h};
	long fpixel[2] = {1, 1};
	if (fits_create_img(fptr, 32, 2, naxis, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}

	char* ttype[] = {"X","Y"};
	char* tform[] = {"D","D"};
	char* tunit[] = {"pix","pix"};
	fits_create_tbl(ofptr, BINARY_TBL, n, 2, ttype, tform,
			tunit, "SCATTER", &status);
	fits_write_col(ofptr, TDOUBLE, 1, 1, 1, n, x, &status);
	fits_report_error(stderr, status);
	assert(!status);
	fits_write_col(ofptr, TDOUBLE, 2, 1, 1, n, y, &status);
	fits_report_error(stderr, status);
	assert(!status);

	fits_modify_comment(ofptr, "TTYPE1", "X coordinate", &status);
	assert(!status);
	fits_modify_comment(ofptr, "TTYPE2", "Y coordinate", &status);
	assert(!status);

	if (fits_close_file(fptr, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}
	return 1;
}
