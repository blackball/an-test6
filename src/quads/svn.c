#include <stdio.h>
#include <string.h>

#include "svn.h"

static char rtnval[64];

const char* svn_date() {
	/* Through the magic of Subversion, the date string on the following line will
	   be replaced by the correct, updated string whenever you run "svn up".  All hail
	   subversion! */
	const char* datestr = "$Date$";
	strncpy(rtnval, datestr + 7, strlen(datestr) - 9);
	return rtnval;
}

int svn_revision() {
	int rev;
	/* See the comment above; the same thing is true of "revstr". Huzzah! */
	if (sscanf("$Revision$", "$Revision$", &rev) != 1)
		return -1;
	return rev;
}

