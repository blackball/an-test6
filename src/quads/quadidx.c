/**
   Quadidx: create .qidx files from .quad files.

   A .quad file lists, for each quad, the stars comprising the quad.
   A .qidx file lists, for each star, the quads that star is a member of.

   Input: .quad
   Output: .qidx
 */

#include <errno.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "quadfile.h"
#include "bl.h"

#define OPTIONS "hf:F"
const char HelpString[] =
"quadidx -f fname\n"
"    [-F]   read traditional (non-FITS) format input.";

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
	int argidx, argchar;
	char *idxfname = NULL;
	char *quadfname = NULL;
	il** quadlist;
	quadfile* quads;
	FILE *idxfid = NULL;
	uint q;
	int i;
	uint numused;
	magicval magic = MAGIC_VAL;
    int fits = 1;
	
	if (argc <= 2) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
		fprintf(stderr, "argc=%d\n", argc);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
        case 'F':
            fits = 0;
            break;
		case 'f':
			idxfname = mk_qidxfn(optarg);
			quadfname = mk_quadfn(optarg);
			break;
		case '?':
			fprintf(stderr, "Unknown option `-%c'.\n", optopt);
		case 'h':
			fprintf(stderr, HelpString);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}

	if (optind < argc) {
		for (argidx = optind; argidx < argc; argidx++)
			fprintf (stderr, "Non-option argument %s\n", argv[argidx]);
		fprintf(stderr, HelpString);
		return (OPT_ERR);
	}

	fprintf(stderr, "quadidx: indexing quads in %s...\n",
	        quadfname);

	quads = quadfile_open(quadfname, fits, 0);
	if (!quads) {
		fprintf(stderr, "Couldn't open quads file %s.\n", quadfname);
		exit(-1);
	}

	fprintf(stderr, "%u quads, %u stars.\n", quads->numquads, quads->numstars);

	fopenout(idxfname, idxfid);
	free_fn(idxfname);

	quadlist = malloc(quads->numstars * sizeof(il*));
	memset(quadlist, 0, quads->numstars * sizeof(il*));

	for (q=0; q<quads->numquads; q++) {
		uint inds[4];

		quadfile_get_starids(quads, q, inds+0, inds+1, inds+2, inds+3);

		// append this quad index to the lists of each of
		// its four stars.
		for (i=0; i<4; i++) {
			il* list;
			uint starind = inds[i];
			list = quadlist[starind];
			// create the list if necessary
			if (!list) {
				list = il_new(10);
				quadlist[starind] = list;
			}
			il_append(list, q);
		}
	}
	
	// first count numused:
	// how many stars are members of quads.
	numused = 0;
	for (i=0; i<quads->numstars; i++) {
		il* list = quadlist[i];
		if (!list) continue;
		numused++;
	}
	printf("%u stars used\n", numused);

	if (fwrite(&magic, sizeof(magic), 1, idxfid) != 1) {
		printf("Error writing magic: %s\n", strerror(errno));
		exit(-1);
	}
	if (fwrite(&numused, sizeof(numused), 1, idxfid) != 1) {
		printf("Error writing numused: %s\n", strerror(errno));
		exit(-1);
	}
	for (i=0; i<quads->numstars; i++) {
		int j;
		uint thisnumq;
		uint thisstar;
		il* list = quadlist[i];
		if (!list) continue;
		thisnumq = (uint)il_size(list);
		thisstar = i;
		if ((fwrite(&thisstar, sizeof(thisstar), 1, idxfid) != 1) ||
			(fwrite(&thisnumq, sizeof(thisnumq), 1, idxfid) != 1)) {
			printf("Error writing qidx entry for star %i: %s\n", i,
				   strerror(errno));
			exit(-1);
		}
		for (j=0; j<thisnumq; j++) {
			int kk;
			kk = il_get(list, j);
			if (fwrite(&kk, sizeof(kk), 1, idxfid) != 1) {
				printf("Error writing qidx quads for star %i: %s\n",
					   i, strerror(errno));
				exit(-1);
			}
		}
		il_free(list);
		quadlist[i] = NULL;
	}
	free(quadlist);
	
	quadfile_close(quads);
	fclose(idxfid);

	fprintf(stderr, "  done.\n");

	return 0;
}

