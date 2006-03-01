#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "export.h"
#include "dimage.h"

#define FREEVEC(a) {if((a)!=NULL) free((char *) (a)); (a)=NULL;}
static void free_memory()
{
}


/********************************************************************/
IDL_LONG idl_deblend (int      argc,
											void *   argv[])
{
	IDL_LONG nx,ny,*nchild, maxnchild, *xcen, *ycen;
	float *cimages, *templates, *image, *invvar, dlim, sigma, 
    tsmooth, tlimit, tfloor, saddle, parallel, minpeak; 
	
	IDL_LONG i;
	IDL_LONG retval=1;

	/* 0. allocate pointers from IDL */
	i=0;
	image=((float *)argv[i]); i++;
	invvar=((float *)argv[i]); i++;
	nx=*((int *)argv[i]); i++;
	ny=*((int *)argv[i]); i++;
	nchild=((IDL_LONG *)argv[i]); i++;
	xcen=((IDL_LONG *)argv[i]); i++;
	ycen=((IDL_LONG *)argv[i]); i++;
	cimages=((float *)argv[i]); i++;
	templates=((float *)argv[i]); i++;
	sigma=*((float *)argv[i]); i++;
	dlim=*((float *)argv[i]); i++;
	tsmooth=*((float *)argv[i]); i++;
	tlimit=*((float *)argv[i]); i++;
	tfloor=*((float *)argv[i]); i++;
	saddle=*((float *)argv[i]); i++;
	parallel=*((float *)argv[i]); i++;
	maxnchild=*((int *)argv[i]); i++;
	minpeak=*((float *)argv[i]); i++;
	
	/* 1. run the fitting routine */
	retval=(IDL_LONG) deblend(image, invvar, nx, ny, (int *) nchild, 
														(int *) xcen, (int *) ycen, cimages, templates, 
														sigma, dlim, tsmooth,
                            tlimit, tfloor, saddle, parallel, maxnchild, 
                            minpeak);
	
	/* 2. free memory and leave */
	free_memory();
	return retval;
}

/***************************************************************************/

