#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <sys/mman.h>

#include "kdtree_io.h"

unsigned int ENDIAN_DETECTOR = 0x01020304;

int write_u8(FILE* fout, unsigned char val) {
    if (fwrite(&val, 1, 1, fout) == 1) {
	return 0;
    } else {
	fprintf(stderr, "Couldn't write u8: %s\n", strerror(errno));
	return 1;
    }
}

int write_u32(FILE* fout, unsigned int val) {
    uint32_t v = htonl((uint32_t)val);
    if (fwrite(&v, 4, 1, fout) == 1) {
	return 0;
    } else {
	fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
	return 1;
    }
}

int write_u32s(FILE* fout, unsigned int* val, int n) {
    int i;
    uint32_t* v = (uint32_t*)malloc(sizeof(uint32_t) * n);
    if (!v) {
	fprintf(stderr, "Couldn't write u32s: couldn't allocate temp array.\n");
	return 1;
    }
    for (i=0; i<n; i++) {
	v[i] = htonl((uint32_t)val[i]);
    }
    if (fwrite(v, sizeof(uint32_t), n, fout) == n) {
	free(v);
	return 0;
    } else {
	fprintf(stderr, "Couldn't write u32s: %s\n", strerror(errno));
	free(v);
	return 1;
    }
}

int write_u32_native(FILE* fout, unsigned int val) {
    uint32_t v = (uint32_t)val;
    if (fwrite(&v, 4, 1, fout) == 1) {
	return 0;
    } else {
	fprintf(stderr, "Couldn't write u32: %s\n", strerror(errno));
	return 1;
    }
}

int write_uints_native(FILE* fout, unsigned int* val, int n) {
    if (fwrite(val, sizeof(unsigned int), n, fout) == n) {
	return 0;
    } else {
	fprintf(stderr, "Couldn't write uints: %s\n", strerror(errno));
	return 1;
    }
}

int write_real(FILE* fout, real val) {
    if (fwrite(&val, sizeof(real), 1, fout) == 1) {
	return 0;
    } else {
	fprintf(stderr, "Couldn't write real: %s\n", strerror(errno));
	return 1;
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

int read_u8(FILE* fin, unsigned char* val) {
    if (fread(val, 1, 1, fin) == 1) {
	return 0;
    } else {
	fprintf(stderr, "Couldn't read u8: %s\n", strerror(errno));
	return 1;
    }
}

int read_u32(FILE* fin, unsigned int* val) {
    uint32_t u;
    if (fread(&u, 4, 1, fin) == 1) {
	*val = ntohl(u);
	return 0;
    } else {
	fprintf(stderr, "Couldn't read u32: %s\n", strerror(errno));
	return 1;
    }
}

int read_u32_native(FILE* fin, unsigned int* val) {
    uint32_t u;
    if (fread(&u, 4, 1, fin) == 1) {
	*val = (unsigned int)u;
	return 0;
    } else {
	fprintf(stderr, "Couldn't read u32 native: %s\n", strerror(errno));
	return 1;
    }
}

int read_u32s(FILE* fin, unsigned int* val, int n) {
    int i;
    uint32_t* u = (uint32_t*)malloc(sizeof(uint32_t) * n);
    if (!u) {
	fprintf(stderr, "Couldn't real uint32s: couldn't allocate temp array.\n");
	return 1;
    }
    if (fread(u, sizeof(uint32_t), n, fin) == n) {
	for (i=0; i<n; i++) {
	    val[i] = ntohl(u[i]);
	}
	free(u);
	return 0;
    } else {
	fprintf(stderr, "Couldn't read u32: %s\n", strerror(errno));
	free(u);
	return 1;
    }
}

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
	write_u32(fout, kdtree->ndata) ||
	write_u32(fout, kdtree->ndim) ||
	write_u32(fout, kdtree->nnodes))
	return 1;

    /* (nope)
      u8    sizeof(kdtree_node_t)
      u8    offset of kdtree_node_t.dim
      u8    offset of kdtree_node_t.l
      u8    offset of kdtree_node_t.r
      u8    offset of kdtree_node_t.pivot
      write_u8(sizeof(kdtree_node_t)) ||
      write_u8(offsetof(kdtree_node_t, dim)) ||
      write_u8(offsetof(kdtree_node_t, l)) ||
      write_u8(offsetof(kdtree_node_t, r)) ||
      write_u8(offsetof(kdtree_node_t, pivot)))
    */

    if (write_u32s(fout, kdtree->perm, kdtree->ndata)) {
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

kdtree_t* kdtree_portable_read(FILE* fin, int use_mmap,
			       void** mmapped, int* mmapped_size) {
    unsigned char ver, rs;
    unsigned int N, D, nnodes;
    int realsize;
    kdtree_t* tree;
    int nodesize;
    int i;

    if (read_u8(fin, &ver) ||
	read_u8(fin, &rs) ||
	read_u32(fin, &N) ||
	read_u32(fin, &D) ||
	read_u32(fin, &nnodes)) {
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
	/*
	  map = mmap(0, size, PROT_READ, MAP_SHARED,
	  fileno(fin), offset);
	*/
	map = mmap(0, size + offset, PROT_READ, MAP_SHARED,
		   fileno(fin), 0);
	if (map == MAP_FAILED) {
	    fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
	    free(tree);
	    return NULL;
	}
	if (mmapped)
	    *mmapped = map;
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

    // check if we can use the kdtree nodes in-place.
    // (would have to be big-endian...)
    //if (offsetof(kdtree_node_t, 

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



int kdtree_write(FILE* fout, kdtree_t* kdtree) {
    int nelems;
    int nodesize;
    int nbytes;

    nodesize = sizeof(kdtree_node_t) + sizeof(real) * kdtree->ndim * 2;

    if (write_u8(fout, 2) ||
	write_u8(fout, sizeof(real)) ||
	write_u8(fout, sizeof(unsigned int)) ||
	write_u8(fout, sizeof(kdtree_node_t)) ||
	write_u32_native(fout, ENDIAN_DETECTOR) ||
	write_u32_native(fout, nodesize) ||
	write_u32_native(fout, kdtree->ndata) ||
	write_u32_native(fout, kdtree->ndim) ||
	write_u32_native(fout, kdtree->nnodes)) {
	fprintf(stderr, "Couldn't write kdtree header.\n");
	return 1;
    }

    if (write_uints_native(fout, kdtree->perm, kdtree->ndata)) {
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

kdtree_t* kdtree_read(FILE* fin, int use_mmap,
		      void** mmapped, int* mmapped_size) {
    unsigned char ver, rs, is, kdns;
    unsigned int endian, ns;
    unsigned int N, D, nnodes;
    kdtree_t* tree;
    int nodesize;

    if (read_u8(fin, &ver) ||
	read_u8(fin, &rs) ||
	read_u8(fin, &is) ||
	read_u8(fin, &kdns) ||
	read_u32_native(fin, &endian) ||
	read_u32_native(fin, &ns) ||
	read_u32_native(fin, &N) ||
	read_u32_native(fin, &D) ||
	read_u32_native(fin, &nnodes)) {
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

    if (use_mmap) {
	void* map;
	int permsize;
	int datasize;
	int treesize;
	off_t offset;
	size_t size;

	offset = (off_t)ftell(fin);

	permsize = sizeof(unsigned int) * N;
	datasize = sizeof(real) * N * D;
	treesize = nodesize * nnodes;

	size = permsize + datasize + treesize;

	map = mmap(0, size+offset, PROT_READ, MAP_SHARED,
		   fileno(fin), 0);

	if (map == MAP_FAILED) {
	    fprintf(stderr, "Couldn't mmap file: %s\n", strerror(errno));
	    free(tree);
	    return NULL;
	}
	if (mmapped)
	    *mmapped = map;
	if (mmapped_size)
	    *mmapped_size = size + offset;

	tree->perm = (unsigned int*) (map + offset);
	tree->data = (real*)         (map + offset + permsize);
	tree->tree = (kdtree_node_t*)(map + offset + permsize + datasize);

    } else {

	tree->perm = (unsigned int*) malloc(sizeof(unsigned int) * N);
	tree->data = (real*)         malloc(sizeof(real) * N * D);
	tree->tree = (kdtree_node_t*)malloc(nodesize * nnodes);

	if (!tree->perm || !tree->data || !tree->tree) {
	    fprintf(stderr, "Couldn't allocate memory for kdtree structures.\n");
	    free(tree->perm);
	    free(tree->data);
	    free(tree->tree);
	    free(tree);
	    return NULL;
	}

	if ((fread(tree->perm, sizeof(unsigned int), N, fin) != N) ||
	    (fread(tree->data, sizeof(real), N*D, fin) != (N*D)) ||
	    (fread(tree->tree, nodesize, nnodes, fin) != nnodes)) {
	    fprintf(stderr, "Couldn't read kdtree structures.\n");
	    free(tree->perm);
	    free(tree->data);
	    free(tree->tree);
	    free(tree);
	    return NULL;
	}
    }
    return tree;
}


