/*
  This file is part of the Astrometry.net suite.
  Copyright 2006, 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <libgen.h>
#include <ctype.h>

#include "fileutil.h"
#include "xylist.h"
#include "bl.h"
#include "boilerplate.h"
#include "fitsioutils.h"

#define OPTIONS "qhdx:y:t:"

void print_help(char* progname) {
    printf("usage:\n"
           "  %s [options] <input-file> <output-file>\n"
           "    [-d]: write double format (float format is default).\n"
           "    [-x <name-of-x-column-to-write>] (default: X)\n"
           "    [-y <name-of-y-column-to-write>] (default: Y)\n"
           "    [-t <Astrometry.net filetype>] (default: %s)\n"
           "    [-q]: quiet\n\n",
           progname, AN_FILETYPE_XYLS);
}

typedef pl xyarray;
#define mk_xyarray(n)         pl_new(n)
#define free_xyarray(l)       pl_free(l)
#define xya_ref(l, i)    (xy*)pl_get((l), (i))
#define xya_set(l, i, v)      pl_set((l), (i), (v))
#define xya_size(l)           pl_size(l)

static xyarray *readxysimple(char* fn);

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char** args) {
    char* infn = NULL;
    char* outfn = NULL;
    int c;
    xyarray* xya;
    xylist* ls;
    int i, N;
    int doubleformat = 0;
    char* xname = NULL;
    char* yname = NULL;
    char* antype = NULL;
    qfits_header* hdr;
    bool verbose = TRUE;

    while ((c = getopt(argc, args, OPTIONS)) != -1) {
        switch (c) {
        case '?':
        case 'h':
            print_help(args[0]);
            exit(0);
        case 'q':
            verbose = FALSE;
            break;
        case 'd':
            doubleformat = 1;
            break;
        case 'x':
            xname = optarg;
            break;
        case 'y':
            yname = optarg;
            break;
        case 't':
            antype = optarg;
            break;
        }
    }

    if ((optind+2) < argc) {
        print_help(args[0]);
        exit(-1);
    }

    infn = args[optind];
    outfn = args[optind + 1];

    if (!infn || !outfn) {
        print_help(args[0]);
        exit(-1);
    }

	if (verbose){
    	printf("input %s, output %s.\n", infn, outfn);
    	printf("reading input...\n");
	}

	xya = readxysimple(infn);
    if (!xya)
        exit(-1);

    ls = xylist_open_for_writing(outfn);
    if (!ls)
        exit(-1);
    if (doubleformat) {
        xylist_set_xtype(ls, TFITS_BIN_TYPE_D);
        xylist_set_ytype(ls, TFITS_BIN_TYPE_D);
    }
    hdr = xylist_get_header(ls);
    qfits_header_add(hdr, "WRITER", basename(args[0]), "This file was written by the program...", NULL);
    boilerplate_add_fits_headers(hdr);
    fits_add_long_comment(hdr, "Command line that produced this file:");
    fits_add_args(hdr, args, argc);
    fits_add_long_comment(hdr, "(end of command line)");

    if (xname)
        xylist_set_xname(ls, xname);
    if (yname)
        xylist_set_yname(ls, yname);
    if (xylist_write_header(ls))
        exit(-1);

    if (antype) {
        ls->antype = antype;
    }

    N = xya_size(xya);
	if (verbose){
    	printf("writing %i fields...\n", N);
	}
    for (i=0; i<N; i++) {
        int j, M;
        xy* list = xya_ref(xya, i);
        if (!list) {
            fprintf(stderr, "list %i is null.", i);
            continue;
        }
        xylist_write_new_field(ls);
        M = xy_size(list);
        for (j=0; j<M; j++) {
            double vals[2];
            vals[0] = xy_refx(list, j);
            vals[1] = xy_refy(list, j);
            xylist_write_entries(ls, vals, 1);
        }
        xylist_fix_field(ls);
    }
    xylist_fix_header(ls);
    xylist_close(ls);

    return 0;
}

// Read a simple xy file; column 1 is x column 2 is y;
// whitespace/comma seperated.
// Lines starting with "#" are ignored.
xyarray *readxysimple(char* fn) {
    uint numxy=0;
    xyarray *thepix = NULL;
    FILE* fid;
    int jj;
    dl* xylist;

    fid = fopen(fn, "r");
    if (!fid) {
        fprintf(stderr, "Couldn't open file %s to read xylist.\n", fn);
        return NULL;
    }

    xylist = dl_new(32);
    while (1) {
        char buf[1024];
        double x,y;
        char* cursor;
        int nread;
        if (!fgets(buf, sizeof(buf), fid)) {
            if (ferror(fid)) {
                fprintf(stderr, "Error reading from %s: %s\n", fn, strerror(errno));
                return NULL;
            }
            break;
        }
        cursor = buf;
        // skip leading whitespace.
        while (*cursor && isspace(*cursor))
            cursor++;
        if (!*cursor) {
            // empty line.
            continue;
        }
        // skip comments
        if (*cursor == '#')
            continue;
        // parse x
        if (sscanf(cursor, "%lg%n", &x, &nread) < 1) {
            fprintf(stderr, "Failed to parse a floating-point number from line: \"%s\"\n", cursor);
            return NULL;
        }
        cursor += nread;
        // skip whitespace or comma
        while (*cursor && (isspace(*cursor) || (*cursor == ',')))
            cursor++;
        if (!*cursor) {
            fprintf(stderr, "Premature end-of-line on line: \"%s\"\n", buf);
            return NULL;
        }
        // parse y
        if (sscanf(cursor, "%lg%n", &y, &nread) < 1) {
            fprintf(stderr, "Failed to parse a floating-point number from line: \"%s\"\n", cursor);
            return NULL;
        }
        //cursor += nread;
        dl_append(xylist, x);
        dl_append(xylist, y);
        numxy++;
    }
    thepix = mk_xyarray(1);

    xya_set(thepix, 0, mk_xy(numxy) );

    if (xya_ref(thepix, 0) == NULL) {
        fprintf(stderr, "ERROR (readxy) - out of memory at field %u\n", 0);
        free_xyarray(thepix);
        return NULL;
    }
    for (jj = 0; jj < numxy; jj++) {
        xy_setx(xya_ref(thepix, 0), jj, dl_get(xylist, jj*2));
        xy_sety(xya_ref(thepix, 0), jj, dl_get(xylist, jj*2+1));
    }
    free(xylist);
    return thepix;
}
