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
	{"xyls",        required_argument, 0, 'x'},
	{"width",       required_argument, 0, 'W'},
	{"height",      required_argument, 0, 'H'},
	{"scale-low",	required_argument, 0, 'L'},
	{"scale-high",	required_argument, 0, 'U'},
	{"scale-units", required_argument, 0, 'u'},
	{"no-tweak",    no_argument,       0, 'T'},
	{"no-guess-scale", no_argument,       0, 'G'},
	{"tweak-order", required_argument, 0, 't'},
	{"dir",         required_argument, 0, 'd'},
	{"backend-config", required_argument, 0, 'c'},
	{0, 0, 0, 0}
};

static const char* OPTIONS = "hi:L:U:u:t:d:c:Tx:W:H:G";

static void print_help(const char* progname) {
	printf("Usage:   %s [options]\n"
		   "  (   --image <filename>   the image to solve   (-i)\n"
		   "   OR --xyls  <filename> ) FITS table of source positions to solve   (-l)\n"
		   "  [--dir <directory>]: place all output files in this directory\n"
		   "  [--scale-units <units>]: in what units are the lower and upper bound specified?   (-u)\n"
		   "     choices:  \"degwidth\"    : width of the image, in degrees\n"
		   "               \"arcminwidth\" : width of the image, in arcminutes\n"
		   "               \"arcsecperpix\": arcseconds per pixel\n"
		   "  [--scale-low  <number>]: lower bound of image scale estimate   (-L)\n"
		   "  [--scale-high <number>]: upper bound of image scale estimate   (-U)\n"
		   "  [--width  <number>]: (mostly for xyls inputs): the original image width   (-W)\n"
		   "  [--height <number>]: (mostly for xyls inputs): the original image height  (-H)\n"
		   "  [--no-tweak]: don't fine-tune WCS by computing a SIP polynomial\n"
		   "  [--no-guess-scale]: don't try to guess the image scale from the FITS headers  (-G)\n"
		   "  [--tweak-order <integer>]: polynomial order of SIP WCS corrections\n"
		   "  [--backend-config <filename>]: use this config file for the \"backend\" program\n"
		   //"  [-h / --help]: print this help.\n"
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
	char* xyls = NULL;
	char* infn;
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
	bool guess_scale = TRUE;
	int width = 0, height = 0;

	lowlevelargs = pl_new(16);
	pl_append(lowlevelargs, "low-level-frontend");

    backendargs = pl_new(16);
    pl_append(backendargs, "backend");

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
		case 'G':
			guess_scale = FALSE;
			break;
		case 'W':
			width = atoi(optarg);
			break;
		case 'H':
			height = atoi(optarg);
			break;
		case 'T':
			pl_append(lowlevelargs, "--no-tweak");
			break;
		case 'L':
			pl_append(lowlevelargs, "--scale-low");
			pl_append(lowlevelargs, optarg);
			break;
		case 'U':
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
		case 'c':
			pl_append(backendargs, "--config");
			pl_append(backendargs, optarg);
			break;
		case 'd':
			outdir = optarg;
			break;
		case 'i':
			image = optarg;
			break;
		case 'x':
			xyls = optarg;
			break;
		}
	}

	if (!(image || xyls)) {
		fprintf(stderr, "You must specify an image or xyls file.\n");
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

	if (image) {
		pl_append(lowlevelargs, "--image");
		pl_append(lowlevelargs, image);
		infn = image;
	} else {
		pl_append(lowlevelargs, "--xyls");
		pl_append(lowlevelargs, xyls);
		infn = xyls;
	}

	if (width) {
		// HACK - memleak
		asprintf(&buf, "%i", width);
		pl_append(lowlevelargs, "--width");
		pl_append(lowlevelargs, buf);
	}
	if (height) {
		asprintf(&buf, "%i", height);
		pl_append(lowlevelargs, "--height");
		pl_append(lowlevelargs, buf);
	}

	cpy = strdup(infn);
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

	if (guess_scale)
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

	printf("Running backend:\n  %s\n", cmd);

    if (run_command_get_outputs(cmd, NULL, NULL, &errmsg)) {
        fprintf(stderr, "Couldn't run \"backend\": %s\n", errmsg);
        exit(-1);
    }

    pl_free(backendargs);

	return 0;
}

