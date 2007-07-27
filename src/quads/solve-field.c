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
#include "xylist.h"

static struct option long_options[] = {
	// flags
	{"help",        no_argument,       0, 'h'},
	{"width",       required_argument, 0, 'W'},
	{"height",      required_argument, 0, 'H'},
	{"scale-low",	required_argument, 0, 'L'},
	{"scale-high",	required_argument, 0, 'U'},
	{"scale-units", required_argument, 0, 'u'},
	{"no-tweak",    no_argument,       0, 'T'},
	{"no-guess-scale", no_argument,    0, 'G'},
	{"tweak-order", required_argument, 0, 't'},
	{"dir",         required_argument, 0, 'd'},
	{"backend-config", required_argument, 0, 'c'},
	{"overwrite",   no_argument,       0, 'O'},
	{0, 0, 0, 0}
};

static const char* OPTIONS = "hL:U:u:t:d:c:TW:H:GO";

static void print_help(const char* progname) {
	printf("Usage:   %s [options]\n"
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
           "  [--overwrite]: overwrite output files if they already exist.  (-O)\n"
           "\n"
           "  [<image-file-1> <image-file-2> ...] [<xyls-file-1> <xyls-file-2> ...]\n"
	       "\n", progname);
}

int main(int argc, char** args) {
	int c;
	bool help = FALSE;
	sl* lowlevelargs;
	char* outdir = NULL;
	char* image = NULL;
	char* xyls = NULL;
	char* cmd;
	int i, f;
	int rtn;
    sl* backendargs;
    char* errmsg;
	bool guess_scale = TRUE;
	int width = 0, height = 0;
    int nllargs;
    int nbeargs;
    bool fromstdin = FALSE;
    bool overwrite = FALSE;

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
		case 'h':
			help = TRUE;
			break;
        case 'O':
            overwrite = TRUE;
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
		}
	}
	rtn = 0;
	if (help) {
		print_help(args[0]);
		exit(rtn);
	}
    if (optind == argc) {
        printf("\nYou didn't specify any files to process, so I'm going to read a list of files on standard input...\n\n");
        fromstdin = TRUE;
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

    if (guess_scale)
        sl_append(lowlevelargs, "--guess-scale");
	// pnm?

    // number of low-level and backend args (not specific to a particular file)
    nllargs = sl_size(lowlevelargs);
    nbeargs = sl_size(backendargs);

    f = optind;
    while (1) {
        char fnbuf[1024];
        char* infile = NULL;
        bool isxyls;
        char* reason;
        int len;
        char* cpy;
        char* base;
        char *matchfn, *rdlsfn, *solvedfn, *wcsfn, *axyfn, *objsfn, *redgreenfn;
        char *ngcfn;
        sl* outfiles;
        bool nextfile;

        if (fromstdin) {
            if (!fgets(fnbuf, sizeof(fnbuf), stdin)) {
                if (ferror(stdin))
                    fprintf(stderr, "Failed to read a filename!\n");
                break;
            }
            len = strlen(fnbuf);
            if (fnbuf[len-1] == '\n')
                fnbuf[len-1] = '\0';
            infile = fnbuf;
        } else {
            if (f == argc)
                break;
            infile = args[f];
            f++;
        }

        sl_remove_from(lowlevelargs, nllargs);
        sl_remove_from(backendargs,  nbeargs);

        printf("Checking if file \"%s\" is xylist or image: ", infile);
        isxyls = xylist_is_file_xylist(image, NULL, NULL, &reason);
        printf(isxyls ? "xyls\n" : "image\n");
        if (!isxyls)
            printf("  (%s)\n", reason);

        if (isxyls) {
            xyls = infile;
            image = NULL;
        } else {
            xyls = NULL;
            image = infile;
        }

        if (image) {
            sl_append(lowlevelargs, "--image");
            sl_append(lowlevelargs, image);
        } else {
            sl_append(lowlevelargs, "--xylist");
            sl_append(lowlevelargs, xyls);
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

        // Choose the base path/filename for output files.
        cpy = strdup(infile);
        if (outdir)
            asprintf(&base, "%s/%s", outdir, basename(cpy));
        else
            base = strdup(basename(cpy));
        free(cpy);
        len = strlen(base);
        // trim .xxx / .xxxx
        if (len > 4) {
            if (base[len - 4] == '.')
                base[len-4] = '\0';
            if (base[len - 5] == '.')
                base[len-5] = '\0';
        }

        // Compute the output filenames.
        outfiles = sl_new(16);

        axyfn      = sl_appendf(outfiles, "%s.axy",      base);
        matchfn    = sl_appendf(outfiles, "%s.match",    base);
        rdlsfn     = sl_appendf(outfiles, "%s.rdls",     base);
        solvedfn   = sl_appendf(outfiles, "%s.solved",   base);
        wcsfn      = sl_appendf(outfiles, "%s.wcs",      base);
        objsfn     = sl_appendf(outfiles, "%s-objs.png", base);
        redgreenfn = sl_appendf(outfiles, "%s-indx.png", base);
        ngcfn      = sl_appendf(outfiles, "%s-ngc.png",  base);
        free(base);
        base = NULL;

        // Check for existing output filenames.
        nextfile = FALSE;
        for (i=0; i<sl_size(outfiles); i++) {
            char* fn = sl_get(outfiles,i);
            if (!file_exists(fn))
                continue;
            if (overwrite) {
                if (unlink(fn)) {
                    printf("Failed to delete an already-existing output file: \"%s\": %s\n", fn, strerror(errno));
                    exit(-1);
                }
            } else {
                printf("Output file \"%s\" already exists.  Bailing out.  "
                       "Use the --overwrite flag to overwrite existing files.\n", fn);
                printf("Continuing to next input file.\n");
                nextfile = TRUE;
                break;
            }
        }
        if (nextfile) {
            sl_free(outfiles);
            continue;
        }

        sl_append(lowlevelargs, "--out");
        sl_append(lowlevelargs, axyfn);
        sl_append(lowlevelargs, "--match");
        sl_append(lowlevelargs, matchfn);
        sl_append(lowlevelargs, "--rdls");
        sl_append(lowlevelargs, rdlsfn);
        sl_append(lowlevelargs, "--solved");
        sl_append(lowlevelargs, solvedfn);
        sl_append(lowlevelargs, "--wcs");
        sl_append(lowlevelargs, wcsfn);

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

        // source extraction overlay

        sl_append(backendargs, axyfn);

        sl_print(backendargs);

        cmd = sl_implode(backendargs, " ");
        //printf("Running backend:\n  %s\n", cmd);
        if (run_command_get_outputs(cmd, NULL, NULL, &errmsg)) {
            fprintf(stderr, "Couldn't run \"backend\": %s\n", errmsg);
            exit(-1);
        }
        free(cmd);

        // sources + index overlay
        // ngc/constellations overlay
        // rdls?
        sl_free(outfiles);
    }

    sl_free(lowlevelargs);
    sl_free(backendargs);

	return 0;
}

