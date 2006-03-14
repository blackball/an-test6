#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "matchobj.h"
#include "hitsfile.h"
#include "matchfile.h"

char* OPTIONS = "hi:f:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<match-file-to-fix> ...]\n"
			"     [-i <index-path>]\n"
			"     [-f <field-path>]\n"
			"\nIf no match files are given, will read from stdin.\n",
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
	char* indexpath = NULL;
	char* fieldpath = NULL;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'i':
			indexpath = optarg;
			break;
		case 'f':
			fieldpath = optarg;
			break;
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
		FILE* outfile = NULL;
		char* fname;
		int nread;
		char outfname[256];

		if (fromstdin) {
			infile = stdin;
			fname = "stdin";
		} else {
			fname = inputfiles[i];
			sprintf(outfname, "%s.tmp", fname);
			fopenin(fname, infile);
			fopenout(outfname, outfile);
		}

		fprintf(stderr, "Reading from %s...\n", fname);
		nread = 0;
		for (;;) {
			MatchObj* mo;
			matchfile_entry me;
			int c;

			// detect EOF and exit gracefully...
			c = fgetc(infile);
			if (c == EOF)
				break;
			else
				ungetc(c, infile);

			if (matchfile_read_match(infile, &mo, &me)) {
				fprintf(stderr, "Failed to read match from %s: %s\n", fname, strerror(errno));
				break;
			}
			if (indexpath)
				me.indexpath = indexpath;
			if (fieldpath)
				me.fieldpath = fieldpath;

			if (matchfile_write_match(outfile, mo, &me)) {
				fprintf(stderr, "Failed to write match to %s: %s\n", outfname, strerror(errno));
				break;
			}
			
			nread++;

		}
		fprintf(stderr, "Read %i matches.\n", nread);

		if (!fromstdin) {
			fclose(infile);
			fclose(outfile);
			if (rename(outfname, fname)) {
				fprintf(stderr, "Failed to rename file %s to %s: %s\n",
						outfname, fname, strerror(errno));
			}
		}
	}

	return 0;
}

