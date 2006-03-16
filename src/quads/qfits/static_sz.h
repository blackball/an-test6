/*----------------------------------------------------------------------------*/
/**
  @file		static_sz.h
  @author	N. Devillard
  @date		Dec 1999
  @version	$Revision: 1.1 $
  @brief	Definitions for various fixed sizes.
*/
/*----------------------------------------------------------------------------*/

/*
	$Id: static_sz.h,v 1.1 2006/03/16 22:10:26 dlang Exp $
	$Author: dlang $
	$Date: 2006/03/16 22:10:26 $
	$Revision: 1.1 $
*/

#ifndef STATIC_SZ_H
#define STATIC_SZ_H

#ifdef __cplusplus
extern "C" {
#endif

/* <dox> */
/*-----------------------------------------------------------------------------
   								Defines
 -----------------------------------------------------------------------------*/

/** Maximum supported file name size */
#define FILENAMESZ				512

/** Maximum number of characters per line in an ASCII file */
#define ASCIILINESZ				1024

/* </dox> */
#ifdef __cplusplus
}
#endif

#endif
/* vim: set ts=4 et sw=4 tw=75 */
