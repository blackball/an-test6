#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <endian.h>
#include <netinet/in.h>
#include <byteswap.h>
#include <assert.h>

#include "usnob.h"
#include "qfits.h"
#include "healpix.h"
#include "starutil.h"
#include "usnob_fits.h"
#include "fitsioutils.h"
#include "boilerplate.h"

#define OPTIONS "ho:N:"

void print_help(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage:\n"
		   "  %s -o <output-filename-template>     [eg, usnob10_%%03i.fits]\n"
		   "  [-N <healpix-nside>]  (default = 8; should be power of two.)\n"
		   "  <input-file> [<input-file> ...]\n"
		   "\n"
		   "The output-filename-template should contain a \"printf\" sequence like \"%%03i\";\n"
		   "we use \"sprintf(filename, output-filename-template, healpix)\" to determine the filename\n"
		   "to be used for each healpix.\n\n"
		   "\nNOTE: WE ASSUME THE USNO-B1.0 FILES ARE GIVEN ON THE COMMAND LINE IN ORDER: 000/b0000.cat, 000/b0001.cat, etc.\n\n\n",
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

	usnob_fits** usnobs;

	int i, HP;
	int slicecounts[180];

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

	printf("Nside = %i, using %i healpixes.\n", Nside, HP);

	usnobs = malloc(HP * sizeof(usnob_fits*));
	memset(usnobs, 0, HP * sizeof(usnob_fits*));

	memset(slicecounts, 0, 180 * sizeof(uint));

	nrecords = 0;
	nobs = 0;
	nfiles = 0;

	printf("Reading USNO files... ");
	fflush(stdout);

	startoptind = optind;
	for (; optind<argc; optind++) {
		char* infn;
		FILE* fid;
		unsigned char* map;
		size_t map_size;
		int i;

		infn = args[optind];
		fid = fopen(infn, "rb");
		if (!fid) {
			fprintf(stderr, "Couldn't open input file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}

		if ((optind > startoptind) && ((optind - startoptind) % 100 == 0)) {
			printf("\nReading file %i of %i: %s\n", optind - startoptind,
				   argc - startoptind, infn);
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

		if (map_size % USNOB_RECORD_SIZE) {
			fprintf(stderr, "Warning, input file %s has size %u which is not divisible into %i-byte records.\n",
					infn, map_size, USNOB_RECORD_SIZE);
		}

		for (i=0; i<map_size; i+=USNOB_RECORD_SIZE) {
			usnob_entry entry;
			int hp;
			int slice;

			if (i && (i % 10000000 * USNOB_RECORD_SIZE == 0)) {
				printf("o");
				fflush(stdout);
			}
			
			if (usnob_parse_entry(map + i, &entry)) {
				fprintf(stderr, "Failed to parse USNOB entry: offset %i in file %s.\n",
						i, infn);
				exit(-1);
			}

			// compute the usnob_id based on its DEC slice and index.
			slice = (int)(entry.dec + 90.0);
			assert(slice < 180);
			assert((slicecounts[slice] & 0xff000000) == 0);
			entry.usnob_id = (slice << 24) | (slicecounts[slice]);
			slicecounts[slice]++;

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
				// etc...

				boilerplate_add_fits_headers(usnobs[hp]->header);

				qfits_header_add(usnobs[hp]->header, "HISTORY", "Created by the program \"usnobtofits\"", NULL, NULL);
				qfits_header_add(usnobs[hp]->header, "HISTORY", "usnobtofits command line:", NULL, NULL);
				fits_add_args(usnobs[hp]->header, args, argc);
				qfits_header_add(usnobs[hp]->header, "HISTORY", "(end of command line)", NULL, NULL);

				if (usnob_fits_write_headers(usnobs[hp])) {
					fprintf(stderr, "Failed to write header for FITS file %s.\n", fn);
					exit(-1);
				}
			}

			if (usnob_fits_write_entry(usnobs[hp], &entry)) {
				fprintf(stderr, "Failed to write FITS entry.\n");
				exit(-1);
			}

			nrecords++;
			nobs += (entry.ndetections == 0 ? 1 : entry.ndetections);
		}

		munmap(map, map_size);

		nfiles++;
		printf(".");
		fflush(stdout);
	}
	printf("\n");

	// close all the files...
	for (i=0; i<HP; i++) {
		if (!usnobs[i])
			continue;
		if (usnob_fits_fix_headers(usnobs[i]) ||
			usnob_fits_close(usnobs[i])) {
			fprintf(stderr, "Failed to close file %i: %s\n", i, strerror(errno));
		}
	}

	printf("Read %u files, %u records, %u observations.\n",
		   nfiles, nrecords, nobs);

	free(usnobs);

	return 0;
}
