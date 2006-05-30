#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "quadfile.h"
#include "catalog.h"
#include "fileutil.h"
#include "starutil.h"

#define OPTIONS "hf:n:o:"

extern char *optarg;
extern int optind, opterr, optopt;

void print_help(char* progname)
{
	fprintf(stderr, "Usage: %s -f <base-name>\n"
			"   -o <output-file-name>\n"
			"   [-n <image-size>]  (default 3000)\n"
			"   [-h]: help\n\n",
	        progname);
}

int main(int argc, char** args) {
    int argchar;
	int N = 3000;
	char* basename = NULL;
	char* outfn = NULL;
	char* fn;
	quadfile* qf;
	catalog* cat;
	uchar* img;
	int nquads, i;
	int maxval;
	FILE* fid;

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
		case 'f':
			basename = optarg;
			break;
		}

	if (!basename || !outfn || !N) {
		print_help(args[0]);
		exit(-1);
	}

	fn = mk_quadfn(basename);
	qf = quadfile_open(fn, 0);
	free_fn(fn);

	fn = mk_catfn(basename);
	cat = catalog_open(fn, 0);
	free_fn(fn);

	fid = fopen(outfn, "wb");
	if (!fid) {
		fprintf(stderr, "Couldn't open file %s to write image: %s\n", outfn, strerror(errno));
		exit(-1);
	}

	img = calloc(1, N*N);
	if (!img) {
		fprintf(stderr, "Couldn't allocate %ix%i image.\n", N, N);
		exit(-1);
	}

	nquads = qf->numquads;
	for (i=0; i<nquads; i++) {
		uint stars[4];
		int j;
		double* xyz;
		quadfile_get_starids(qf, i, stars, stars+1,
							 stars+2, stars+3);
		for (j=0; j<4; j++) {
			double px, py;
			int X, Y;
			xyz = catalog_get_star(cat, stars[j]);
			project_hammer_aitoff_x(xyz[0], xyz[1], xyz[2],
								  &px, &py);
			px = 0.5 + (px - 0.5) * 0.95;
			py = 0.5 + (py - 0.5) * 0.95;
			X = (int)rint(px * N);
			Y = (int)rint(py * N);
			if (img[Y*N + X] < 255)
				img[Y*N + X]++;
		}
	}

	maxval = 0;
	for (i=0; i<(N*N); i++)
		if (img[i] > maxval)
			maxval = img[i];
	for (i=0; i<(N*N); i++)
		img[i] = ((maxval * img[i]) / 255);

	fprintf(fid, "P5 %d %d %d\n",N,N, 255);
	if (fwrite(img, 1, N*N, fid) != (N*N)) {
		fprintf(stderr, "Failed to write image file: %s\n", strerror(errno));
		exit(-1);
	}
	fclose(fid);

	catalog_close(cat);
	quadfile_close(qf);
	free(img);

	return 0;
}
