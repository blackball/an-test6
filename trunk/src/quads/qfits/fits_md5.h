/*----------------------------------------------------------------------------*/
/**
   @file    fits_md5.h
   @author  N. Devillard
   @date    May 2001
   @version $Revision: 1.1 $
   @brief   FITS data block MD5 computation routine.

   This module offers MD5 computation over all data areas of a FITS file.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id: fits_md5.h,v 1.1 2006/03/16 22:10:26 dlang Exp $
	$Author: dlang $
	$Date: 2006/03/16 22:10:26 $
	$Revision: 1.1 $
*/

#ifndef FITS_MD5_H
#define FITS_MD5_H

/*-----------------------------------------------------------------------------
   								Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-----------------------------------------------------------------------------
						Function ANSI prototypes
 -----------------------------------------------------------------------------*/

/* <dox> */
/*----------------------------------------------------------------------------*/
/**
  @brief    Compute the MD5 hash of data zones in a FITS file.
  @param    filename    Name of the FITS file to examine.
  @return   1 statically allocated character string, or NULL.

  This function expects the name of a FITS file.
  It will compute the MD5 hash on all data blocks in the main data section
  and possibly extensions (including zero-padding blocks if necessary) and
  return it as a string suitable for inclusion into a FITS keyword.

  The returned string is statically allocated inside this function,
  so do not free it or modify it. This function returns NULL in case
  of error.
 */
/*----------------------------------------------------------------------------*/
char * qfits_datamd5(char * filename);
/* </dox> */

#endif
/* vim: set ts=4 et sw=4 tw=75 */
