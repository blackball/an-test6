/*
 This file is part of the Astrometry.net suite.
 Copyright 2007 Dustin Lang.

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

#include "qfits.h"

extern char *optarg;
extern int optind, opterr, optopt;

const char* OPTIONS = "h";

static void printHelp(char* progname) {
    printf("Usage:   %s  <input> <output>\n"
           "\n", progname);
}

int main(int argc, char** args) {
    int argchar;
	char* infn = NULL;
	char* outfn = NULL;
	char* progname = args[0];

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
        case '?':
        case 'h':
			printHelp(progname);
            return 0;
        default:
            return -1;
        }

    if (optind != argc-2) {
        printHelp(progname);
        exit(-1);
    }

    infn = args[optind];
    outfn = args[optind+1];


    return 0;
}


