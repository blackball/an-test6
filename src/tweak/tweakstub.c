#include <stdio.h>
#include <stdlib.h>

#define LINECHARS 80
#define HDULINES 36
#define TYPE_IDX 0
#define TYPE_XY 1


// reads in the two files index.rd.fits and field.xy.fits into two double arrays
// designed as a debugging stub to help get tweak started


int tinyfitsread(FILE *fileptr, double **thedata, int fitstype);

int main(void) 
{
  double *xydata,*idxdata;
  unsigned int nxy,nidx,jj;
  FILE *idxfptr,*xyfptr;

  // open the files
  idxfptr = fopen("index.rd.fits","r");
  if(idxfptr==NULL) {fprintf(stderr,"Unable to open file ./index.rd.fits\n"); return(-1); }
  xyfptr = fopen("field.xy.fits","r");
  if(xyfptr==NULL) {fprintf(stderr,"Unable to open file ./field.xy.fits\n"); fclose(idxfptr); return(-1); }

  // read the data from them
  nidx=tinyfitsread(idxfptr,&idxdata,TYPE_IDX);
  nxy=tinyfitsread(xyfptr,&xydata,TYPE_XY);

  if(nidx<=0) fprintf(stderr,"Could not read index objects from ./index.rd.fits\n");
  if(nxy<=0) fprintf(stderr,"Could not read field objects from ./field.xy.fits\n");

  // HOGG -- do stuff here, for example
  for(jj=0;jj<nidx;jj++) {
	 printf("index object (ra,dec) = (%g,%g)\n",idxdata[2*jj],idxdata[2*jj+1]);
  }
  for(jj=0;jj<nxy;jj++) {
	 printf("field object (x,y,flux) = (%g,%g,%g)\n",xydata[3*jj],xydata[3*jj+1],xydata[3*jj+2]);
  }

  // free memory and close files
  free(xydata); free(idxdata);
  fclose(xyfptr); fclose(idxfptr);

  return(0);

}



int tinyfitsread(FILE *fileptr, double **thedata, int fitstype) 
{
  // thedata: we will alloc but caller must free 
  int counter,lines,thechar,hdus,endofblock;
  unsigned int naxis2=0;
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
	 *thedata = (double*)malloc(2*naxis2*sizeof(double));
	 return fread(*thedata,sizeof(double),2*naxis2,fileptr);
  }
  if(fitstype==TYPE_XY) {
	 int jj,numread;
	 double *dataptr;
	 float *tmpdata;
	 *thedata = (double*)malloc(3*naxis2*sizeof(double));
	 tmpdata = (float*)malloc(3*naxis2*sizeof(float));
	 numread=fread(tmpdata,sizeof(float),3*naxis2,fileptr);
	 dataptr=*thedata;
	 for(jj=0;jj<numread;jj++) {
		dataptr[jj]=(double)tmpdata[jj];
	 }
	 return(numread);
  }
  fprintf(stderr,"unrecognized fits type in tinyfitsread\n");
  return(-1);
}
