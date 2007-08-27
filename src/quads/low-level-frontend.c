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

 low-level-frontend --guess-scale --image mypic.fits --scale-low 2 \
 --scale-high 4 --scale-units degwide --tweak-order 4 --out mypic.axy

 low-level-frontend --xylist sdss.fits --scale-low 0.38 --scale-high 0.41 \
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
 * ANDEPTH# - The deepening strategy.
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

#include "qfits.h"

static struct option long_options[] = {
	{"help",		no_argument,	   0, 'h'},
	{"guess-scale", no_argument,	   0, 'g'},
	{"cancel",		required_argument, 0, 'C'},
	{"solved",		required_argument, 0, 'S'},
	{"match",		required_argument, 0, 'M'},
	{"rdls",		required_argument, 0, 'R'},
	{"wcs",			required_argument, 0, 'W'},
	{"image",		required_argument, 0, 'i'},
	{"pnm",			required_argument, 0, 'P'},
	{"force-ppm",	no_argument,       0, 'f'},
	{"width",		required_argument, 0, 'w'},
	{"height",		required_argument, 0, 'e'},
	{"scale-low",	required_argument, 0, 'L'},
	{"scale-high",	required_argument, 0, 'H'},
	{"scale-units", required_argument, 0, 'u'},
    {"fields",      required_argument, 0, 'F'},
	{"depth",		required_argument, 0, 'd'},
	{"tweak-order", required_argument, 0, 't'},
	{"out",			required_argument, 0, 'o'},
	{"no-plot",		no_argument,	   0, 'p'},
	{"no-rdls",		no_argument,	   0, 'r'},
	{"xylist",		required_argument, 0, 'x'},
	{"no-tweak",	no_argument,	   0, 'T'},
	{"no-fits2fits",    no_argument,       0, '2'},
	{0, 0, 0, 0}
};

static const char* OPTIONS = "hg:i:L:H:u:t:o:prx:w:e:TP:S:R:W:M:C:fd:F:2";

static void print_help(const char* progname) {
	// FIXME - add rest of args!
	printf("Usage:	 %s [options] -o <output augmented xylist filename>\n"
		   "  (				   -i <image-input-file>\n"
		   "   OR  -x <xylist-input-file>  )\n"
	       "  [--no-tweak]: don't fine-tune WCS by computing a SIP polynomial  (-T)\n"
           "  [--no-fits2fits]: don't sanitize FITS files; assume they're already sane.  (-2)\n"
		   "\n", progname);
}

static char* get_path(const char* prog, const char* me) {
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
    return path;
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
    char cmdbuf[1024];
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
    il* depths;
    // contains ranges of fields as pairs of ints.
    il* fields;
    int lo, hi;
    bool nof2f = FALSE;
    char* me = args[0];

    depths = il_new(4);
    fields = il_new(16);
    cmd = sl_new(16);

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
        case '2':
            nof2f = TRUE;
            break;
        case 'F':
            if (sscanf(optarg, "%d/%d", &lo, &hi) == 2) {
                if (hi < lo) {
                    fprintf(stderr, "Error parsing range of fields \"%s\".\n", optarg);
                    exit(-1);
                }
                il_append(fields, lo);
                il_append(fields, hi);
            } else if (sscanf(optarg, "%i", &lo) == 1) {
                // hi == lo
                il_append(fields, lo);
                il_append(fields, lo);
            } else {
                fprintf(stderr, "Error parsing fields specification \"%s\".\n", optarg);
                exit(-1);
            }
            break;
        case 'd':
            il_append(depths, atoi(optarg));
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
		char* sortedxylsfn;

		uncompressedfn = "/tmp/uncompressed";
		sanitizedfn = "/tmp/sanitized";
		if (savepnmfn)
			pnmfn = savepnmfn;
		else
			pnmfn = "/tmp/pnm";

        sl_append_nocopy(cmd, get_path("image2pnm.py", me));
        if (nof2f)
            sl_append(cmd, "--no-fits2fits");
        sl_append(cmd, "--infile");
        sl_appendf(cmd, "\"%s\"", imagefn);
        sl_append(cmd, "--uncompressed-outfile");
        sl_appendf(cmd, "\"%s\"", uncompressedfn);
        sl_append(cmd, "--sanitized-fits-outfile");
        sl_appendf(cmd, "\"%s\"", sanitizedfn);
        sl_append(cmd, "--outfile");
        sl_appendf(cmd, "\"%s\"", pnmfn);
        if (force_ppm)
            sl_append(cmd, "--ppm");

        cmdstr = sl_implode(cmd, " ");
        sl_remove_all(cmd);

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
			printf("  %s\n", sl_get(lines, i));
			if (!strcmp("fits", sl_get(lines, i)))
				isfits = TRUE;
		}
		sl_free(lines);

		// Get image W, H, depth.
		snprintf(cmdbuf, sizeof(cmdbuf), "pnmfile \"%s\"", pnmfn);
		if (run_command_get_outputs(cmdbuf, &lines, NULL, &errmsg)) {
            fprintf(stderr, "%s\n", errmsg);
			fprintf(stderr, "Failed to run pnmfile: %s\n", strerror(errno));
			exit(-1);
		}
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
                sl_appendf(cmd, "\"%s\"", fitsimgfn);

                cmdstr = sl_implode(cmd, " ");
                sl_remove_all(cmd);

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
						printf("Scale estimate: %g\n", scale);
						dl_append(scales, scale * 0.99);
						dl_append(scales, scale * 1.01);
						guessed_scale = TRUE;
					}
				}

				sl_free(lines);

			}

		} else {
			fitsimgfn = "/tmp/fits";

			if (pnmtype == 'P') {
				printf("Converting PPM image to FITS...\n");
				snprintf(cmdbuf, sizeof(cmdbuf),
						 "ppmtopgm \"%s\" | pnmtofits > \"%s\"", pnmfn, fitsimgfn);
				if (run_command_get_outputs(cmdbuf, NULL, NULL, &errmsg)) {
                    fprintf(stderr, "%s\n", errmsg);
					fprintf(stderr, "Failed to convert PPM to FITS.\n");
					exit(-1);
				}
			} else {
				printf("Converting PGM image to FITS...\n");
				snprintf(cmdbuf, sizeof(cmdbuf),
						 "pnmtofits %s > \"%s\"", pnmfn, fitsimgfn);
				if (run_command_get_outputs(cmdbuf, NULL, NULL, &errmsg)) {
                    fprintf(stderr, "%s\n", errmsg);
					fprintf(stderr, "Failed to convert PGM to FITS.\n");
					exit(-1);
				}
			}
		}

		printf("Running fits2xy...\n");

		xylsfn = "/tmp/xyls";

        sl_append_nocopy(cmd, get_path("fits2xy", me));
        sl_append(cmd, "-O");
        sl_appendf(cmd, "-o \"%s\"", xylsfn);
        sl_appendf(cmd, "\"%s\"", fitsimgfn);

        cmdstr = sl_implode(cmd, " ");
        sl_remove_all(cmd);

		printf("Command: %s\n", cmdstr);
		if (run_command_get_outputs(cmdstr, NULL, NULL, &errmsg)) {
            free(cmdstr);
            fprintf(stderr, "%s\n", errmsg);
			fprintf(stderr, "Failed to run fits2xy.\n");
			exit(-1);
		}
        free(cmdstr);

		printf("Running tabsort...\n");

		sortedxylsfn = "/tmp/sorted";

		// sort the table by FLUX.
        sl_append_nocopy(cmd, get_path("tabsort", me));
        sl_appendf(cmd, "-i \"%s\"", xylsfn);
        sl_appendf(cmd, "-o \"%s\"", sortedxylsfn);
        sl_append(cmd, "-c FLUX");
        sl_append(cmd, "-d");

        cmdstr = sl_implode(cmd, " ");
        sl_remove_all(cmd);

		printf("Command: %s\n", cmdstr);
		if (run_command_get_outputs(cmdstr, NULL, NULL, &errmsg)) {
            free(cmdstr);
            fprintf(stderr, "%s\n", errmsg);
			fprintf(stderr, "Failed to run tabsort.\n");
			exit(-1);
		}
        free(cmdstr);
		xylsfn = sortedxylsfn;

	} else {
		// xylist.
		// if --xylist is given:
		//	 -fits2fits.py sanitize
        if (!nof2f) {
            char* sanexylsfn;

            sanexylsfn = "/tmp/sanexyls";

            sl_append_nocopy(cmd, get_path("fits2fits.py", me));
            sl_appendf(cmd, "\"%s\"", xylsfn);
            sl_appendf(cmd, "\"%s\"", sanexylsfn);

            cmdstr = sl_implode(cmd, " ");
            sl_remove_all(cmd);
            printf("Command: %s\n", cmdstr);

            if (run_command_get_outputs(cmdstr, NULL, NULL, &errmsg)) {
                free(cmdstr);
                fprintf(stderr, "%s\n", errmsg);
                fprintf(stderr, "Failed to run fits2fits.py\n");
                exit(-1);
            }
            free(cmdstr);
            xylsfn = sanexylsfn;
        }

	}

	// start piling FITS headers in there.
	hdr = qfits_header_read(xylsfn);
	if (!hdr) {
		fprintf(stderr, "Failed to read FITS header from file %s.\n", xylsfn);
		exit(-1);
	}

	orig_nheaders = hdr->n;

	/*
	 if (!(W && H)) {
	 // FIXME - if an xylist was given, look for existing IMAGEW, IMAGEH headers.
	 }
	 */
	fits_header_add_int(hdr, "IMAGEW", W, "image width");
	fits_header_add_int(hdr, "IMAGEH", H, "image height");
	qfits_header_add(hdr, "ANRUN", "T", "Solve this field!", NULL);

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

    for (i=0; i<il_size(depths); i++) {
        int depth = il_get(depths, i);
        char key[64];
        sprintf(key, "ANDEPTH%i", (i+1));
		fits_header_addf(hdr, key, "", "%i", depth);
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

	fclose(fout);
	qfits_header_destroy(hdr);

	return 0;
}
