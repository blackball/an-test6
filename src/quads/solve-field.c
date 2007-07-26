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


TODO:

(0) To me, it is important that if you type:
"solve-field myfile.png" and NOTHING else, it should Just Work.

(1) it assumes you have "." in your path, which I never do.

> Right, so what should it do: look at args[0] to figure out where the other
> executables are?

(2) It assumes you have netpbm tools installed which the main build
doesn't require.

> I think it will only complain if it needs one of the netpbm programs to do
> its work - and it cannot do anything sensible (except print a friendly
> error message) if they don't exist.

(3) Ok - maybe if you don't specify any filenames it can expect to read them
> on stdin - that way you can pipe it up with find:
>
> find dir | solve-field
> cat filelist | solve-field

 (4) What should do if one of the files already exists - bail out, but have a
> command-line flag that makes it overwrite them if they already exist?

I'd say overwrite it, unless a flag --noclobber is activated

(5) by default, we produce:
- thumbnails myfile-objs.png (source extraction)
- myfile-idx.png (sources we used to solve and index objects we found)
- myfile-ngc.png (ngc labels)
   -- but there should be a flag (e.g --pngs=off) to supress this
- myfile.wcs.fits fits header

* by default, we do not produce an entirely new fits file but this can
be turned on

 (6) * by default, the output files corresponding to each source file go in
the same directory as the source file, but you can invoke a flag to
dump them somewhere else

(7) * by default, we output to stdout a single line for each file something like:
myimage.png: unsolved using X field objects
or
myimage.png: solved using X field objects, RA=rr,DEC=dd, size=AxB
pixels=UxV arcmin


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
//#include "xylist.h"

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
		   "   OR --xyls  <filename>  FITS table of source positions to solve   (-l)\n"
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

static void add_file_arg(sl* args, const char* argname, const char* file,
						 const char* dir) {
    sl_append(args, argname);
	if (dir)
		sl_appendf(args, "%s/%s", dir, file);
	else
		sl_append(args, file);
}

int main(int argc, char** args) {
	int c;
	bool help = FALSE;
	sl* lowlevelargs;
	char* outdir = NULL;
	char* image = NULL;
	char* xyls = NULL;
	char* infn;
	char* cpy;
	char* base;
	char axy[1024];
	char* cmd;
	int i;
	int rtn;
    sl* backendargs;
    char* errmsg;
	bool guess_scale = TRUE;
	int width = 0, height = 0;
	//xylist* xy;

	lowlevelargs = sl_new(16);
	sl_append(lowlevelargs, "low-level-frontend");

    backendargs = sl_new(16);
    sl_append(backendargs, "backend");

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
			sl_append(lowlevelargs, "--no-tweak");
			break;
		case 'L':
			sl_append(lowlevelargs, "--scale-low");
			sl_append(lowlevelargs, optarg);
			break;
		case 'U':
			sl_append(lowlevelargs, "--scale-high");
			sl_append(lowlevelargs, optarg);
			break;
		case 'u':
			sl_append(lowlevelargs, "--scale-units");
			sl_append(lowlevelargs, optarg);
			break;
		case 't':
			sl_append(lowlevelargs, "--tweak-order");
			sl_append(lowlevelargs, optarg);
			break;
		case 'c':
			sl_append(backendargs, "--config");
			sl_append(backendargs, optarg);
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

	rtn = 0;
	if (!(image || xyls)) {
		fprintf(stderr, "You must specify an image or xyls file.\n");
		rtn = -1;
		help = 1;
	}
	if (optind != argc) {
		fprintf(stderr, "You specified arguments that I didn't understand:\n");
		for (i=optind; i<argc; i++) {
			fprintf(stderr, "  %s\n", args[i]);
		}
		fprintf(stderr, "\n");
		rtn = -1;
		help = 1;
	}
	if (help) {
		print_help(args[0]);
		exit(rtn);
	}

    if (outdir) {
		asprintf(&cmd, "mkdir -p %s", outdir);
        rtn = system(cmd);
		free(cmd);
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
		sl_append(lowlevelargs, "--image");
		sl_append(lowlevelargs, image);
		infn = image;
	} else {
		sl_append(lowlevelargs, "--xylist");
		sl_append(lowlevelargs, xyls);
		infn = xyls;
		/*
		  if (!width || !height) {
		  // Load the xylist and compute the min/max.
		  }
		*/
	}

	if (width) {
		sl_append(lowlevelargs, "--width");
		sl_appendf(lowlevelargs, "%i", width);
	}
	if (height) {
		sl_append(lowlevelargs, "--height");
		sl_appendf(lowlevelargs, "%i", height);
	}

	cpy = strdup(infn);
	base = strdup(basename(cpy));
	free(cpy);
	if (outdir)
		snprintf(axy, sizeof(axy), "%s/%s.axy", outdir, base);
    else
		snprintf(axy, sizeof(axy), "%s.axy", base);
	free(base);

	sl_append(lowlevelargs, "--out");
	sl_append(lowlevelargs, axy);

    add_file_arg(lowlevelargs, "--match",  "match.fits", outdir);
    add_file_arg(lowlevelargs, "--rdls",   "rdls.fits",  outdir);
    add_file_arg(lowlevelargs, "--solved", "solved",     outdir);
    add_file_arg(lowlevelargs, "--wcs",    "wcs.fits",   outdir);

	if (guess_scale)
		sl_append(lowlevelargs, "--guess-scale");

	// pnm?

	sl_print(lowlevelargs);

	cmd = sl_implode(lowlevelargs, " ");
	printf("Running low-level-frontend:\n  %s\n", cmd);
	rtn = system(cmd);
	free(cmd);
	if (rtn == -1) {
		fprintf(stderr, "Failed to system() low-level-frontend: %s\n", strerror(errno));
		exit(-1);
	}
	if (WEXITSTATUS(rtn)) {
		fprintf(stderr, "low-level-frontend exited with exit status %i.\n", WEXITSTATUS(rtn));
		exit(-1);
	}

	sl_free(lowlevelargs);

    sl_append(backendargs, axy);

	cmd = sl_implode(backendargs, " ");
	printf("Running backend:\n  %s\n", cmd);
    if (run_command_get_outputs(cmd, NULL, NULL, &errmsg)) {
        fprintf(stderr, "Couldn't run \"backend\": %s\n", errmsg);
        exit(-1);
    }
	free(cmd);

    sl_free(backendargs);

	return 0;
}

