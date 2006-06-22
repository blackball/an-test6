#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
//#include <math.h>
//#include <assert.h>
#include <libgen.h>

#include "qidxfile.h"
#include "fileutil.h"

#define OPTIONS "h"

extern char *optarg;
extern int optind, opterr, optopt;

void print_help(char* progname)
{
	fprintf(stderr, "Usage: %s\n"
			"   [-h]: help\n"
			"   <base-name> [<base-name> ...]\n\n",
	        progname);
}

static inline void ensure_hist_size(uint** hist, uint* size, uint newsize) {
	if (newsize <= *size)
		return;
	*hist = realloc(*hist, newsize*sizeof(uint));
	memset((*hist) + (*size), 0, (newsize - *size) * sizeof(uint));
	*size = newsize;
}

int main(int argc, char** args) {
    int argchar;
	char* basefn;
	qidxfile* qf;
	uint* sumhist = NULL;
	uint Nsumhist = 0;
	uint i;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'h':
			print_help(args[0]);
			exit(0);
		}

	if (optind == argc) {
		print_help(args[0]);
		exit(-1);
	}

	for (; optind<argc; optind++) {
		uint* quads;
		uint nquads;
		uint* hist = NULL;
		uint Nhist = 0;
		char* fn;

		basefn = args[optind];
		fn = mk_qidxfn(basefn);
		fprintf(stderr, "Reading qidx from %s...\n", fn);
		fflush(stderr);
		qf = qidxfile_open(fn, 0);
		if (!qf) {
			fprintf(stderr, "Couldn't read qidx from %s.\n", fn);
			exit(-1);
		}

		fprintf(stderr, "Reading %i stars from %s...\n", qf->numstars, fn);
		fflush(stderr);
		free_fn(fn);

		for (i=0; i<qf->numstars; i++) {
			qidxfile_get_quads(qf, i, &quads, &nquads);
			ensure_hist_size(&hist, &Nhist, nquads+1);
			hist[nquads]++;
		}

		qidxfile_close(qf);

		printf("%s = [ ", basename(basefn));
		for (i=0; i<Nhist; i++)
			printf("%i, ", hist[i]);
		printf("];\n");

		ensure_hist_size(&sumhist, &Nsumhist, Nhist);
		for (i=0; i<Nhist; i++)
			sumhist[i] += hist[i];

		free(hist);
	}

	printf("sum = [ ");
	for (i=0; i<Nsumhist; i++)
		printf("%i, ", sumhist[i]);
	printf("];\n");

	return 0;
}
