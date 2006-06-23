#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "qfits.h"
#include "bl.h"
#include "fitsioutils.h"

char* OPTIONS = "hc:i:o:";

void printHelp(char* progname) {
	printf("%s    -i <input-file>\n"
		   "      -o <output-file>\n"
		   "      -c <column-name> ...\n\n",
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
	pl* cols;
	char* progname = argv[0];
	int nextens;
	int ext;
	int NC;
	int start, size;
	char* buf;

	qfits_table* outtable;

	cols = pl_new(16);

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1)
        switch (argchar) {
        case 'c':
			pl_append(cols, optarg);
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

	if (!infn || !outfn || !pl_size(cols)) {
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

	qfits_err_statset(1);

	// copy the main header exactly.
	if (qfits_get_hdrinfo(infn, 0, &start, &size)) {
		fprintf(stderr, "Couldn't get main header.\n");
		exit(-1);
	}
	buf = malloc(size);
	if (fread(buf, 1, size, fin) != size) {
		fprintf(stderr, "Error reading main header: %s\n", strerror(errno));
		exit(-1);
	}
	if (fwrite(buf, 1, size, fout) != size) {
		fprintf(stderr, "Error writing main header: %s\n", strerror(errno));
		exit(-1);
	}

	NC = pl_size(cols);
	nextens = qfits_query_n_ext(infn);
	for (ext=1; ext<=nextens; ext++) {
		int c2, c;
		int columns[NC];
		int sizes[NC];
		int offsets[NC];
		int offset = 0;
		int off, n;
		int totalsize = 0;
		unsigned char* buffer;
		const int BLOCK=1000;
		qfits_table* table;
		qfits_header* header;
		qfits_header* tablehdr;

		if (!qfits_is_table(infn, ext)) {
			fprintf(stderr, "extention %i isn't a table.\n", ext);
			// HACK - just copy it.
			return -1;
		}
		table = qfits_table_open(infn, ext);
		if (!table) {
			fprintf(stderr, "failed to open table: file %s, extension %i.\n", infn, ext);
			return -1;
		}
		header = qfits_header_readext(infn, ext);
		if (!header) {
			fprintf(stderr, "failed to read header: extension %i\n", ext);
			exit(-1);
		}

		outtable = qfits_table_new(outfn, QFITS_BINTABLE, 0, NC, table->nr);
		outtable->tab_w = 0;
		
		for (c=0; c<pl_size(cols); c++) {
			columns[c] = -1;
		}
		for (c=0; c<pl_size(cols); c++) {
			char* colname = pl_get(cols, c);
			for (c2=0; c2<table->nc; c2++) {
				qfits_col* col = table->col + c2;
				if (strcasecmp(col->tlabel, colname))
					continue;
				columns[c] = c2;
				sizes[c] = col->atom_nb * col->atom_size;
				offsets[c] = offset;
				offset += sizes[c];

				qfits_col_fill(outtable->col + c,
							   col->atom_nb, col->atom_dec_nb,
							   col->atom_size, col->atom_type,
							   col->tlabel, col->tunit,
							   col->nullval, col->tdisp,
							   col->zero_present,
							   col->zero,
							   col->scale_present,
							   col->scale,
							   outtable->tab_w);
				outtable->tab_w += sizes[c];

				break;
			}
		}
		for (c=0; c<pl_size(cols); c++) {
			if (columns[c] == -1) {
				fprintf(stderr, "Extension %i: failed to find column named %s\n",
						ext, (char*)pl_get(cols, c));
				exit(-1);
			}
		}
		totalsize = offset;

		tablehdr = qfits_table_ext_header_default(outtable);
		// add any headers from the original table that aren't part of the BINTABLE extension.
		{
			char key[FITS_LINESZ+1];
			char val[FITS_LINESZ+1];
			char com[FITS_LINESZ+1];
			char lin[FITS_LINESZ+1];
			int i;
			for (i=0; i<header->n; i++) {
				qfits_header_getitem(header, i, key, val, com, lin);
				if (!strcasecmp(key, "XTENSION") ||
					!strcasecmp(key, "BITPIX") ||
					!strncasecmp(key, "NAXIS...", 5) ||
					!strcasecmp(key, "PCOUNT") ||
					!strcasecmp(key, "GCOUNT") ||
					!strcasecmp(key, "TFIELDS") ||
					!strncasecmp(key, "TFORM...", 5) ||
					!strncasecmp(key, "TTYPE...", 5) ||
					!strncasecmp(key, "TUNIT...", 5) ||
					!strncasecmp(key, "TNULL...", 5) ||
					!strncasecmp(key, "TSCAL...", 5) ||
					!strncasecmp(key, "TZERO...", 5) ||
					!strncasecmp(key, "TDISP...", 5) ||
					!strncasecmp(key, "THEAP...", 5) ||
					!strncasecmp(key, "TDIM...", 4) ||
					!strcasecmp(key, "END"))
					continue;
				qfits_header_add(tablehdr, key, val, com, lin);
			}
		}

		qfits_header_dump(tablehdr, fout);
		qfits_header_destroy(tablehdr);

		buffer = malloc(totalsize * BLOCK);

		for (off=0; off<table->nr; off+=n) {
			if (off + BLOCK > table->nr)
				n = table->nr - off;
			else
				n = BLOCK;
			for (c=0; c<pl_size(cols); c++)
				qfits_query_column_seq_to_array(table, columns[c], off, n, buffer + offsets[c], totalsize);
			if (fwrite(buffer, totalsize, n, fout) != n) {
				fprintf(stderr, "Error writing a block of data: ext %i: %s\n", ext, strerror(errno));
				exit(-1);
			}
		}

		qfits_table_close(outtable);
		free(buffer);

		fits_pad_file(fout);

		qfits_header_destroy(header);
		qfits_table_close(table);
	}

	pl_free(cols);
	return 0;
}
