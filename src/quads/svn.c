#include <stdio.h>
#include <string.h>

#include "svn.h"

static char rtnval[64];

const char* svn_date() {
	/* Through the magic of Subversion, the date string on the following line will
	   be replaced by the correct, updated string whenever you run "svn up".  All hail
	   subversion! */
	const char* datestr = "$Date$";
	// (I want to trim off the "$Date$" trailing: 7 chars at front and 2 at back)
	strncpy(rtnval, datestr + 7, strlen(datestr) - 9);
	return rtnval;
}

int svn_revision() {
	int rev;
	/* See the comment above; the same thing is true of "revstr". Huzzah! */
	const char* revstr = "$Revision$";
	// revstr+1 to avoid having "$" in the format string - otherwise svn seems to
	// consider it close enough to the Revision keyword anchor to do replacement!
	if (sscanf(revstr + 1, "Revision: %i $", &rev) != 1)
		return -1;
	return rev;
}

