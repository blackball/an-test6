#ifndef KDTREE_IO_H
#define KDTREE_IO_H

#include "kdtree.h"

/**
   Write a Keir-style kdtree to a file.

   The file format is:

   u8    version = 1
   u8    sizeof(real)
   u32   ndata              (number of data items)
   u32   dim                (dimensionality of data items)
   u32   nnodes             (number of nodes in the tree)
   [ndata] u32              (perm)
   [ndata * dim] real       (data)
   [nnodes] kdtree_nodes    (tree)

   Note that "u32" values are in network byte order
   (ie, big-endian).

   If "use_mmap" is set, and "mmapped" is non-null, the location
   at which memory was mapped will be placed in "*mmapped"; the
   size mmaped will be placed in "*mmapped_size".  It's
   the caller's responsibility to munmap it when finished.

   Returns:
   0 if all is quiet on the Western front
   1 otherwise

   Original author: dstn
 */
int kdtree_portable_write(FILE* fout, kdtree_t* kdtree);

kdtree_t* kdtree_portable_read(FILE* fin, int use_mmap,
			       void** mmapped, int* mmapped_size);


/*
   The file format is:

   u8    version = 2
   u8    sizeof(real)
   u8    sizeof(unsigned int)
   u8    sizeof(kdtree_node_t)
   u32   endianness detector: an int with value 0x01020304.
   u32   nodesize
   u32   ndata              (number of data items)
   u32   dim                (dimensionality of data items)
   u32   nnodes             (number of nodes in the tree)
   [ndata] ints             (perm)
   [ndata * dim] reals      (data)
   [nnodes] kdtree_nodes    (tree)
   Everything is in local enddianness.  If sizeof(real) doesn't
   match we just bail out.

   Returns:
   0 if all is quiet on the Western front
   1 otherwise

   Original author: dstn
*/
int kdtree_write(FILE* fout, kdtree_t* kdtree);

kdtree_t* kdtree_read(FILE* fin, int use_mmap,
		      void** mmapped, int* mmapped_size);

#endif
