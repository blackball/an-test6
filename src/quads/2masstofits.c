#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <math.h>  // to get NAN
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <zlib.h>

#include "2mass.h"
#include "2mass_catalog.h"
#include "healpix.h"
#include "starutil.h"
#include "boilerplate.h"

#define OPTIONS "ho:N:"

void print_help(char* progname) {
	boilerplate_help_header(stdout);
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-N <healpix-nside>]  (default = 8.)\n"
		   "  <input-file> [<input-file> ...]\n"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int c;
	char* outfn = NULL;
	int startoptind;
	int Nside = 8;
	int HP;
	int i;

	twomass_catalog** cats;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'N':
			Nside = atoi(optarg);
			break;
		case 'o':
			outfn = optarg;
			break;
		}
    }

	if (!outfn || (optind == argc)) {
		print_help(args[0]);
		exit(-1);
	}

	if (Nside < 1) {
		fprintf(stderr, "Nside must be >= 1.\n");
		print_help(args[0]);
		exit(-1);
	}

	HP = 12 * Nside * Nside;
	cats = calloc(HP, sizeof(twomass_catalog*));

	printf("Nside = %i, using %i healpixes.\n", Nside, HP);

	printf("Reading 2MASS files... ");
	fflush(stdout);

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		gzFile* fiz = NULL;
		char line[1024];
		int nentries;

		infn = args[optind];
		printf("\nReading file %i of %i: %s\n", 1 + optind - startoptind,
			   argc - startoptind, infn);
		infn = args[optind];
		fiz = gzopen(infn, "rb");
		if (!fiz) {
			fprintf(stderr, "Failed to open file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}
		nentries = 0;
		for (;;) {
			twomass_entry e;
			int hp;

			if (gzeof(fiz))
				break;

			if (gzgets(fiz, line, 1024) == Z_NULL) {
				if (gzeof(fiz))
					break;
				fprintf(stderr, "Failed to read a line from file %s: %s\n", infn, strerror(errno));
				exit(-1);
			}

			if (twomass_parse_entry(&e, line)) {
				fprintf(stderr, "Failed to parse 2MASS entry from file %s.\n", infn);
				exit(-1);
			}

			hp = radectohealpix_nside(deg2rad(e.ra), deg2rad(e.dec), Nside);
			if (!cats[hp]) {
				char fn[256];
				char val[256];
				sprintf(fn, outfn, hp);
				cats[hp] = twomass_catalog_open_for_writing(fn);
				if (!cats[hp]) {
					fprintf(stderr, "Failed to open 2MASS catalog for writing to file %s (hp %i).\n", fn, hp);
					exit(-1);
				}
				// header remarks...
				boilerplate_add_fits_headers(cats[hp]->header);
				sprintf(val, "%u", hp);
				qfits_header_add(cats[hp]->header, "HEALPIX", val, "The healpix number of this catalog.", NULL);
				sprintf(val, "%u", Nside);
				qfits_header_add(cats[hp]->header, "NSIDE", val, "The healpix resolution.", NULL);

				qfits_header_add(cats[hp]->header, "COMMENT", "The fields are as described in the 2MASS documentation:", NULL, NULL);
				qfits_header_add(cats[hp]->header, "COMMENT", "  ftp://ftp.ipac.caltech.edu/pub/2mass/allsky/format_psc.html", NULL, NULL);
				qfits_header_add(cats[hp]->header, "COMMENT", "with a few exceptions:", NULL, NULL);
				qfits_header_add(cats[hp]->header, "COMMENT", "* the photometric quality flag values are:", NULL, NULL);
				sprintf(val, "    %i: 'X' in 2MASS, No brightness info available.", TWOMASS_QUALITY_NO_BRIGHTNESS);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'U' in 2MASS, The brightness val is an upper bound.", TWOMASS_QUALITY_UPPER_LIMIT_MAG);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'F' in 2MASS, No magnitude sigma is available", TWOMASS_QUALITY_NO_SIGMA);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'E' in 2MASS, Profile-fit photometry was bad", TWOMASS_QUALITY_BAD_FIT);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'A' in 2MASS, Best quality", TWOMASS_QUALITY_A);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'B' in 2MASS, ...", TWOMASS_QUALITY_B);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'C' in 2MASS, ...", TWOMASS_QUALITY_C);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'D' in 2MASS, Worst quality", TWOMASS_QUALITY_D);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);

				qfits_header_add(cats[hp]->header, "COMMENT", "* the confusion/contamination flag values are:", NULL, NULL);
				sprintf(val, "    %i: '0' in 2MASS, No problems.", TWOMASS_CC_NONE);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'p' in 2MASS, Persistence.", TWOMASS_CC_PERSISTENCE);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'c' in 2MASS, Confusion.", TWOMASS_CC_CONFUSION);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'd' in 2MASS, Diffraction.", TWOMASS_CC_DIFFRACTION);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 's' in 2MASS, Stripe.", TWOMASS_CC_STRIPE);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: 'b' in 2MASS, Band merge.", TWOMASS_CC_BANDMERGE);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);

				qfits_header_add(cats[hp]->header, "COMMENT", "* the association flag values are:", NULL, NULL);
				sprintf(val, "    %i: none.", TWOMASS_ASSOCIATION_NONE);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: Tycho.", TWOMASS_ASSOCIATION_TYCHO);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "    %i: USNO A-2.", TWOMASS_ASSOCIATION_USNOA2);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);

				//qfits_header_add(cats[hp]->header, "COMMENT", "* the NULL float value is IEEE NaN.", NULL, NULL);
				sprintf(val, "* the NULL value for floats is %f", TWOMASS_NULL);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				//qfits_header_add(cats[hp]->header, "COMMENT", "* the NULL float value is IEEE NaN.", NULL, NULL);
				qfits_header_add(cats[hp]->header, "COMMENT", "* the NULL value for the 'ext_key' aka 'xsc_key' field is", NULL, NULL);
				sprintf(val, "   %i (0x%x).", TWOMASS_KEY_NULL, TWOMASS_KEY_NULL);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);
				sprintf(val, "* the NULL value for the 'prox_angle' and 'phi_opt' fields is %i (0x%x).", TWOMASS_ANGLE_NULL, TWOMASS_ANGLE_NULL);
				qfits_header_add(cats[hp]->header, "COMMENT", val, NULL, NULL);

				if (twomass_catalog_write_headers(cats[hp])) {
					fprintf(stderr, "Failed to write 2MASS catalog headers: %s\n", fn);
					exit(-1);
				}
			}
			if (twomass_catalog_write_entry(cats[hp], &e)) {
				fprintf(stderr, "Failed to write 2MASS catalog entry.\n");
				exit(-1);
			}

			nentries++;
			if (!(nentries % 100000)) {
				printf(".");
				fflush(stdout);
			}
		}
		gzclose(fiz);
		printf("\n");

		printf("Read %i entries.\n", nentries);
	}

	for (i=0; i<HP; i++) {
		if (!cats[i])
			continue;
		if (twomass_catalog_fix_headers(cats[i]) ||
			twomass_catalog_close(cats[i])) {
			fprintf(stderr, "Failed to close 2MASS catalog.\n");
			exit(-1);
		}
	}
	free(cats);

	return 0;
}

