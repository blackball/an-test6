#include <string.h>
#include <stdio.h>
#include "fitsio.h"
#include "dimage.h"

#define MAXNPEAKS 100000

static float *x=NULL;
static float *y=NULL;
static float *flux=NULL;

int main(int argc, char *argv[])
{
    fitsfile *fptr;         /* FITS file pointer, defined in fitsio.h */
    char card[FLEN_CARD];   /* Standard string lengths defined in fitsio.h */
    int status = 0, single = 0, hdupos, nkeys, ii, anynull;
    int naxis,naxis1,naxis2,bitpix,itype;
		int maxnpeaks=MAXNPEAKS, npeaks;
    long naxisn[2];
    long fpixel[2]={1L,1L};
    int tfields=0, i;
    unsigned long int bscale=1L,bzero=0L,kk,jj;
    float *thedata=NULL;
		float sigma;

    if (argc != 2) {
      fprintf(stderr,"Usage: dumpimage filename[ext] \n");
      fprintf(stderr,"\n");
      fprintf(stderr,"List FITS header keywords in a single extension, or, if \n");
      fprintf(stderr,"ext is not given, list the keywords in all the extensions. \n");
      fprintf(stderr,"\n");
      fprintf(stderr,"Examples: \n");
      fprintf(stderr,"   listhead file.fits      - list every header in the file \n");
      fprintf(stderr,"   listhead file.fits[0]   - list primary array header \n");
      fprintf(stderr,"   listhead file.fits[2]   - list header of 2nd extension \n");
      fprintf(stderr,"   listhead file.fits+2    - same as above \n");
      fprintf(stderr,"   listhead file.fits[GTI] - list header of GTI extension\n");
      fprintf(stderr,"\n");
      fprintf(stderr,"Note that it may be necessary to enclose the input file\n");
      fprintf(stderr,"name in single quote characters on the Unix command line.\n");
      return(0);
    }

    if (!fits_open_file(&fptr, argv[1], READONLY, &status))
    {
      fits_get_hdu_num(fptr, &hdupos);  /* Get the current HDU position */
      //fits_movrel_hdu(fptr, 1, NULL, &status);  /* try to move to next HDU */

      // check status
      fprintf(stderr, "Header listing for HDU #%d:\n", hdupos-1);
      fits_get_hdrspace(fptr, &nkeys, NULL, &status); /* get # of keywords */
      for (ii = 1; ii <= nkeys; ii++) { /* Read and print each keywords */
	if (fits_read_record(fptr, ii, card, &status))break;
	fprintf(stderr,"%s\n", card);
      }
      fprintf(stderr,"END\n\n");  /* terminate listing with END */

      itype=fits_get_img_equivtype(fptr,&bitpix,&status);
      if(status) {
	fits_report_error(stderr, status);
	exit(-1);
      }
      fits_get_img_dim(fptr,&naxis,&status);
      if(status) {
	fits_report_error(stderr, status);
	exit(-1);
      }
      if(naxis!=2) {
	fprintf(stderr,"NAXIS must be 2.\n");
	exit(-1);
      }
      fits_get_img_size(fptr,2,naxisn,&status);
      if(status) {
	fits_report_error(stderr, status);
	exit(-1);
      }

      fprintf(stderr,"Got naxis=%d,na1=%d,na2=%d,bitpix=%d\n",
	     naxis,naxisn[0],naxisn[1],bitpix);
      //fprintf(stderr,"bscale=%lu bzero=%lu\n",bscale,bzero);

      thedata=(float *)malloc(naxisn[0]*naxisn[1]*sizeof(float));
      if(thedata==NULL) {
	fprintf(stderr,"Failed allocating data array.\n");
	exit(-1);
      }

      fits_read_pix(fptr,TFLOAT,fpixel,naxisn[0]*naxisn[1],NULL,thedata,
		    NULL,&status);

			x=(float *) malloc(maxnpeaks*sizeof(float));
			y=(float *) malloc(maxnpeaks*sizeof(float));
			flux=(float *) malloc(maxnpeaks*sizeof(float));
			simplexy( thedata, naxisn[0], naxisn[1], 1., 8., 1., 3., 1000, 
								maxnpeaks, &sigma, x, y, flux, &npeaks);
			
			fprintf(stdout, "# X Y FLUX\n");
			for(i=0;i<npeaks;i++) 
				fprintf(stdout, "%e %e %e\n", x[i], y[i], flux[i]);

    }


    if (status == END_OF_FILE)  status = 0; /* Reset after normal error */

    fits_close_file(fptr, &status);


    if (status) fits_report_error(stderr, status); /* print any error message */
    return(status);
}


/*

if(0) {
      fits_read_key(fptr,TINT,"NAXIS",&naxis,NULL,&status);
      if(status) {printf("NAXIS not found\n"); exit(-1);}
      if(naxis!=2) {printf("NAXIS must be 2\n"); exit(-1);}
      fits_read_key(fptr,TINT,"NAXIS1",&naxis1,NULL,&status);
      if(status) {printf("NAXIS1 not found\n"); exit(-1);}
      fits_read_key(fptr,TINT,"NAXIS2",&naxis2,NULL,&status);
      if(status) {printf("NAXIS2 not found\n"); exit(-1);}
      fits_read_key(fptr,TINT,"BITPIX",&bitpix,NULL,&status);
      if(status) {printf("BITPIX not found\n"); exit(-1);}
      fits_read_key(fptr,TINT,"TFIELDS",&tfields,NULL,&status2);
      if(tfields>0) {printf("TABLE not IMAGE\n"); exit(-1);}
      fits_read_key(fptr,TULONG,"BSCALE",&bscale,NULL,&status2);
      fits_read_key(fptr,TULONG,"BZERO",&bzero,NULL,&status2);

}
*/
