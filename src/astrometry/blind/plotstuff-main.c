/*
  This file is part of the Astrometry.net suite.
  Copyright 2009 Dustin Lang.

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

 Reads a plot control file that contains instructions on what to plot
 and how to plot it.

 There are a few plotting module that understand different commands
 and plot different kinds of things.

 Common plotting:

 plot_color (<r> <g> <b> <a> or color name)
 plot_lw <linewidth>
 plot_marker <marker-shape>
 plot_markersize <radius>
 plot_wcs <filename>

 Image:

 image_file <fn>
 image_format <format>
 image

 Xy:

 xy_file <xylist>
 xy_ext <fits-extension>
 xy_xcol <column-name>
 xy_ycol <column-name>
 xy_xoff <pixel-offset>
 xy_yoff <pixel-offset>
 xy_firstobj <obj-num>
 xy_nobjs <n>
 xy_scale <factor>
 xy_bgcolor <r> <g> <b> <a>
 xy

 Annotations:

 annotations
 annotations_bgcolor
 annotations_fontsize
 annotations_font

 */

#include <stdio.h>

#include "plotstuff.h"
#include "boilerplate.h"
#include "log.h"
#include "errors.h"


static const char* OPTIONS = "hW:H:o:JP";

static void printHelp(char* progname) {
	boilerplate_help_header(stdout);
	printf("\nUsage: %s [options] > output.png\n"
           "  [-o <output-file>] (default: stdout)\n"
           "  [-P]              Write PPM output instead of PNG.\n"
		   "  [-J]              Write PDF output.\n"
		   "  [-W <width>   ]   Width of output image (default: data-dependent).\n"
		   "  [-H <height>  ]   Height of output image (default: data-dependent).\n",
           progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *args[]) {
	int loglvl = LOG_MSG;
	int argchar;
	char* progname = args[0];
	plot_args_t pargs;

	memset(&pargs, 0, sizeof(pargs));
	pargs.fout = stdout;
	pargs.outformat = PLOTSTUFF_FORMAT_PNG;

	while ((argchar = getopt(argc, args, OPTIONS)) != -1)
		switch (argchar) {
        case 'o':
            pargs.outfn = optarg;
            break;
        case 'P':
            pargs.outformat = PLOTSTUFF_FORMAT_PPM;
            break;
		case 'J':
            pargs.outformat = PLOTSTUFF_FORMAT_PDF;
			break;
		case 'W':
			pargs.W = atoi(optarg);
			break;
		case 'H':
			pargs.H = atoi(optarg);
			break;
		case 'v':
			loglvl++;
			break;
		case 'h':
			printHelp(progname);
            exit(0);
		case '?':
		default:
			printHelp(progname);
            exit(-1);
		}

	if (optind != argc) {
		printHelp(progname);
		exit(-1);
	}
	if (!pargs.W || !pargs.H) {
		ERROR("You must specify width and height of the output image.\n");
		exit(-1);
	}

	log_init(loglvl);

    // log errors to stderr, not stdout.
    errors_log_to(stderr);

	if (plotstuff_init(&pargs))
		exit(-1);

	for (;;) {
		if (plotstuff_read_and_run_command(&pargs, stdin))
			break;
	}

	if (plotstuff_output(&pargs))
		exit(-1);

	plotstuff_free(&pargs);

	return 0;
}
