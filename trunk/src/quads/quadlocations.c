#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "quadfile.h"
#include "catalog.h"
#include "fileutil.h"
#include "starutil.h"
#include "kdtree_io.h"
#include "kdtree_fits_io.h"

#define OPTIONS "hn:o:"

extern char *optarg;
extern int optind, opterr, optopt;

void print_help(char* progname)
{
	fprintf(stderr, "Usage: %s\n"
			"   -o <output-file-name>\n"
			"   [-n <image-size>]  (default 3000)\n"
			"   [-h]: help\n"
			"   <base-name> [<base-name> ...]\n\n"
			"Requires both objs and quad files.\n\n",
	        progname);
}

int main(int argc, char** args) {
    int argchar;
	int N = 3000;
	char* basename;
	char* outfn = NULL;
	char* fn;
	quadfile* qf;
	catalog* cat = NULL;
	kdtree_t* skdt = NULL;
	int* invperm = NULL;
	uchar* img;
	uint* counts;
	int i;
	int maxval;
	FILE* fid;
	unsigned char* starcounts;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'o':
			outfn = optarg;
			break;
        case 'n':
            N = atoi(optarg);
            break;
		case 'h':
			print_help(args[0]);
			exit(0);
		}

	if (!outfn || !N || (optind == argc)) {
		print_help(args[0]);
		exit(-1);
	}

	fid = fopen(outfn, "wb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to write image: %s\n", outfn, strerror(errno));
		exit(-1);
	}

	counts = calloc(sizeof(uint), N*N);
	if (!counts) {
		fprintf(stderr, "Couldn't allocate %ix%i image.\n", N, N);
		exit(-1);
	}

	for (; optind<argc; optind++) {
		int Nstars;
		basename = args[optind];
		printf("Reading files with basename %s\n", basename);

		fn = mk_quadfn(basename);
		qf = quadfile_open(fn, 0);
		if (!qf) {
			fprintf(stderr, "Failed to open quad file %s.\n", fn);
			continue;
		}
		free_fn(fn);

		fn = mk_catfn(basename);
		cat = catalog_open(fn, 0);
		free_fn(fn);
		if (cat) {
			Nstars = cat->numstars;
		} else {
			fn = mk_streefn(basename);
			skdt = kdtree_fits_read_file(fn);
			if (!skdt) {
				fprintf(stderr, "Failed to read star kdtree %s.\n", fn);
				continue;
			}
			if (skdt->perm) {
				invperm = realloc(invperm, skdt->ndata * sizeof(int));
				kdtree_inverse_permutation(skdt, invperm);
			}
			Nstars = skdt->ndata;
		}

		printf("Counting stars in quads...\n");
		starcounts = calloc(sizeof(unsigned char), Nstars);
		for (i=0; i<qf->numquads; i++) {
			uint stars[4];
			int j;
			if (!(i % 200000)) {
				printf(".");
				fflush(stdout);
			}
			quadfile_get_starids(qf, i, stars, stars+1,
								 stars+2, stars+3);
			for (j=0; j<4; j++) {
				assert(stars[j] < Nstars);
				assert(starcounts[stars[j]] < 255);
				starcounts[stars[j]]++;
			}
		}
		printf("\n");


		printf("Computing image...\n");
		for (i=0; i<Nstars; i++) {
			double* xyz;
			double px, py;
			int X, Y;
			if (!(i % 100000)) {
				printf(".");
				fflush(stdout);
			}
			if (!starcounts[i])
				continue;
			if (cat)
				xyz = catalog_get_star(cat, i);
			else
				xyz = skdt->data + skdt->ndim * (invperm ? invperm[i] : i);
			project_hammer_aitoff_x(xyz[0], xyz[1], xyz[2], &px, &py);
			px = 0.5 + (px - 0.5) * 0.99;
			py = 0.5 + (py - 0.5) * 0.99;
			X = (int)rint(px * N);
			Y = (int)rint(py * N);
			counts[Y*N + X] += starcounts[i];
		}
		printf("\n");

		if (cat)
			catalog_close(cat);
		if (skdt)
			kdtree_close(skdt);

		quadfile_close(qf);

		free(starcounts);
	}

	maxval = 0;
	for (i=0; i<(N*N); i++)
		if (counts[i] > maxval)
			maxval = counts[i];
	printf("maxval is %i.\n", maxval);

	img = calloc(1, N*N);
	if (!img) {
		fprintf(stderr, "Couldn't allocate %ix%i image.\n", N, N);
		exit(-1);
	}

	for (i=0; i<(N*N); i++)
		img[i] = (int)rint((255.0 * (double)counts[i]) / (double)maxval);

	fprintf(fid, "P5 %d %d %d\n",N,N, 255);
	if (fwrite(img, 1, N*N, fid) != (N*N)) {
		fprintf(stderr, "Failed to write image file: %s\n", strerror(errno));
		exit(-1);
	}
	fclose(fid);

	free(img);
	free(counts);

	return 0;
}
