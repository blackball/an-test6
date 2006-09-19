#include "svn.h"

const char* svn_date() {
	return "$Date$";
}

int svn_revision() {
	int rev;
	if (sscanf("$Revision$", "Revision: %u", &rev) != 1)
		return -1;
	return rev;
}

