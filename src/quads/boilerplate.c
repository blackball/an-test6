#include <stdio.h>

#include "boilerplate.h"
#include "fitsioutils.h"
#include "svn.h"

void boilerplate_help_header(FILE* fid) {
	fprintf(fid, "This program is part of the Astrometry.net suite.\n");
	fprintf(fid, "For details, visit  http://astrometry.net .\n");
	fprintf(fid, "Subversion URL %s\n", svn_url());
    fprintf(fid, "Revision %i, date %s.\n", svn_revision(), svn_date());
}

void boilerplate_add_fits_headers(qfits_header* hdr) {
	int Nval = 256;
	char val[Nval];
	qfits_header_add(hdr, "HISTORY", "Created by the Astrometry.net suite.", NULL, NULL);
	qfits_header_add(hdr, "HISTORY", "For more details, see http://astrometry.net .", NULL, NULL);
	fits_add_long_history(hdr, "Subversion URL %s", svn_url());
	snprintf(val, Nval, "Subversion revision %i", svn_revision());
	qfits_header_add(hdr, "HISTORY", val, NULL, NULL);
	snprintf(val, Nval, "Subversion date %s", svn_date());
	qfits_header_add(hdr, "HISTORY", val, NULL, NULL);
}

