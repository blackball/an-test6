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
#include <assert.h>

#include "scriptutils.h"
#include "ioutils.h"

char* escape_filename(const char* fn) {
    char* escape = "|&;()<> \t\n\\'\"";
    int nescape = 0;
    int len = strlen(fn);
    int i;
    char* result;
    int j;

    for (i=0; i<len; i++) {
        char* cp = strchr(escape, fn[i]);
        if (!cp) continue;
        nescape++;
    }
    result = malloc(len + nescape + 1);
    for (i=0, j=0; i<len; i++, j++) {
        char* cp = strchr(escape, fn[i]);
        if (!cp) {
            result[j] = fn[i];
        } else {
            result[j] = '\\';
            j++;
            result[j] = fn[i];
        }
    }
    assert(j == (len + nescape));
    result[j] = '\0';
    return result;
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

