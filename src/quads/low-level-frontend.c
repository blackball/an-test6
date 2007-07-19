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

#include <getopt.h>

#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "math.h"
#include "fitsioutils.h"
#include "fileutil.h"

#include "qfits.h"

static struct option long_options[] = {
	{"help",		optional_argument, 0, 'h'},
	{"guess-scale", optional_argument, 0, 'g'},
	{"image",		optional_argument, 0, 'i'},
	{"scale-low",	optional_argument, 0, 'L'},
	{"scale-high",	optional_argument, 0, 'H'},
	{"scale-units", optional_argument, 0, 'u'},
	{"tweak-order", optional_argument, 0, 't'},
	{"out",			required_argument, 0, 'o'},
	{"noplot",		optional_argument, 0, 'p'},
	{"nordls",		optional_argument, 0, 'r'},
	{"xylist",		optional_argument, 0, 'x'},
	{0, 0, 0, 0}
};

static const char* OPTIONS = "hg:i:L:H:u:t:o:prx:";

static void print_help(const char* progname) {
	printf("Usage:	 %s [options] -o <output augmented xylist filename>\n"
		   "\n", progname);
}

int main(int argc, char** args) {
	int c;
	int help_flag = 0;

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
	if (help_flag) {
		print_help(args[0]);
		exit(0);
	}




	// if --image is given:
	//	 -run image2pnm.py
	//	 -if it's a FITS image, keep the original (well, sanitized version)
	//	 -otherwise, run ppmtopgm (if necessary) and pnmtofits.
	//	 -run fits2xy to generate xylist

	// if --xylist is given:
	//	 -fits2fits.py?

	// start piling FITS headers in there.


	return 0;
}
