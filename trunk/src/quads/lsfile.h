#ifndef LS_FILE_H
#define LS_FILE_H

#include <stdio.h>
#include "blocklist.h"

/**
   __ls files look like this:

   NumFields=20
   # comment
   3,3.7,5.6,4.3,1,3,7
   2,7,8,9,0

   ie, for each field, the number of objects is given,
   followed by the positions of the objects.

   Returns a blocklist containing a blocklist* for
   each field.  Each of these blocklists is a list of
   doubles of the positions of the objects.

   The "dimension" argument says how many elements to read
   per field, ie, what the dimensionality of the position is.
*/
blocklist* read_ls_file(FILE* fid, int dimension);

/**
   Frees the list returned by "read_ls_file".
*/
void ls_file_free(blocklist* l);


/**
   Returns the number of fields in this file, or -1 if
   reading fails.
*/
int read_ls_file_header(FILE* fid);

/**
   Reads one field from the file.

   The returned blocklist* contains (dimension * numpoints)
   doubles.
*/
blocklist* read_ls_file_field(FILE* fid, int dimension);

#endif
