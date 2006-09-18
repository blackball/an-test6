/*
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include "pnm.h"

#include "pnmutils.h"

static const char* OPTIONS = "h";

static void printHelp(char* progname) {
	fprintf(stderr, "usage: %s\n"
			"    <input-image-1>  <input-image-2>\n"
			"Writes gain values that yield the nearest match between the images.\n"
			"\n", progname);
}

extern char *optarg;
extern int optind, opterr, optopt;

int
main(int argc, char** args) {
	char* progname = args[0];
	int argchar;
	char* fn1;
	char* fn2;
	double redgain, bluegain, greengain;

    pnm_init(&argc, args);

    while ((argchar = getopt (argc, args, OPTIONS)) != -1)
        switch (argchar) {
		case 'h':
			printHelp(progname);
			exit(0);
		}

	if (optind != argc - 2) {
		printHelp(progname);
		exit(-1);
	}
	fn1 = args[optind];
	fn2 = args[optind+1];

	if (pnmutils_whitebalance(fn1, fn2, &redgain, &greengain, &bluegain)) {
		exit(-1);
	}

	printf("R %g, G %g, B %g\n", redgain, greengain, bluegain);

    return 0;
}
