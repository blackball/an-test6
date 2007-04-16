/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

  The Astrometry.net suite is free software; you can redistribute
  it and/or modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation, version 2.

  The Astrometry.net suite is distributed in the hope that it will be
  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with the Astrometry.net suite ; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
*/

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "qfits.h"
#include "bl.h"

char* OPTIONS = "he:i:o:ba";

void printHelp(char* progname) {
	printf("%s    -i <input-file>\n"
		   "      -o <output-file>\n"
		   "      [-a]: write out ALL extensions; the output filename should be\n"
		   "            a \"sprintf\" pattern such as  \"extension-%%04i\".\n"
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
	int allexts = 0;
	int Next = -1;

	exts = il_new(16);

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
		case 'a':
			allexts = 1;
			break;
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

	if (infn) {
		Next = qfits_query_n_ext(infn);
		if (Next == -1) {
			fprintf(stderr, "Couldn't determine how many extensions are in file %s.\n", infn);
			exit(-1);
		} else {
			printf("File %s contains %i FITS extensions.\n", infn, Next);
		}
	}

	if (infn && !outfn) {
		for (i=0; i<=Next; i++) {
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
		exit(0);
	}

	if (!infn || !outfn || !(il_size(exts) || allexts)) {
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

	if (!allexts) {
		fout = fopen(outfn, "wb");
		if (!fout) {
			fprintf(stderr, "Failed to open output file %s: %s\n", outfn, strerror(errno));
			exit(-1);
		}
	} else
		for (i=0; i<=Next; i++)
			il_append(exts, i);

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

		if (allexts) {
			char fn[256];
			sprintf(fn, outfn, ext);
			fout = fopen(fn, "wb");
			if (!fout) {
				fprintf(stderr, "Failed to open output file %s: %s\n", fn, strerror(errno));
				exit(-1);
			}
		}

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

		if (allexts)
			if (fclose(fout)) {
				fprintf(stderr, "Failed to close output file: %s\n", strerror(errno));
				exit(-1);
			}
	}

	munmap(map, mapsize);
	if (!allexts)
		fclose(fout);
	il_free(exts);
	return 0;
}
