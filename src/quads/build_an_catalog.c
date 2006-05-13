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

#define OPTIONS "ho:N:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-N <healpix-nside>]  (default = 8; should be power of two.)\n"
		   "  <input-file> [<input-file> ...]\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

static void init_catalog(an_catalog** cats, char* outfn, int hp, int Nside) {
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

void qfits_dispfn(char* str) {
	printf("qfits: %s\n", str);
}

int main(int argc, char** args) {
	char* outfn = NULL;
	int c;
	int startoptind;
	//uint nrecords, nobs;
	int Nside = 8;
	int i, HP;
	an_catalog** cats;
	int64_t starid;
	int version = 0;
	int nusnob = 0, ntycho = 0;

	int BLOCK = 100000;
	void* entries;
	entries = malloc(BLOCK * imax(sizeof(usnob_entry), sizeof(tycho2_entry)));

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

	cats = malloc(HP * sizeof(an_catalog*));
	memset(cats, 0, HP * sizeof(an_catalog*));

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
			int n, off;
			usnob_entry* entry = (usnob_entry*)entries;
			int N = usnob_fits_count_entries(usnob);
			printf("Reading %i entries from USNO-B catalog file %s\n", N, infn);
			for (off=0; off<N; off+=BLOCK) {
				int j, ob;
				if (off + BLOCK > N)
					n = N - off;
				else
					n = BLOCK;

				if (off % 100000 == 0) {
					printf(".");
					fflush(stdout);
				}

				if (usnob_fits_read_entries(usnob, off, n, entry)) {
					fprintf(stderr, "Failed to read USNO-B entries.\n");
					exit(-1);
				}
				for (i=0; i<n; i++) {
					if (!entry[i].ndetections)
						// Tycho-2 star.  Ignore it.
						continue;
					if (entry[i].diffraction_spike)
						// may be a diffraction spike.  Ignore it.
						continue;

					memset(&an, 0, sizeof(an));

					an.ra = entry[i].ra;
					an.dec = entry[i].dec;
					an.motion_ra = entry[i].mu_ra;
					an.motion_dec = entry[i].mu_dec;
					an.sigma_ra = entry[i].sigma_ra;
					an.sigma_dec = entry[i].sigma_dec;
					an.sigma_motion_ra = entry[i].sigma_mu_ra;
					an.sigma_motion_dec = entry[i].sigma_mu_dec;

					an.id = an_catalog_get_id(version, starid);
					starid++;

					ob = 0;
					for (j=0; j<5; j++) {
						//if (entry.obs[j].mag == 0.0)
						if (entry[i].obs[j].field == 0)
							continue;
						an.obs[ob].mag = entry[i].obs[j].mag;
						// estimate from USNO-B paper section 5: photometric calibn
						an.obs[ob].sigma_mag = 0.25;
						an.obs[ob].id = entry[i].usnob_id;
						an.obs[ob].catalog = AN_SOURCE_USNOB;
						an.obs[ob].band = usnob_get_survey_band(entry[i].obs[j].survey);
						ob++;
					}
					an.nobs = ob;

					hp = radectohealpix_nside(deg2rad(an.ra), deg2rad(an.dec), Nside);
					if (!cats[hp]) {
						init_catalog(cats, outfn, hp, Nside);
					}
					an_catalog_write_entry(cats[hp], &an);
					nusnob++;
				}
			}
			usnob_fits_close(usnob);
			printf("\n");

		} else if (tycho) {
			int n, off;
			tycho2_entry* entry = (tycho2_entry*)entries;
			int N = tycho2_fits_count_entries(tycho);
			printf("Reading %i entries from Tycho-2 catalog file %s\n", N, infn);
			for (off=0; off<N; off+=BLOCK) {
				int ob;
				if (off + BLOCK > N)
					n = N - off;
				else
					n = BLOCK;

				if (off % 100000 == 0) {
					printf(".");
					fflush(stdout);
				}

				//printf("off=%i, n=%i, N=%i\n", off, n, N);
				if (tycho2_fits_read_entries(tycho, off, n, entry)) {
					fprintf(stderr, "Failed to read Tycho-2 entries.\n");
					exit(-1);
				}

				for (i=0; i<n; i++) {
					memset(&an, 0, sizeof(an));

					an.ra = entry[i].RA;
					an.dec = entry[i].DEC;
					an.sigma_ra = entry[i].sigma_RA;
					an.sigma_dec = entry[i].sigma_DEC;
					an.motion_ra = entry[i].pmRA;
					an.motion_dec = entry[i].pmDEC;
					an.sigma_motion_ra = entry[i].sigma_pmRA;
					an.sigma_motion_dec = entry[i].sigma_pmDEC;

					an.id = an_catalog_get_id(version, starid);
					starid++;

					ob = 0;
					if (entry[i].mag_BT != 0.0) {
						an.obs[ob].catalog = AN_SOURCE_TYCHO2;
						an.obs[ob].band = 'B';
						an.obs[ob].id = tycho2_id_to_int(entry[i].tyc1, entry[i].tyc2, entry[i].tyc3);
						an.obs[ob].mag = entry[i].mag_BT;
						an.obs[ob].sigma_mag = entry[i].sigma_BT;
						ob++;
					}
					if (entry[i].mag_VT != 0.0) {
						an.obs[ob].catalog = AN_SOURCE_TYCHO2;
						an.obs[ob].band = 'V';
						an.obs[ob].id = tycho2_id_to_int(entry[i].tyc1, entry[i].tyc2, entry[i].tyc3);
						an.obs[ob].mag = entry[i].mag_VT;
						an.obs[ob].sigma_mag = entry[i].sigma_VT;
						ob++;
					}
					if (entry[i].mag_HP != 0.0) {
						an.obs[ob].catalog = AN_SOURCE_TYCHO2;
						an.obs[ob].band = 'H';
						an.obs[ob].id = tycho2_id_to_int(entry[i].tyc1, entry[i].tyc2, entry[i].tyc3);
						an.obs[ob].mag = entry[i].mag_HP;
						an.obs[ob].sigma_mag = entry[i].sigma_HP;
						ob++;
					}
					an.nobs = ob;
					if (!an.nobs) {
						fprintf(stderr, "Tycho entry %i: no observations!\n", off + i);
						continue;
					}

					hp = radectohealpix_nside(deg2rad(an.ra), deg2rad(an.dec), Nside);
					if (!cats[hp]) {
						init_catalog(cats, outfn, hp, Nside);
					}
					an_catalog_write_entry(cats[hp], &an);
					ntycho++;
				}
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

	free(entries);

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

