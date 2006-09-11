/*----------------------------------------------------------------------------*/
/**
   @file    fits_rw.c
   @author  N. Devillard
   @date    Mar 2000
   @version $Revision: 1.5 $
   @brief   FITS header reading/writing.
*/
/*----------------------------------------------------------------------------*/

/*
    $Id: fits_rw.c,v 1.5 2006/07/25 14:52:43 dlang Exp $
    $Author: dlang $
    $Date: 2006/07/25 14:52:43 $
    $Revision: 1.5 $
*/

/*-----------------------------------------------------------------------------
                                Includes
 -----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "qfits.h"
#include "fits_p.h"
#include "xmemory.h"
#include "qerror.h"

/*-----------------------------------------------------------------------------
                        Private to this module  
 -----------------------------------------------------------------------------*/
static int is_blank_line(char * s)
{
    int     i ;

    for (i=0 ; i<(int)strlen(s) ; i++) {
        if (s[i]!=' ') return 0 ;
    }
    return 1 ;
}

/*-----------------------------------------------------------------------------
                            Function codes
 -----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
/**
  @brief    Read a FITS header from a file to an internal structure.
  @param    filename    Name of the file to be read
  @return   Pointer to newly allocated qfits_header or NULL in error case.

  This function parses a FITS (main) header, and returns an allocated
  qfits_header object. The qfits_header object contains a linked-list of
  key "tuples". A key tuple contains:

  - A keyword
  - A value
  - A comment
  - An original FITS line (as read from the input file)

  Direct access to the structure is not foreseen, use accessor
  functions in fits_h.h

  Value, comment, and original line might be NULL pointers.
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_read(const char * filename)
{
    /* Forward job to readext */
    return qfits_header_readext(filename, 0);
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Read a FITS header from a 'hdr' file.
  @param    filename    Name of the file to be read
  @return   Pointer to newly allocated qfits_header or NULL in error case

  This function parses a 'hdr' file, and returns an allocated qfits_header 
  object. A hdr file is an ASCII format were the header is written with a 
  carriage return after each line. The command dfits typically displays 
  a hdr file.
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_read_hdr(char * filename)
{
    qfits_header    *   hdr ;
    FILE            *   in ;
    char                line[81];
    char            *   key,
                    *   val,
                    *   com ;
    int                 i, j ;

    /* Check input */
    if (filename==NULL) return NULL ;

    /* Initialise */
    key = val = com = NULL ; 

    /* Open the file */
    if ((in=fopen(filename, "r"))==NULL) {
        qfits_error("cannot read [%s]", filename) ;
        return NULL ;
    }
    
    /* Create the header */
    hdr = qfits_header_new() ;
    
    /* Go through the file */
    while (fgets(line, 81, in)!=NULL) {
        for (i=0 ; i<81 ; i++) {
            if (line[i] == '\n') {
                for (j=i ; j<81 ; j++) line[j] = ' ' ;
                line[80] = (char)0 ;
                break ;
            }
        }
        if (!strcmp(line, "END")) {
            line[3] = ' ';
            line[4] = (char)0 ;
        }
        
        /* Rule out blank lines */
        if (!is_blank_line(line)) {

            /* Get key, value, comment for the current line */
            key = qfits_getkey(line);
            val = qfits_getvalue(line);
            com = qfits_getcomment(line);

            /* If key or value cannot be found, trigger an error */
            if (key==NULL) {
                qfits_header_destroy(hdr);
                fclose(in) ;
                return NULL ;
            }
            /* Append card to linked-list */
            qfits_header_append(hdr, key, val, com, NULL);
        }
    }
    fclose(in) ;

    /* The last key should be 'END' */
    if (strlen(key)!=3) {
        qfits_header_destroy(hdr);
        return NULL ;
    } 
    if (key[0]!='E' || key[1]!='N' || key[2]!='D') {
        qfits_header_destroy(hdr);
        return NULL ;
    }
    
    return hdr ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Read a FITS header from a 'hdr' string
  @param    hdr_str         String containing the hdr file
  @param    nb_char         Number of characters in the string
  @return   Pointer to newly allocated qfits_header or NULL in error case

  This function parses a 'hdr' string, and returns an allocated qfits_header 
  object. 
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_read_hdr_string(
        unsigned char   *   hdr_str,
        int                 nb_char)
{
    qfits_header    *   hdr ;
    char                line[81];
    char            *   key,
                    *   val,
                    *   com ;
    int                 ind ;
    int                 i, j ;

    /* Check input */
    if (hdr_str==NULL) return NULL ;

    /* Initialise */
    key = val = com = NULL ; 

    /* Create the header */
    hdr = qfits_header_new() ;
    
    /* Go through the file */
    ind = 0 ;
    while (ind <= nb_char - 80) {
        strncpy(line, (char*)hdr_str + ind, 80) ;
        line[80] = (char)0 ;
        for (i=0 ; i<81 ; i++) {
            if (line[i] == '\n') {
                for (j=i ; j<81 ; j++) line[j] = ' ' ;
                line[80] = (char)0 ;
                break ;
            }
        }
        if (!strcmp(line, "END")) {
            line[3] = ' ';
            line[4] = (char)0 ;
        }
        
        /* Rule out blank lines */
        if (!is_blank_line(line)) {

            /* Get key, value, comment for the current line */
            key = qfits_getkey(line);
            val = qfits_getvalue(line);
            com = qfits_getcomment(line);

            /* If key or value cannot be found, trigger an error */
            if (key==NULL) {
                qfits_header_destroy(hdr);
                return NULL ;
            }
            /* Append card to linked-list */
            qfits_header_append(hdr, key, val, com, NULL);
        }
        ind += 80 ;
    }

    /* The last key should be 'END' */
    if (strlen(key)!=3) {
        qfits_header_destroy(hdr);
        return NULL ;
    } 
    if (key[0]!='E' || key[1]!='N' || key[2]!='D') {
        qfits_header_destroy(hdr);
        return NULL ;
    }
    
    return hdr ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Read an extension header from a FITS file.
  @param    filename    Name of the FITS file to read
  @param    xtnum       Extension number to read, starting from 0.
  @return   Newly allocated qfits_header structure.

  Strictly similar to qfits_header_read() but reads headers from
  extensions instead. If the requested xtension is 0, this function
  returns the main header.

  Returns NULL in case of error.
 */
/*----------------------------------------------------------------------------*/
qfits_header * qfits_header_readext(const char * filename, int xtnum)
{
    qfits_header*   hdr ;
    int             n_ext ;
    char            line[81];
    char        *   where ;
    char        *   start ;
    char        *   key,
                *   val,
                *   com ;
    int             seg_start ;
    int             seg_size ;
    size_t          size ;

    /* Check input */
    if (filename==NULL || xtnum<0) {
		qfits_error("null string or invalid ext num.");
        return NULL ;
	}

    /* Check that there are enough extensions */
    if (xtnum>0) {
        n_ext = qfits_query_n_ext(filename);
        if (xtnum>n_ext) {
			qfits_error("invalid ext num: %i > %i.", xtnum, n_ext);
            return NULL ;
        }
    }

    /* Get offset to the extension header */
    if (qfits_get_hdrinfo(filename, xtnum, &seg_start, &seg_size)!=0) {
		qfits_error("qfits_get_hdrinfo failed.");
        return NULL ;
    }

    /* Memory-map the input file */
    start = falloc(filename, seg_start, &size) ;
    if (start==NULL) {
		qfits_error("mmapping input file failed.");
		return NULL ;
	}

    hdr   = qfits_header_new() ;
    where = start ;
    while (1) {
        memcpy(line, where, 80);
        line[80] = (char)0;

        /* Rule out blank lines */
        if (!is_blank_line(line)) {

            /* Get key, value, comment for the current line */
            key = qfits_getkey(line);
            val = qfits_getvalue(line);
            com = qfits_getcomment(line);

            /* If key or value cannot be found, trigger an error */
            if (key==NULL) {
                qfits_header_destroy(hdr);
                hdr = NULL ;
                break ;
            }
            /* Append card to linked-list */
            qfits_header_append(hdr, key, val, com, line);
            /* Check for END keyword */
            if (strlen(key)==3)
                if (key[0]=='E' &&
                    key[1]=='N' &&
                    key[2]=='D')
                    break ;
        }
        where += 80 ;
        /* If reaching the end of file, trigger an error */
        if ((int)(where-start)>=(int)(seg_size+80)) {
            qfits_header_destroy(hdr);
            hdr = NULL ;
            break ;
        }
    }
    fdealloc(start, seg_start, size) ;
    return hdr ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Pad an existing file with zeros to a multiple of 2880.
  @param    filename    Name of the file to pad.
  @return   void

  This function simply pads an existing file on disk with enough zeros
  for the file size to reach a multiple of 2880, as required by FITS.
 */
/*----------------------------------------------------------------------------*/
void qfits_zeropad(char * filename)
{
    struct stat sta ;
    int         size ;
    int         remaining;
    FILE    *   out ;
    char    *   buf;

    if (filename==NULL) return ;

    /* Get file size in bytes */
    if (stat(filename, &sta)!=0) {
        return ;
    }
    size = (int)sta.st_size ;
    /* Compute number of zeros to pad */
    remaining = size % FITS_BLOCK_SIZE ;
    if (remaining==0) return ;
    remaining = FITS_BLOCK_SIZE - remaining ;

    /* Open file, dump zeros, exit */
    if ((out=fopen(filename, "a"))==NULL)
        return ;
    buf = calloc(remaining, sizeof(char));
    fwrite(buf, 1, remaining, out);
    fclose(out);
    free(buf);
    return ;
}

/*----------------------------------------------------------------------------*/
/**
  @brief    Identify if a file is a FITS file.
  @param    filename name of the file to check
  @return   int 0, 1, or -1

  Returns 1 if the file name looks like a valid FITS file. Returns
  0 else. If the file does not exist, returns -1.
 */
/*----------------------------------------------------------------------------*/
int is_fits_file(const char *filename)
{
    FILE  *   fp ;
    char  *   magic ;
    int       isfits ;

    if (filename==NULL) return -1 ;
    if ((fp = fopen(filename, "r"))==NULL) {
        qfits_error("cannot open file [%s]: %s", filename, strerror(errno)) ;
        return -1 ;
    }

    magic = calloc(FITS_MAGIC_SZ+1, sizeof(char)) ;
    fread(magic, 1, FITS_MAGIC_SZ, fp) ;
    fclose(fp) ;
    magic[FITS_MAGIC_SZ] = (char)0 ;
    if (strstr(magic, FITS_MAGIC)!=NULL)
        isfits = 1 ;
    else
        isfits = 0 ;
    free(magic) ;
    return isfits ;
}

/* vim: set ts=4 et sw=4 tw=75 */
