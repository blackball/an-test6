#include <string.h>
#include <stdio.h>
#include "fitsio.h"

int main(int argc, char *argv[])
{
    fitsfile *fptr;      /* FITS file pointer, defined in fitsio.h */
    char *val, value[1000], nullstr[]="*";
    char keyword[FLEN_KEYWORD], colname[FLEN_VALUE];
    int status = 0;   /*  CFITSIO status value MUST be initialized to zero!  */
    int hdunum, hdutype, ncols, ii, anynul, dispwidth[1000];
    long nelements[1000];
    int firstcol, lastcol = 1, linewidth;
    int elem, firstelem, lastelem = 0, nelems;
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

    if (!fits_open_file(&fptr, argv[1], READONLY, &status))
    {
      if ( fits_get_hdu_num(fptr, &hdunum) == 1 )
          /* This is the primary array;  try to move to the */
          /* first extension and see if it is a table */
          fits_movabs_hdu(fptr, 2, &hdutype, &status);
       else
          fits_get_hdu_type(fptr, &hdutype, &status); /* Get the HDU type */

      if (hdutype == IMAGE_HDU)
          printf("Error: this program only displays tables, not images\n");
      else
      {
        fits_get_num_rows(fptr, &nrows, &status);
        fits_get_num_cols(fptr, &ncols, &status);

        /* find the number of columns that will fit within 80 characters */
        for (;;) {
          linewidth = 0;
          /* go on to the next element in the current column. */
          /* (if such an element does not exist, the inner 'for' loop
             does not get run and we skip to the next column.) */
          firstcol = lastcol;
          firstelem = lastelem + 1;
          elem = firstelem;

		  if ((firstcol == ncols) && (firstelem >= nelements[ncols]))
			  break;

          for (lastcol = firstcol; lastcol <= ncols; lastcol++) {
             int typecode;
             fits_get_col_display_width
                (fptr, lastcol, &dispwidth[lastcol], &status);
             fits_get_coltype
                (fptr, lastcol, &typecode, &nelements[lastcol], NULL, &status);
			 typecode = abs(typecode);
             if (typecode == TBIT)
				 nelements[lastcol] = (nelements[lastcol] + 7)/8;
			 else if (typecode == TSTRING)
				 nelements[lastcol] = 1;
             nelems = nelements[lastcol];
             for (lastelem = elem; lastelem <= nelems; lastelem++) {
                 linewidth += dispwidth[lastcol] + 1;
                 if (linewidth > 80) {
                     /* adding this element would overflow the line, so
                        don't include it - and if it's a column of only
                        one element, don't include the whole column. */
                     if (lastelem > 1)
                         lastelem--;
                     else {
                         lastcol--;
                         lastelem = nelements[lastcol];
                     }
                     break;
                 }
             }
             if (linewidth > 80)
                 break;
             /* start at the first element of the next column. */
             elem = 1;
          }

          /* if we exited the loop naturally, (not via break) then include all columns. */
          if (lastcol > ncols) {
              lastcol = ncols;
              lastelem = nelements[lastcol];
          }

          /* print column names as column headers */
          printf("\n    ");
          for (ii = firstcol; ii <= lastcol; ii++) {
              int maxelem;
              fits_make_keyn("TTYPE", ii, keyword, &status);
              fits_read_key(fptr, TSTRING, keyword, colname, NULL, &status);
              colname[dispwidth[ii]] = '\0';  /* truncate long names */
              kk = ((ii == firstcol) ? firstelem : 1);
              maxelem = ((ii == lastcol) ? lastelem : nelements[ii]);

              for (; kk <= maxelem; kk++) {
                  if (kk > 1) {
                      char buf[32];
                      int len = snprintf(buf, 32, "(%li)", kk);
                      printf("%*.*s%s ",dispwidth[ii]-len, dispwidth[ii]-len, colname, buf);
                  } else
                      printf("%*s ",dispwidth[ii], colname);
              }
          }
          printf("\n");  /* terminate header line */

          /* print each column, row by row (there are faster ways to do this) */
          val = value;
          for (jj = 1; jj <= nrows && !status; jj++) {
              printf("%4d ", (int)jj);
              for (ii = firstcol; ii <= lastcol; ii++)
              {
                  kk = ((ii == firstcol) ? firstelem : 1);
                  nelems = ((ii == lastcol) ? lastelem : nelements[ii]);
                  for (; kk <= nelems; kk++)
                  {
                      /* read value as a string, regardless of intrinsic datatype */
                      if (fits_read_col_str (fptr,ii,jj,kk, 1, nullstr,
                                             &val, &anynul, &status) )
                          break;  /* jump out of loop on error */
                      printf("%-*s ",dispwidth[ii], value);
                  }
              }
              printf("\n");
          }
        }
      }
      fits_close_file(fptr, &status);
    }

    if (status) fits_report_error(stderr, status); /* print any error message */
    return(status);
}
