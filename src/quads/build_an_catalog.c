#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>

#include "an_catalog.h"
#include "usnob_fits.h"
#include "tycho2_fits.h"
#include "fitsioutils.h"
#include "starutil.h"
#include "healpix.h"
#include "mathutil.h"
#include "boilerplate.h"
#include "fitsioutils.h"

#define OPTIONS "ho:N:"

static void print_help(char* progname) {
	boilerplate_help_header(stdout);
    printf("\nUsage: %s\n"
		   "   -o <output-filename-template>     (eg, an_hp%%03i.fits)\n"
		   "  [-N <healpix-nside>]  (default = 9)\n"
		   "  <input-file> [<input-file> ...]\n"
		   "\n"
		   "\nThe input files should be USNO-B1.0 and Tycho-2 files in FITS format.\n"
		   "(To generate these files, see \"usnobtofits\" and \"tycho2tofits\".)"
		   "\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

static void init_catalog(an_catalog** cats, char* outfn, int hp, int Nside, int argc, char** args) {
	char fn[256];
	char val[256];
	sprintf(fn, outfn, hp);
	cats[hp] = an_catalog_open_for_writing(fn);
	if (!cats[hp]) {
		fprintf(stderr, "Failed to initialized FITS output file %s.\n", fn);
		exit(-1);
	}
	// header remarks...
	sprintf(val, "%u", hp);
	qfits_header_add(cats[hp]->header, "HEALPIX", val, "The healpix number of this catalog.", NULL);
	sprintf(val, "%u", Nside);
	qfits_header_add(cats[hp]->header, "NSIDE", val, "The healpix resolution.", NULL);
	// etc...

	boilerplate_add_fits_headers(cats[hp]->header);

	qfits_header_add(cats[hp]->header, "HISTORY", "Created by the program \"build_an_catalog\"", NULL, NULL);
	qfits_header_add(cats[hp]->header, "HISTORY", "build_an_catalog command line:", NULL, NULL);
	fits_add_args(cats[hp]->header, args, argc);
	qfits_header_add(cats[hp]->header, "HISTORY", "(end of command line)", NULL, NULL);

	if (an_catalog_write_headers(cats[hp])) {
		fprintf(stderr, "Failed to write header for FITS file %s.\n", fn);
		exit(-1);
	}
}

static uint tycho2_id_to_int(int tyc1, int tyc2, int tyc3) {
	uint id = 0;
	// tyc1 and tyc2 each fit in 14 bits.
	assert((tyc1 & ~0x3fff) == 0);
	assert((tyc2 & ~0x3fff) == 0);
	// tyc3 fits in 3 bits.
	assert((tyc3 & ~0x7) == 0);
	id |= tyc1;
	id |= (tyc2 << 14);
	id |= (tyc3 << (14 + 14));
	return id;
}

static void qfits_dispfn(char* str) {
	printf("qfits: %s\n", str);
}

int main(int argc, char** args) {
	char* outfn = NULL;
	int c;
	int startoptind;
	int Nside = 9;
	int i, HP;
	an_catalog** cats;
	int64_t starid;
	int version = 0;
	int nusnob = 0, ntycho = 0;
	int BLOCK = 100000;

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

	qfits_err_register(qfits_dispfn);
	qfits_err_statset(1);

	HP = 12 * Nside * Nside;

	cats = calloc(HP, sizeof(an_catalog*));

	starid = 0;

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		usnob_fits* usnob = NULL;
		tycho2_fits* tycho = NULL;
		qfits_header* hdr;
		bool is_usnob = FALSE;
		bool is_tycho = FALSE;
		an_entry an;
		int hp;

		infn = args[optind];
		printf("Opening catalog file %s...\n", infn);
		hdr = qfits_header_read(infn);
		if (!hdr) {
			fprintf(stderr, "Couldn't read FITS header in file %s.\n", infn);
			exit(-1);
		}
		is_usnob = qfits_header_getboolean(hdr, "USNOB", 0);
		if (!is_usnob) {
			is_tycho = qfits_header_getboolean(hdr, "TYCHO_2", 0);
		}
		qfits_header_destroy(hdr);
		if (!is_usnob && !is_tycho) {
			// guess...
			printf("Guessing catalog type (this may generate a warning)...\n");
			usnob = usnob_fits_open(infn);
			if (!usnob) {
				tycho = tycho2_fits_open(infn);
				if (!tycho) {
					fprintf(stderr, "Couldn't figure out what catalog file %s came from.\n", infn);
					exit(-1);
				}
			}
		} else if (is_usnob) {
			usnob = usnob_fits_open(infn);
			if (!usnob) {
				fprintf(stderr, "Couldn't open USNO-B catalog: %s\n", infn);
				exit(-1);
			}
		} else if (is_tycho) {
			tycho = tycho2_fits_open(infn);
			if (!tycho) {
				fprintf(stderr, "Couldn't open Tycho-2 catalog: %s\n", infn);
				exit(-1);
			}
		}

		/*
		  USNO-B and Tycho-2 are actually mutually exclusive by design
		  (of USNO-B); USNO-B cut out a patch of sky around each Tycho-2
		  star (in the main catalog & first supplement) and pasted in the
		  Tycho-2 entry.
		  Therefore, we don't need to correlate stars between the catalogs.
		*/
		if (usnob) {
			usnob_entry* entry;
			int N = usnob_fits_count_entries(usnob);
			printf("Reading %i entries from USNO-B catalog file %s\n", N, infn);
			usnob->br.blocksize = BLOCK;
			for (i=0; i<N; i++) {
				int ob, j;

				if (!(i % 100000)) {
					printf(".");
					fflush(stdout);
				}
				entry = usnob_fits_read_entry(usnob);
				if (!entry) {
					fprintf(stderr, "Failed to read USNO-B entry.\n");
					exit(-1);
				}
				if (!entry->ndetections)
					// Tycho-2 star.  Ignore it.
					continue;
				if (entry->diffraction_spike)
					// may be a diffraction spike.  Ignore it.
					continue;

				memset(&an, 0, sizeof(an));

				an.ra = entry->ra;
				an.dec = entry->dec;
				an.motion_ra = entry->mu_ra;
				an.motion_dec = entry->mu_dec;
				an.sigma_ra = entry->sigma_ra;
				an.sigma_dec = entry->sigma_dec;
				an.sigma_motion_ra = entry->sigma_mu_ra;
				an.sigma_motion_dec = entry->sigma_mu_dec;

				an.id = an_catalog_get_id(version, starid);
				starid++;

				ob = 0;
				for (j=0; j<5; j++) {
					//if (entry.obs[j].mag == 0.0)
					if (entry->obs[j].field == 0)
						continue;
					an.obs[ob].mag = entry->obs[j].mag;
					// estimate from USNO-B paper section 5: photometric calibn
					an.obs[ob].sigma_mag = 0.25;
					an.obs[ob].id = entry->usnob_id;
					an.obs[ob].catalog = AN_SOURCE_USNOB;
					an.obs[ob].band = usnob_get_survey_band(entry->obs[j].survey);
					ob++;
				}
				an.nobs = ob;

				hp = radectohealpix_nside(deg2rad(an.ra), deg2rad(an.dec), Nside);
				if (!cats[hp]) {
					init_catalog(cats, outfn, hp, Nside, argc, args);
				}
				an_catalog_write_entry(cats[hp], &an);
				nusnob++;
			}
			usnob_fits_close(usnob);
			printf("\n");

		} else if (tycho) {
			tycho2_entry* entry;
			int N = tycho2_fits_count_entries(tycho);
			printf("Reading %i entries from Tycho-2 catalog file %s\n", N, infn);
			tycho->br.blocksize = BLOCK;
			for (i=0; i<N; i++) {
				int ob;
				if (!(i % 100000)) {
					printf(".");
					fflush(stdout);
				}
				entry = tycho2_fits_read_entry(tycho);
				if (!entry) {
					fprintf(stderr, "Failed to read Tycho-2 entry.\n");
					exit(-1);
				}

				memset(&an, 0, sizeof(an));

				an.ra = entry->RA;
				an.dec = entry->DEC;
				an.sigma_ra = entry->sigma_RA;
				an.sigma_dec = entry->sigma_DEC;
				an.motion_ra = entry->pmRA;
				an.motion_dec = entry->pmDEC;
				an.sigma_motion_ra = entry->sigma_pmRA;
				an.sigma_motion_dec = entry->sigma_pmDEC;

				an.id = an_catalog_get_id(version, starid);
				starid++;

				ob = 0;
				if (entry->mag_BT != 0.0) {
					an.obs[ob].catalog = AN_SOURCE_TYCHO2;
					an.obs[ob].band = 'B';
					an.obs[ob].id = tycho2_id_to_int(entry->tyc1, entry->tyc2, entry->tyc3);
					an.obs[ob].mag = entry->mag_BT;
					an.obs[ob].sigma_mag = entry->sigma_BT;
					ob++;
				}
				if (entry->mag_VT != 0.0) {
					an.obs[ob].catalog = AN_SOURCE_TYCHO2;
					an.obs[ob].band = 'V';
					an.obs[ob].id = tycho2_id_to_int(entry->tyc1, entry->tyc2, entry->tyc3);
					an.obs[ob].mag = entry->mag_VT;
					an.obs[ob].sigma_mag = entry->sigma_VT;
					ob++;
				}
				if (entry->mag_HP != 0.0) {
					an.obs[ob].catalog = AN_SOURCE_TYCHO2;
					an.obs[ob].band = 'H';
					an.obs[ob].id = tycho2_id_to_int(entry->tyc1, entry->tyc2, entry->tyc3);
					an.obs[ob].mag = entry->mag_HP;
					an.obs[ob].sigma_mag = entry->sigma_HP;
					ob++;
				}
				an.nobs = ob;
				if (!an.nobs) {
					fprintf(stderr, "Tycho entry %i: no observations!\n", i);
					continue;
				}

				hp = radectohealpix_nside(deg2rad(an.ra), deg2rad(an.dec), Nside);
				if (!cats[hp]) {
					init_catalog(cats, outfn, hp, Nside, argc, args);
				}
				an_catalog_write_entry(cats[hp], &an);
				ntycho++;
			}
			tycho2_fits_close(tycho);
			printf("\n");
		}

		// update and sync each output file...
		for (i=0; i<HP; i++) {
			off_t offset;
			if (!cats[i]) continue;
			if (an_catalog_fix_headers(cats[i])) {
				fprintf(stderr, "Error fixing the header or closing AN catalog for healpix %i.\n", i);
			}
			offset = ftello(cats[i]->fid);
			if (fits_pad_file(cats[i]->fid) ||
				fdatasync(fileno(cats[i]->fid))) {
				fprintf(stderr, "Error padding and syncing AN catalog file for healpix %i.\n", i);
			}
			fseeko(cats[i]->fid, offset, SEEK_SET);
		}
	}

	printf("Read %i USNO-B objects and %i Tycho-2 objects.\n",
		   nusnob, ntycho);

	for (i=0; i<HP; i++) {
		char val[32];
		if (!cats[i]) continue;
		sprintf(val, "%u", cats[i]->nentries);
		qfits_header_mod(cats[i]->header, "NOBJS", val, "Number of objects in this catalog.");
		if (an_catalog_fix_headers(cats[i]) ||
			an_catalog_close(cats[i])) {
			fprintf(stderr, "Error fixing the header or closing AN catalog for healpix %i.\n", i);
		}
	}
	free(cats);

	return 0;
}

