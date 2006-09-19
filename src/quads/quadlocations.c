#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "quadfile.h"
#include "catalog.h"
#include "kdtree.h"
#include "fileutil.h"
#include "starutil.h"
#include "bl.h"
#include "starkd.h"
#include "boilerplate.h"

#define OPTIONS "hn:o:"

extern char *optarg;
extern int optind, opterr, optopt;

void print_help(char* progname)
{
	boilerplate_help_header(stderr);
	fprintf(stderr, "Usage: %s\n"
			"   -o <output-image-file-name>\n"
			"   [-n <image-size>]  (default 3000)\n"
			"   [-h]: help\n"
			"   <base-name> [<base-name> ...]\n\n"
			"Requires both (objs or skdt) and quad files.  Writes a PGM image.\n"
			"If you specify multiple -o and -n options, multiple image files will be written.\n\n",
	        progname);
}

int main(int argc, char** args) {
    int argchar;
	const int Ndefault = 3000;
	int N = Ndefault;
	char* basename;
	char* outfn = NULL;
	char* fn;
	quadfile* qf;
	catalog* cat = NULL;
	startree* skdt = NULL;
	uchar* img = NULL;
	int i;
	int maxval;
	unsigned char* starcounts;
	il* imgsizes;
	pl* outnames;
	uint** counts;
	int Nimgs;

	imgsizes = il_new(8);
	outnames = pl_new(8);

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'o':
			outfn = optarg;
			pl_append(outnames, strdup(optarg));
			break;
        case 'n':
            N = atoi(optarg);
			il_append(imgsizes, N);
            break;
		case 'h':
			print_help(args[0]);
			exit(0);
		}

	if (!outfn || !N || (optind == argc)) {
		print_help(args[0]);
		exit(-1);
	}

	for (i=il_size(imgsizes); i<pl_size(outnames); i++)
		il_append(imgsizes, Ndefault);

	Nimgs = pl_size(outnames);

	counts = malloc(Nimgs * sizeof(uint*));
	for (i=0; i<Nimgs; i++) {
		int n = il_get(imgsizes, i);
		counts[i] = calloc(n*n, sizeof(int));
		if (!counts) {
			fprintf(stderr, "Couldn't allocate %ix%i image.\n", N, N);
			exit(-1);
		}
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
		printf("Trying to open catalog file %s...\n", fn);
		cat = catalog_open(fn, 0);
		free_fn(fn);
		if (cat) {
			Nstars = cat->numstars;
		} else {
			fn = mk_streefn(basename);
			printf("Trying to open star kdtree %s instead...\n", fn);
			skdt = startree_open(fn);
			if (!skdt) {
				fprintf(stderr, "Failed to read star kdtree %s.\n", fn);
				continue;
			}
			Nstars = startree_N(skdt);
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
			double starpos[3];
			double px, py;
			int X, Y;
			int j;
			if (!(i % 100000)) {
				printf(".");
				fflush(stdout);
			}
			if (!starcounts[i])
				continue;
			if (cat)
				xyz = catalog_get_star(cat, i);
			else {
				startree_get(skdt, i, starpos);
				xyz = starpos;
			}
			project_hammer_aitoff_x(xyz[0], xyz[1], xyz[2], &px, &py);
			px = 0.5 + (px - 0.5) * 0.99;
			py = 0.5 + (py - 0.5) * 0.99;
			for (j=0; j<Nimgs; j++) {
				N = il_get(imgsizes, j);
				X = (int)rint(px * N);
				Y = (int)rint(py * N);
				(counts[j])[Y*N + X] += starcounts[i];
			}
		}
		printf("\n");

		if (cat)
			catalog_close(cat);
		if (skdt)
			startree_close(skdt);

		quadfile_close(qf);

		free(starcounts);
	}

	for (i=0; i<Nimgs; i++) {
		FILE* fid;
		int j;
		outfn = pl_get(outnames, i);
		N = il_get(imgsizes, i);

		maxval = 0;
		for (j=0; j<(N*N); j++)
			if (counts[i][j] > maxval)
				maxval = counts[i][j];
		printf("maxval is %i.\n", maxval);

		fid = fopen(outfn, "wb");
		if (!fid) {
			fprintf(stderr, "Couldn't open file %s to write image: %s\n", outfn, strerror(errno));
			exit(-1);
		}
		img = realloc(img, N*N);
		if (!img) {
			fprintf(stderr, "Couldn't allocate %ix%i image.\n", N, N);
			exit(-1);
		}
		for (j=0; j<(N*N); j++)
			img[j] = (int)rint((255.0 * (double)counts[i][j]) / (double)maxval);

		fprintf(fid, "P5 %d %d %d\n",N,N, 255);
		if (fwrite(img, 1, N*N, fid) != (N*N)) {
			fprintf(stderr, "Failed to write image file: %s\n", strerror(errno));
			exit(-1);
		}
		fclose(fid);
		free(outfn);
	}

	free(img);
	free(counts);

	return 0;
}
