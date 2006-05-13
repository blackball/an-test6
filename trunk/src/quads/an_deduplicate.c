/**
   deduplicate: finds pairs of stars within a given distance of each other and removes
   one of the stars (the one that appears second).

   input: .objs catalogue
   output: .objs catalogue

   original author: dstn
*/

#include <math.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "dualtree_rangesearch.h"
#include "bl.h"
#include "an_catalog.h"

#define OPTIONS "hi:o:d:"
const char HelpString[] =
"deduplicate -d dist -i <input-file> -o <output-file>\n"
"  radius: (in arcseconds) is the de-duplication radius: a star found within\n"
"      this radius of another star will be discarded\n";

extern char *optarg;
extern int optind, opterr, optopt;

il* duplicates = NULL;
void duplicate_found(void* nil, int ind1, int ind2, double dist2);

int npoints = 0;
void progress(void* nil, int ndone);

kdtree_t *starkd = NULL;

int main(int argc, char *argv[]) {
    int argidx, argchar;
    double duprad = 0.0;
    char *infname = NULL, *outfname = NULL;
	double* thestars;
	int levels;
	int Nleaf;
	an_catalog* catout;
	an_catalog* cat;
	char* fn;
	double maxdist;
	an_entry* entries;
	int BLOCK = 100000;
	int off, n;
	int dupind = 0;
	int skipind = -1;
	int srcind;
	int N, Ndup;
	char val[32];

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
		switch (argchar) {
		case 'd':
			duprad = atof(optarg);
			if (duprad < 0.0) {
				printf("Couldn't parse \'radius\': \"%s\"\n", optarg);
				exit(-1);
			}
			break;
		case 'i':
			infname = optarg;
			break;
		case 'o':
			outfname = optarg;
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
		return (HELP_ERR);
    }
    if (!infname || !outfname || duprad < 0.0) {
		fprintf(stderr, HelpString);
		return (HELP_ERR);
    }

	fn = infname;
	cat = an_catalog_open(fn);
    if (!cat) {
		fprintf(stderr, "Couldn't open catalog %s\n", fn);
		exit(-1);
	}
    fprintf(stderr, "Reading catalogue file %s...\n", fn);
    fflush(stderr);

	fn = outfname;
	catout = an_catalog_open_for_writing(fn);
	if (!catout) {
		fprintf(stderr, "Couldn'tn open file %s for writing.\n", fn);
		exit(-1);
	}
    fprintf(stderr, "Will write to catalogue %s\n", fn);

	// discard the default header; copy the source catalog's header.
	qfits_header_destroy(catout->header);
	catout->header = qfits_header_copy(qfits_header_read(infname));
	sprintf(val, "%g", duprad);
	if (qfits_header_getstr(catout->header, "DEDUP")) {
		qfits_header_mod(catout->header, "DEDUP", val, "Deduplication radius (arcsec)");
	} else {
		qfits_header_add(catout->header, "DEDUP", val, "Deduplication radius (arcsec)", NULL);
	}

	if (an_catalog_write_headers(catout)) {
		fprintf(stderr, "Couldn't write headers.\n");
		exit(-1);
	}

    fprintf(stderr, "Catalog has %u objects.\n", cat->nentries);

	thestars = malloc(cat->nentries * 3 * sizeof(double));

	entries = malloc(BLOCK * sizeof(an_entry));

	for (off=0; off<cat->nentries; off+=n) {
		int i;
		if (off + BLOCK > cat->nentries)
			n = cat->nentries - off;
		else
			n = BLOCK;
		an_catalog_read_entries(cat, off, n, entries);
		for (i=0; i<n; i++) {
			an_entry* entry = ((an_entry*)entries) + i;
			thestars[3*(off+i)+0] = radec2x(deg2rad(entry->ra), deg2rad(entry->dec));
			thestars[3*(off+i)+1] = radec2y(deg2rad(entry->ra), deg2rad(entry->dec));
			thestars[3*(off+i)+2] = radec2z(deg2rad(entry->ra), deg2rad(entry->dec));
		}
	}

	duplicates = il_new(256);

	Nleaf = 10;
	levels = (int)((log((double)cat->nentries) - log((double)Nleaf))/log(2.0));
	if (levels < 1) {
		levels = 1;
	}
	fprintf(stderr, "Building KD tree (with %i levels)...", levels);
	fflush(stderr);

	starkd = kdtree_build(thestars, cat->nentries, 3, levels);
	if (!starkd) {
		fprintf(stderr, "Couldn't create star kdtree.\n");
		exit(-1);
	}
	fprintf(stderr, "Done.\n");

	// convert from arcseconds to distance on the unit sphere.
	maxdist = sqrt(arcsec2distsq(duprad));
	fprintf(stderr, "Running dual-tree search to find duplicates...\n");
	fflush(stderr);
	// de-duplicate.
	npoints = cat->nentries;
	dualtree_rangesearch(starkd, starkd,
						 RANGESEARCH_NO_LIMIT, maxdist,
						 duplicate_found, NULL,
						 progress, NULL);
	fprintf(stderr, "Done!");

	kdtree_free(starkd);

	N = cat->nentries;
	Ndup = il_size(duplicates);
	fprintf(stderr, "Removing %i duplicate stars (%i stars remain)...\n",
			Ndup, N-Ndup);
	fflush(stderr);

	for (off=0; off<cat->nentries; off+=n) {
		int i;
		if (off + BLOCK > cat->nentries)
			n = cat->nentries - off;
		else
			n = BLOCK;
		an_catalog_read_entries(cat, off, n, entries);
		for (i=0; i<n; i++) {
			// find the next index to skip
			srcind = off + i;
			if (skipind == -1) {
				if (dupind < Ndup) {
					skipind = il_get(duplicates, dupind);
					dupind++;
				}
				// shouldn't happen, I think...
				if ((skipind > 0) && (skipind < srcind)) {
					fprintf(stderr, "Warning: duplicate index %i skipped\n", skipind);
					fprintf(stderr, "skipind=%i, srcind=%i\n", skipind, srcind);
					skipind = -1;
				}
			}
			if (srcind == skipind) {
				skipind = -1;
				continue;
			}
			an_catalog_write_entry(catout, entries + i);
		}
	}
	il_free(duplicates);
	free(entries);

	if (an_catalog_fix_headers(catout) ||
		an_catalog_close(catout)) {
		fprintf(stderr, "Error closing output catalog.\n");
		exit(-1);
	}

	an_catalog_close(cat);

	fprintf(stderr, "Done!\n");
	return 0;
}

int last_pct = 0;
void progress(void* nil, int ndone) {
	//int pct = (int)(100.0 * ndone / (double)npoints);
	int pct = (int)(1000.0 * ndone / (double)npoints);
	if (pct != last_pct) {
		//printf("%i %% done.\n", pct);
		//printf("%.1f %% done.\n", 0.1 * pct);
		if (pct % 10 == 0)
			printf("(%i %%)", pct/10);
		else
			printf(".");
		fflush(stdout);
	}
	last_pct = pct;
}

void duplicate_found(void* nil, int ind1, int ind2, double dist2) {
	int i1, i2;
	// map back to normal indices (not kdtree ones)
	i1 = starkd->perm[ind1];
	i2 = starkd->perm[ind2];
	if (i1 <= i2) return;
	// append the larger of the two.
	il_insert_unique_ascending(duplicates, i1);
}


