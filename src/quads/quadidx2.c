#include <errno.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "blocklist.h"

#define OPTIONS "hf:"
const char HelpString[] =
    "quadidx2 -f fname\n";

extern char *optarg;
extern int optind, opterr, optopt;

char *idxfname = NULL;
char *quadfname = NULL;
off_t posmarker, cposmarker;

blocklist** quadlist;

int main(int argc, char *argv[]) {
	int argidx, argchar;
	sidx numstars;
	qidx numquads;
	dimension DimQuads;
	double index_scale;
	char readStatus;
	FILE *idxfid = NULL, *quadfid = NULL;
	sidx q;
	int i;
	sidx numused;
	magicval magic = MAGIC_VAL;
	
	if (argc <= 2) {
		fprintf(stderr, HelpString);
		return (OPT_ERR);
		fprintf(stderr, "argc=%d\n", argc);
	}

	while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
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

	fprintf(stderr, "quadidx2: indexing quads in %s...\n",
	        quadfname);

	fopenin(quadfname, quadfid);
	free_fn(quadfname);
	readStatus = read_quad_header(quadfid, &numquads, &numstars, 
								  &DimQuads, &index_scale);
	if (readStatus == READ_FAIL)
		fprintf(stderr, "ERROR (quadidx) -- read_quad_header failed\n");

	posmarker = ftello(quadfid);

	fopenout(idxfname, idxfid);
	free_fn(idxfname);

	quadlist = (blocklist**)malloc(numstars * sizeof(blocklist*));
	memset(quadlist, 0, numstars * sizeof(blocklist*));

	for (q=0; q<numquads; q++) {
		qidx iA, iB, iC, iD;
		qidx inds[4];
		
		readonequad(quadfid, &iA, &iB, &iC, &iD);

		inds[0] = iA;
		inds[1] = iB;
		inds[2] = iC;
		inds[3] = iD;

		// append this quad index to the lists of each of
		// its four stars.
		for (i=0; i<4; i++) {
			blocklist* list;
			sidx starind = inds[i];
			list = quadlist[starind];
			// create the list if necessary
			if (!list) {
				list = blocklist_new(10, sizeof(qidx));
				quadlist[starind] = list;
			}
			blocklist_append(list, &q);
		}
	}
	
	// first count numused:
	// how many stars are members of quads.
	numused = 0;
	for (i=0; i<numstars; i++) {
		blocklist* list = quadlist[i];
		if (!list) continue;
		numused++;
	}
	printf("%li stars used\n", numused);
	if (fwrite(&magic, sizeof(magic), 1, idxfid) != 1) {
		printf("Error writing magic: %s\n", strerror(errno));
		exit(-1);
	}
	if (fwrite(&numused, sizeof(numused), 1, idxfid) != 1) {
		printf("Error writing numused: %s\n", strerror(errno));
		exit(-1);
	}
	for (i=0; i<numstars; i++) {
		int j;
		qidx thisnumq;
		sidx thisstar;
		blocklist* list = quadlist[i];
		if (!list) continue;
		thisnumq = (qidx)blocklist_count(list);
		thisstar = i;
		if ((fwrite(&thisstar, sizeof(thisstar), 1, idxfid) != 1) ||
			(fwrite(&thisnumq, sizeof(thisnumq), 1, idxfid) != 1)) {
			printf("Error writing qidx entry for star %i: %s\n", i,
				   strerror(errno));
			exit(-1);
		}
		for (j=0; j<thisnumq; j++) {
			qidx kk;
			blocklist_get(list, j, &kk);
			if (fwrite(&kk, sizeof(kk), 1, idxfid) != 1) {
				printf("Error writing qidx quads for star %i: %s\n",
					   i, strerror(errno));
				exit(-1);
			}
		}
		blocklist_free(list);
		quadlist[i] = NULL;
	}
	free(quadlist);
	
	fclose(quadfid);
	fclose(idxfid);

	fprintf(stderr, "  done.\n");

	return 0;
}

