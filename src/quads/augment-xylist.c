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

/**
 * Accepts an xylist and command-line options, and produces an augmented
 * xylist.

 augment-xylist --guess-scale --image mypic.fits --scale-low 2 \
 --scale-high 4 --scale-units degwide --tweak-order 4 --out mypic.axy

 augment-xylist --xylist sdss.fits --scale-low 0.38 --scale-high 0.41 \
 --scale-units arcsecperpix --fields 1-9999 --noplots --nordls \
 --out sdss.axy

 * IMAGEW - Image width.
 * IMAGEH - Image height.
 * ANRUN - the "Go" button. 

 * ANPOSERR - Field positional error, in pixels.
 * ANSOLVED - Output filename for solved file.
 * ANMATCH - Output filename for match file.
 * ANRDLS - Output filename for index RDLS file.
 * ANWCS - Output filename for WCS file.
 * ANCANCEL - Input filename - if this file exists, the job is cancelled.
 * ANTLIM - Wall-clock time limit, in seconds
 * ANCLIM - CPU time limit, in seconds
 * ANPARITY - "BOTH", "POS", "NEG" (default "BOTH")
 * ANTWEAK - FITS Boolean ie 'T' or 'F' (default 'T')
 * ANTWEAKO - Tweak order.
 * ANAPPL# - Lower bound on scale (in arcseconds per pixel)
 * ANAPPU# - Upper bound on scale (in arcseconds per pixel)
 * ANDPL#/ANDPU# - The range of depths (field objects) to examine
 * ANFDL#/ANFDU# - Add fields to be solved, specified in inclusive table HDU ranges starting from 1. The ANFD'''L'''/ANFD'''U''' stands for Upper and Lower. Defaults to all HDU's, but if a single ANFD[LU]# is specified, then only the fields explicitly listed are used. Multiple ANFD[LU]#'s can be present and the solved field are the union of the specified ranges. Only valid in the primary header.
 * ANFD#### - Add single fields to be solved. Only valid in the primary header.
 * ANODDSPR - Odds ratio required before to printing a solution.
 * ANODDSKP - Odds ratio required before to keep a solution.
 * ANODDSSL - Odds ratio required before to consider a solution solved.
 * ANIMFRAC - Fraction of the rectangular image that the field actually occupies (eg for round fields). Real in (0, 1], default 1.
 * ANCTOL - Code tolerance.
 * ANDISTR - Distractor fraction.

 * ANW#PIX1 CRPIX1
 * ANW#PIX2 CRPIX2
 * ANW#VAL1 CRVAL1
 * ANW#VAL2 CRVAL2
 * ANW#CD11 CD1_1
 * ANW#CD12 CD1_2
 * ANW#CD21 CD2_1
 * ANW#CD22 CD2_2 

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <getopt.h>

#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "math.h"
#include "fitsioutils.h"
#include "scriptutils.h"

#include "qfits.h"

static struct option long_options[] = {
	{"help",		   no_argument,	      0, 'h'},
	{"verbose",        no_argument,       0, 'v'},
	{"image",		   required_argument, 0, 'i'},
	{"xylist",		   required_argument, 0, 'x'},
	{"guess-scale",    no_argument,	      0, 'g'},
	{"cancel",		   required_argument, 0, 'C'},
	{"solved",		   required_argument, 0, 'S'},
	{"match",		   required_argument, 0, 'M'},
	{"rdls",		   required_argument, 0, 'R'},
	{"wcs",			   required_argument, 0, 'W'},
	{"pnm",			   required_argument, 0, 'P'},
	{"force-ppm",	   no_argument,       0, 'f'},
	{"width",		   required_argument, 0, 'w'},
	{"height",		   required_argument, 0, 'e'},
	{"scale-low",	   required_argument, 0, 'L'},
	{"scale-high",	   required_argument, 0, 'H'},
	{"scale-units",    required_argument, 0, 'u'},
    {"fields",         required_argument, 0, 'F'},
	{"depth",		   required_argument, 0, 'd'},
	{"tweak-order",    required_argument, 0, 't'},
	{"out",			   required_argument, 0, 'o'},
	{"no-plot",		   no_argument,	      0, 'p'},
	{"no-tweak",	   no_argument,	      0, 'T'},
	{"no-fits2fits",   no_argument,       0, '2'},
	{"temp-dir",       required_argument, 0, 'm'},
	{"x-column",       required_argument, 0, 'X'},
	{"y-column",       required_argument, 0, 'Y'},
    {"sort-column",    required_argument, 0, 's'},
    {"sort-ascending", no_argument,       0, 'a'},
	{0, 0, 0, 0}
};

static const char* OPTIONS = "hg:i:L:H:u:t:o:px:w:e:TP:S:R:W:M:C:fd:F:2m:X:Y:s:av";

static void print_help(const char* progname) {
	printf("Usage:	 %s [options] -o <output augmented xylist filename>\n"
		   "  (    -i <image-input-file>\n"
		   "   OR  -x <xylist-input-file>  )\n"
	       "  [--guess-scale]: try to guess the image scale from the FITS headers  (-g)\n"
           "  [--cancel <filename>]: filename whose creation signals the process to stop  (-C)\n"
           "  [--solved <filename>]: output filename for solved file  (-S)\n"
           "  [--match  <filename>]: output filename for match file   (-M)\n"
           "  [--rdls   <filename>]: output filename for RDLS file    (-R)\n"
           "  [--wcs    <filename>]: output filename for WCS file     (-W)\n"
           "  [--pnm <filename>]: save the PNM file in <filename>  (-P)\n"
           "  [--force-ppm]: force the PNM file to be a PPM  (-f)\n"
           "  [--width  <int>]: specify the image width  (for xyls inputs)  (-w)\n"
           "  [--height <int>]: specify the image height (for xyls inputs)  (-e)\n"
           "  [--x-column <name>]: for xyls inputs: the name of the FITS column containing the X coordinate of the sources  (-X)\n"
           "  [--y-column <name>]: for xyls inputs: the name of the FITS column containing the Y coordinate of the sources  (-Y)\n"
           "  [--sort-column <name>]: for xyls inputs: the name of the FITS column that should be used to sort the sources  (-s)\n"
           "  [--sort-ascending]: when sorting, sort in ascending (smallest first) order   (-a)\n"
	       "  [--scale-units <units>]: in what units are the lower and upper bound specified?   (-u)\n"
	       "     choices:  \"degwidth\"    : width of the image, in degrees\n"
	       "               \"arcminwidth\" : width of the image, in arcminutes\n"
	       "               \"arcsecperpix\": arcseconds per pixel\n"
	       "  [--scale-low  <number>]: lower bound of image scale estimate   (-L)\n"
	       "  [--scale-high <number>]: upper bound of image scale estimate   (-H)\n"
           "  [--fields <number>]: specify a field (ie, FITS extension) to solve  (-F)\n"
           "  [--fields <min>/<max>]: specify a range of fields (FITS extensions) to solve; inclusive  (-F)\n"
		   "  [--depth <number>]: number of field objects to look at   (-d)\n"
	       "  [--tweak-order <integer>]: polynomial order of SIP WCS corrections  (-t)\n"
           "  [--no-fits2fits]: don't sanitize FITS files; assume they're already sane  (-2)\n"
	       "  [--no-tweak]: don't fine-tune WCS by computing a SIP polynomial  (-T)\n"
           "  [--no-plots]: don't create any PNG plots  (-p)\n"
           "  [--temp-dir <dir>]: where to put temp files, default /tmp  (-m)\n"
           "  [--verbose]: be more chatty!\n"
		   "\n", progname);
}

static int parse_depth_string(il* depths, const char* str);
static int parse_fields_string(il* fields, const char* str);

static void append_file(sl* list, const char* fn) {
    sl_append_nocopy(list, escape_filename(fn));
}

int main(int argc, char** args) {
	int c;
	int rtn;
	int help_flag = 0;
	char* outfn = NULL;
	char* imagefn = NULL;
	char* xylsfn = NULL;
    sl* cmd;
    char* cmdstr;
	int W, H;
	double scalelo = 0.0, scalehi = 0.0;
	char* scaleunits = NULL;
	qfits_header* hdr;
	bool tweak = TRUE;
	int tweak_order = 0;
	int orig_nheaders;
	FILE* fout;
	char* savepnmfn = NULL;
    bool force_ppm = FALSE;
	bool guess_scale = FALSE;
	dl* scales;
	int i;
	bool guessed_scale = FALSE;
	char* cancelfile = NULL;
	char* solvedfile = NULL;
	char* matchfile = NULL;
	char* rdlsfile = NULL;
	char* wcsfile = NULL;
    const char* errmsg = NULL;
    // contains ranges of depths as pairs of ints.
    il* depths;
    // contains ranges of fields as pairs of ints.
    il* fields;
    bool nof2f = FALSE;
    char* xcol = NULL;
    char* ycol = NULL;
    char* me = args[0];
    char* tempdir = "/tmp";
    // this is just to avoid leaking temp filenames...
    sl* tempfiles;
    char* sortcol = NULL;
    bool descending = TRUE;
    bool dosort = FALSE;
    bool verbose = FALSE;

    depths = il_new(4);
    fields = il_new(16);
    cmd = sl_new(16);
    tempfiles = sl_new(4);

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
			help_flag = 1;
			break;
        case 'v':
            verbose = TRUE;
            break;
        case 's':
            sortcol = optarg;
            break;
        case 'a':
            descending = FALSE;
            break;
        case 'X':
            xcol = optarg;
            break;
        case 'Y':
            ycol = optarg;
            break;
        case 'm':
            tempdir = optarg;
            break;
        case '2':
            nof2f = TRUE;
            break;
        case 'F':
            if (parse_fields_string(fields, optarg)) {
                fprintf(stderr, "Failed to parse fields specification \"%s\".\n", optarg);
                exit(-1);
            }
            break;
        case 'd':
            if (parse_depth_string(depths, optarg)) {
                fprintf(stderr, "Failed to parse depth specification: \"%s\"\n", optarg);
                exit(-1);
            }
            break;
		case 'o':
			outfn = optarg;
			break;
		case 'i':
			imagefn = optarg;
			break;
		case 'x':
			xylsfn = optarg;
			break;
		case 'L':
			scalelo = atof(optarg);
			break;
		case 'H':
			scalehi = atof(optarg);
			break;
		case 'u':
			scaleunits = optarg;
			break;
		case 'w':
			W = atoi(optarg);
			break;
		case 'e':
			H = atoi(optarg);
			break;
		case 'T':
			tweak = FALSE;
			break;
		case 't':
			tweak_order = atoi(optarg);
			break;
		case 'P':
			savepnmfn = optarg;
			break;
        case 'f':
            force_ppm = TRUE;
            break;
		case 'g':
			guess_scale = TRUE;
			break;
		case 'S':
			solvedfile = optarg;
			break;
		case 'C':
			cancelfile = optarg;
			break;
		case 'M':
			matchfile = optarg;
			break;
		case 'R':
			rdlsfile = optarg;
			break;
		case 'W':
			wcsfile = optarg;
			break;
		case '?':
			break;
		default:
			exit( -1);
		}
	}

	rtn = 0;
	if (optind != argc) {
		int i;
		printf("Unknown arguments:\n  ");
		for (i=optind; i<argc; i++) {
			printf("%s ", args[i]);
		}
		printf("\n");
		help_flag = 1;
		rtn = -1;
	}
	if (!outfn) {
		printf("Output filename (-o / --out) is required.\n");
		help_flag = 1;
		rtn = -1;
	}
	if (!(imagefn || xylsfn)) {
		printf("Require either an image (-i / --image) or an XYlist (-x / --xylist) input file.\n");
		help_flag = 1;
		rtn = -1;
	}
	if (!((!scaleunits) ||
		  (!strcasecmp(scaleunits, "degwidth")) ||
		  (!strcasecmp(scaleunits, "arcminwidth")) ||
		  (!strcasecmp(scaleunits, "arcsecperpix")))) {
		printf("Unknown scale units \"%s\".\n", scaleunits);
		help_flag = 1;
		rtn = -1;
	}
	/*
	  if (xylsfn && !(W && H)) {
	  printf("If you give an xylist, you must also specify the image width and height (-w / --width) and (-e / --height).\n");
	  help_flag = 1;
	  rtn = -1;
	  }
	*/
	if (help_flag) {
		print_help(args[0]);
		exit(rtn);
	}

	scales = dl_new(16);

	if (imagefn) {
		// if --image is given:
		//	 -run image2pnm.py
		//	 -if it's a FITS image, keep the original (well, sanitized version)
		//	 -otherwise, run ppmtopgm (if necessary) and pnmtofits.
		//	 -run fits2xy to generate xylist
		char *uncompressedfn;
		char *sanitizedfn;
		char *pnmfn;				
		sl* lines;
		bool isfits;
		char *fitsimgfn;
		char* line;
		char pnmtype;
		int maxval;
		char typestr[256];

        uncompressedfn = create_temp_file("uncompressed", tempdir);
		sanitizedfn = create_temp_file("sanitized", tempdir);
        sl_append_nocopy(tempfiles, uncompressedfn);
        sl_append_nocopy(tempfiles, sanitizedfn);

		if (savepnmfn)
			pnmfn = savepnmfn;
		else {
            pnmfn = create_temp_file("pnm", tempdir);
            sl_append_nocopy(tempfiles, pnmfn);
        }

        sl_append_nocopy(cmd, get_path("image2pnm.py", me));
        if (!verbose)
            sl_append(cmd, "--quiet");
        if (nof2f)
            sl_append(cmd, "--no-fits2fits");
        sl_append(cmd, "--infile");
        append_file(cmd, imagefn);
        sl_append(cmd, "--uncompressed-outfile");
        append_file(cmd, uncompressedfn);
        sl_append(cmd, "--sanitized-fits-outfile");
        append_file(cmd, sanitizedfn);
        sl_append(cmd, "--outfile");
        append_file(cmd, pnmfn);
        if (force_ppm)
            sl_append(cmd, "--ppm");

        cmdstr = sl_implode(cmd, " ");
        sl_remove_all(cmd);

        if (verbose)
            printf("Running: %s\n", cmdstr);
		if (run_command_get_outputs(cmdstr, &lines, NULL, &errmsg)) {
            free(cmdstr);
            fprintf(stderr, "%s\n", errmsg);
			fprintf(stderr, "Failed to run image2pnm.\n");
			exit(-1);
		}
        free(cmdstr);

		isfits = FALSE;
		for (i=0; i<sl_size(lines); i++) {
            if (verbose)
                printf("  %s\n", sl_get(lines, i));
			if (!strcmp("fits", sl_get(lines, i)))
				isfits = TRUE;
		}
		sl_free(lines);

		// Get image W, H, depth.
        sl_append(cmd, "pnmfile");
        append_file(cmd, pnmfn);

        cmdstr = sl_implode(cmd, " ");
        sl_remove_all(cmd);

        if (verbose)
            printf("Running: %s\n", cmdstr);
		if (run_command_get_outputs(cmdstr, &lines, NULL, &errmsg)) {
            free(cmdstr);
            fprintf(stderr, "%s\n", errmsg);
			fprintf(stderr, "Failed to run pnmfile: %s\n", strerror(errno));
			exit(-1);
		}
        free(cmdstr);
		if (sl_size(lines) == 0) {
			fprintf(stderr, "No output from pnmfile.\n");
			exit(-1);
		}
		line = sl_get(lines, 0);
		// eg	"/tmp/pnm:	 PPM raw, 800 by 510  maxval 255"
		if (strlen(pnmfn) + 1 >= strlen(line)) {
			fprintf(stderr, "Failed to parse output from pnmfile: %s\n", line);
			exit(-1);
		}
		line += strlen(pnmfn) + 1;
		if (sscanf(line, " P%cM %255s %d by %d maxval %d",
				   &pnmtype, typestr, &W, &H, &maxval) != 5) {
			fprintf(stderr, "Failed to parse output from pnmfile: %s\n", line);
			exit(-1);
		}
		sl_free(lines);

		if (isfits) {
			fitsimgfn = sanitizedfn;

			if (guess_scale) {
                sl_append_nocopy(cmd, get_path("fits-guess-scale", me));
                append_file(cmd, fitsimgfn);
                
                cmdstr = sl_implode(cmd, " ");
                sl_remove_all(cmd);

                if (verbose)
                    printf("Running: %s\n", cmdstr);
				if (run_command_get_outputs(cmdstr, &lines, NULL, &errmsg)) {
                    free(cmdstr);
                    fprintf(stderr, "%s\n", errmsg);
					fprintf(stderr, "Failed to run fits-guess-scale: %s\n", strerror(errno));
					exit(-1);
				}
                free(cmdstr);

				for (i=0; i<sl_size(lines); i++) {
					char type[256];
					double scale;
					line = sl_get(lines, i);
					if (sscanf(line, "scale %255s %lg", type, &scale) == 2) {
                        if (verbose)
                            printf("Scale estimate: %g\n", scale);
						dl_append(scales, scale * 0.99);
						dl_append(scales, scale * 1.01);
						guessed_scale = TRUE;
					}
				}

				sl_free(lines);

			}

		} else {
			fitsimgfn = create_temp_file("fits", tempdir);
            sl_append_nocopy(tempfiles, fitsimgfn);
            
			if (pnmtype == 'P') {
                if (verbose)
                    printf("Converting PPM image to FITS...\n");

                sl_append(cmd, "ppmtopgm");
                append_file(cmd, pnmfn);
                sl_append(cmd, "| pnmtofits >");
                append_file(cmd, fitsimgfn);

                cmdstr = sl_implode(cmd, " ");
                sl_remove_all(cmd);
                if (verbose)
                    printf("Running: %s\n", cmdstr);
				if (run_command_get_outputs(cmdstr, NULL, NULL, &errmsg)) {
                    free(cmdstr);
                    fprintf(stderr, "%s\n", errmsg);
					fprintf(stderr, "Failed to convert PPM to FITS.\n");
					exit(-1);
				}
                free(cmdstr);
			} else {
                if (verbose)
                    printf("Converting PGM image to FITS...\n");

                sl_append(cmd, "pnmtofits");
                append_file(cmd, pnmfn);
                sl_append(cmd, ">");
                append_file(cmd, fitsimgfn);

                cmdstr = sl_implode(cmd, " ");
                sl_remove_all(cmd);
                if (verbose)
                    printf("Running: %s\n", cmdstr);
				if (run_command_get_outputs(cmdstr, NULL, NULL, &errmsg)) {
                    free(cmdstr);
                    fprintf(stderr, "%s\n", errmsg);
					fprintf(stderr, "Failed to convert PGM to FITS.\n");
					exit(-1);
				}
                free(cmdstr);
			}
		}

        printf("Extracting sources...\n");
        if (verbose)
            printf("Running fits2xy...\n");

		xylsfn = create_temp_file("xyls", tempdir);
        sl_append_nocopy(tempfiles, xylsfn);

        sl_append_nocopy(cmd, get_path("fits2xy", me));
        sl_append(cmd, "-O");
        sl_append(cmd, "-o");
        append_file(cmd, xylsfn);
        append_file(cmd, fitsimgfn);
        if (!verbose)
            sl_append(cmd, "-q");

        cmdstr = sl_implode(cmd, " ");
        sl_remove_all(cmd);

        if (verbose)
            printf("Running: %s\n", cmdstr);
		if (run_command_get_outputs(cmdstr, NULL, NULL, &errmsg)) {
            free(cmdstr);
            fprintf(stderr, "%s\n", errmsg);
			fprintf(stderr, "Failed to run fits2xy.\n");
			exit(-1);
		}
        free(cmdstr);

        dosort = TRUE;

	} else {
		// xylist.
		// if --xylist is given:
		//	 -fits2fits.py sanitize
        if (!nof2f) {
            char* sanexylsfn;

            sanexylsfn = create_temp_file("sanexyls", tempdir);
            sl_append_nocopy(tempfiles, sanexylsfn);

            sl_append_nocopy(cmd, get_path("fits2fits.py", me));
            append_file(cmd, xylsfn);
            append_file(cmd, sanexylsfn);

            cmdstr = sl_implode(cmd, " ");
            sl_remove_all(cmd);

            if (verbose)
                printf("Running: %s\n", cmdstr);
            if (run_command_get_outputs(cmdstr, NULL, NULL, &errmsg)) {
                free(cmdstr);
                fprintf(stderr, "%s\n", errmsg);
                fprintf(stderr, "Failed to run fits2fits.py\n");
                exit(-1);
            }
            free(cmdstr);
            xylsfn = sanexylsfn;
        }

        if (sortcol)
            dosort = TRUE;

	}

    if (dosort) {
        char* sortedxylsfn;

        if (!sortcol)
            sortcol = "FLUX";

		sortedxylsfn = create_temp_file("sorted", tempdir);
        sl_append_nocopy(tempfiles, sortedxylsfn);

		// sort the table by FLUX.
        sl_append_nocopy(cmd, get_path("tabsort", me));
        sl_append(cmd, "-i");
        append_file(cmd, xylsfn);
        sl_append(cmd, "-o");
        append_file(cmd, sortedxylsfn);
        sl_append(cmd, "-c");
        append_file(cmd, sortcol);
        if (descending)
            sl_append(cmd, "-d");
        if (!verbose)
            sl_append(cmd, "-q");

        cmdstr = sl_implode(cmd, " ");
        sl_remove_all(cmd);
        if (verbose)
            printf("Running: %s\n", cmdstr);
		if (run_command_get_outputs(cmdstr, NULL, NULL, &errmsg)) {
            free(cmdstr);
            fprintf(stderr, "%s\n", errmsg);
			fprintf(stderr, "Failed to run tabsort.\n");
			exit(-1);
		}
        free(cmdstr);
		xylsfn = sortedxylsfn;
    }

	// start piling FITS headers in there.
	hdr = qfits_header_read(xylsfn);
	if (!hdr) {
		fprintf(stderr, "Failed to read FITS header from file %s.\n", xylsfn);
		exit(-1);
	}

	orig_nheaders = hdr->n;

    if (!(W && H)) {
        // If an xylist was given, look for existing IMAGEW, IMAGEH headers.  Otherwise die.
        W = qfits_header_getint(hdr, "IMAGEW", 0);
        H = qfits_header_getint(hdr, "IMAGEH", 0);
        if (!(W && H)) {
            fprintf(stderr, "Error: image width and height must be specified for XYLS inputs.\n");
            exit(-1);
        }
    } else {
        fits_header_add_int(hdr, "IMAGEW", W, "image width");
        fits_header_add_int(hdr, "IMAGEH", H, "image height");
    }
	qfits_header_add(hdr, "ANRUN", "T", "Solve this field!", NULL);

    if (xcol) {
        qfits_header_add(hdr, "ANXCOL", xcol, "Name of column containing X coords", NULL);
    }
    if (ycol) {
        qfits_header_add(hdr, "ANYCOL", xcol, "Name of column containing Y coords", NULL);
    }

	if (scalelo > 0.0 && scalehi > 0.0) {
		double appu, appl;
		if (!scaleunits)
			scaleunits = "degwide";
		if (!strcasecmp(scaleunits, "degwidth")) {
			appl = deg2arcsec(scalelo) / (double)W;
			appu = deg2arcsec(scalehi) / (double)W;
		} else if (!strcasecmp(scaleunits, "arcminwidth")) {
			appl = arcmin2arcsec(scalelo) / (double)W;
			appu = arcmin2arcsec(scalehi) / (double)W;
		} else if (!strcasecmp(scaleunits, "arcsecperpix")) {
			appl = scalelo;
			appu = scalehi;
		} else {
			exit(-1);
		}

		dl_append(scales, appl);
		dl_append(scales, appu);
	}

	if ((dl_size(scales) > 0) && guessed_scale) {
		qfits_header_add(hdr, "ANAPPDEF", "T", "try the default scale range too.", NULL);
	}
	for (i=0; i<dl_size(scales)/2; i++) {
		char key[64];
		sprintf(key, "ANAPPL%i", i+1);
		fits_header_add_double(hdr, key, dl_get(scales, i*2), "scale: arcsec/pixel min");
		sprintf(key, "ANAPPU%i", i+1);
		fits_header_add_double(hdr, key, dl_get(scales, i*2+1), "scale: arcsec/pixel max");
	}

	qfits_header_add(hdr, "ANTWEAK", (tweak ? "T" : "F"), (tweak ? "Tweak: yes please!" : "Tweak: no, thanks."), NULL);
	if (tweak && tweak_order) {
		fits_header_add_int(hdr, "ANTWEAKO", tweak_order, "Tweak order");
	}

	if (solvedfile)
		qfits_header_add(hdr, "ANSOLVED", solvedfile, "output filename", NULL);
	if (cancelfile)
		qfits_header_add(hdr, "ANCANCEL", cancelfile, "output filename", NULL);
	if (matchfile)
		qfits_header_add(hdr, "ANMATCH", matchfile, "output filename", NULL);
	if (rdlsfile)
		qfits_header_add(hdr, "ANRDLS", rdlsfile, "output filename", NULL);
	if (wcsfile)
		qfits_header_add(hdr, "ANWCS", wcsfile, "output filename", NULL);

    for (i=0; i<il_size(depths)/2; i++) {
        int depthlo, depthhi;
        char key[64];
        depthlo = il_get(depths, 2*i);
        depthhi = il_get(depths, 2*i + 1);
        sprintf(key, "ANDPL%i", (i+1));
		fits_header_addf(hdr, key, "", "%i", depthlo);
        sprintf(key, "ANDPU%i", (i+1));
		fits_header_addf(hdr, key, "", "%i", depthhi);
    }

    for (i=0; i<il_size(fields)/2; i++) {
        int lo = il_get(fields, 2*i);
        int hi = il_get(fields, 2*i + 1);
        char key[64];
        if (lo == hi) {
            sprintf(key, "ANFD%i", (i+1));
            fits_header_add_int(hdr, key, lo, "field to solve");
        } else {
            sprintf(key, "ANFDL%i", (i+1));
            fits_header_add_int(hdr, key, lo, "field range: low");
            sprintf(key, "ANFDU%i", (i+1));
            fits_header_add_int(hdr, key, hi, "field range: high");
        }
    }

	fout = fopen(outfn, "wb");
	if (!fout) {
		fprintf(stderr, "Failed to open output file: %s\n", strerror(errno));
		exit(-1);
	}

	if (qfits_header_dump(hdr, fout)) {
		fprintf(stderr, "Failed to write FITS header.\n");
		exit(-1);
	}

	// copy blocks from xyls to output.
	{
		FILE* fin;
		char block[FITS_BLOCK_SIZE];
		int startblock;
		int nblocks;
		struct stat st;
		startblock = fits_blocks_needed(orig_nheaders * FITS_LINESZ);
		if (stat(xylsfn, &st)) {
			fprintf(stderr, "Failed to stat() xyls file \"%s\": %s\n", xylsfn, strerror(errno));
			exit(-1);
		}
		nblocks = st.st_size / FITS_BLOCK_SIZE;
		fin = fopen(xylsfn, "rb");
		if (!fin) {
			fprintf(stderr, "Failed to open xyls file \"%s\": %s\n", xylsfn, strerror(errno));
			exit(-1);
		}
		if (fseeko(fin, startblock * FITS_BLOCK_SIZE, SEEK_SET)) {
			fprintf(stderr, "Failed to seek in xyls file \"%s\": %s\n", xylsfn, strerror(errno));
			exit(-1);
		}
		//printf("Copying FITS blocks %i to %i from xyls to output.\n", startblock, nblocks);
		for (i=startblock; i<nblocks; i++) {
			if (fread(block, 1, FITS_BLOCK_SIZE, fin) != FITS_BLOCK_SIZE) {
				fprintf(stderr, "Failed to read xyls file \"%s\": %s\n", xylsfn, strerror(errno));
				exit(-1);
			}
			if (fwrite(block, 1, FITS_BLOCK_SIZE, fout) != FITS_BLOCK_SIZE) {
				fprintf(stderr, "Failed to write output file: %s\n", strerror(errno));
				exit(-1);
			}
		}
		fclose(fin);
	}

    il_free(depths);
    il_free(fields);
    sl_free(cmd);
    sl_free(tempfiles);

	fclose(fout);
	qfits_header_destroy(hdr);

	return 0;
}

static int parse_depth_string(il* depths, const char* str) {
    // -10,10-20,20-30,40-
    while (str && *str) {
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
        if (lo < 0) {
            fprintf(stderr, "Depth %i is invalid: must be >= 0.\n", lo);
            return -1;
        }
        if (lo > hi) {
            fprintf(stderr, "Depth range %i to %i is invalid: max must be >= min!\n", lo, hi);
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

static int parse_fields_string(il* fields, const char* str) {
    // 10,11,20-25,30,40-50
    while (str && *str) {
        unsigned int lo, hi;
        int nread;
        if (sscanf(str, "%u-%u", &lo, &hi) == 2) {
            sscanf(str, "%*u-%*u%n", &nread);
        } else if (sscanf(str, "%u", &lo) == 1) {
            sscanf(str, "%*u%n", &nread);
            hi = lo;
        } else {
            fprintf(stderr, "Failed to parse fragment: \"%s\"\n", str);
            return -1;
        }
        if (lo <= 0) {
            fprintf(stderr, "Field number %i is invalid: must be >= 1.\n", lo);
            return -1;
        }
        if (lo > hi) {
            fprintf(stderr, "Field range %i to %i is invalid: max must be >= min!\n", lo, hi);
            return -1;
        }
        il_append(fields, lo);
        il_append(fields, hi);
        str += nread;
        while ((*str == ',') || isspace(*str))
            str++;
    }
    return 0;
}

