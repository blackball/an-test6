#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "starutil.h"
#include "fileutil.h"
#include "mathutil.h"
#include "matchobj.h"
#include "matchfile.h"

char* OPTIONS = "ho:";

void printHelp(char* progname) {
	fprintf(stderr, "Usage: %s [options]\n"
			"   -o <output-match-file>\n",
			progname);
}

int main(int argc, char *argv[]) {
    int argchar;
	char* progname = argv[0];
	char* matchfname = NULL;
	int i;
	matchfile* mf;
	MatchObj mo;

    while ((argchar = getopt (argc, argv, OPTIONS)) != -1) {
		switch (argchar) {
		case 'h':
			printHelp(progname);
			return (HELP_ERR);
		case 'o':
			matchfname = optarg;
			break;
		}
	}
	if (!matchfname) {
		printHelp(progname);
		exit(-1);
	}

	mf = matchfile_open_for_writing(matchfname);
	if (!mf) {
		fprintf(stderr, "Failed to open matchfile for writing: %s\n", matchfname);
		exit(-1);
	}

	memset(&mo, 0, sizeof(MatchObj));
	mo.sMin[0] = 0.980323;
	mo.sMin[1] = 0.009857;
	mo.sMin[2] = -0.197152;
	mo.sMax[0] = 0.984121;
	mo.sMax[1] = -0.008832;
	mo.sMax[2] = -0.177282;

	if (matchfile_write_header(mf) ||
		matchfile_write_match(mf, &mo) ||
		matchfile_fix_header(mf) ||
		matchfile_close(mf)) {
		fprintf(stderr, "Error writing match.\n");
		exit(-1);
	}

	return 0;
}
