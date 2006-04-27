#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

#include "usnob.h"
#include "qfits.h"

#define OPTIONS "hr:R:d:D:o:"

void print_help(char* progname) {
    printf("usage:\n"
		   "  %s -o <output-filename>\n"
		   "  [-r <minimum RA>]\n"
		   "  [-R <maximum RA>]\n"
		   "  [-d <minimum DEC>]\n"
		   "  [-D <maximum DEC>]\n"
		   "           In units of degrees.\n"
		   "           If your values are negative, add \"--\" in between the argument and value\n"
		   "           eg,   -r -- -100\n"
		   "  <input-file> [<input-file> ...]\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
	char* outfn = NULL;
	double ramin = 0.0;
	double ramax = 360.0;
	double decmin = -90.0;
	double decmax = 90.0;
    int c;
	int startoptind;
	uint nrecords, nobs, nfiles;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
		case '?':
        case 'h':
			print_help(args[0]);
			exit(0);
		case 'd':
			decmin = atof(optarg);
			break;
		case 'D':
			decmax = atof(optarg);
			break;
		case 'r':
			ramin = atof(optarg);
			break;
		case 'R':
			ramax = atof(optarg);
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

	nrecords = 0;
	nobs = 0;
	nfiles = 0;

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
			if (usnob_parse_entry(map + i, &entry)) {
				fprintf(stderr, "Failed to parse USNOB entry: offset %i in file %s.\n",
						i, infn);
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

	printf("Read %u files, %u records, %u observations.\n",
		   nfiles, nrecords, nobs);
	
	return 0;
}
