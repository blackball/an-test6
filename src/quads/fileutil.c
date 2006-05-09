#include <stdlib.h>
#include <string.h>
#include "fileutil.h"

char *mk_filename(const char *basename, const char *extension)
{
	char *fname;
	fname = malloc(strlen(basename) + strlen(extension) + 1);
	sprintf(fname, "%s%s", basename, extension);
	return fname;
}


