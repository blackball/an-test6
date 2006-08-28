/**
   \file Applies a star kdtree permutation array to the corresponding
   .id file, and also rewrites the .quad file to produce new .id, .quad
   and .skdt files that are consistent and don't require permutation.

   In:  .quad, .id, .skdt
   Out: .quad, .id, .skdt

   Original author: dstn
*/
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>

#include "kdtree.h"
#include "starutil.h"
#include "fileutil.h"
#include "quadfile.h"
#include "idfile.h"
#include "fitsioutils.h"
#include "qfits.h"
#include "starkd.h"

#define OPTIONS "hf:o:q:"

void printHelp(char* progname) {
	printf("%s -f <input-basename> [-q <input-quadfile-basename>] -o <output-basename>\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char **args) {
    int argchar;
    quadfile* qfin;
	quadfile* qfout;
	idfile* idin;
	idfile* idout = NULL;
	startree* treein;
	startree* treeout;
	char* progname = args[0];
	char* basein = NULL;
	char* baseout = NULL;
	char* basequadin = NULL;
	char* fn;
	int i;
	int healpix;
	int starhp;
	int lastgrass;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'q':
			basequadin = optarg;
			break;
        case 'f':
			basein = optarg;
			break;
        case 'o':
			baseout = optarg;
			break;
        case '?':
            fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        case 'h':
			printHelp(progname);
            return 0;
        default:
            return -1;
        }

	if (!basein || !baseout) {
		printHelp(progname);
		exit(-1);
	}

	fn = mk_streefn(basein);
	printf("Reading star tree from %s ...\n", fn);
	treein = startree_open(fn);
	if (!treein) {
		fprintf(stderr, "Failed to read star kdtree from %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	if (basequadin)
		fn = mk_quadfn(basequadin);
	else
		fn = mk_quadfn(basein);
	printf("Reading quadfile from %s ...\n", fn);
	qfin = quadfile_open(fn, 0);
	if (!qfin) {
		fprintf(stderr, "Failed to read quadfile from %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	fn = mk_idfn(basein);
	printf("Reading id file from %s ...\n", fn);
	idin = idfile_open(fn, 0);
	if (!idin) {
		fprintf(stderr, "Failed to read id file from %s.  Will not generate output id file.\n", fn);
		//exit(-1);
	}
	free_fn(fn);

	if (idin) {
		healpix = idin->healpix;
		if (qfin->healpix != healpix) {
			fprintf(stderr, "Quadfile header says it's healpix %i, but idfile say %i.\n",
					qfin->healpix, idin->healpix);
		}
	} else
		healpix = qfin->healpix;

	starhp = qfits_header_getint(treein->header, "HEALPIX", -1);
	if (starhp == -1)
		fprintf(stderr, "Warning, input star kdtree didn't have a HEALPIX header.\n");
	else if (starhp != healpix) {
		fprintf(stderr, "Idfile says it's healpix %i, but star kdtree says %i.\n",
				healpix, starhp);
		exit(-1);
	}

	fn = mk_quadfn(baseout);
	printf("Writing quadfile to %s ...\n", fn);
	qfout = quadfile_open_for_writing(fn);
	if (!qfout) {
		fprintf(stderr, "Failed to write quadfile to %s.\n", fn);
		exit(-1);
	}
	free_fn(fn);

	if (idin) {
		fn = mk_idfn(baseout);
		printf("Writing id to %s ...\n", fn);
		idout = idfile_open_for_writing(fn);
		if (!idout) {
			fprintf(stderr, "Failed to write id file to %s.\n", fn);
			exit(-1);
		}
		free_fn(fn);
	}

	qfout->healpix = healpix;
	if (idin)
		idout->healpix = healpix;

	qfout->numstars          = qfin->numstars;
	qfout->index_scale       = qfin->index_scale;
	qfout->index_scale_lower = qfin->index_scale_lower;
	qfout->indexid           = qfin->indexid;

	qfits_header_add(qfout->header, "HISTORY", "unpermute-stars command line:", NULL, NULL);
	fits_add_args(qfout->header, args, argc);
	qfits_header_add(qfout->header, "HISTORY", "(end of unpermute-stars command line)", NULL, NULL);
	qfits_header_add(qfout->header, "HISTORY", "** unpermute-stars: history from input:", NULL, NULL);
	fits_copy_all_headers(qfin->header, qfout->header, "HISTORY");
	qfits_header_add(qfout->header, "HISTORY", "** unpermute-stars: end of history from input.", NULL, NULL);
	qfits_header_add(qfout->header, "COMMENT", "** unpermute-stars: comments from input:", NULL, NULL);
	fits_copy_all_headers(qfin->header, qfout->header, "COMMENT");
	qfits_header_add(qfout->header, "COMMENT", "** unpermute-stars: end of comments from input.", NULL, NULL);

	if (idin) {
		qfits_header_add(idout->header, "HISTORY", "unpermute-stars command line:", NULL, NULL);
		fits_add_args(idout->header, args, argc);
		qfits_header_add(idout->header, "HISTORY", "(end of unpermute-stars command line)", NULL, NULL);
		qfits_header_add(idout->header, "HISTORY", "** unpermute-stars: history from input:", NULL, NULL);
		fits_copy_all_headers(idin->header, idout->header, "HISTORY");
		qfits_header_add(idout->header, "HISTORY", "** unpermute-stars: end of history from input.", NULL, NULL);
		qfits_header_add(idout->header, "COMMENT", "** unpermute-stars: comments from input:", NULL, NULL);
		fits_copy_all_headers(idin->header, idout->header, "COMMENT");
		qfits_header_add(idout->header, "COMMENT", "** unpermute-stars: end of comments from input.", NULL, NULL);
	}

	if (quadfile_write_header(qfout) ||
		(idin && idfile_write_header(idout))) {
		fprintf(stderr, "Failed to write quadfile or idfile header.\n");
		exit(-1);
	}

	if (idin) {
		printf("Writing IDs...\n");
		lastgrass = 0;
		for (i=0; i<treein->tree->ndata; i++) {
			uint64_t id;
			int ind;
			if (i*80/treein->tree->ndata != lastgrass) {
				printf(".");
				fflush(stdout);
				lastgrass = i*80/treein->tree->ndata;
			}
			ind = treein->tree->perm[i];
			id = idfile_get_anid(idin, ind);
			if (idfile_write_anid(idout, id)) {
				fprintf(stderr, "Failed to write idfile entry.\n");
				exit(-1);
			}
		}
		printf("\n");
	}

	printf("Writing quads...\n");

	startree_compute_inverse_perm(treein);

	lastgrass = 0;
	for (i=0; i<qfin->numquads; i++) {
		int j;
		uint stars[4];
		if (i*80/qfin->numquads != lastgrass) {
			printf(".");
			fflush(stdout);
			lastgrass = i*80/qfin->numquads;
		}
		quadfile_get_starids(qfin, i, stars, stars+1, stars+2, stars+3);
		for (j=0; j<4; j++)
			stars[j] = treein->inverse_perm[stars[j]];
		if (quadfile_write_quad(qfout, stars[0], stars[1], stars[2], stars[3])) {
			fprintf(stderr, "Failed to write quadfile entry.\n");
			exit(-1);
		}
	}
	printf("\n");

	if (quadfile_fix_header(qfout) ||
		quadfile_close(qfout) ||
		(idin && (idfile_fix_header(idout) ||
				  idfile_close(idout)))) {
		fprintf(stderr, "Failed to close output quadfile or idfile.\n");
		exit(-1);
	}

	treeout = startree_new();
	treeout->tree   = calloc(1, sizeof(kdtree_t));
	treeout->tree->tree   = treein->tree->tree;
	treeout->tree->data   = treein->tree->data;
	treeout->tree->ndata  = treein->tree->ndata;
	treeout->tree->ndim   = treein->tree->ndim;
	treeout->tree->nnodes = treein->tree->nnodes;

	fits_copy_header(qfin->header, treeout->header, "HEALPIX");
	qfits_header_add(treeout->header, "HISTORY", "unpermute-stars command line:", NULL, NULL);
	fits_add_args(treeout->header, args, argc);
	qfits_header_add(treeout->header, "HISTORY", "(end of unpermute-stars command line)", NULL, NULL);
	qfits_header_add(treeout->header, "HISTORY", "** unpermute-stars: history from input:", NULL, NULL);
	fits_copy_all_headers(treein->header, treeout->header, "HISTORY");
	qfits_header_add(treeout->header, "HISTORY", "** unpermute-stars: end of history from input.", NULL, NULL);
	qfits_header_add(treeout->header, "COMMENT", "** unpermute-stars: comments from input:", NULL, NULL);
	fits_copy_all_headers(treein->header, treeout->header, "COMMENT");
	qfits_header_add(treeout->header, "COMMENT", "** unpermute-stars: end of comments from input.", NULL, NULL);

	quadfile_close(qfin);
	if (idin)
		idfile_close(idin);

	fn = mk_streefn(baseout);
	printf("Writing star kdtree to %s ...\n", fn);
	if (startree_write_to_file(treeout, fn)) {
		fprintf(stderr, "Failed to write star kdtree.\n");
		exit(-1);
	}
	free_fn(fn);
	startree_close(treeout);
	startree_close(treein);

	return 0;
}
