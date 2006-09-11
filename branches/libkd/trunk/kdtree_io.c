#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include <sys/mman.h>

#include "kdtree_io.h"
#include "ioutils.h"

kdtree_t* kdtree_read_file(char* fn) {
	FILE* fid;
	kdtree_t* kdtree;
	fid = fopen(fn, "rb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to read kdtree: %s\n",
				fn, strerror(errno));
		return NULL;
	}
	kdtree = kdtree_read(fid);
	fclose(fid);
	return kdtree;
}

int kdtree_write_file(kdtree_t* kdtree, char* fn) {
	FILE* fid;
    int res;
	fid = fopen(fn, "wb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to write kdtree: %s\n",
				fn, strerror(errno));
		return -1;
	}
	res = kdtree_write(fid, kdtree);
    if (res) {
        fprintf(stderr, "Couldn't write kdtree.\n");
    }
    if (fclose(fid)) {
        fprintf(stderr, "Couldn't close file %s after writing kdtree: %s\n",
                fn, strerror(errno));
        return -1;
    }
    return res;
}

int write_real(FILE* fout, real val) {
	if (sizeof(real) == sizeof(double)) {
		return write_double(fout, (double)val);
	} else {
		return write_float(fout, (float)val);
	}
}

int write_reals(FILE* fout, real* val, int nelems) {
    if (fwrite(val, sizeof(real), nelems, fout) == nelems) {
		return 0;
    } else {
		fprintf(stderr, "Couldn't write reals: %s\n", strerror(errno));
		return 1;
    }
}

/*
  int read_reals(FILE* fin, real* val, int n, int realsize) {
  int i;
  if (realsize == sizeof(real)) {
  if (fread(val, sizeof(real), n, fin) == n) {
  return 0;
  } else {
  fprintf(stderr, "Couldn't read reals: %s\n", strerror(errno));
  return 1;
  }
  } else if (realsize == sizeof(float)) {
  float* f = (float*)malloc(sizeof(float) * n);
  if (!f) {
  fprintf(stderr, "Couldn't read floats: couldn't allocate temp array (size %i)\n", n);
  return 1;
  }
  if (fread(f, sizeof(float), n, fin) == n) {
  for (i=0; i<n; i++) {
  val[i] = (real)f[i];
  }
  free(f);
  return 0;
  } else {
  fprintf(stderr, "Couldn't read floats: %s\n", strerror(errno));
  free(f);
  return 1;
  }
  } else if (realsize == sizeof(double)) {
  double* f = (double*)malloc(sizeof(double) * n);
  if (!f) {
  fprintf(stderr, "Couldn't read doubles: couldn't allocate temp array (size %i)\n", n);
  return 1;
  }
  if (fread(f, sizeof(double), n, fin) == n) {
  for (i=0; i<n; i++) {
  val[i] = (real)f[i];
  }
  free(f);
  return 0;
  } else {
  fprintf(stderr, "Couldn't read doubles: %s\n", strerror(errno));
  free(f);
  return 1;
  }
  } else {
  fprintf(stderr, "Couldn't read real: size of real in file is %i, "
  "but sizeof local float/double is %i/%i.\n", realsize,
  sizeof(float), sizeof(double));
  return 1;
  }
  }

  int read_real(FILE* fin, real* val, int realsize) {
  if (realsize == sizeof(real)) {
  if (fread(val, sizeof(real), 1, fin) == 1) {
  return 0;
  } else {
  fprintf(stderr, "Couldn't read real: %s\n", strerror(errno));
  return 1;
  }
  } else if (realsize == sizeof(float)) {
  float f;
  if (fread(&f, sizeof(float), 1, fin) == 1) {
  *val = (real)f;
  return 0;
  } else {
  fprintf(stderr, "Couldn't read float: %s\n", strerror(errno));
  return 1;
  }
  } else if (realsize == sizeof(double)) {
  double d;
  if (fread(&d, sizeof(double), 1, fin) == 1) {
  *val = (real)d;
  return 0;
  } else {
  fprintf(stderr, "Couldn't read real: %s\n", strerror(errno));
  return 1;
  }
  } else {
  fprintf(stderr, "Couldn't read real: size of real in file is %i, "
  "but sizeof local float/double is %i/%i.\n", realsize,
  sizeof(float), sizeof(double));
  return 1;
  }
  }
*/
/*
  int write_kdtree_node(FILE* fout, kdtree_node_t* node, int D) {
  real* bbox;
  bbox = (real*)((char*)node + sizeof(kdtree_node_t));
  if (write_u32(fout, node->dim) ||
  write_u32(fout, node->l) ||
  write_u32(fout, node->r) ||
  write_real(fout, node->pivot) ||
  write_reals(fout, bbox, 2*D)) {
  fprintf(stderr, "Couldn't write kdtree node.");
  return 1;
  }
  return 0;
  }
  int read_kdtree_node(FILE* fin, kdtree_node_t* node, int D, int realsize) {
  real* bbox;
  bbox = (real*)((char*)node + sizeof(kdtree_node_t));
  if (read_u32(fin, &(node->dim)) ||
  read_u32(fin, &(node->l)) ||
  read_u32(fin, &(node->r)) ||
  read_real(fin, &(node->pivot), realsize) ||
  read_reals(fin, bbox, 2*D, realsize)) {
  fprintf(stderr, "Couldn't read kdtree node: %s.\n", strerror(errno));
  return 1;
  }
  return 0;
  }
  int kdtree_portable_write(FILE* fout, kdtree_t* kdtree) {
  int nelems;
  int nodesize;
  int i;
  if (write_u8(fout, 1) ||
  write_u8(fout, sizeof(real)) ||
  write_u32_portable(fout, kdtree->ndata) ||
  write_u32_portable(fout, kdtree->ndim) ||
  write_u32_portable(fout, kdtree->nnodes))
  return 1;
  if (write_u32s_portable(fout, kdtree->perm, kdtree->ndata)) {
  fprintf(stderr, "Couldn't write kdtree permutation vector.\n");
  return 1;
  }
  nelems = kdtree->ndata * kdtree->ndim;
  if (write_reals(fout, kdtree->data, nelems)) {
  fprintf(stderr, "Couldn't write kdtree data.\n");
  return 1;
  }
  nodesize = sizeof(kdtree_node_t) + sizeof(real) * kdtree->ndim * 2;
  for (i=0; i<kdtree->nnodes; i++) {
  kdtree_node_t* node = (kdtree_node_t*)(((char*)kdtree->tree) + (i * nodesize));
  if (write_kdtree_node(fout, node, kdtree->ndim)) {
  fprintf(stderr, "Couldn't write kdtree node %i.\n", i);
  return 1;
  }
  }
  return 0;
  }
*/
/*
  kdtree_t* kdtree_portable_read(FILE* fin, int use_mmap,
  void** mmapped, int* mmapped_size) {
  unsigned char ver, rs;
  unsigned int N, D, nnodes;
  int realsize;
  kdtree_t* tree;
  int nodesize;
  int i;
  int err = 0;

  if (!err) err |= read_u8(fin, &ver);
  if (!err) err |= read_u8(fin, &rs);
  if (!err) err |= read_u32_portable(fin, &N);
  if (!err) err |= read_u32_portable(fin, &D);
  if (!err) err |= read_u32_portable(fin, &nnodes);
  if (err) {
  fprintf(stderr, "Couldn't read header.\n");
  return NULL;
  }
  if (ver != 1) {
  fprintf(stderr, "Version: %i isn't 1.\n", ver);
  return NULL;
  }
  realsize = (unsigned int)rs;
  if (realsize != sizeof(real)) {
  fprintf(stderr, "Warning: size of real in file is %i, but locally it's %i.  Will have to convert values.\n",
  realsize, sizeof(real));
  if (use_mmap) {
  fprintf(stderr, "Error: can't mmap: size of real differs.\n");
  return NULL;
  }
  }

  tree = (kdtree_t*)malloc(sizeof(kdtree_t));
  if (!tree) {
  fprintf(stderr, "Couldn't allocate kdtree.\n");
  return NULL;
  }

  tree->ndata = N;
  tree->ndim = D;
  tree->nnodes = nnodes;

  tree->perm = (unsigned int*)malloc(N * sizeof(unsigned int));
  if (!tree->perm) {
  fprintf(stderr, "Couldn't allocate kdtree permutation vector.\n");
  free(tree);
  }
  if (read_u32s(fin, tree->perm, N)) {
  fprintf(stderr, "Couldn't read kdtree permutation vector.\n");
  free(tree->perm);
  free(tree);
  return NULL;
  }

  if (use_mmap) {
  void* map;
  off_t offset = (off_t)ftell(fin);
  size_t size = realsize * N * D;
  map = mmap(0, size + offset, PROT_READ, MAP_SHARED, fileno(fin), 0);
  if (map == MAP_FAILED) {
  fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
  free(tree);
  return NULL;
  }
  tree->mmapped = map;
  if (mmapped_size)
  *mmapped_size = size + offset;

  tree->data = (real*)(map + offset);

  } else {
  tree->data = (real*)malloc(sizeof(real) * N * D);
  if (read_reals(fin, tree->data, N*D, realsize)) {
  fprintf(stderr, "Couldn't read kdtree data.\n");
  free(tree);
  return NULL;
  }
  }

  nodesize = sizeof(kdtree_node_t) + sizeof(real) * D * 2;
  tree->tree = (kdtree_node_t*)malloc(nodesize * nnodes);
  if (!tree->tree) {
  fprintf(stderr, "Couldn't allocate kdtree nodes.\n");
  if (use_mmap) {
  munmap(tree->data, sizeof(real)*N*D);
  } else {
  free(tree->data);
  }
  free(tree);
  return NULL;
  }

  for (i=0; i<nnodes; i++) {
  kdtree_node_t* node = (kdtree_node_t*)(((char*)tree->tree) + (i * nodesize));
  if (read_kdtree_node(fin, node, D, realsize)) {
  fprintf(stderr, "Couldn't read kdtree node %i.\n", i);
  if (use_mmap) {
  munmap(tree->data, sizeof(real)*N*D);
  } else {
  free(tree->data);
  }
  free(tree->tree);
  free(tree);
  return NULL;
  }
  }
  return tree;
  }
*/


int kdtree_write(FILE* fout, kdtree_t* kdtree) {
    int nelems;
    int nodesize;
    int nbytes;
	int err = 0;

    nodesize = sizeof(kdtree_node_t) + sizeof(real) * kdtree->ndim * 2;

	if (!err) err |= write_u8(fout, 2);
	if (!err) err |= write_u8(fout, sizeof(real));
	if (!err) err |= write_u8(fout, sizeof(unsigned int));
	if (!err) err |= write_u8(fout, sizeof(kdtree_node_t));
	if (!err) err |= write_u32(fout, ENDIAN_DETECTOR);
	if (!err) err |= write_u32(fout, nodesize);
	if (!err) err |= write_u32(fout, kdtree->ndata);
	if (!err) err |= write_u32(fout, kdtree->ndim);
	if (!err) err |= write_u32(fout, kdtree->nnodes);
	if (err) {
		fprintf(stderr, "Couldn't write kdtree header.\n");
		return 1;
    }

    if (write_uints(fout, kdtree->perm, kdtree->ndata)) {
		fprintf(stderr, "Couldn't write kdtree permutation vector.\n");
		return 1;
    }

    nelems = kdtree->ndata * kdtree->ndim;
    if (write_reals(fout, kdtree->data, nelems)) {
		fprintf(stderr, "Couldn't write kdtree data.\n");
		return 1;
    }

    nbytes = nodesize * kdtree->nnodes;
    if (fwrite(kdtree->tree, 1, nbytes, fout) != nbytes) {
		fprintf(stderr, "Couldn't write kdtree nodes\n");
		return 1;
    }
    return 0;
}

kdtree_t* kdtree_read(FILE* fin) {
    unsigned char ver, rs, is, kdns;
    unsigned int endian, ns;
    unsigned int N, D, nnodes;
    kdtree_t* tree;
    int nodesize;
	int err = 0;
	char* map;
	int permsize;
	int datasize;
	int treesize;
	off_t offset;
	size_t size;


    if (!err) err |= read_u8(fin, &ver);
	if (!err) err |= read_u8(fin, &rs);
	if (!err) err |= read_u8(fin, &is);
	if (!err) err |= read_u8(fin, &kdns);
	if (!err) err |= read_u32(fin, &endian);
	if (!err) err |= read_u32(fin, &ns);
	if (!err) err |= read_u32(fin, &N);
	if (!err) err |= read_u32(fin, &D);
	if (!err) err |= read_u32(fin, &nnodes);
	if (err) {
		fprintf(stderr, "Couldn't read header.\n");
		return NULL;
    }
    if (ver != 2) {
		fprintf(stderr, "Version: %i isn't 2.\n", ver);
		return NULL;
    }
    if (rs != sizeof(real)) {
		fprintf(stderr, "Size of real doesn't match: %i, %i\n",
				(int)rs, sizeof(real));
		return NULL;
    }
    if (is != sizeof(unsigned int)) {
		fprintf(stderr, "Size of uint doesn't match: %i, %i\n",
				(int)is, sizeof(unsigned int));
		return NULL;
    }
    if (kdns != sizeof(kdtree_node_t)) {
		fprintf(stderr, "Size of kdtree node doesn't match: %i, %i\n",
				(int)kdns, sizeof(kdtree_node_t));
		return NULL;
    }
    if (endian != ENDIAN_DETECTOR) {
		fprintf(stderr, "Endianness doesn't match: 0x%x, 0x%x\n",
				endian, ENDIAN_DETECTOR);
		return NULL;
    }

    nodesize = sizeof(kdtree_node_t) + sizeof(real) * D * 2;

    if (ns != nodesize) {
		fprintf(stderr, "Nodesize of kdtree doesn't match: %i, %i\n",
				ns, nodesize);
		return NULL;
    }

    // all checks complete.

    tree = (kdtree_t*)malloc(sizeof(kdtree_t));
    if (!tree) {
		fprintf(stderr, "Couldn't allocate kdtree.\n");
		return NULL;
    }

    tree->ndata = N;
    tree->ndim = D;
    tree->nnodes = nnodes;

	offset = ftello(fin);

	// size of permutation array.
	permsize = sizeof(unsigned int) * N;
	// size of data block.
	datasize = sizeof(real) * N * D;
	// size of the kdtree nodes.
	treesize = nodesize * nnodes;

	size = permsize + datasize + treesize;

	map = mmap(0, size+offset, PROT_READ, MAP_SHARED,
			   fileno(fin), 0);

	if (map == MAP_FAILED) {
		fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
		free(tree);
		return NULL;
	}
	tree->mmapped = map;
	tree->mmapped_size = size + offset;

	tree->perm = (unsigned int*) (map + offset);
	tree->data = (real*)         (map + offset + permsize);
	tree->tree = (kdtree_node_t*)(map + offset + permsize + datasize);

    return tree;
}


void kdtree_close(kdtree_t* kd) {
	if (!kd) return;
	munmap(kd->mmapped, kd->mmapped_size);
	free(kd);
}
