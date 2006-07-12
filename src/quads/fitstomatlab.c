#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "fitsio.h"

int main(int argc, char *argv[])
{
    fitsfile *fptr;      /* FITS file pointer, defined in fitsio.h */
    char *val, value[1000], nullstr[]="*";
    char keyword[FLEN_KEYWORD], colname[FLEN_VALUE];
    int status = 0;   /*  CFITSIO status value MUST be initialized to zero!  */
    int hdunum, hdutype, ncols, ii, anynul;
    long jj, nrows, kk;

    if (argc != 2) {
      printf("Usage:  tablist filename[ext][col filter][row filter] \n");
      printf("\n");
      printf("List the contents of a FITS table \n");
      printf("\n");
      printf("Examples: \n");
      printf("  tablist tab.fits[GTI]           - list the GTI extension\n");
      printf("  tablist tab.fits[1][#row < 101] - list first 100 rows\n");
      printf("  tablist tab.fits[1][col X;Y]    - list X and Y cols only\n");
      printf("  tablist tab.fits[1][col -PI]    - list all but the PI col\n");
      printf("  tablist tab.fits[1][col -PI][#row < 101]  - combined case\n");
      printf("\n");
      printf("Display formats can be modified with the TDISPn keywords.\n");
      return(0);
    }

    if (fits_open_file(&fptr, argv[1], READONLY, &status))
		goto bailout;

	if ( fits_get_hdu_num(fptr, &hdunum) == 1 )
		/* This is the primary array;  try to move to the */
		/* first extension and see if it is a table */
		fits_movabs_hdu(fptr, 2, &hdutype, &status);
	else
		fits_get_hdu_type(fptr, &hdutype, &status); /* Get the HDU type */

	if (hdutype == IMAGE_HDU) {
		printf("Error: this program only displays tables, not images\n");
		fits_close_file(fptr, &status);
		goto bailout;
	}

	fits_get_num_rows(fptr, &nrows, &status);
	fits_get_num_cols(fptr, &ncols, &status);

	// column names become variable names.
	for (ii = 1; ii <= ncols; ii++) {
		int typecode;
		long nelems;
		fits_make_keyn("TTYPE", ii, keyword, &status);
		fits_read_key(fptr, TSTRING, keyword, colname, NULL, &status);
		fits_get_coltype(fptr, ii, &typecode, &nelems, NULL, &status);
		if (abs(typecode) == TBIT)
			nelems = 1;

		printf("%s = [ ", colname);

		// print each column, row by row (there are faster ways to do this)
		val = value;
		for (jj=1; jj<=nrows && !status; jj++) {
			for (kk=1; kk<=nelems; kk++) {
				/* read value as a string, regardless of intrinsic datatype */
				if (fits_read_col_str (fptr, ii, jj, kk, 1, nullstr,
									   &val, &anynul, &status) )
					break;  /* jump out of loop on error */
				// remove whitespace.
				val = value;
				if (typecode != TSTRING) {
					char* endp;
					while (*val && isspace(*val)) val++;
					endp = val + strlen(val) - 1;
					while (endp > val && isspace(*endp)) endp--;
					endp[1] = '\0';
				}
				printf("%s, ", val);
			}
			printf("; ");
		}
		printf("];\n");
	}
	fits_close_file(fptr, &status);
 bailout:
    if (status)
		fits_report_error(stderr, status); /* print any error message */
    return(status);
}

