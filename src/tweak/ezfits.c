
#include <assert.h>
#include <stdarg.h>
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

int ezscatter(char* fn, double* x, double *y,
                   double* a, double *d, int n)
{

	int status = 0;
	fitsfile *fptr;        /* FITS file pointer to output file */
	if (fits_create_file(&fptr, fn, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}

	char* ttype[] = {"X","Y", "RA","DEC"};
	char* tform[] = {"D","D","D","D"};
	char* tunit[] = {"pix","pix","degrees","degrees"};
	fits_create_tbl(fptr, BINARY_TBL, n, 4, ttype, tform,
			tunit, "SCATTER", &status);
	fits_write_col(fptr, TDOUBLE, 1, 1, 1, n, x, &status);
	fits_report_error(stderr, status);
	assert(!status);
	fits_write_col(fptr, TDOUBLE, 2, 1, 1, n, y, &status);
	fits_report_error(stderr, status);
	assert(!status);
	fits_write_col(fptr, TDOUBLE, 3, 1, 1, n, a, &status);
	fits_report_error(stderr, status);
	assert(!status);
	fits_write_col(fptr, TDOUBLE, 4, 1, 1, n, d, &status);
	fits_report_error(stderr, status);
	assert(!status);

	fits_modify_comment(fptr, "TTYPE1", "X coordinate", &status);
	assert(!status);
	fits_modify_comment(fptr, "TTYPE2", "Y coordinate", &status);
	assert(!status);
	fits_modify_comment(fptr, "TTYPE3", "ra of point", &status);
	assert(!status);
	fits_modify_comment(fptr, "TTYPE4", "dec of point", &status);
	assert(!status);

	if (fits_close_file(fptr, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}
	return 1;
}

int ezscatterappend(char* fn, double* x, double *y,
                    double* a, double *d, int n)
{

	int status = 0;
	fitsfile *fptr;        /* FITS file pointer to output file */
	if (fits_create_file(&fptr, fn, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}

	char* ttype[] = {"X","Y", "RA","DEC"};
	char* tform[] = {"D","D","D","D"};
	char* tunit[] = {"pix","pix","degrees","degrees"};
	fits_create_tbl(fptr, BINARY_TBL, n, 4, ttype, tform,
			tunit, "SCATTER", &status);
	fits_write_col(fptr, TDOUBLE, 1, 1, 1, n, x, &status);
	fits_report_error(stderr, status);
	assert(!status);
	fits_write_col(fptr, TDOUBLE, 2, 1, 1, n, y, &status);
	fits_report_error(stderr, status);
	assert(!status);
	fits_write_col(fptr, TDOUBLE, 3, 1, 1, n, a, &status);
	fits_report_error(stderr, status);
	assert(!status);
	fits_write_col(fptr, TDOUBLE, 4, 1, 1, n, d, &status);
	fits_report_error(stderr, status);
	assert(!status);

	fits_modify_comment(fptr, "TTYPE1", "X coordinate", &status);
	assert(!status);
	fits_modify_comment(fptr, "TTYPE2", "Y coordinate", &status);
	assert(!status);
	fits_modify_comment(fptr, "TTYPE3", "ra of point", &status);
	assert(!status);
	fits_modify_comment(fptr, "TTYPE4", "dec of point", &status);
	assert(!status);

	if (fits_close_file(fptr, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}
	return 1;
}

/*
int eztable(char* fn, char* tblfmt, int n args, ...) {
        va_list ap;

        va_start(ap, n args);
        max = va_arg(ap, int);
        for(i = 2; i <= n_args; i++) {
                if((a = va_arg(ap, int)) > max)
                        max = a;
        }

        va_end(ap);
        return max;
}
*/
