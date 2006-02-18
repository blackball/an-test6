#include <stdio.h>
#include <errno.h>
#include <string.h>

#define NO_KD_INCLUDES 1
#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "blocklist.h"
#include "matchobj.h"
#include "hitsfile.h"
#include "hitlist.h"
#include "matchfile.h"

char* OPTIONS = "h";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-h] help\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	bool fromstdin = FALSE;
	int i;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		default:
			return (OPT_ERR);
		}
	}
	if (optind < argc) {
		ninputfiles = argc - optind;
		inputfiles = argv + optind;
	} else {
		fromstdin = TRUE;
		ninputfiles = 1;
	}

	for (i=0; i<ninputfiles; i++) {
		FILE* infile = NULL;
		char* fname;
		int nread;

		if (fromstdin) {
			fname = "stdin";
		} else {
			fname = inputfiles[i];
		}

		if (fromstdin) {
			infile = stdin;
		} else {
			fopenin(fname, infile);
		}

		fprintf(stderr, "Reading from %s...\n", fname);
		fflush(stderr);
		nread = 0;
		for (;;) {
			MatchObj* mo;
			matchfile_entry me;
			int c;
			int fieldnum;

			// detect EOF and exit gracefully...
			c = fgetc(infile);
			if (c == EOF)
				break;
			else
				ungetc(c, infile);

			if (matchfile_read_match(infile, &mo, &me)) {
				fprintf(stderr, "Failed to read match from %s: %s\n", fname, strerror(errno));
				fflush(stderr);
				break;
			}
			nread++;
			fieldnum = me.fieldnum;

			printf("\n");
			printf("Field %i\n", fieldnum);
			printf("Parity %i\n", me.parity);
			printf("IndexPath %s\n", me.indexpath);
			printf("FieldPath %s\n", me.fieldpath);
			printf("CodeTol %g\n", me.codetol);
			//printf("FieldUnits [%g, %g]\n", me.fieldunits_lower, me.fieldunits_upper);
			printf("Quad %i\n", (int)mo->quadno);
			printf("Stars %i %i %i %i\n", (int)mo->iA, (int)mo->iB, (int)mo->iC, (int)mo->iD);
			printf("FieldObjs %i %i %i %i\n", (int)mo->fA, (int)mo->fB, (int)mo->fC, (int)mo->fD);
			printf("CodeErr %g\n", mo->code_err);

			free(me.indexpath);
			free(me.fieldpath);
		}
		fprintf(stderr, "Read %i matches.\n", nread);
		fflush(stderr);

		if (!fromstdin)
			fclose(infile);
	}


	return 0;
}

