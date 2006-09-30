
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
	if (fits_create_img(fptr, 8, 2, naxis, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}
	if (fits_write_pix(fptr, datatype, fpixel, w*h, data, &status)) {
		fits_report_error(stderr, status);
		return 0;
	}

	fits_close_file(fptr, &status);
	return 1;
}
