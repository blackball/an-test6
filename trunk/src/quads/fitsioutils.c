#include <string.h>

#include "qfits.h"
#include "fitsioutils.h"
#include "ioutils.h"

// how many FITS blocks are required to hold 'size' bytes?
int fits_blocks_needed(int size) {
	size += FITS_BLOCK_SIZE - 1;
	return size - (size % FITS_BLOCK_SIZE);
}

char fits_endian_string[16];
int  fits_endian_string_inited = 0;

static void fits_init_endian_string() {
    if (!fits_endian_string_inited) {
        fits_endian_string_inited = 1;
        uint endian = ENDIAN_DETECTOR;
        unsigned char* cptr = (unsigned char*)&endian;
        sprintf(fits_endian_string, "%02x:%02x:%02x:%02x", (uint)cptr[0], (uint)cptr[1], (uint)cptr[2], (uint)cptr[3]);
    }
}

void fits_fill_endian_string(char* str) {
    fits_init_endian_string();
    strcpy(str, fits_endian_string);
}

char* fits_get_endian_string() {
    fits_init_endian_string();
    return fits_endian_string;
}

void fits_add_endian(qfits_header* header) {
	qfits_header_add(header, "ENDIAN", fits_get_endian_string(), "Endianness detector: u32 0x01020304 written ", NULL);
	qfits_header_add(header, "", NULL, " in the order it is stored in memory.", NULL);
}

int fits_find_table_column(char* fn, char* colname, int* pstart, int* psize) {
    int i, nextens, start, size;
    int istable;

	nextens = qfits_query_n_ext(fn);
	for (i=0; i<=nextens; i++) {
        qfits_table* table;
		if (qfits_get_datinfo(fn, i, &start, &size) == -1) {
			fprintf(stderr, "error getting start/size for ext %i.\n", i);
            return -1;
        }
		fprintf(stderr, "ext %i starts at %i, has size %i.\n", i, start, size);
		/*
          fprintf(stderr, "ext %i is %sa table.\n", i,
          (istable ? "" : "not "));
        */
		if (!qfits_is_table(fn, i))
            continue;
        table = qfits_table_open(fn, i);
        int c;
        for (c=0; c<table->nc; c++) {
            qfits_col* col = table->col + c;
            //fprintf(stderr, "  col %i: %s\n", c, col->tlabel);
            if (strcmp(col->tlabel, colname) == 0) {
                *pstart = start;
                *psize = size;
                qfits_table_close(table);
                return 0;
            }
        }
        qfits_table_close(table);
    }
    return -1;
}

void fits_add_uint_size(qfits_header* header) {
    char val[8];
	sprintf(val, "%i", sizeof(uint));
	qfits_header_add(header, "UINT_SZ", val, "sizeof(uint)", NULL);
}

void fits_add_double_size(qfits_header* header) {
    char val[8];
	sprintf(val, "%i", sizeof(double));
	qfits_header_add(header, "DUBL_SZ", val, "sizeof(double)", NULL);
}

int fits_check_uint_size(qfits_header* header) {
    int uintsz;
	uintsz = qfits_header_getint(header, "UINT_SZ", -1);
	if (sizeof(uint) != uintsz) {
		fprintf(stderr, "File was written with sizeof(uint)=%i, but currently sizeof(uint)=%i.\n",
				uintsz, sizeof(uint));
        return -1;
	}
    return 0;
}

int fits_check_double_size(qfits_header* header) {
    int doublesz;
	doublesz = qfits_header_getint(header, "DUBL_SZ", -1);
	if (sizeof(double) != doublesz) {
		fprintf(stderr, "File was written with sizeof(double)=%i, but currently sizeof(double)=%i.\n",
				doublesz, sizeof(double));
        return -1;
	}
    return 0;
}

int fits_check_endian(qfits_header* header) {
	char endian[256];
    char* str;
	snprintf(endian, sizeof(endian), "%s", qfits_header_getstr(header, "ENDIAN"));
	// "endian" contains ' characters around the string, like
	// '04:03:02:01', so we start at index 1, and only compare the length of
	// str.
    str = fits_get_endian_string();
	if (strncmp(str, endian+1, strlen(str)) != 0) {
		fprintf(stderr, "File was written with endianness %s, this machine has endianness %s.\n", endian, str);
		return -1;
	}
    return 0;
}
