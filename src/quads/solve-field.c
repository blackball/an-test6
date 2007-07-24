/*
  This file is part of the Astrometry.net suite.
  Copyright 2007 Dustin Lang, Keir Mierle and Sam Roweis.

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


/**
   A command-line interface to the blind solver system.

   eg:

   solve-a-field --image mypic.fits --tweak-order 4 \
     --scale-low 2 --scale-high 4 --scale-units degwide --dir mypic-results

   must run:
   mkdir -p mypic-results
   low-level-frontend --guess-scale --image mypic.fits --scale-low 2 \
        --scale-high 4 --scale-units degwide --tweak-order 4 \
		--out mypic-results/mypic.axy \
        --match match.fits --solved solved --rdls rdls.fits \
		--wcs wcs.fits
   backend mypic-results/mypic.axy
   render-job mypic-results/mypic.axy
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <libgen.h>

#include <getopt.h>

#include "an-bool.h"
#include "bl.h"
#include "ioutils.h"

static struct option long_options[] = {
	// flags
	{"help",        no_argument,       0, 'h'},
	{"image",       required_argument, 0, 'i'},
	{"scale-low",	required_argument, 0, 'L'},
	{"scale-high",	required_argument, 0, 'H'},
	{"scale-units", required_argument, 0, 'u'},
	{"tweak-order", required_argument, 0, 't'},
	{"dir",         required_argument, 0, 'd'},
	{0, 0, 0, 0}
};

static const char* OPTIONS = "hi:L:H:u:t:d:";

static void print_help(const char* progname) {
	printf("Usage:   %s [options]\n"
	       "\n", progname);
}

static void add_file_arg(pl* args, char* argname, char* file, char* dir) {
    char* addpath;
    pl_append(args, argname);
    // HACK - we leak this strdup()'d memory...
    if (dir) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, file);
        addpath = strdup(path);
    } else {
        addpath = strdup(file);
    }
    pl_append(args, addpath);
}

int main(int argc, char** args) {
	int c;
	bool help = FALSE;
	pl* lowlevelargs;
	char* outdir = NULL;
	char* image = NULL;
	char* cpy;
	char* base;
	char axy[1024];
	char cmd[1024];
	char* buf;
	int len;
	int i;
	int rtn;
    pl* backendargs;
    char* errmsg;

	lowlevelargs = pl_new(16);
	pl_append(lowlevelargs, "low-level-frontend");

	while (1) {
		int option_index = 0;
		c = getopt_long(argc, args, OPTIONS, long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 0:
			/* If this option set a flag, do nothing else now. */
			if (long_options[option_index].flag != 0)
				break;
			break;
		case 'h':
			help = TRUE;
			break;
		case 'L':
			pl_append(lowlevelargs, "--scale-low");
			pl_append(lowlevelargs, optarg);
			break;
		case 'H':
			pl_append(lowlevelargs, "--scale-high");
			pl_append(lowlevelargs, optarg);
			break;
		case 'u':
			pl_append(lowlevelargs, "--scale-units");
			pl_append(lowlevelargs, optarg);
			break;
		case 't':
			pl_append(lowlevelargs, "--tweak-order");
			pl_append(lowlevelargs, optarg);
			break;
		case 'd':
			outdir = optarg;
			break;
		case 'i':
			image = optarg;
			break;
		}
	}

	if (!image) {
		fprintf(stderr, "You must specify an image file.\n");
		help = 1;
	}
	if (help) {
		print_help(args[0]);
		exit(0);
	}

    if (outdir) {
        snprintf(cmd, sizeof(cmd), "mkdir -p %s", outdir);
        rtn = system(cmd);
        if (rtn == -1) {
            fprintf(stderr, "Failed to system() mkdir.\n");
            exit(-1);
        }
        rtn = WEXITSTATUS(rtn);
        if (rtn) {
            fprintf(stderr, "mkdir failed: status %i.\n", rtn);
            exit(-1);
        }
        /*
         struct stat st;
         if (stat(outdir, &st)) {
         if (errno == ENOENT) {
         // try to mkdir it.
         */
    }

	pl_append(lowlevelargs, "--image");
	pl_append(lowlevelargs, image);

	cpy = strdup(image);
	base = strdup(basename(cpy));
	free(cpy);
	if (outdir)
		snprintf(axy, sizeof(axy), "%s/%s.axy", outdir, base);
    else
		snprintf(axy, sizeof(axy), "%s.axy", base);
	free(base);

	pl_append(lowlevelargs, "--out");
	pl_append(lowlevelargs, axy);

    add_file_arg(lowlevelargs, "--match",  "match.fits", outdir);
    add_file_arg(lowlevelargs, "--rdls",   "rdls.fits",  outdir);
    add_file_arg(lowlevelargs, "--solved", "solved",     outdir);
    add_file_arg(lowlevelargs, "--wcs",    "wcs.fits",   outdir);

	pl_append(lowlevelargs, "--guess-scale");

	// pnm?

	buf = cmd;
	len = sizeof(cmd);
	for (i=0; i<pl_size(lowlevelargs); i++) {
		int nw = snprintf(buf, len, "%s%s", (i ? " " : ""),
						  (char*)pl_get(lowlevelargs, i));
		if (nw > len) {
			fprintf(stderr, "Command line too long.\n");
			exit(-1);
		}
		buf += nw;
		len -= nw;
	}

	printf("Running low-level-frontend:\n  %s\n", cmd);
	rtn = system(cmd);
	if (rtn == -1) {
		fprintf(stderr, "Failed to system() low-level-frontend: %s\n", strerror(errno));
		exit(-1);
	}
	if (WEXITSTATUS(rtn)) {
		fprintf(stderr, "low-level-frontend exited with exit status %i.\n", WEXITSTATUS(rtn));
		exit(-1);
	}

	pl_free(lowlevelargs);

    backendargs = pl_new(16);

    pl_append(backendargs, "backend");
    pl_append(backendargs, axy);

	buf = cmd;
	len = sizeof(cmd);
	for (i=0; i<pl_size(backendargs); i++) {
		int nw = snprintf(buf, len, "%s%s", (i ? " " : ""),
						  (char*)pl_get(backendargs, i));
		if (nw > len) {
			fprintf(stderr, "Command line too long.\n");
			exit(-1);
		}
		buf += nw;
		len -= nw;
	}

    if (run_command_get_outputs(cmd, NULL, NULL, &errmsg)) {
        fprintf(stderr, "Couldn't run \"backend\": %s\n", errmsg);
        exit(-1);
    }

    pl_free(backendargs);

	return 0;
}

