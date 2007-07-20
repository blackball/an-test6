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

 * IMAGEW - Image width. Positive real
 * IMAGEH - Image height. Positive real
 * ANRUN - the "Go" button. 

 * ANPOSERR - Field positional error, in pixels. Positive real. Default 1.
 * ANSOLVED - Output filename for solved file. One byte per field, with value 0x1 if the field solved, 0x0 otherwise. The file is only created if at least one field solves.
 * ANMATCH - Output filename for match file. FITS table describing the quad match that solved the field.
 * ANRDLS - Output filename for index RDLS file. The (RA,Dec) of index stars that overlap the field.
 * ANWCS - Output filename for WCS file. A FITS header containing the header cards required to specify the solved WCS.
 * ANCANCEL - Input filename - if this file exists, the job is cancelled.
 * ANTLIM - Wall-clock time limit, in seconds (default inf)
 * ANCLIM - CPU time limit, in seconds (default inf)
 * ANPARITY - "BOTH", "POS", "NEG" (default "BOTH")
 * ANTWEAK - FITS Boolean ie 'T' or 'F' (default 'T')
 * ANTWEAKO - Tweak order. Integer in [1, 10], default=3. Why 3? Radial distortion requires 4.
 * ANAPPL# - Lower bound on scale (in arcseconds per pixel) for field#, # = 1, 2, ... (default determined by the backend.)
 * ANAPPU# - Upper bound on scale (in arcseconds per pixel) for field#, # = 1, 2, ... (default determined by the backend.)
 * ANDEPTH# - The deepening strategy. # = 1, 2, ... and is the depth (number of field objects) to examine in round #. Eg, ANDEPTH1=20, ANDEPTH2=40 means look up to field object 20 in all indices at all scales, then proceed up to object 40 in all indices at all scales.
 * ANFDL#/ANFDU# - Add fields to be solved, specified in inclusive table HDU ranges starting from 1. The ANFD'''L'''/ANFD'''U''' stands for Upper and Lower. Defaults to all HDU's, but if a single ANFD[LU]# is specified, then only the fields explicitly listed are used. Multiple ANFD[LU]#'s can be present and the solved field are the union of the specified ranges. Only valid in the primary header.
 * ANFD#### - Add single fields to be solved. Only valid in the primary header.
 * ANODDSPR - Odds ratio required before to printing a solution. Default 1e3.
 * ANODDSKP - Odds ratio required before to keep a solution. Default 1e9.
 * ANODDSSL - Odds ratio required before to consider a solution solved. Default 1e9.
 * ANIMFRAC - Fraction of the rectangular image that the field actually occupies (eg for round fields). Real in (0, 1], default 1.
 * ANCTOL - Code tolerance. Units of distance in 4-D code space. Real > 0, default 0.01.
 * ANDISTR - Distractor fraction. Real in (0, 1). Default is 0.25.

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

#include <getopt.h>

#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "math.h"
#include "fitsioutils.h"

#include "qfits.h"

static struct option long_options[] = {
	{"help",		optional_argument, 0, 'h'},
	{"guess-scale", optional_argument, 0, 'g'},
	{"image",		optional_argument, 0, 'i'},
    {"width",       optional_argument, 0, 'w'},
    {"height",      optional_argument, 0, 'e'},
	{"scale-low",	optional_argument, 0, 'L'},
	{"scale-high",	optional_argument, 0, 'H'},
	{"scale-units", optional_argument, 0, 'u'},
	{"tweak-order", optional_argument, 0, 't'},
	{"out",         required_argument, 0, 'o'},
	{"noplot",		optional_argument, 0, 'p'},
	{"nordls",		optional_argument, 0, 'r'},
	{"xylist",		optional_argument, 0, 'x'},
    {"no-tweak",    optional_argument, 0, 'T'},
    {"tweak-order", optional_argument, 0, 'O'},
	{0, 0, 0, 0}
};

static const char* OPTIONS = "hg:i:L:H:u:t:o:prx:w:e:TO:";

static void print_help(const char* progname) {
	printf("Usage:	 %s [options] -o <output augmented xylist filename>\n"
		   "  (	   -i <image-input-file>\n"
		   "   OR  -x <xylist-input-file>  )\n"
		   "\n", progname);
}

static int run_command_get_outputs(char* cmd, pl** outlines, pl** errlines) {
	int outpipe[2];
	int errpipe[2];
	pid_t pid;

	if (outlines) {
		if (pipe(outpipe) == -1) {
			fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
			return -1;
		}
	}
	if (errlines) {
		if (pipe(errpipe) == -1) {
			fprintf(stderr, "Error creating pipe: %s\n", strerror(errno));
			return -1;
		}
	}

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Error fork()ing: %s\n", strerror(errno));
		return -1;
	} else if (pid == 0) {
		// Child process.
		if (outlines) {
			close(outpipe[0]);
			// bind stdout to the pipe.
			if (dup2(outpipe[1], STDOUT_FILENO) == -1) {
				fprintf(stderr, "Failed to dup2 stdout: %s\n", strerror(errno));
				_exit( -1);
			}
		}
		if (errlines) {
			close(errpipe[0]);
			// bind stdout to the pipe.
			if (dup2(errpipe[1], STDERR_FILENO) == -1) {
				fprintf(stderr, "Failed to dup2 stdout: %s\n", strerror(errno));
				_exit( -1);
			}
		}
		// Use a "system"-like command to allow fancier commands.
		if (execlp("/bin/sh", "/bin/sh", "-c", cmd, (char*)NULL)) {
			fprintf(stderr, "Failed to execlp blind: %s\n", strerror(errno));
			_exit( -1);
		}
		// execlp doesn't return.
	} else {
		FILE *fout, *ferr;
		int status;
		// Parent process.
		if (outlines) {
			close(outpipe[1]);
			fout = fdopen(outpipe[0], "r");
			if (!fout) {
				fprintf(stderr, "Failed to fdopen pipe: %s\n", strerror(errno));
				return -1;
			}
		}
		if (errlines) {
			close(errpipe[1]);
			ferr = fdopen(errpipe[0], "r");
			if (!ferr) {
				fprintf(stderr, "Failed to fdopen pipe: %s\n", strerror(errno));
				return -1;
			}
		}
		// Wait for command to finish.
		// FIXME - do we need to read from the pipes to prevent the command
		// from blocking?
		//printf("Waiting for blind to finish (PID %i).\n", (int)pid);
		do {
			if (waitpid(pid, &status, 0) == -1) {
				fprintf(stderr, "Failed to waitpid(): %s.\n", strerror(errno));
				return -1;
			}
			if (WIFSIGNALED(status)) {
				fprintf(stderr, "Command was killed by signal %i.\n", WTERMSIG(status));
				return -1;
			} else {
				int exitval = WEXITSTATUS(status);
				if (exitval == 127) {
					fprintf(stderr, "Command not found.\n");
					return exitval;
				} else if (exitval) {
					fprintf(stderr, "Command failed: return value %i.\n", exitval);
					return exitval;
				}
			}
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
		//printf("Blind finished successfully.\n");

		if (outlines) {
			*outlines = fid_get_lines(fout, FALSE);
			fclose(fout);
		}
		if (errlines) {
			*errlines = fid_get_lines(ferr, FALSE);
			fclose(ferr);
		}
	}
	return 0;
}

int main(int argc, char** args) {
	int c;
	int help_flag = 0;
	char* outfn = NULL;
	char* imagefn = NULL;
	char* xylsfn = NULL;
	char cmd[1024];
	int W, H;
	double scalelo = 0.0, scalehi = 0.0;
	char* scaleunits = NULL;
	qfits_header* hdr;
    bool tweak = TRUE;
    int tweak_order = 0;
    int orig_nheaders;
    FILE* fout;

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
        case 'O':
            tweak_order = atoi(optarg);
            break;
		case '?':
			break;
		default:
			exit( -1);
		}
	}

	if (optind != argc) {
		printf("Extra arguments...\n");
		help_flag = 1;
	}
	if (!outfn) {
		printf("Output filename (-o / --out) is required.\n");
		help_flag = 1;
	}
	if (!(imagefn || xylsfn)) {
		printf("Require either an image (-i / --image) or an XYlist (-x / --xylist) input file.\n");
		help_flag = 1;
	}
	if (!((!scaleunits) ||
          (!strcasecmp(scaleunits, "degwidth")) ||
          (!strcasecmp(scaleunits, "arcminwidth")) ||
          (!strcasecmp(scaleunits, "arcsecperpix")))) {
		printf("Unknown scale units \"%s\".\n", scaleunits);
		help_flag = 1;
	}
    if (xylsfn && !(W && H)) {
        printf("If you give an xylist, you must also specify the image width and height (-w / --width) and (-e / --height).\n");
		help_flag = 1;
    }
	if (help_flag) {
		print_help(args[0]);
		exit(0);
	}

	if (imagefn) {
		// if --image is given:
		//	 -run image2pnm.py
		//	 -if it's a FITS image, keep the original (well, sanitized version)
		//	 -otherwise, run ppmtopgm (if necessary) and pnmtofits.
		//	 -run fits2xy to generate xylist
		char *uncompressedfn;
		char *sanitizedfn;
		char *pnmfn;				
		pl* lines;
		int i;
		bool isfits;
		char *fitsimgfn;
        char* line;
        char pnmtype;
        int maxval;
        char typestr[256];

		uncompressedfn = "/tmp/uncompressed";
		sanitizedfn = "/tmp/sanitized";
		pnmfn = "/tmp/pnm";

		snprintf(cmd, sizeof(cmd),
				 "image2pnm.py --infile \"%s\""
				 "	--uncompressed-outfile \"%s\""
				 "	--sanitized-fits-outfile \"%s\""
				 "	--outfile \"%s\"",
				 imagefn, uncompressedfn, sanitizedfn, pnmfn);
		printf("Running: %s\n", cmd);
		if (run_command_get_outputs(cmd, &lines, NULL)) {
			fprintf(stderr, "Failed to run image2pnm: %s\n", strerror(errno));
			exit(-1);
		}

		isfits = FALSE;
		for (i=0; i<pl_size(lines); i++) {
			printf("  %s\n", (char*)pl_get(lines, i));
			if (!strcmp("fits", (char*)pl_get(lines, i)))
				isfits = TRUE;
		}
		pl_free_elements(lines);
		pl_free(lines);

		// Get image W, H, depth.
		snprintf(cmd, sizeof(cmd), "pnmfile \"%s\"", pnmfn);
		if (run_command_get_outputs(cmd, &lines, NULL)) {
			fprintf(stderr, "Failed to run pnmfile: %s\n", strerror(errno));
			exit(-1);
		}
        if (pl_size(lines) == 0) {
            fprintf(stderr, "No output from pnmfile.\n");
            exit(-1);
        }
        line = pl_get(lines, 0);
        // eg   "/tmp/pnm:       PPM raw, 800 by 510  maxval 255"
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

		// FIXME - Do something with the PNM?

		if (isfits) {

			// FIXME - guess scale, if required.

			fitsimgfn = sanitizedfn;

		} else {
			fitsimgfn = "/tmp/fits";

			if (pnmtype == 'P') {
				printf("Converting PPM image to FITS...\n");
				snprintf(cmd, sizeof(cmd),
						 "ppmtopgm \"%s\" | pnmtofits > \"%s\"", pnmfn, fitsimgfn);
				if (run_command_get_outputs(cmd, NULL, NULL)) {
					fprintf(stderr, "Failed to convert PPM to FITS.\n");
					exit(-1);
				}
			} else {
				printf("Converting PGM image to FITS...\n");
				snprintf(cmd, sizeof(cmd),
						 "pnmtofits %s > \"%s\"", pnmfn, fitsimgfn);
				if (run_command_get_outputs(cmd, NULL, NULL)) {
					fprintf(stderr, "Failed to convert PGM to FITS.\n");
					exit(-1);
				}
			}
		}

		printf("Running fits2xy...\n");

		xylsfn = "/tmp/xyls";

		snprintf(cmd, sizeof(cmd),
				 "fits2xy -O -o \"%s\" \"%s\"", xylsfn, fitsimgfn);
		printf("Command: %s\n", cmd);
		if (run_command_get_outputs(cmd, NULL, NULL)) {
			fprintf(stderr, "Failed to run fits2xy.\n");
			exit(-1);
		}

	} else {
		// xylist.
		// if --xylist is given:
		//	 -fits2fits.py sanitize
		char* sanexylsfn;

		sanexylsfn = "/tmp/sanexyls";
		snprintf(cmd, sizeof(cmd),
				 "fits2fits.py \"%s\" \"%s\"", xylsfn, sanexylsfn);
		printf("Command: %s\n", cmd);
		if (run_command_get_outputs(cmd, NULL, NULL)) {
			fprintf(stderr, "Failed to run fits2fits.py\n");
			exit(-1);
		}
		xylsfn = sanexylsfn;

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

        fits_header_add_double(hdr, "ANAPPL1", appl, "scale: arcsec/pixel min");
        fits_header_add_double(hdr, "ANAPPU1", appu, "scale: arcsec/pixel max");
	}

    qfits_header_add(hdr, "ANTWEAK", (tweak ? "T" : "F"), (tweak ? "Tweak: yes please!" : "Tweak: no, thanks."), NULL);
    if (tweak && tweak_order) {
        fits_header_add_int(hdr, "ANTWEAKO", tweak_order, "Tweak order");
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
        int i;
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
        printf("Copying FITS blocks %i to %i from xyls to output.\n", startblock, nblocks);
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

    fclose(fout);
    qfits_header_destroy(hdr);

	return 0;
}
