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

-add quad to base-idx.png
-depending on image size, do or don't plot constellations (plot-constellations -C)

(1) it assumes you have "." in your path, which I never do.

> Right, so what should it do: look at args[0] to figure out where the other
> executables are?

(2) It assumes you have netpbm tools installed which the main build
doesn't require.

> I think it will only complain if it needs one of the netpbm programs to do
> its work - and it cannot do anything sensible (except print a friendly
> error message) if they don't exist.

(5) by default, we produce:
- myfile-ngc.png (ngc labels)
   -- but there should be a flag (e.g --pngs=off) to supress this
* by default, we do not produce an entirely new fits file but this can
be turned on

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
#include "matchfile.h"
#include "qfits_error.h"

static struct option long_options[] = {
	{"help",        no_argument,       0, 'h'},
	{"width",       required_argument, 0, 'W'},
	{"height",      required_argument, 0, 'H'},
	{"scale-low",	required_argument, 0, 'L'},
	{"scale-high",	required_argument, 0, 'U'},
	{"scale-units", required_argument, 0, 'u'},
    {"fields",      required_argument, 0, 'F'},
	{"depth",       required_argument, 0, 'D'},
	{"no-tweak",    no_argument,       0, 'T'},
	{"no-guess-scale", no_argument,    0, 'G'},
	{"tweak-order", required_argument, 0, 't'},
	{"dir",         required_argument, 0, 'd'},
	{"backend-config", required_argument, 0, 'c'},
	{"files-on-stdin", no_argument,       0, 'f'},
	{"overwrite",      no_argument,       0, 'O'},
	{"no-plots",    no_argument,       0, 'P'},
	{"no-fits2fits",    no_argument,       0, '2'},
	{0, 0, 0, 0}
};

static const char* OPTIONS = "hL:U:u:t:d:c:TW:H:GOPD:fF:2";

static void print_help(const char* progname) {
	printf("Usage:   %s [options]\n"
	       "  [--dir <directory>]: place all output files in this directory\n"
	       "  [--scale-units <units>]: in what units are the lower and upper bound specified?   (-u)\n"
	       "     choices:  \"degwidth\"    : width of the image, in degrees\n"
	       "               \"arcminwidth\" : width of the image, in arcminutes\n"
	       "               \"arcsecperpix\": arcseconds per pixel\n"
	       "  [--scale-low  <number>]: lower bound of image scale estimate   (-L)\n"
	       "  [--scale-high <number>]: upper bound of image scale estimate   (-U)\n"
           "  [--fields <number>]: specify a field (ie, FITS extension) to solve\n"
           "  [--fields <min>/<max>]: specify a range of fields (FITS extensions) to solve; inclusive\n"
	       "  [--width  <number>]: (mostly for xyls inputs): the original image width   (-W)\n"
	       "  [--height <number>]: (mostly for xyls inputs): the original image height  (-H)\n"
		   "  [--depth <number>]: number of field objects to look at   (-D)\n"
	       "  [--no-tweak]: don't fine-tune WCS by computing a SIP polynomial  (-T)\n"
	       "  [--no-guess-scale]: don't try to guess the image scale from the FITS headers  (-G)\n"
           "  [--no-plots]: don't create any PNG plots.  (-P)\n"
           "  [--no-fits2fits]: don't sanitize FITS files; assume they're already sane.  (-2)\n"
	       "  [--tweak-order <integer>]: polynomial order of SIP WCS corrections.  (-t <#>)\n"
	       "  [--backend-config <filename>]: use this config file for the \"backend\" program.  (-c <file>)\n"
	       "  [--overwrite]: overwrite output files if they already exist.  (-O)\n"
	       "  [--files-on-stdin]: read files to solve on stdin, one per line (-f)\n"
	       "\n"
	       "  [<image-file-1> <image-file-2> ...] [<xyls-file-1> <xyls-file-2> ...]\n"
	       "\n", progname);
}

static int run_command(const char* cmd, bool* ctrlc) {
	int rtn;
	printf("Running command:\n  %s\n", cmd);
	rtn = system(cmd);
	if (rtn == -1) {
		fprintf(stderr, "Failed to run command: %s\n", strerror(errno));
		return -1;
	}
	if (WIFSIGNALED(rtn)) {
        if (ctrlc && (WTERMSIG(rtn) == SIGTERM))
            *ctrlc = TRUE;
		return -1;
	}
	rtn = WEXITSTATUS(rtn);
	if (rtn) {
		fprintf(stderr, "Command exited with exit status %i.\n", rtn);
	}
	return rtn;
}

static char* get_path(const char* prog, const char* me) {
    //char* path = find_executable(prog, me);
    char* cpy;
    char* dir;
    char* path;

    // First look for it in solve-field's directory.
    cpy = strdup(me);
    dir = strdup(dirname(cpy));
    free(cpy);
    asprintf_safe(&path, "%s/%s", dir, prog);
    free(dir);
    if (file_executable(path))
        return path;

    // Otherwise, let the normal PATH-search machinery find it...
    free(path);
    path = strdup(prog);
    /*
     if (!path) {
     fprintf(stderr, "Failed to find executable \"%s\".\n", prog);
     exit(-1);
     }
     */
    return path;
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
	const char* errmsg;
	bool guess_scale = TRUE;
	int width = 0, height = 0;
	int nllargs;
	int nbeargs;
	bool fromstdin = FALSE;
	bool overwrite = FALSE;
    bool makeplots = TRUE;
    char* me = args[0];
    //char* cpy;
    //char* mydir;

    // Add this program's directory to PATH.
    /*
     cpy = strdup(me);
     mydir = strdup(dirname(cpy));
     free(cpy);
     if (mydir) {
     char* path;
     char* newpath;
     path = getenv("PATH");
     asprintf_safe(&newpath, "PATH=%s:%s", path, mydir);
     putenv(newpath);
     // Note, we leak "newpath" because it becomes part of the environment, so
     // changing it changes the environment.
     free(mydir);
     }
     */

	lowlevelargs = sl_new(16);
	sl_append_nocopy(lowlevelargs, get_path("low-level-frontend", me));
	//sl_append(lowlevelargs, "low-level-frontend");

	backendargs = sl_new(16);
	sl_append_nocopy(backendargs, get_path("backend", me));
	//sl_append(backendargs, "backend");

	while (1) {
		int option_index = 0;
		c = getopt_long(argc, args, OPTIONS, long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			help = TRUE;
			break;
        case '2':
            sl_append(lowlevelargs, "--no-fits2fits");
            break;
        case 'F':
            sl_append(lowlevelargs, "--fields");
            sl_appendf(lowlevelargs, optarg);
            break;
        case 'D':
            sl_append(lowlevelargs, "--depth");
            sl_appendf(lowlevelargs, "%i", atoi(optarg));
            break;
        case 'P':
            makeplots = FALSE;
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
		case 'f':
			fromstdin = TRUE;
			break;
		}
	}

	if (optind == argc) {
		printf("ERROR: You didn't specify any files to process.\n");
		help = TRUE;
	}

	rtn = 0;
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
		 if (stat(outdir, &st)) 
		 if (errno == ENOENT) 
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
		char tmpfn[1024];
		char* infile = NULL;
		bool isxyls;
		const char* reason;
		int len;
		char* cpy;
		char* base;
		char *matchfn, *rdlsfn, *solvedfn, *wcsfn, *axyfn, *objsfn, *redgreenfn;
		char *ngcfn, *ppmfn=NULL, *indxylsfn;
        char* downloadfn;
        char* suffix = NULL;
		sl* outfiles;
		bool nextfile;
		int fid;
		sl* cmdline;
        bool ctrlc = FALSE;

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

        cmdline = sl_new(16);

        // Remove arguments that might have been added in previous trips through this loop
		sl_remove_from(lowlevelargs, nllargs);
		sl_remove_from(backendargs,  nbeargs);

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
			if (base[len - 4] == '.') {
				base[len-4] = '\0';
                suffix = base + len - 3;
            }
			if (base[len - 5] == '.') {
				base[len-5] = '\0';
                suffix = base + len - 4;
            }
		}

		// the output filenames.
		outfiles = sl_new(16);

		axyfn      = sl_appendf(outfiles, "%s.axy",       base);
		matchfn    = sl_appendf(outfiles, "%s.match",     base);
		rdlsfn     = sl_appendf(outfiles, "%s.rdls",      base);
		solvedfn   = sl_appendf(outfiles, "%s.solved",    base);
		wcsfn      = sl_appendf(outfiles, "%s.wcs",       base);
		objsfn     = sl_appendf(outfiles, "%s-objs.png",  base);
		redgreenfn = sl_appendf(outfiles, "%s-indx.png",  base);
		ngcfn      = sl_appendf(outfiles, "%s-ngc.png",   base);
		indxylsfn  = sl_appendf(outfiles, "%s-indx.xyls", base);
        if (suffix)
            downloadfn = sl_appendf(outfiles, "%s-downloaded.%s", base, suffix);
        else
            downloadfn = sl_appendf(outfiles, "%s-downloaded", base);
		free(base);
		base = NULL;

		// Check for existing output filenames.
		nextfile = FALSE;
		for (i = 0; i < sl_size(outfiles); i++) {
			char* fn = sl_get(outfiles, i);
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

        // Download URL...
        if ((strncasecmp(infile, "http://", 7) == 0) ||
            (strncasecmp(infile, "ftp://", 6) == 0)) {
            sl_append(cmdline, "wget");
            sl_appendf(cmdline, "-O \"%s\"", downloadfn);
            sl_appendf(cmdline, "\"%s\"", infile);

            cmd = sl_implode(cmdline, " ");
            sl_remove_all(cmdline);

            printf("Running:\n  %s\n", cmd);
            fflush(NULL);
            if (run_command(cmd, &ctrlc)) {
                fflush(NULL);
                fprintf(stderr, "wget command %s; exiting.\n",
                        (ctrlc ? "was cancelled" : "failed"));
                exit(-1);
            }
            free(cmd);

            infile = downloadfn;
        }

		printf("Checking if file \"%s\" is xylist or image: ", infile);
        fflush(NULL);
        // turn on QFITS error reporting.
        qfits_err_statset(1);
		isxyls = xylist_is_file_xylist(infile, NULL, NULL, &reason);
		printf(isxyls ? "xyls\n" : "image\n");
		if (!isxyls) {
			printf("  (%s)\n", reason);
		}
        fflush(NULL);

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

		if (image) {
			sprintf(tmpfn, "/tmp/tmp.solve-field.XXXXXX");
			fid = mkstemp(tmpfn);
			if (fid == -1) {
				fprintf(stderr, "Failed to create temp file: %s\n", strerror(errno));
				exit(-1);
			}
			ppmfn = sl_append(outfiles, tmpfn);

			sl_append(lowlevelargs, "--pnm");
			sl_append(lowlevelargs, ppmfn);
			sl_append(lowlevelargs, "--force-ppm");
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

		cmd = sl_implode(lowlevelargs, " ");
		//printf("Running low-level-frontend:\n  %s\n", cmd);
        fflush(NULL);
		if (run_command(cmd, &ctrlc)) {
            fflush(NULL);
            fprintf(stderr, "low-level-frontend %s; exiting.\n",
                    (ctrlc ? "was cancelled" : "failed"));
			exit(-1);
        }
		free(cmd);

        if (makeplots) {
            // source extraction overlay
            // plotxy -i harvard.axy -I /tmp/pnm -C red -P -w 2 -N 50 | plotxy -w 2 -r 3 -I - -i harvard.axy -C red -n 50 > harvard-objs.png
            sl_append_nocopy(cmdline, get_path("plotxy", me));
            //sl_append(cmdline, "plotxy");
            sl_append(cmdline, "-i");
            sl_append(cmdline, axyfn);
            if (image) {
                sl_append(cmdline, "-I");
                sl_append(cmdline, ppmfn);
            }
            sl_append(cmdline, "-P");
            sl_append(cmdline, "-C red -w 2 -N 50 -x 1 -y 1");
            
            sl_append(cmdline, "|");

            sl_append_nocopy(cmdline, get_path("plotxy", me));
            //sl_append(cmdline, "plotxy");
            sl_append(cmdline, "-i");
            sl_append(cmdline, axyfn);
            sl_append(cmdline, "-I - -w 2 -r 3 -C red -n 50 -N 200 -x 1 -y 1");

            sl_append(cmdline, ">");
            sl_append(cmdline, objsfn);

            cmd = sl_implode(cmdline, " ");
            sl_remove_all(cmdline);

            printf("Running plot command:\n  %s\n", cmd);
            fflush(NULL);
            if (run_command(cmd, &ctrlc)) {
                fflush(NULL);
                fprintf(stderr, "Plotting command %s; exiting.\n",
                        (ctrlc ? "was cancelled" : "failed"));
                exit(-1);
            }
            free(cmd);
        }

		sl_append(backendargs, axyfn);
		cmd = sl_implode(backendargs, " ");
		printf("Running backend:\n  %s\n", cmd);
        fflush(NULL);
		if (run_command_get_outputs(cmd, NULL, NULL, &errmsg)) {
            fflush(NULL);
			fprintf(stderr, "backend failed: %s\n", errmsg);
			fprintf(stderr, "exiting.\n");
			exit(-1);
		}
		free(cmd);
        fflush(NULL);

		if (!file_exists(solvedfn)) {
			// boo.
			printf("Field didn't solve.\n");
		} else {
			matchfile* mf;
			MatchObj* mo;

			// index rdls to xyls.
            sl_append_nocopy(cmdline, get_path("wcs-rd2xy", me));
            //sl_append(cmdline, "wcs-rd2xy");
			sl_append(cmdline, "-w");
			sl_append(cmdline, wcsfn);
			sl_append(cmdline, "-i");
			sl_append(cmdline, rdlsfn);
			sl_append(cmdline, "-o");
			sl_append(cmdline, indxylsfn);

			cmd = sl_implode(cmdline, " ");
			sl_remove_all(cmdline);
			printf("Running wcs-rd2xy:\n  %s\n", cmd);
            fflush(NULL);
			if (run_command(cmd, &ctrlc)) {
                fflush(NULL);
                fprintf(stderr, "wcs-rd2xy %s; exiting.\n",
                        (ctrlc ? "was cancelled" : "failed"));
				exit(-1);
            }
			free(cmd);

            if (makeplots) {
                // sources + index overlay
                sl_append_nocopy(cmdline, get_path("plotxy", me));
                //sl_append(cmdline, "plotxy");
                sl_append(cmdline, "-i");
                sl_append(cmdline, axyfn);
                if (image) {
                    sl_append(cmdline, "-I");
                    sl_append(cmdline, ppmfn);
                }
                sl_append(cmdline, "-P");
                sl_append(cmdline, "-C red -w 2 -r 6 -N 200 -x 1 -y 1");
                sl_append(cmdline, "|");
                sl_append_nocopy(cmdline, get_path("plotxy", me));
                //sl_append(cmdline, "plotxy");
                sl_append(cmdline, "-i");
                sl_append(cmdline, indxylsfn);
                sl_append(cmdline, "-I - -w 2 -r 4 -C green -x 1 -y 1");

                mf = matchfile_open(matchfn);
                if (!mf) {
                    fprintf(stderr, "Failed to read matchfile %s.\n", matchfn);
                    exit(-1);
                }
                // just read the first match...
                mo = matchfile_buffered_read_match(mf);
                if (!mo) {
                    fprintf(stderr, "Failed to read a match from matchfile %s.\n", matchfn);
                    exit(-1);
                }

                sl_append(cmdline, " -P |");
                sl_append_nocopy(cmdline, get_path("plotquad", me));
                //sl_append(cmdline, "plotquad");
                sl_append(cmdline, "-I -");
                sl_append(cmdline, "-C green");
                sl_append(cmdline, "-w 2");
                for (i=0; i<8; i++)
                    sl_appendf(cmdline, " %g", mo->quadpix[i]);
                
                matchfile_close(mf);
			
                sl_append(cmdline, ">");
                sl_append(cmdline, redgreenfn);

                cmd = sl_implode(cmdline, " ");
                sl_remove_all(cmdline);
                printf("Running plot command:\n  %s\n", cmd);
                fflush(NULL);
                if (run_command(cmd, &ctrlc)) {
                    fflush(NULL);
                    fprintf(stderr, "Plotting commands %s; exiting.\n",
                            (ctrlc ? "were cancelled" : "failed"));
                    exit(-1);
                }
                free(cmd);
            }

            if (image && makeplots) {
                sl_append_nocopy(cmdline, get_path("plot-constellations", me));
                //sl_append(cmdline, "plot-constellations");
				sl_append(cmdline, "-w");
				sl_append(cmdline, wcsfn);
				sl_append(cmdline, "-i");
				sl_append(cmdline, ppmfn);
				sl_append(cmdline, "-N");
				sl_append(cmdline, "-C");
				sl_append(cmdline, "-o");
				sl_append(cmdline, ngcfn);

				cmd = sl_implode(cmdline, " ");
				sl_remove_all(cmdline);
				printf("Running plot-constellations:\n  %s\n", cmd);
                fflush(NULL);
				if (run_command(cmd, &ctrlc)) {
                    fflush(NULL);
                    fprintf(stderr, "plot-constellations %s; exiting.\n",
                            (ctrlc ? "was cancelled" : "failed"));
                    exit(-1);
                }
				free(cmd);
			}

			// create field rdls?
		}

        fflush(NULL);

        sl_free(cmdline);
		sl_free(outfiles);
	}

	sl_free(lowlevelargs);
	sl_free(backendargs);

	return 0;
}

