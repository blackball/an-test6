#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "starutil.h"
#include "mathutil.h"

const char* OPTIONS = "hum:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s <solved-file> ...\n"
			"    [-u]: print UNsolved fields\n"
			"    [-m <max-field>]: for unsolved mode, max field number.\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    int argchar;
	char* progname = args[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	int i;
	bool unsolved = FALSE;
	int maxfield = 0;

    while ((argchar = getopt (argc, args, OPTIONS)) != -1) {
		switch (argchar) {
		case 'u':
			unsolved = TRUE;
			break;
		case 'm':
			maxfield = atoi(optarg);
			break;
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}
	if (optind < argc) {
		ninputfiles = argc - optind;
		inputfiles = args + optind;
	} else {
		printHelp(progname);
		exit(-1);
	}

	for (i=0; i<ninputfiles; i++) {
		FILE* fid;
		off_t off;
		bool* map;
		int mapsize;
		int j;
		int lim;
		fid = fopen(inputfiles[i], "rb");
		if (!fid) {
			fprintf(stderr, "Failed to open input file %s: %s\n",
					inputfiles[i], strerror(errno));
			exit(-1);
		}
		fseeko(fid, 0, SEEK_END);
		off = ftello(fid);
		mapsize = off;
		map = mmap(NULL, mapsize, PROT_READ, MAP_SHARED, fileno(fid), 0);
		if (map == MAP_FAILED) {
			fprintf(stderr, "Failed to mmap input file %s: %s\n",
					inputfiles[i], strerror(errno));
			exit(-1);
		}
		fclose(fid);
		printf("File %s\n", inputfiles[i]);
		if (maxfield)
			lim = imin(maxfield, mapsize);
		else
			lim = mapsize;

		for (j=0; j<lim; j++)
			if (unsolved && !map[j])
				printf("%i ", j);
			else if (!unsolved && map[j])
				printf("%i ", j);
		if (unsolved)
			// all fields beyond the end of the file are unsolved.
			for (; j<maxfield; j++)
				printf("%i ", j);
		printf("\n");

		munmap(map, mapsize);
	}

	return 0;
}
