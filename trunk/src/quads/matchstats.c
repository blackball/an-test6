#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#include "matchobj.h"
#include "matchfile.h"
#include "histogram.h"

char* OPTIONS = "hA:B:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options] [<input-match-file> ...]\n"
			"   [-A first-field]  (default 0)\n"
			"   [-B last-field]\n",
			progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char** inputfiles = NULL;
	int ninputfiles = 0;
	int i;
	int firstfield=0, lastfield=INT_MAX;

	int nmatches;
	histogram* hobjsused;
	histogram* htimeused;
	double timeunits = 0.01;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'A':
			firstfield = atoi(optarg);
			break;
		case 'B':
			lastfield = atoi(optarg);
			break;
		case 'h':
		default:
			printHelp(progname);
			exit(-1);
		}
	}
	if (optind < argc) {
		ninputfiles = argc - optind;
		inputfiles = argv + optind;
	} else {
		printHelp(progname);
		exit(-1);
	}

	if (lastfield < firstfield) {
		fprintf(stderr, "Last field (-B) must be at least as big as first field (-A)\n");
		exit(-1);
	}

	hobjsused = histogram_new_binsize(0.0, 200.0, 1.0);
	htimeused = histogram_new_binsize(0.0, 2.0, timeunits);
	nmatches = 0;

	for (i=0; i<ninputfiles; i++) {
		matchfile* mf;
		int f;

		fprintf(stderr, "Opening file %s...\n", inputfiles[i]);
		mf = matchfile_open(inputfiles[i]);
		if (!mf) {
			fprintf(stderr, "Failed to open matchfile %s.\n", inputfiles[i]);
			exit(-1);
		}

		printf("objs_time=[");
		// we assume the matchfiles are sorted by field number.
		for (f=firstfield; f<=lastfield; f++) {
			bool eof = FALSE;
			//fprintf(stderr, "Field %i.\n", f);

			while (1) {
				MatchObj* mo = matchfile_buffered_read_match(mf);
				if (!mo) {
					eof = TRUE;
					break;
				}
				assert(mo->fieldnum >= f);
				if (mo->fieldnum > f) {
					matchfile_buffered_read_pushback(mf);
					break;
				}

				nmatches++;
				histogram_add(hobjsused, mo->objs_tried);
				histogram_add(htimeused, mo->timeused);
				printf("%i,%f;", mo->objs_tried, mo->timeused);
			}
			if (eof)
				break;
		}
		printf("];\n");

		matchfile_close(mf);
	}

	printf("Mean number of objects used: %g\n", histogram_mean(hobjsused));
	printf("objs_used = ");
	histogram_print_matlab(hobjsused, stdout);
	printf(";\n");

	printf("Mean time used: %g s\n", histogram_mean(htimeused));
	printf("time_used = ");
	histogram_print_matlab(htimeused, stdout);
	printf(";\n");

	histogram_free(hobjsused);
	histogram_free(htimeused);

	return 0;
}


