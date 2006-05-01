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

#define OPTIONS "ho:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename-template>\n"
		   "  [-N <healpix-nside>]  (default = 8; should be power of two.)\n"
		   "  <input-file> [<input-file> ...]\n"
		   "\n\nNOTE: WE ASSUME THE FILES ARE PROCESSED IN ORDER: 000/b0000.cat, 000/b0001.cat, etc.\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int init_fits_file(FILE** fids, qfits_header** headers, char* outfn,
				   int hp, qfits_table* table, int Nside) {
	char fn[256];
	char val[256];
    qfits_header* tablehdr;
    qfits_header* header;

	sprintf(fn, outfn, hp);
	fids[hp] = fopen(fn, "wb");
	if (!fids[hp]) {
		fprintf(stderr, "Couldn't open output file %s for writing: %s\n", fn, strerror(errno));
		return -1;
	}

	// the header
	headers[hp] = header = qfits_table_prim_header_default();

	// header remarks...
	sprintf(val, "%u", hp);
	qfits_header_add(header, "HEALPIX", val, "The healpix number of this catalog.", NULL);
	sprintf(val, "%u", Nside);
	qfits_header_add(header, "NSIDE", val, "The healpix resolution.", NULL);
	sprintf(val, "%u", 0);
	qfits_header_add(header, "NOBJS", val, "Number of objects in this catalog.", NULL);
	// etc...

	qfits_header_dump(header, fids[hp]);
	tablehdr = qfits_table_ext_header_default(table);
	qfits_header_dump(tablehdr, fids[hp]);
	qfits_header_destroy(tablehdr);
	return 0;
}

int main(int argc, char** args) {
	char* outfn = NULL;
    int c;
	int startoptind;
	uint nrecords, nobs, nfiles;
	int Nside = 8;

	FILE** fids;
	uint* hprecords;
	qfits_header** headers;

	qfits_table* table;

	int i, HP;
	int slicecounts[180];

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
	fids = malloc(HP * sizeof(FILE*));
	memset(fids, 0, HP * sizeof(FILE*));

	headers = malloc(HP * sizeof(qfits_header*));
	memset(headers, 0, HP * sizeof(qfits_header*));

	hprecords = malloc(HP * sizeof(uint));
	memset(hprecords, 0, HP*sizeof(uint));

	memset(slicecounts, 0, 180 * sizeof(uint));

	// get FITS table definition...
	table = usnob_fits_get_table();

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
			FILE* fid;
			int slice;

			// debug
			off_t offset1;
			off_t offset2;

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

			if (!fids[hp]) {
				if (init_fits_file(fids, headers, outfn, hp, table, Nside)) {
					fprintf(stderr, "Failed to initialized FITS file %i.\n", hp);
					exit(-1);
				}
			}

			fid = fids[hp];
			hprecords[hp]++;

			// debug
			offset1 = ftello(fid);

			if (usnob_fits_write_entry(fid, &entry)) {
				fprintf(stderr, "Failed to write FITS entry.\n");
				exit(-1);
			}

			// debug
			offset2 = ftello(fid);
			assert((offset2 - offset1) == table->tab_w);

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
		qfits_header* header;
		qfits_header* tablehdr;
		char val[256];
		FILE* fid;
		off_t offset;
	
		fid = fids[i];
		if (!fid) continue;
		header = headers[i];

		offset = ftello(fid);
		fseeko(fid, 0, SEEK_SET);

		sprintf(val, "%u", hprecords[i]);
		qfits_header_mod(header, "NOBJS", val, "Number of objects in this catalog.");
		qfits_header_dump(header, fid);
        qfits_header_destroy(header);

		table->nr = hprecords[i];
		tablehdr = qfits_table_ext_header_default(table);
		qfits_header_dump(tablehdr, fid);
		qfits_header_destroy(tablehdr);

		fseek(fid, offset, SEEK_SET);

		fits_pad_file(fid);

		if (fclose(fid)) {
			fprintf(stderr, "Failed to close file %i: %s\n", i, strerror(errno));
		}
	}

	printf("Read %u files, %u records, %u observations.\n",
		   nfiles, nrecords, nobs);

	qfits_table_close(table);

	free(headers);
	free(fids);
	free(hprecords);
	
	return 0;
}
