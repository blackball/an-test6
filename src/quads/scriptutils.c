/*
 This file is part of the Astrometry.net suite.
 Copyright 2007 Dustin Lang, Keir Mierle and Sam Roweis.

 The Astrometry.net suite is free software; you can redistribute
 it and/or modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation, version 2.

 The Astrometry.net suite is distributed in the hope that it will be
 useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with the Astrometry.net suite ; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA	 02110-1301 USA
 */

#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

#include "scriptutils.h"
#include "ioutils.h"

int parse_depth_string(il* depths, const char* str) {
    // -10,10-20,20-30,40-
    for (; str && *str;) {
        unsigned int lo, hi;
        int nread;
        lo = hi = 0;
        if (sscanf(str, "%u-%u", &lo, &hi) == 2) {
            sscanf(str, "%*u-%*u%n", &nread);
        } else if (sscanf(str, "%u-", &lo) == 1) {
            sscanf(str, "%*u-%n", &nread);
        } else if (sscanf(str, "-%u", &hi) == 1) {
            sscanf(str, "-%*u%n", &nread);
        } else if (sscanf(str, "%u", &hi) == 1) {
            sscanf(str, "%*u%n", &nread);
        } else {
            return -1;
        }
        il_append(depths, lo);
        il_append(depths, hi);
        str += nread;
        while ((*str == ',') || isspace(*str))
            str++;
    }
    return 0;
}

char* get_path(const char* prog, const char* me) {
    char* cpy;
    char* dir;
    char* path;

    // First look for it in this directory.
    if (me) {
        cpy = strdup(me);
        dir = strdup(dirname(cpy));
        free(cpy);
        asprintf_safe(&path, "%s/%s", dir, prog);
        free(dir);
    } else {
        asprintf_safe(&path, "./%s", prog);
    }
    if (file_executable(path))
        return path;

    // Otherwise, let the normal PATH-search machinery find it...
    free(path);
    path = strdup(prog);
    return path;
}

char* create_temp_file(char* fn, char* dir) {
    char* tempfile;
    int fid;
    asprintf_safe(&tempfile, "%s/tmp.%s.XXXXXX", dir, fn);
    fid = mkstemp(tempfile);
    if (fid == -1) {
        fprintf(stderr, "Failed to create temp file: %s\n", strerror(errno));
        exit(-1);
    }
    close(fid);
    return tempfile;
}

