#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "healpix.h"
#include "starutil.h"
#include "lsfile.h"

int convert_file(char* fn)
{
	int i, j, slen, numfields, npoints, healpixes[12];
	FILE* rdf, *hpf;
	char* hpfn;

	// Figure out the name of the .hpls file: Yummy string parsing in C!
	slen = strlen(fn);
	i = slen - 1;
	while (i >= 0 && fn[i--] != '.')
		;
	if (i == -1) {
		fprintf(stderr, "Missing extension on %s? Need .rdls filename\n", fn);
		return 1;
	}
	hpfn = malloc(slen + 1);
	strcpy(hpfn, fn);
	hpfn[slen - 1] = 's';
	hpfn[slen - 2] = 'l';
	hpfn[slen - 3] = 'p';
	hpfn[slen - 4] = 'h';
	fprintf(stderr, "hpfn: %s\n", hpfn);

	// Open the two files for input and output
	rdf = fopen(fn, "r");
	if (!rdf) {
		fprintf(stderr, "Couldn't open %s: %s\n", fn, strerror(errno));
		return 1;
	}

	hpf = fopen(hpfn, "w");
	if (!hpf) {
		fprintf(stderr, "Couldn't open %s for writing: %s\n", hpfn, strerror(errno));
		return 1;
	}
	free(hpfn);
	hpfn = NULL;

	// First line: numfields
	numfields = read_ls_file_header(rdf);
	if (!numfields == -1) {
		fprintf(stderr, "parse error in %s: numfields\n", fn);
		return 1;
	}

	fprintf(hpf, "NumFields=%i\n", numfields);

	for (j = 0; j < numfields; j++) {
		int nhp;
		// Second line and subsequent lines: npoints,ra,dec,ra,dec,...
		dl* points = read_ls_file_field(rdf, 2);
		if (!points) {
			fprintf(stderr, "parse error in %s: field %i\n", fn, j);
			return 1;
		}

		for (i = 0; i < 12; i++) {
			healpixes[i] = 0;
		}

		npoints = dl_size(points) / 2;

		for (i = 0; i < npoints; i++) {
			double ra, dec;
			int hp;

			ra  = dl_get(points, i*2);
			dec = dl_get(points, i*2 + 1);

			ra *= M_PI / 180.0;
			dec *= M_PI / 180.0;

			hp = radectohealpix(ra, dec);
			if ((hp < 0) || (hp >= 12)) {
				printf("hp=%i\n", hp);
				continue;
			}
			healpixes[hp] = 1;
		}
		nhp = 0;
		for (i = 0; i < 12; i++) {
			nhp++;
		}
		fprintf(hpf, "%i", nhp);
		for (i = 0; i < 12; i++) {
			if (healpixes[i])
				fprintf(hpf, ",%i", i);
		}
		fprintf(hpf, "\n");

		dl_free(points);
	}

	fclose(rdf);
	fclose(hpf);
	return 0;
}

int main(int argc, char** args)
{
	int i;
	if (argc == 1) {
		fprintf(stderr, "Usage: %s <rdls-file> ...\n", args[0]);
		return 1;
	}

	for (i = 1; i < argc; i++)
		convert_file(args[i]);

	return 0;
}
