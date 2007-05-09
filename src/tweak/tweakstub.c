// ----------------------------------------------------------------------
// name:
//   tweakstub
// purpose:
//   testbed for Hogg and Sam to work on tweak
// revision history:
//   2007-05-08  started - Sam
//   2007-05-09  added outline of tweak ops - Hogg
// ----------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#define LINECHARS 80
#define HDULINES 36
#define TYPE_IDX 0
#define TYPE_XY 1

int tinyfitsread(FILE *fileptr, double **thedata, int fitstype);

int main(void) 
{
  double *xydata,*idxdata;
  unsigned int nxy,nidx,jj;
  FILE *idxfptr,*xyfptr;

  // open the files with coordinates
  idxfptr = fopen("index.rd.fits","r");
  if(idxfptr==NULL) {fprintf(stderr,"Unable to open file ./index.rd.fits\n"); return(-1); }
  xyfptr = fopen("field.xy.fits","r");
  if(xyfptr==NULL) {fprintf(stderr,"Unable to open file ./field.xy.fits\n"); fclose(idxfptr); return(-1); }

  // read data from the files
  nidx=tinyfitsread(idxfptr,&idxdata,TYPE_IDX);
  if(nidx<=0) fprintf(stderr,"Read nothing from ./index.rd.fits\n");
  nxy=tinyfitsread(xyfptr,&xydata,TYPE_XY);
  if(nxy<=0) fprintf(stderr,"Read nothing from ./field.xy.fits\n");
  fprintf(stderr,"Read %d index and %d field objects.\n",nidx,nxy);

  // open the files with WCS information
  // [tbd]

  // stuff WCS information into keir+dstn SIP structure
  // [tbd]

  // transform index objects to x,y
  for(jj=0;jj<nidx;jj++) {
    printf("index object %d : (ra,dec) = (%g,%g)\n",
	   jj+1,idxdata[2*jj],idxdata[2*jj+1]);
    // [tbd]
  }

  // transform field objects to ra,dec
  for(jj=0;jj<nxy;jj++) {
    printf("field object %d : (x,y,flux) = (%g,%g,%g)\n",
	   jj+1,xydata[3*jj],xydata[3*jj+1],xydata[3*jj+2]);
    // [tbd]
  }

  // do super-slow double loop, finding correspondences
  // - this double-loop can be replaced with KD-tree fu by dstn+keir later
  // - hard-coded hard tolerance is bad but okay for now
  // - check that it doesn't matter if we do the correspondences in the
  //   image or on the sky.
  // [tbd]

  // begin loop over ransac trials

  // choose five (?) correspondences at random
  // [tbd]

  // pack data into matrices in preparation for linear fitting
  // [tbd]

  // perform linear fit with matrix fu
  // [tbd]

  // apply result to all correspondences and ask how many are inliers
  // - just counting inliers with hard tolerance
  // - replace with EM fu later
  // [tbd]

  // if this is the best ransac trial so far, save the result
  // [tbd]

  // end loop over ransac trials

  // choose / gather / reload inliers at the best ransac trial
  // [tbd]

  // fit using best-ransac inliers
  // - this repeats the matrix fu: pack matrices and then decomp them
  // [tbd]

  // pack fit into WCS
  // - this step is non-trivial
  // - need to do some QA / asserts here
  // [tbd]

  // write out WCS as a new FITS header
  // - can probably use dstn/keir WCS fu here
  // [tbd]

  // free memory and close files
  free(xydata); free(idxdata);
  fclose(xyfptr); fclose(idxfptr);
  return(0);
}

int tinyfitsread(FILE *fileptr, double **thedata, int fitstype) 
{
  // thedata: we will alloc but caller must free 
  int counter,lines,thechar,hdus,endofblock,numread,jj;
  unsigned int naxis2=0;
  unsigned char *cptr,tmp;
  unsigned char printline[LINECHARS+1];
  for(hdus=0;hdus<2;hdus++) {
	 endofblock=0;
	 while(!endofblock) {
		for(lines=0;lines<HDULINES;lines++) {
		  printline[LINECHARS]=0;
		  for(counter=0;counter<LINECHARS;counter++) {
			 thechar=fgetc(fileptr);
			 if(thechar==EOF) {
				fprintf(stderr,"EOF encountered in header!\n");
				return(-1);
			 }
			 printline[counter]=(unsigned char)thechar;
		  }
		  if(!endofblock) {
			 sscanf(printline,"NAXIS2 = %u %*s",&naxis2);
			 if(printline[0]=='E' && printline[1]=='N' && printline[2]=='D') endofblock=1;
		  }
		}
	 }
  }
  if(naxis2<=0) {
	 fprintf(stderr,"unable to find NAXIS2 keyword\n");
	 return(-1);
  }

  if(fitstype==TYPE_IDX) {
	 double *dataptr;
	 *thedata = (double*)malloc(2*naxis2*sizeof(double));
	 numread=fread(*thedata,sizeof(double),2*naxis2,fileptr);
	 dataptr=*thedata;
	 for(jj=0;jj<numread;jj++) {
		cptr = (unsigned char *)(dataptr+jj); // byte swap madness
		tmp = cptr[0];cptr[0] = cptr[7]; cptr[7] = tmp;tmp = cptr[1];	
		cptr[1] = cptr[6];cptr[6] = tmp;	tmp = cptr[2];cptr[2] = cptr[5];
		cptr[5] =tmp;tmp = cptr[3];	cptr[3] = cptr[4];cptr[4] = tmp;
	 } 
	 return(naxis2);
  }

  if(fitstype==TYPE_XY) {
	 double *dataptr;
	 float *tmpdata;
	 *thedata = (double*)malloc(3*naxis2*sizeof(double));
	 tmpdata = (float*)malloc(3*naxis2*sizeof(float));
	 numread=fread(tmpdata,sizeof(float),3*naxis2,fileptr);
	 dataptr=*thedata;
	 for(jj=0;jj<numread;jj++) {
		cptr = (unsigned char *)(tmpdata+jj); // byte swap madness
		tmp = cptr[0];cptr[0] = cptr[3];cptr[3] =tmp;tmp = cptr[1];cptr[1] = cptr[2];cptr[2] = tmp;
		dataptr[jj]=(double)tmpdata[jj];
	 }
	 return(naxis2);
  }

  fprintf(stderr,"unrecognized fits type in tinyfitsread\n");
  return(-1);

}
