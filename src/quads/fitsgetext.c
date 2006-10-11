#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "qfits.h"
#include "bl.h"

char* OPTIONS = "he:i:o:b";

void printHelp(char* progname) {
	printf("%s    -i <input-file>\n"
		   "      -o <output-file>\n"
		   "      [-b]: print sizes and offsets in FITS blocks (of 2880 bytes)\n"
		   "      -e <extension-number> ...\n\n",
		   progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;

	char* infn = NULL;
	char* outfn = NULL;
	FILE* fin = NULL;
	FILE* fout = NULL;
	il* exts;
	int i;
	char* map;
	int mapsize;
	char* progname = argv[0];
	int inblocks = 0;

	exts = il_new(16);

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'b':
			inblocks = 1;
			break;
        case 'e':
			il_append(exts, atoi(optarg));
            break;
        case 'i':
			infn = optarg;
			break;
        case 'o':
			outfn = optarg;
			break;
        case '?':
        case 'h':
			printHelp(progname);
            return 0;
        default:
            return -1;
        }

	if (infn && !outfn) {
		int next = qfits_query_n_ext(infn);
		if (next == -1) {
			fprintf(stderr, "Couldn't determine how many extensions are in file %s.\n", infn);
		} else {
			printf("File %s contains %i FITS extensions.\n", infn, next);
			for (i=0; i<=next; i++) {
				int hdrstart, hdrlen, datastart, datalen;
				if (qfits_get_hdrinfo(infn, i, &hdrstart,  &hdrlen ) ||
					qfits_get_datinfo(infn, i, &datastart, &datalen)) {
					fprintf(stderr, "Error getting extents of extension %i.\n", i);
					exit(-1);
				}
				if (inblocks) {
					printf("Extension %i : header start %i , length %i ; data start %i , length %i blocks.\n",
						   i, hdrstart/FITS_BLOCK_SIZE, hdrlen/FITS_BLOCK_SIZE,
						   datastart/FITS_BLOCK_SIZE, datalen/FITS_BLOCK_SIZE);
				} else {
					printf("Extension %i : header start %i , length %i ; data start %i , length %i .\n",
						   i, hdrstart, hdrlen, datastart, datalen);
				}
			}
		}
		exit(0);
	}

	if (!infn || !outfn || !il_size(exts)) {
		printHelp(progname);
		exit(-1);
	}

	if (infn) {
		fin = fopen(infn, "rb");
		if (!fin) {
			fprintf(stderr, "Failed to open input file %s: %s\n", infn, strerror(errno));
			exit(-1);
		}
	}

	if (outfn) {
		fout = fopen(outfn, "wb");
		if (!fout) {
			fprintf(stderr, "Failed to open output file %s: %s\n", outfn, strerror(errno));
			exit(-1);
		}
	}

	fseeko(fin, 0, SEEK_END);
	mapsize = ftello(fin);
	fseeko(fin, 0, SEEK_SET);
	map = mmap(NULL, mapsize, PROT_READ, MAP_SHARED, fileno(fin), 0);
	if (map == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap input file: %s\n", strerror(errno));
		exit(-1);
	}
	fclose(fin);

	for (i=0; i<il_size(exts); i++) {
		int hdrstart, hdrlen, datastart, datalen;
		int ext = il_get(exts, i);

		if (qfits_get_hdrinfo(infn, ext, &hdrstart,  &hdrlen ) ||
			qfits_get_datinfo(infn, ext, &datastart, &datalen)) {
			fprintf(stderr, "Error getting extents of extension %i.\n", ext);
			exit(-1);
		}
		if (inblocks)
			printf("Writing extension %i: header start %i, length %i, data start %i, length %i blocks.\n",
				   ext, hdrstart/FITS_BLOCK_SIZE, hdrlen/FITS_BLOCK_SIZE, datastart/FITS_BLOCK_SIZE, datalen/FITS_BLOCK_SIZE);
		else
			printf("Writing extension %i: header start %i, length %i, data start %i, length %i.\n",
				   ext, hdrstart, hdrlen, datastart, datalen);
		
		if ((hdrlen  && (fwrite(map + hdrstart , 1, hdrlen , fout) != hdrlen )) ||
			(datalen && (fwrite(map + datastart, 1, datalen, fout) != datalen))) {
			fprintf(stderr, "Failed to write extension %i: %s\n", ext, strerror(errno));
			exit(-1);
		}
	}

	munmap(map, mapsize);
	fclose(fout);
	il_free(exts);
	return 0;
}
