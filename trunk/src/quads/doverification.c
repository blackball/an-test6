#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "bl.h"
#include "matchobj.h"
#include "hitsfile.h"
#include "hitlist_healpix.h"
#include "matchfile.h"

char* OPTIONS = "ho:H:M:m:v:f:F:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   <-f index-path>\n"
			"   <-F field-filename>\n"
 			"   [-H hits-file]\n"
			"   [-M write-successful-matches-file]\n"
			"   [-m agreement-tolerance-arcsec]\n"
			"   [-v verification-tolerance-arcsec]\n"
 			"   [-o overlaps_needed_to_agree]\n\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

char* agreefname = NULL;
FILE* agreefid = NULL;
FILE *hitfid = NULL;
char *hitfname = NULL;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	hits_header hitshdr;
	char** inputfiles = NULL;
	int ninputfiles = 0;
	int noverlap = 50;
	bool agree = FALSE;
	double agreetolarcsec = 7.0;
	double verifytolarcsec = 3.0;
	int firstfield=0, lastfield=INT_MAX;
	char* indexpath = NULL;
	char* fieldfname = NULL;

	FILE* infiles[ninputfiles];
	int nread = 0;
	int f;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'f':
			indexpath = optarg;
			break;
		case 'F':
			fieldfname = optarg;
			break;
		case 'o':
			noverlap = atoi(optarg);
			break;
		case 'M':
			agreefname = optarg;
			break;
		case 'H':
			hitfname = optarg;
			break;
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		case 'm':
			agreetolarcsec = atof(optarg);
			break;
		case 'v':
			verifytolarcsec = atof(optarg);
			break;
		default:
			return (OPT_ERR);
		}
	}
	if (optind < argc) {
		ninputfiles = argc - optind;
		inputfiles = argv + optind;
	} else {
		printHelp(progname);
	}

	if (hitfname) {
		fopenout(hitfname, hitfid);
	} else {
		hitfid = stdout;
	}
	if (agreefname) {
		fopenout(agreefname, agreefid);
		agree = TRUE;
	}

	// write HITS header.
	hits_header_init(&hitshdr);
	hitshdr.nfields = 0;
	hitshdr.min_matches_to_agree = min_matches_to_agree;
	hits_write_header(hitfid, &hitshdr);

	for (i=0; i<ninputfiles; i++) {
		fprintf(stderr, "Opening file %s...\n", inputfiles[i]);
		fopenin(inputfiles[i], infiles[i]);
	}
	for (f=firstfield; f<=lastfield; f++) {
		bool alldone = TRUE;
		int fieldnum;
		hitlist* hl;

		for (i=0; i<ninputfiles; i++)
			if (infiles[i])
				alldone = FALSE;
		if (alldone)
			break;

		fprintf(stderr, "Field %i.\n", f);

		hl = hitlist_healpix_new(agreetolarcsec);

		for (i=0; i<ninputfiles; i++) {
			MatchObj* mo;
			matchfile_entry me;
			matchfile_entry* mecopy;
			int c;
			FILE* infile = infiles[i];
			int nr = 0;
			char* fname = inputfiles[i];
			int nagree;

			if (!infile)
				continue;

			for (;;) {
				off_t offset = ftello(infile);

				// detect EOF and exit gracefully...
				c = fgetc(infile);
				if (c == EOF) {
					fclose(infile);
					infiles[i] = NULL;
					break;
				} else
					ungetc(c, infile);

				if (matchfile_read_match(infile, &mo, &me)) {
					fprintf(stderr, "Failed to read match from %s: %s\n", fname, strerror(errno));
					fflush(stderr);
					break;
				}
				if (me.fieldnum != fieldnum) {
					fseeko(infile, offset, SEEK_SET);
					free_MatchObj(mo);
					free(me.indexpath);
					free(me.fieldpath);
					break;
				}
				nread++;
				nr++;

				if (nread % 10000 == 9999) {
					fprintf(stderr, ".");
					fflush(stderr);
				}

				if (agree) {
					mecopy = (matchfile_entry*)malloc(sizeof(matchfile_entry));
					memcpy(mecopy, &me, sizeof(matchfile_entry));
					mo->extra = mecopy;
				} else {
					mo->extra = NULL;
					free(me.indexpath);
					free(me.fieldpath);
				}

				// compute (x,y,z) center, scale, rotation.
				hitlist_healpix_compute_vector(mo);
				// add the match...
				nagree = hitlist_healpix_add_hit(hl, mo);
				if (nagree > 1) {


				}
			}
			fprintf(stderr, "File %s: read %i matches.\n", inputfiles[i], nr);
		}

	}

	return 0;
}


