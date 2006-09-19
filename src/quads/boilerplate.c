#include "boilerplate.h"
#include "svn.h"

void boilerplate_help_header(FILE* fid) {
	fprintf(fid, "This program is part of the Astrometry.net suite.\n");
	fprintf(fid, "For details, visit  http://www.astrometry.net .\n");
    fprintf(fid, "Subversion revision %i, date %s.\n", svn_revision(), svn_date());
}


