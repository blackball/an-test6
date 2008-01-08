/*
  This file is part of the Astrometry.net suite.
  Copyright 2008 Dustin Lang.

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
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "gnu-specific.h"
#include "gnu-specific-config.h"

#if NEED_CANONICALIZE_FILE_NAME
char* canonicalize_file_name(const char* fn) {
    char* path = malloc(1024);
    char* canon;
    canon = realpath(fn, path);
    if (!canon) {
        free(path);
        return NULL;
    }
    path = realloc(path, strlen(path) + 1);
    return path;
}
#endif

