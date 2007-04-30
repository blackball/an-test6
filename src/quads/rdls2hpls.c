/*
  This file is part of the Astrometry.net suite.
  Copyright 2006-2007, Dustin Lang, Keir Mierle and Sam Roweis.

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

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "healpix.h"
#include "starutil.h"
#include "rdlist.h"

int convert_file(char* fn)
{
	int i, j, slen, numfields, npoints, healpixes[12];
	FILE* hpf;
	rdlist* rdls;
	char* hpfn;

	// Figure out the name of the .hpls file: Yummy string parsing in C!
	slen = strlen(fn);
	i = slen - 1;
	for (;;) {
		while (i >= 0 && fn[i] != '.')
			i--;
		if (i == -1) {
			fprintf(stderr, "Missing extension on %s? Need .rdls filename\n", fn);
			return 1;
		}
		if (strcmp(fn + i+1, "rdls") == 0)
			break;
	}
	hpfn = malloc(slen + 1);
	strcpy(hpfn, fn);
	hpfn[i + 1] = 'h';
	hpfn[i + 2] = 'p';
	hpfn[i + 3] = 'l';
	hpfn[i + 4] = 's';
	fprintf(stderr, "hpfn: %s\n", hpfn);

	// Open the two files for input and output
	rdls = rdlist_open(fn);
	if (!rdls) {
		fprintf(stderr, "Couldn't open RDLS %s.\n", fn);
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
	numfields = rdlist_n_fields(rdls);
	fprintf(hpf, "NumFields=%i\n", numfields);

	for (j = 0; j < numfields; j++) {
		int nhp;
		// Second line and subsequent lines: npoints,ra,dec,ra,dec,...
		dl* points = rdlist_get_field(rdls, j);
		if (!points) {
			fprintf(stderr, "Failed to read RDLS field %i.\n", j);
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

			ra=deg2rad(ra);
			dec=deg2rad(dec);

			hp = radectohealpix(ra, dec, 1);
			if ((hp < 0) || (hp >= 12)) {
				//printf("hp=%i\n", hp);
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

	rdlist_close(rdls);
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
