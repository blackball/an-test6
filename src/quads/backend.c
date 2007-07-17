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
 * Accepts an augmented xylist that describes a field or set of fields to solve.
 * Reads a config file to find local indices, and merges information about the
 * indices with the job description to create an input file for 'blind'.  Runs blind
 * and merges the results.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <getopt.h>

#include "ioutils.h"
#include "bl.h"
#include "an-bool.h"
#include "solver.h"
#include "math.h"
#include "fitsioutils.h"

#include "qfits.h"

static int help_flag;

static struct option long_options[] =
	{
		// flags
		//{"verbose", no_argument,       &verbose_flag, 1},
		{"help",    no_argument,       &help_flag,    1},
		{"config",  optional_argument,   0, 'c'},
		{0, 0, 0, 0}
	};

static const char* OPTIONS = "hc:";

static void print_help(const char* progname) {
	printf("Usage:   %s [options] <augmented xylist>\n"
		   "\n", progname);
}

static int parse_config_file(FILE* fconf, pl* indexes) {
	while (1) {
        char buffer[10240];
        char* nextword;
        char* line;
        if (!fgets(buffer, sizeof(buffer), fconf)) {
			if (feof(fconf))
				break;
			printf("Failed to read a line from the config file: %s\n", strerror(errno));
            return -1;
		}
        line = buffer;
        // strip off newline
        if (line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = '\0';
        // skip leading whitespace:
        while (*line && isspace(*line))
            line++;
		// skip comments
        if (line[0] == '#')
            continue;
        // skip blank lines.
        if (line[0] == '\0')
            continue;

		if (is_word(line, "index ", &nextword)) {
			pl_append(indexes, strdup(nextword));
		} else {
			printf("Didn't understand this config file line: \"%s\"\n", line);
		}

	}
	return 0;
}

struct job_t {
	char* fieldfile;
	double imagew;
	double imageh;
	bool run;
	double poserr;
	char* solvedfile;
	char* matchfile;
	char* rdlsfile;
	char* wcsfile;
	char* cancelfile;
	int timelimit;
	int cpulimit;
	int parity;
	bool tweak;
	int tweakorder;
	dl* scales;
	il* depths;
	il* fields;
	double odds_toprint;
	double odds_tokeep;
	double odds_tosolve;
	double image_fraction;
	double codetol;
	double distractor_fraction;
	// Contains tan_t structs.
	bl* verify_wcs;
};
typedef struct job_t job_t;

job_t* job_new() {
	job_t* job = calloc(1, sizeof(job_t));
	if (!job) {
		printf("Failed to allocate a new job_t.\n");
		return NULL;
	}
	// Default values:
	job->poserr = 1.0;
	job->parity = PARITY_BOTH;
	job->tweak = TRUE;
	job->tweakorder = 3;
	job->scales = dl_new(8);
	job->depths = il_new(8);
	job->fields = il_new(32);
	job->odds_toprint = 1e3;
	job->odds_tokeep = 1e9;
	job->odds_tosolve = 1e9;
	job->image_fraction = 1.0;
	job->codetol = 0.01;
	job->distractor_fraction = 0.25;
	job->verify_wcs = bl_new(8, sizeof(tan_t));

	return job;
}

void job_free(job_t* job) {
	if (!job)
		return;
	dl_free(job->scales);
	il_free(job->depths);
	il_free(job->fields);
	bl_free(job->verify_wcs);
	free(job);
}

void job_print(job_t* job) {
	int i;
	printf("Image size: %g x %g\n", job->imagew, job->imageh);
	printf("Positional error: %g pix\n", job->poserr);
	printf("Solved file: %s\n", job->solvedfile);
	printf("Match file: %s\n", job->matchfile);
	printf("RDLS file: %s\n", job->rdlsfile);
	printf("WCS file: %s\n", job->wcsfile);
	printf("Cancel file: %s\n", job->cancelfile);
	printf("Time limit: %i sec\n", job->timelimit);
	printf("CPU limit: %i sec\n", job->cpulimit);
	printf("Parity: %s\n", (job->parity == PARITY_NORMAL ? "pos" :
							(job->parity == PARITY_FLIP ? "neg" :
							 (job->parity == PARITY_BOTH ? "both" : "(unknown)"))));
	printf("Tweak: %s\n", (job->tweak ? "yes" : "no"));
	printf("Tweak order: %i\n", job->tweakorder);
	printf("Odds to print: %g\n", job->odds_toprint);
	printf("Odds to keep: %g\n", job->odds_tokeep);
	printf("Odds to solve: %g\n", job->odds_tosolve);
	printf("Image fraction: %g\n", job->image_fraction);
	printf("Distractor fraction: %g\n", job->distractor_fraction);
	printf("Code tolerance: %g\n", job->codetol);
	printf("Scale ranges:\n");
	for (i=0; i<dl_size(job->scales)/2; i++) {
		double lo, hi;
		lo = dl_get(job->scales, i*2);
		hi = dl_get(job->scales, i*2+1);
		printf("  [%g, %g] arcsec/pix\n", lo, hi);
	}
	printf("Depths:");
	for (i=0; i<il_size(job->depths); i++) {
		int depth = il_get(job->depths, i);
		printf(" %i", depth);
	}
	printf("\n");
	printf("Fields:");
	for (i=0; i<il_size(job->fields)/2; i++) {
		int lo, hi;
		lo = il_get(job->fields, i*2);
		hi = il_get(job->fields, i*2+1);
		if (lo == hi)
			printf(" %i", lo);
		else
			printf(" %i-%i", lo, hi);
	}
	printf("\n");
	printf("Verify WCS:\n");
	for (i=0; i<bl_size(job->verify_wcs); i++) {
		tan_t* wcs = bl_access(job->verify_wcs, i);
		printf("  crpix (%g, %g)\n", wcs->crpix[0], wcs->crpix[1]);
		printf("  crval (%g, %g)\n", wcs->crval[0], wcs->crval[1]);
		printf("  cd  = ( %g, %g )\n", wcs->cd[0][0], wcs->cd[0][1]);
		printf("        ( %g, %g )\n", wcs->cd[1][0], wcs->cd[1][1]);
	}
	printf("Run: %s\n", (job->run ? "yes" : "no"));
}

/*
Required keywords:

    * IMAGEW - Image width. Positive real
    * IMAGEH - Image height. Positive real
    * ANRUN - the "Go" button. 

Optional keywords:

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
    * ANINTRLV - The interleaving strategy to use. Will be a string. 

WCS to verify: if the image already has a WCS that you want to verify, convert it to TAN format and specify it using these headers. If there are multiple WCS instances to try, list them all, using increasing # values for each one. The first one should have #=1, the second one #=2, etc.

    * ANW#PIX1 CRPIX1
    * ANW#PIX2 CRPIX2
    * ANW#VAL1 CRVAL1
    * ANW#VAL2 CRVAL2
    * ANW#CD11 CD1_1
    * ANW#CD12 CD1_2
    * ANW#CD21 CD2_1
    * ANW#CD22 CD2_2 
*/

void job_write_blind_input(job_t* job, FILE* fout, pl* indexes) {
	int i, j, k;
	bool firsttime = TRUE;
	fprintf(fout, "timelimit %i\n", job->timelimit);
	fprintf(fout, "cpulimit %i\n", job->cpulimit);
	for (i=0;; i++) {
		int startobj, endobj;
		if (il_size(job->depths) < 2) {
			if (i > 0)
				break;
			startobj = 0;
			endobj = 0;
		} else {
			if (i >= il_size(job->depths)-1)
				break;
			startobj = il_get(job->depths, i);
			endobj = il_get(job->depths, i+1);
		}

		for (j=0; j<dl_size(job->scales)/2; j++) {
			fprintf(fout, "sdepth %i\n", startobj);
			if (endobj)
				fprintf(fout, "depth %i\n", endobj);
			fprintf(fout, "fieldunits_lower %g\n", dl_get(job->scales, j*2));
			fprintf(fout, "fieldunits_upper %g\n", dl_get(job->scales, j*2+1));

			// FIXME - select the indices that should be checked.
			for (k=0; k<pl_size(indexes); k++) {
				fprintf(fout, "index %s\n", (char*)pl_get(indexes, k));
			}

			fprintf(fout, "fields");
			for (k=0; k<il_size(job->fields)/2; k++) {
				int lo = il_get(job->fields, k*2);
				int hi = il_get(job->fields, k*2+1);
				if (lo == hi)
					fprintf(fout, " %i", lo);
				else
					fprintf(fout, " %i/%i", lo, hi);
			}
			fprintf(fout, "\n");

			fprintf(fout, "parity %i\n", job->parity);
			fprintf(fout, "verify_pix %g\n", job->poserr);
			fprintf(fout, "tol %g\n", job->codetol);
			fprintf(fout, "distractors %g\n", job->distractor_fraction);
			fprintf(fout, "ratio_toprint %g\n", job->odds_toprint);
			fprintf(fout, "ratio_tokeep %g\n", job->odds_tokeep);
			fprintf(fout, "ratio_tosolve %g\n", job->odds_tosolve);
			fprintf(fout, "ratio_tobail %g\n", 1e-100);

			if (job->tweak) {
				fprintf(fout, "tweak\n");
				fprintf(fout, "tweak_aborder %i\n", job->tweakorder);
				fprintf(fout, "tweak_abporder %i\n", job->tweakorder);
				fprintf(fout, "tweak_skipshift\n");
			}

			fprintf(fout, "field %s\n", job->fieldfile);
			if (job->solvedfile)
				fprintf(fout, "solved %s\n", job->solvedfile);
			if (job->matchfile)
				fprintf(fout, "match %s\n", job->matchfile);
			if (job->rdlsfile)
				fprintf(fout, "indexrdls %s\n", job->rdlsfile);
			if (job->wcsfile)
				fprintf(fout, "wcs %s\n", job->wcsfile);
			if (job->cancelfile)
				fprintf(fout, "cancel %s\n", job->cancelfile);

			if (firsttime) {
				for (k=0; k<bl_size(job->verify_wcs); k++) {
					tan_t* wcs = bl_access(job->verify_wcs, k);
					fprintf(fout, "verify_wcs %g %g %g %g %g %g %g %g\n",
							wcs->crval[0], wcs->crval[1],
							wcs->crpix[0], wcs->crpix[1],
							wcs->cd[0][0], wcs->cd[0][1],
							wcs->cd[1][0], wcs->cd[1][1]);
				}
				firsttime = FALSE;
			}

			fprintf(fout, "run\n\n");
		}

	}
}

   
int main(int argc, char** args) {
	int c;
	char* configfn = "backend.cfg";
	FILE* fconf;
	int i;
	pl* indexes;

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
			printf("option %s", long_options[option_index].name);
			if (optarg)
				printf(" with arg %s", optarg);
			printf("\n");
			break;
		case 'h':
			help_flag = 1;
			break;
		case 'c':
			configfn = optarg;
			break;
		case '?':
			break;
		default:
			exit(-1);
		}
	}

	if (optind == argc) {
		// Need extra args: filename
		printf("You must specify a job file.");
		help_flag = 1;
	}
	if (help_flag) {
		print_help(args[0]);
		exit(0);
	}

	indexes = pl_new(16);

	// Read config file.
	fconf = fopen(configfn, "r");
	if (!fconf) {
		printf("Failed to open config file \"%s\": %s.\n", configfn, strerror(errno));
		exit(-1);
	}
	if (parse_config_file(fconf, indexes)) {
		printf("Failed to parse config file.\n");
		exit(-1);
	}
	fclose(fconf);

	if (!pl_size(indexes)) {
		printf("You must list at least one index in the config file (%s)\n", configfn);
		exit(-1);
	}

	for (i=optind; i<argc; i++) {
		char* jobfn;
		qfits_header* hdr;
		job_t* job = NULL;
		double dnil = -HUGE_VAL;
		char* pstr;
		int n;

		jobfn = args[i];

		// Read primary header.
		hdr = qfits_header_read(jobfn);
		if (!hdr) {
			printf("Failed to parse FITS header from file \"%s\".\n", jobfn);
			exit(-1);
		}

		job = job_new();
		if (!job) {
			printf("Failed to allocate a job struct.\n");
			exit(-1);
		}

		job->fieldfile = jobfn;
		job->imagew = qfits_header_getdouble(hdr, "IMAGEW", dnil);
		job->imageh = qfits_header_getdouble(hdr, "IMAGEH", dnil);
		if ((job->imagew == dnil) || (job->imageh == dnil) ||
			(job->imagew <= 0.0) || (job->imageh <= 0.0)) {
			printf("Must specify positive \"IMAGEW\" and \"IMAGEH\".\n");
			exit(-1);
		}
		job->run = qfits_header_getboolean(hdr, "ANRUN", 0);
		job->poserr = qfits_header_getdouble(hdr, "ANPOSERR", job->poserr);
		job->solvedfile = fits_get_dupstring(hdr, "ANSOLVED");
		job->matchfile = fits_get_dupstring(hdr, "ANMATCH");
		job->rdlsfile = fits_get_dupstring(hdr, "ANRDLS");
		job->wcsfile = fits_get_dupstring(hdr, "ANWCS");
		job->cancelfile = fits_get_dupstring(hdr, "ANCANCEL");
		job->timelimit = qfits_header_getint(hdr, "ANTLIM", job->timelimit);
		job->cpulimit = qfits_header_getint(hdr, "ANCLIM", job->cpulimit);
		pstr = qfits_pretty_string(qfits_header_getstr(hdr, "ANPARITY"));
		if (pstr && !strcmp(pstr, "NEG")) {
			job->parity = PARITY_FLIP;
		} else if (pstr && !strcmp(pstr, "POS")) {
			job->parity = PARITY_NORMAL;
		}
		job->tweak = qfits_header_getboolean(hdr, "ANTWEAK", job->tweak);
		job->tweakorder = qfits_header_getint(hdr, "ANTWEAKO", job->tweakorder);
		n = 1;
		while (1) {
			char key[64];
			double lo, hi;
			sprintf(key, "ANAPPL%i", n);
			lo = qfits_header_getdouble(hdr, key, dnil);
			if (lo == dnil)
				break;
			sprintf(key, "ANAPPU%i", n);
			hi = qfits_header_getdouble(hdr, key, dnil);
			if (hi == dnil)
				break;
			dl_append(job->scales, lo);
			dl_append(job->scales, hi);
			n++;
		}
		n = 1;
		while (1) {
			char key[64];
			int depth;
			sprintf(key, "ANDEPTH%i", n);
			depth = qfits_header_getint(hdr, key, -1);
			if (depth == -1)
				break;
			il_append(job->depths, depth);
			n++;
		}
		n = 1;
		while (1) {
			char key[64];
			int lo, hi;
			sprintf(key, "ANFDL%i", n);
			lo = qfits_header_getint(hdr, key, -1);
			if (lo == -1)
				break;
			sprintf(key, "ANFDU%i", n);
			hi = qfits_header_getint(hdr, key, -1);
			if (hi == -1)
				break;
			il_append(job->fields, lo);
			il_append(job->fields, hi);
			n++;
		}
		n = 1;
		while (1) {
			char key[64];
			int fld;
			sprintf(key, "ANFD%i", n);
			fld = qfits_header_getint(hdr, key, -1);
			if (fld == -1)
				break;
			il_append(job->fields, fld);
			il_append(job->fields, fld);
			n++;
		}
		job->odds_toprint = qfits_header_getdouble(hdr, "ANODDSPR", job->odds_toprint);
		job->odds_tokeep  = qfits_header_getdouble(hdr, "ANODDSKP", job->odds_tokeep);
		job->odds_tosolve = qfits_header_getdouble(hdr, "ANODDSSL", job->odds_tosolve);
		job->image_fraction = qfits_header_getdouble(hdr, "ANIMFRAC", job->image_fraction);
		job->codetol = qfits_header_getdouble(hdr, "ANCTOL", job->codetol);
		job->distractor_fraction = qfits_header_getdouble(hdr, "ANDISTR", job->distractor_fraction);
		n = 1;
		while (1) {
			char key[64];
			tan_t wcs;
			char* keys[] = { "ANW%iPIX1", "ANW%iPIX2", "ANW%iVAL1", "ANW%iVAL2",
							 "ANW%iCD11", "ANW%iCD12", "ANW%iCD21", "ANW%iCD22" };
			double* vals[] = { &(wcs. crval[0]), &(wcs.crval[1]),
							   &(wcs.crpix[0]), &(wcs.crpix[1]),
							   &(wcs.cd[0][0]), &(wcs.cd[0][1]),
							   &(wcs.cd[1][0]), &(wcs.cd[1][1]) };
			int j;
			int bail = 0;
			for (j=0; j<8; j++) {
				sprintf(key, keys[j], n);
				*(vals[j]) = qfits_header_getdouble(hdr, key, dnil);
				if (*(vals[j]) == dnil) {
					bail = 1;
					break;
				}
			}
			if (bail)
				break;

			bl_append(job->verify_wcs, &wcs);
			n++;
		}

		// Add some defaults:

		// Default: solve first field.
		if (job->run && !il_size(job->fields)) {
			il_append(job->fields, 0);
		}

		// Default: image width 0.1 - 180 degrees.
		// FIXME - this should depend on the available indices!
		if (!dl_size(job->scales)) {
			double arcsecperpix;
			arcsecperpix = deg2arcsec(0.1) / job->imagew;
			dl_append(job->scales, arcsecperpix);
			arcsecperpix = deg2arcsec(180) / job->imagew;
			dl_append(job->scales, arcsecperpix);
		}

		// Go!
		printf("Running job:\n");
		job_print(job);

		job_write_blind_input(job, stdout, indexes);




		//cleanup:
		job_free(job);
		qfits_header_destroy(hdr);
	}


	for (i=0; i<pl_size(indexes); i++)
		free(pl_get(indexes, i));
	pl_free(indexes);

	exit(0);
}
