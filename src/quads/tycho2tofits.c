#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>

#include "tycho2.h"
#include "tycho2_fits.h"
#include "starutil.h"

#define OPTIONS "ho:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-N <healpix-nside>]  (default = 8; should be power of two.)\n"
		   "  <input-file> [<input-file> ...]\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	char* outfn = NULL;
    int c;
	int startoptind;
	uint nrecords, nobs, nfiles;
	int Nside = 8;

	//tycho2_fits** tycs;
	tycho2_fits* tyc;
	int HP;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'n':
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

	/*
	  usnobs = malloc(HP * sizeof(usnob_fits*));
	  memset(usnobs, 0, HP * sizeof(usnob_fits*));
	*/

	nrecords = 0;
	nobs = 0;
	nfiles = 0;

	tyc = tycho2_fits_open_for_writing(outfn);
	if (!tyc) {
		fprintf(stderr, "Couldn't open output file %s.\n", outfn);
		exit(-1);
	}
	if (tycho2_fits_write_headers(tyc)) {
		fprintf(stderr, "Couldn't write Tycho-2 FITS headers.\n");
		exit(-1);
	}

	printf("Reading Tycho-2 files... \n");
	fflush(stdout);

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		FILE* fid;
		char* map;
		size_t map_size;
		int i;
		bool supplement;
		uint recsize;

		infn = args[optind];
		fid = fopen(infn, "rb");
		if (!fid) {
			fprintf(stderr, "Couldn't open input file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}

		if (fseeko(fid, 0, SEEK_END)) {
			fprintf(stderr, "Couldn't seek to end of input file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}
		map_size = ftello(fid);
		fseeko(fid, 0, SEEK_SET);
		map = mmap(NULL, map_size, PROT_READ, MAP_SHARED, fileno(fid), 0);
		if (map == MAP_FAILED) {
			fprintf(stderr, "Couldn't mmap input file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}
		fclose(fid);

		supplement = tycho2_guess_is_supplement(map);
		printf("File %s: supplement format: %s\n", infn, (supplement ? "Yes" : "No"));

		if (supplement) {
			recsize = TYCHO_SUPPLEMENT_RECORD_SIZE;
		} else {
			recsize = TYCHO_RECORD_SIZE;
		}

		if (map_size % recsize) {
			fprintf(stderr, "Warning, input file %s has size %u which is not divisible into %i-byte records.\n",
					infn, map_size, recsize);
		}

		for (i=0; i<map_size; i+=recsize) {
			tycho2_entry entry;
			//int hp;

			if (supplement) {
				if (tycho2_supplement_parse_entry(map + i, &entry)) {
					fprintf(stderr, "Failed to parse TYCHO-2 supplement entry: offset %i in file %s.\n",
							i, infn);
					exit(-1);
				}
			} else {
				if (tycho2_parse_entry(map + i, &entry)) {
					fprintf(stderr, "Failed to parse TYCHO-2 entry: offset %i in file %s.\n",
							i, infn);
					exit(-1);
				}
			}
			//printf("RA, DEC (%g, %g)\n", entry.RA, entry.DEC);

			if (tycho2_fits_write_entry(tyc, &entry)) {
				fprintf(stderr, "Failed to write Tycho-2 FITS entry.\n");
				exit(-1);
			}

			if (i && ((i/recsize) % 100000 == 0)) {
				printf(".");
				fflush(stdout);
			}

			/*
			  hp = radectohealpix_nside(deg2rad(entry.ra), deg2rad(entry.dec), Nside);

			  if (!usnobs[hp]) {
				char fn[256];
				char val[256];
				sprintf(fn, outfn, hp);
				usnobs[hp] = usnob_fits_open_for_writing(fn);
				if (!usnobs[hp]) {
				fprintf(stderr, "Failed to initialized FITS file %i (filename %s).\n", hp, fn);
				exit(-1);
				}

				// header remarks...
				sprintf(val, "%u", hp);
				qfits_header_add(usnobs[hp]->header, "HEALPIX", val, "The healpix number of this catalog.", NULL);
				sprintf(val, "%u", Nside);
				qfits_header_add(usnobs[hp]->header, "NSIDE", val, "The healpix resolution.", NULL);
				qfits_header_add(usnobs[hp]->header, "NOBJS", "0", "", NULL);
				// etc...

				if (usnob_fits_write_headers(usnobs[hp])) {
				fprintf(stderr, "Failed to write header for FITS file %s.\n", fn);
				exit(-1);
				}
				}

				if (usnob_fits_write_entry(usnobs[hp], &entry)) {
				fprintf(stderr, "Failed to write FITS entry.\n");
				exit(-1);
				}
			*/

			nrecords++;
			nobs += entry.nobs;
		}

		munmap(map, map_size);

		nfiles++;
		printf(".");
		fflush(stdout);
	}
	printf("\n");

	tycho2_fits_fix_headers(tyc);
	if (tycho2_fits_close(tyc)) {
		fprintf(stderr, "Failed to close Tycho-2 FITS file.\n");
		exit(-1);
	}

	// close all the files...
	/*
	  for (i=0; i<HP; i++) {
	  char val[256];
	  if (!usnobs[i])
	  continue;
	  sprintf(val, "%u", usnobs[i]->nentries);
	  qfits_header_mod(usnobs[i]->header, "NOBJS", val, "Number of objects in this catalog.");
	  usnob_fits_fix_headers(usnobs[i]);
	  if (usnob_fits_close(usnobs[i])) {
	  fprintf(stderr, "Failed to close file %i: %s\n", i, strerror(errno));
	  }
	  }
	*/
	
	printf("Read %u files, %u records, %u observations.\n",
		   nfiles, nrecords, nobs);
	
	//free(usnobs);

	return 0;
}

